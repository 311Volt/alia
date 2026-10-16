#ifndef ALIA_AUDIO_HERMITE_RESAMPLER_HPP
#define ALIA_AUDIO_HERMITE_RESAMPLER_HPP

#include "audio_effect.hpp"
#include <memory>

namespace alia {
    /// Rate converter using the 6-point, 5th-order Hermite x-form interpolator
    /// (O. Niemitalo, "Polynomial Interpolators for High-Quality Resampling of
    /// Oversampled Audio", 2001). Channels are unchanged; no anti-alias filter is
    /// applied, so downsampling aliases content above the output Nyquist rate.
    /// Chain chebyshev2_lowpass::for_downsampling before this effect to prevent it.
    /// Output k samples input at exactly k * input_rate / output_rate frames:
    /// rational phase, no drift or time shift. Input before frame 0 is silence.
    /// Each output requires lookahead_frames future input frames. Feed that many
    /// silent frames to flush at EOF, limiting output to the desired duration.
    /// Construction allocates the fixed window; processing and reset do not.
    /// After moving, the source may only be destroyed or assigned another value.
    class hermite_resampler final : public audio_effect {
    public:
        static constexpr std::size_t lookahead_frames = 3;
        hermite_resampler(int channels, int input_rate, int output_rate);
        hermite_resampler(hermite_resampler &&) noexcept;
        hermite_resampler &operator=(hermite_resampler &&) noexcept;

    private:
        audio_effect_progress
        do_process(const float *in, std::size_t in_frames, float *out, std::size_t out_frames) override;
        void do_reset() noexcept override;
        std::size_t do_input_frames_for_output(std::size_t) const override;
        std::size_t do_output_frames_for_input(std::size_t) const override;
        void compact() noexcept;

        static constexpr std::size_t chunk_frames = 512;
        static constexpr std::size_t buffer_frames = 5 + chunk_frames;
        std::unique_ptr<float[]> buf_;
        std::size_t channels_;
        std::uint64_t numerator_, denominator_, frac_ = 0;
        float inv_denominator_;
        // pos_ centers the next six-tap window in held_ frames. skip_ counts
        // incoming frames before that window after a large downsampling step.
        std::size_t held_ = 2, pos_ = 2, skip_ = 0;
    };
} // namespace alia
#endif
