#include "hermite_resampler.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <numeric>

namespace alia {
    namespace {
        std::uint64_t add_frames(std::uint64_t a, std::uint64_t b) {
            if (b > (std::numeric_limits<std::uint64_t>::max)() - a)
                throw std::length_error("resampler frame count overflow");
            return a + b;
        }

        std::uint64_t multiply_frames(std::uint64_t a, std::uint64_t b) {
            if (a && b > (std::numeric_limits<std::uint64_t>::max)() / a)
                throw std::length_error("resampler frame count overflow");
            return a * b;
        }

        std::size_t frame_count(std::uint64_t n) {
            if (n > (std::numeric_limits<std::size_t>::max)())
                throw std::length_error("resampler frame count is too large");
            return static_cast<std::size_t>(n);
        }

        std::array<float, 6> hermite_weights(float t) noexcept {
            // Niemitalo's x-form coefficients expanded by tap, ordered y[-2..3].
            // Integer coefficients share a factor of 1/24. At t=0 and t=1 these
            // give the unit vectors on y[0] and y[1], respectively.
            // https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf, p. 45.
            constexpr float scale = 1.0f / 24.0f;
            const auto t2 = t * t;
            return {
                scale * t * (2 + t * (-3 + t * (-1 + t * (3 - t)))),
                scale * t * (-16 + t * (26 + t * (-1 + t * (-14 + 5 * t)))),
                1 + scale * t2 * (-50 + t * (10 + t * (26 - 10 * t))),
                scale * t * (16 + t * (36 + t * (-14 + t * (-24 + 10 * t)))),
                scale * t * (-2 + t * (-11 + t * (7 + t * (11 - 5 * t)))),
                scale * t2 * (2 + t * (-1 + t * (-2 + t)))
            };
        }
    } // namespace

    hermite_resampler::hermite_resampler(int channels, int input_rate, int output_rate)
        : audio_effect(
              {{sample_point_formats::f32, channels}, input_rate}, {{sample_point_formats::f32, channels}, output_rate}
          ),
          channels_(static_cast<std::size_t>(channels)) {
        const auto divisor = std::gcd(input_rate, output_rate);
        numerator_ = static_cast<std::uint64_t>(input_rate / divisor);
        denominator_ = static_cast<std::uint64_t>(output_rate / divisor);
        inv_denominator_ = static_cast<float>(1.0 / denominator_);
        if (channels_ > (std::numeric_limits<std::size_t>::max)() / sizeof(float) / buffer_frames)
            throw std::length_error("resampler window is too large");
        buf_ = std::make_unique<float[]>(buffer_frames * channels_);
        do_reset();
    }

    hermite_resampler::hermite_resampler(hermite_resampler &&) noexcept = default;
    hermite_resampler &hermite_resampler::operator=(hermite_resampler &&) noexcept = default;

    void hermite_resampler::do_reset() noexcept {
        std::fill_n(buf_.get(), 2 * channels_, 0.0f);
        held_ = pos_ = 2;
        frac_ = skip_ = 0;
    }

    std::size_t hermite_resampler::do_input_frames_for_output(std::size_t n) const {
        if (!n)
            return 0;
        const auto phase = add_frames(frac_, multiply_frames(n - 1, numerator_));
        const auto end = add_frames(add_frames(pos_, phase / denominator_), 4);
        return frame_count(add_frames(skip_, end > held_ ? end - held_ : 0));
    }

    std::size_t hermite_resampler::do_output_frames_for_input(std::size_t m) const {
        const auto available = add_frames(held_, m > skip_ ? m - skip_ : 0);
        const auto first_end = add_frames(pos_, 4);
        if (available < first_end)
            return 0;
        const auto limit = multiply_frames(add_frames(available - first_end, 1), denominator_);
        return frame_count(add_frames((limit - 1 - frac_) / numerator_, 1));
    }

    void hermite_resampler::compact() noexcept {
        const auto discard = pos_ - 2;
        if (discard > held_) {
            skip_ = discard - held_;
            held_ = 0;
        } else {
            held_ -= discard;
            if (held_ && discard)
                std::memmove(buf_.get(), buf_.get() + discard * channels_, held_ * channels_ * sizeof(float));
        }
        pos_ = 2;
    }

    audio_effect_progress hermite_resampler::do_process(
        const float *in, std::size_t in_frames, float *out, std::size_t out_frames
    ) {
        audio_effect_progress progress;
        const auto integer_step = static_cast<std::size_t>(numerator_ / denominator_);
        const auto fractional_step = numerator_ % denominator_;
        while (progress.produced < out_frames) {
            const auto discarded = (std::min)(skip_, in_frames - progress.consumed);
            skip_ -= discarded;
            progress.consumed += discarded;
            if (skip_)
                break;

            const auto copied = (std::min)({
                do_input_frames_for_output(out_frames - progress.produced),
                in_frames - progress.consumed,
                buffer_frames - held_
            });
            if (copied) {
                std::copy_n(in + progress.consumed * channels_, copied * channels_, buf_.get() + held_ * channels_);
                progress.consumed += copied;
                held_ += copied;
            }

            const auto previous_output = progress.produced;
            while (pos_ + lookahead_frames < held_ && progress.produced < out_frames) {
                auto *destination = out + progress.produced * channels_;
                if (!frac_) {
                    // Preserve every bit (including signed zero) at integer phase.
                    std::copy_n(buf_.get() + pos_ * channels_, channels_, destination);
                } else {
                    const auto weights = hermite_weights(static_cast<float>(frac_) * inv_denominator_);
                    const auto *taps = buf_.get() + (pos_ - 2) * channels_;
                    for (std::size_t channel = 0; channel < channels_; ++channel) {
                        float value = weights[0] * taps[channel];
                        for (std::size_t tap = 1; tap < weights.size(); ++tap)
                            value += weights[tap] * taps[tap * channels_ + channel];
                        destination[channel] = value;
                    }
                }
                ++progress.produced;
                pos_ += integer_step;
                frac_ += fractional_step;
                if (frac_ >= denominator_) {
                    frac_ -= denominator_;
                    ++pos_;
                }
            }
            compact();
            if (!copied && progress.produced == previous_output)
                break;
        }
        return progress;
    }
} // namespace alia
