#ifndef ALIA_AUDIO_AUDIO_EFFECT_HPP
#define ALIA_AUDIO_AUDIO_EFFECT_HPP

#include "audio_clip.hpp"

namespace alia {
    struct audio_effect_progress {
        std::size_t consumed = 0; // Input frames now owned by the effect's state.
        std::size_t produced = 0; // Output frames written.
        bool operator==(const audio_effect_progress &) const = default;
    };

    /// Extension point for realtime transforms of interleaved float32 audio.
    /// Formats are fixed at construction; channels and rates may differ.
    /// process() consumes input and produces output until the output is full or
    /// no more output can be made. Consumed, unrendered input stays in the effect.
    /// Empty input may drain buffered output; empty output never advances state.
    /// On valid input, process/reset/queries never allocate, block or lock: derived
    /// effects must provision all state at construction. Not thread-safe. Input
    /// and output storage must not overlap, even when a call uses only part of it.
    class audio_effect {
    public:
        virtual ~audio_effect() = default;
        audio_effect(const audio_effect &) = delete;
        audio_effect &operator=(const audio_effect &) = delete;

        [[nodiscard]] audio_stream_format input_format() const noexcept;
        [[nodiscard]] audio_stream_format output_format() const noexcept;

        /// Frame formats must match exactly; rates must match for nonempty views.
        /// Throws invalid_argument for mismatches, misalignment or overlap.
        audio_effect_progress process(const_audio_clip_view input, any_audio_clip_view output);
        /// Interleaved points; sizes must be multiples of their channel counts.
        /// Throws invalid_argument for incomplete frames or overlapping storage.
        audio_effect_progress process(std::span<const float> input, std::span<float> output);

        /// Return to the freshly constructed state, for example after a seek.
        void reset() noexcept;
        /// Exact input needed to produce n frames from the current state. Given
        /// this much input and n output slots, process consumes exactly this count.
        /// Zero for n == 0, or when buffered input already suffices.
        [[nodiscard]] std::size_t input_frames_for_output(std::size_t n) const;
        /// Exact output from m input frames with unbounded output capacity.
        /// Queries throw length_error if the count or its arithmetic overflows.
        [[nodiscard]] std::size_t output_frames_for_input(std::size_t m) const;

    protected:
        /// Throws invalid_argument unless both formats are f32, with positive
        /// rates and channel counts and representable frame strides.
        audio_effect(audio_stream_format input, audio_stream_format output);
        audio_effect(audio_effect &&) noexcept = default;
        audio_effect &operator=(audio_effect &&) noexcept = default;

        virtual audio_effect_progress
        do_process(const float *in, std::size_t in_frames, float *out, std::size_t out_frames) = 0;
        virtual void do_reset() noexcept = 0;
        virtual std::size_t do_input_frames_for_output(std::size_t) const = 0;
        virtual std::size_t do_output_frames_for_input(std::size_t) const = 0;

    private:
        audio_effect_progress
        process_buffers(const float *in, std::size_t in_frames, float *out, std::size_t out_frames);
        audio_stream_format input_, output_;
    };
} // namespace alia
#endif
