#include "audio_effect.hpp"

namespace alia {
    namespace {
        void validate_effect_format(audio_stream_format format) {
            if (format.frame.point != sample_point_formats::f32 || format.sample_rate <= 0)
                throw std::invalid_argument("audio effect requires float32 audio and a positive sample rate");
            (void)format.frame.bytes_per_frame();
        }
    } // namespace

    audio_effect::audio_effect(audio_stream_format input, audio_stream_format output) : input_(input), output_(output) {
        validate_effect_format(input);
        validate_effect_format(output);
    }

    audio_stream_format audio_effect::input_format() const noexcept {
        return input_;
    }

    audio_stream_format audio_effect::output_format() const noexcept {
        return output_;
    }

    audio_effect_progress audio_effect::process(const_audio_clip_view input, any_audio_clip_view output) {
        if (input.format() != input_.frame || output.format() != output_.frame)
            throw std::invalid_argument("audio effect frame format mismatch");
        if ((input.frame_count() && input.sample_rate() != input_.sample_rate) ||
            (output.frame_count() && output.sample_rate() != output_.sample_rate))
            throw std::invalid_argument("audio effect sample rate mismatch");
        return process_buffers(
            reinterpret_cast<const float *>(input.bytes().data()),
            input.frame_count(),
            reinterpret_cast<float *>(output.bytes().data()),
            output.frame_count()
        );
    }

    audio_effect_progress audio_effect::process(std::span<const float> input, std::span<float> output) {
        const auto in_channels = static_cast<std::size_t>(input_.frame.channels);
        const auto out_channels = static_cast<std::size_t>(output_.frame.channels);
        if (input.size() % in_channels || output.size() % out_channels)
            throw std::invalid_argument("audio effect requires complete interleaved frames");
        return process_buffers(
            input.data(), input.size() / in_channels, output.data(), output.size() / out_channels
        );
    }

    audio_effect_progress audio_effect::process_buffers(
        const float *in, std::size_t in_frames, float *out, std::size_t out_frames
    ) {
        const auto in_address = reinterpret_cast<std::uintptr_t>(in);
        const auto out_address = reinterpret_cast<std::uintptr_t>(out);
        if (in_address % alignof(float) || out_address % alignof(float))
            throw std::invalid_argument("unaligned audio effect storage");
        const auto in_bytes = detail::audio_byte_count(input_.frame, in_frames);
        const auto out_bytes = detail::audio_byte_count(output_.frame, out_frames);
        // Subtract addresses instead of forming endpoints, which could wrap.
        if (in_bytes && out_bytes &&
            (in_address <= out_address ? out_address - in_address < in_bytes : in_address - out_address < out_bytes))
            throw std::invalid_argument("audio effect input and output storage overlap");
        if (!out_frames || (!in_frames && !do_output_frames_for_input(0)))
            return {};
        return do_process(in, in_frames, out, out_frames);
    }

    void audio_effect::reset() noexcept {
        do_reset();
    }

    std::size_t audio_effect::input_frames_for_output(std::size_t n) const {
        return do_input_frames_for_output(n);
    }

    std::size_t audio_effect::output_frames_for_input(std::size_t m) const {
        return do_output_frames_for_input(m);
    }
} // namespace alia
