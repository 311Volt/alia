#ifndef ALIA_AUDIO_AUDIO_MIXER_ACCESS_HPP
#define ALIA_AUDIO_AUDIO_MIXER_ACCESS_HPP

#include "audio_mixer.hpp"

namespace alia::detail {
    struct source_block {
        std::size_t frames; // Real frames; the rest of the requested block is silence.
        bool ended;
        bool discontinuity;
    };

    struct mixer_source {
        virtual ~mixer_source() = default;
        virtual bool active() const noexcept = 0;
        // Observe a posted seek or invalidated source before draining lookahead.
        virtual bool discontinuity_pending() const noexcept { return false; }
        // Interleaved f32 at the source rate/channels; frames is never zero.
        virtual source_block render(std::byte *f32_out, std::size_t frames) noexcept = 0;
    };

    struct mixer_slot;
    class mixer_voice {
    public:
        mixer_voice(mixer_runtime &, mixer_source &, audio_stream_format, float pan);
        ~mixer_voice();
        mixer_voice(mixer_voice &&) noexcept;
        mixer_voice &operator=(mixer_voice &&) noexcept;
        mixer_voice(const mixer_voice &) = delete;
        mixer_voice &operator=(const mixer_voice &) = delete;
        void suspend() noexcept;
        void resume() noexcept;
        void reset() noexcept; // Slot must be suspended.
        void set_pan(float) noexcept;
        [[nodiscard]] bool output_failed() const;
        [[nodiscard]] audio_stream_format mix_format() const;

    private:
        void detach() noexcept;
        mixer_runtime *mixer_ = nullptr;
        mixer_slot *slot_ = nullptr;
    };

    struct audio_mixer_access {
        static mixer_runtime &get(audio_mixer &);
    };
} // namespace alia::detail
#endif
