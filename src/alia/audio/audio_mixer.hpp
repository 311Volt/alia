#ifndef ALIA_AUDIO_AUDIO_MIXER_HPP
#define ALIA_AUDIO_AUDIO_MIXER_HPP

#include "audio_stream.hpp"

namespace alia {
    struct audio_mixer_config {
        std::optional<audio_output_device> device;
        int sample_rate = 0; // Zero chooses the endpoint's preferred rate.
        int channels = 2; // L3 stream fallback converts the bus to the endpoint format.
        int period_frames = 0;
        float gain = 1.0f; // Must be finite; negative gain inverts polarity.
        std::size_t max_players = 32;
        std::size_t one_shot_voices = 32;
    };

    struct audio_one_shot_params {
        float gain = 1.0f;
        float pan = 0.0f; // Finite, in [-1, 1]; only affects stereo buses.
    };

    namespace detail {
        struct mixer_runtime;
        struct audio_mixer_access;
    }

    /// Flat, move-only mixer owning one initially stopped output stream.
    /// The context must outlive the mixer; the mixer must outlive its players.
    /// Control methods require external synchronization and must not run in an
    /// audio callback. Attach/detach and one-shot calls are serialized internally.
    /// Sources are resampled and mapped to the bus. Downmixing is not normalized
    /// and there is no limiter: summed peaks may exceed 1.
    /// A default/moved-from mixer is invalid: controls throw logic_error and
    /// state/rate/channel queries return stopped/zero (gain returns unity).
    class audio_mixer {
    public:
        audio_mixer();
        explicit audio_mixer(audio_context &, const audio_mixer_config & = {});
        ~audio_mixer();
        audio_mixer(audio_mixer &&) noexcept;
        audio_mixer &operator=(audio_mixer &&) noexcept;
        audio_mixer(const audio_mixer &) = delete;
        audio_mixer &operator=(const audio_mixer &) = delete;

        void start(); // Throws after output failure.
        void stop(); // Synchronously quiesces output; preserves voice positions.
        [[nodiscard]] audio_stream_state state() const;
        [[nodiscard]] int sample_rate() const;
        [[nodiscard]] int channels() const;
        void set_gain(float);
        [[nodiscard]] float gain() const;

        /// Play from zero on a free voice; false only when the pool is full.
        /// Empty clips succeed without using a voice. May allocate on the caller.
        /// Throws after output failure, or invalid_argument for invalid format,
        /// channel mapping, gain or pan. Clip storage must outlive the mixer or
        /// the next stop_one_shots(), even after the voice finishes.
        bool play_one_shot(const_audio_clip_view, const audio_one_shot_params & = {});
        void stop_one_shots(); // Synchronous; attached players are unaffected.

    private:
        friend struct detail::audio_mixer_access;
        std::unique_ptr<detail::mixer_runtime> runtime_;
    };
} // namespace alia
#endif
