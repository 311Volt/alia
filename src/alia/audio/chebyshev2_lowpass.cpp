#include "chebyshev2_lowpass.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace alia {
    namespace {
        void validate_params(int sample_rate, const chebyshev2_lowpass::params &parameters) {
            if (parameters.order < 0 || parameters.order > chebyshev2_lowpass::max_order || parameters.order % 2)
                throw std::invalid_argument("chebyshev2_lowpass order must be even and at most max_order");
            if (!parameters.order)
                return;
            if (!(parameters.stopband_hz > 0 && parameters.stopband_hz < sample_rate / 2.0))
                throw std::invalid_argument("chebyshev2_lowpass stopband edge must lie between 0 and Nyquist");
            if (!(parameters.stopband_db >= 1 && parameters.stopband_db <= 200))
                throw std::invalid_argument("chebyshev2_lowpass stopband attenuation must be 1 to 200 dB");
        }

        // One conjugate pole pair and zero pair of the analog prototype whose
        // stopband edge is at omega_s, mapped by s = (1 - 1/z) / (1 + 1/z) and
        // scaled to unity DC gain. theta indexes the Chebyshev polynomial root.
        template <typename TSection>
        TSection design_section(double omega_s, double mu, double theta) noexcept {
            // Type I prototype pole sigma + j*omega; type II poles are its
            // reciprocal scaled by omega_s, and zeros lie at +-j*omega_s/cos(theta).
            const auto sigma = -std::sinh(mu) * std::sin(theta);
            const auto omega = std::cosh(mu) * std::cos(theta);
            const auto norm = sigma * sigma + omega * omega;
            const auto damping = -2 * omega_s * sigma / norm; // -2 Re(pole)
            const auto pole_sq = omega_s * omega_s / norm;    // |pole|^2
            const auto zero = omega_s / std::cos(theta);
            const auto zero_sq = zero * zero;
            const auto a0 = 1 + damping + pole_sq;
            const auto gain = pole_sq / zero_sq / a0;
            return {
                static_cast<float>(gain * (1 + zero_sq)),
                static_cast<float>(2 * gain * (zero_sq - 1)),
                static_cast<float>(2 * (pole_sq - 1) / a0),
                static_cast<float>((1 - damping + pole_sq) / a0)
            };
        }
    } // namespace

    chebyshev2_lowpass::chebyshev2_lowpass(int channels, int sample_rate, params parameters)
        : audio_effect(
              {{sample_point_formats::f32, channels}, sample_rate}, {{sample_point_formats::f32, channels}, sample_rate}
          ),
          channels_(static_cast<std::size_t>(channels)) {
        validate_params(sample_rate, parameters);
        section_count_ = static_cast<std::size_t>(parameters.order / 2);
        if (!section_count_)
            return;
        if (channels_ > (std::numeric_limits<std::size_t>::max)() / sizeof(float) / (2 * section_count_))
            throw std::length_error("chebyshev2_lowpass state is too large");

        const auto order = static_cast<double>(parameters.order);
        const auto omega_s = std::tan(std::numbers::pi * parameters.stopband_hz / sample_rate);
        const auto epsilon = 1 / std::sqrt(std::pow(10.0, parameters.stopband_db / 10) - 1);
        const auto mu = std::asinh(1 / epsilon) / order;
        for (std::size_t k = 0; k < section_count_; ++k) {
            const auto theta = std::numbers::pi * static_cast<double>(2 * k + 1) / (2 * order);
            sections_[k] = design_section<section>(omega_s, mu, theta);
        }
        state_ = std::make_unique<float[]>(2 * section_count_ * channels_);
    }

    chebyshev2_lowpass chebyshev2_lowpass::for_downsampling(int channels, int input_rate, int output_rate) {
        if (output_rate <= 0)
            throw std::invalid_argument("chebyshev2_lowpass output rate must be positive");
        const auto stopband_hz = 0.6 * output_rate;
        const auto order = stopband_hz < input_rate / 2.0 ? 8 : 0;
        return chebyshev2_lowpass(channels, input_rate, {stopband_hz, 50, order});
    }

    chebyshev2_lowpass::chebyshev2_lowpass(chebyshev2_lowpass &&) noexcept = default;
    chebyshev2_lowpass &chebyshev2_lowpass::operator=(chebyshev2_lowpass &&) noexcept = default;

    int chebyshev2_lowpass::order() const noexcept {
        return static_cast<int>(2 * section_count_);
    }

    void chebyshev2_lowpass::do_reset() noexcept {
        std::fill_n(state_.get(), 2 * section_count_ * channels_, 0.0f);
        flush_phase_ = 0;
    }

    std::size_t chebyshev2_lowpass::do_input_frames_for_output(std::size_t n) const {
        return n;
    }

    std::size_t chebyshev2_lowpass::do_output_frames_for_input(std::size_t m) const {
        return m;
    }

    void chebyshev2_lowpass::filter(const float *in, float *out, std::size_t frames) noexcept {
        const auto points = frames * channels_;
        auto *state = state_.get();
        for (std::size_t index = 0; index < section_count_; ++index) {
            const auto [b0, b1, a1, a2] = sections_[index];
            // The first section reads the input; later ones filter out in place.
            const auto *source = index ? out : in;
            for (std::size_t channel = 0; channel < channels_; ++channel, state += 2) {
                auto s1 = state[0], s2 = state[1];
                for (std::size_t i = channel; i < points; i += channels_) {
                    const auto x = source[i];
                    const auto y = b0 * x + s1;
                    s1 = b1 * x - a1 * y + s2;
                    s2 = b0 * x - a2 * y;
                    out[i] = y;
                }
                state[0] = s1;
                state[1] = s2;
            }
        }
    }

    void chebyshev2_lowpass::flush_state() noexcept {
        auto *const begin = state_.get();
        for (auto *value = begin; value != begin + 2 * section_count_ * channels_; ++value)
            if (std::abs(*value) < 1e-15f)
                *value = 0;
    }

    audio_effect_progress chebyshev2_lowpass::do_process(
        const float *in, std::size_t in_frames, float *out, std::size_t out_frames
    ) {
        const auto frames = (std::min)(in_frames, out_frames);
        if (!section_count_) {
            std::copy_n(in, frames * channels_, out);
            return {frames, frames};
        }
        // Flush at fixed stream positions so chunk boundaries never matter.
        for (std::size_t done = 0; done < frames;) {
            const auto count = (std::min)(frames - done, flush_interval - flush_phase_);
            filter(in + done * channels_, out + done * channels_, count);
            done += count;
            flush_phase_ += count;
            if (flush_phase_ == flush_interval) {
                flush_state();
                flush_phase_ = 0;
            }
        }
        return {frames, frames};
    }
} // namespace alia
