#ifndef ALIA_AUDIO_CHEBYSHEV2_LOWPASS_HPP
#define ALIA_AUDIO_CHEBYSHEV2_LOWPASS_HPP

#include "audio_effect.hpp"
#include <array>
#include <memory>

namespace alia {
    /// Chebyshev type II IIR low-pass: a cascade of order / 2 biquads in
    /// transposed direct form II, designed by the prewarped bilinear transform.
    /// The passband is monotonic and the stopband equiripple at exactly
    /// -stopband_db from stopband_hz up to Nyquist. Phase is not linear.
    /// Rate and channels are unchanged; processing is 1:1 with no lookahead.
    /// Order 0 copies every bit and ignores the other parameters.
    /// State below 1e-15 is flushed to zero every flush_interval frames, at
    /// fixed stream positions, so silence never leaves denormals behind and
    /// chunking never changes output bits. Non-finite input poisons the state
    /// until reset.
    /// Construction allocates the state; processing and reset do not.
    /// After moving, the source may only be destroyed or assigned another value.
    class chebyshev2_lowpass final : public audio_effect {
    public:
        static constexpr int max_order = 16;
        static constexpr std::size_t flush_interval = 256;

        struct params {
            double stopband_hz = 0;   // 0 < stopband_hz < sample_rate / 2
            double stopband_db = 50;  // 1 <= stopband_db <= 200
            int order = 8;            // even, 0..max_order
        };

        /// Throws invalid_argument for invalid formats or parameters.
        chebyshev2_lowpass(int channels, int sample_rate, params parameters);
        /// Anti-alias pre-filter for hermite_resampler(channels, input_rate,
        /// output_rate): order 8, 50 dB from 0.6 * output_rate. Content between
        /// 0.5 and 0.6 * output_rate aliases only into 0.4..0.5 * output_rate,
        /// above the -3 dB point near 0.45 * output_rate. Order 0 when that edge
        /// is at or above the input Nyquist rate, i.e. ratios up to 1.2.
        static chebyshev2_lowpass for_downsampling(int channels, int input_rate, int output_rate);

        chebyshev2_lowpass(chebyshev2_lowpass &&) noexcept;
        chebyshev2_lowpass &operator=(chebyshev2_lowpass &&) noexcept;

        [[nodiscard]] int order() const noexcept;

    private:
        audio_effect_progress
        do_process(const float *in, std::size_t in_frames, float *out, std::size_t out_frames) override;
        void do_reset() noexcept override;
        std::size_t do_input_frames_for_output(std::size_t) const override;
        std::size_t do_output_frames_for_input(std::size_t) const override;
        void filter(const float *in, float *out, std::size_t frames) noexcept;
        void flush_state() noexcept;

        // Symmetric numerator: b2 == b0. Denominator normalized so a0 == 1.
        struct section {
            float b0, b1, a1, a2;
        };
        std::array<section, max_order / 2> sections_{};
        std::unique_ptr<float[]> state_; // [section][channel][s1, s2]
        std::size_t channels_, section_count_;
        std::size_t flush_phase_ = 0; // Frames since the last flush position.
    };
} // namespace alia
#endif
