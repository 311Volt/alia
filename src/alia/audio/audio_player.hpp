#ifndef ALIA_AUDIO_AUDIO_PLAYER_HPP
#define ALIA_AUDIO_AUDIO_PLAYER_HPP

#include "audio_reader.hpp"
#include "audio_stream.hpp"

namespace alia {
    class audio_mixer;
    enum class play_mode {
        once,
        loop
    };
    enum class audio_player_state {
        stopped,
        playing,
        paused,
        failed
    };

    struct audio_player_config {
        std::optional<audio_output_device> device;
        int sample_rate = 0; // Zero uses the source rate; other rates must match it.
        int period_frames = 0;
        float gain = 1.0f; // Must be finite; negative gain inverts polarity.
        float pan = 0.0f; // Finite, in [-1, 1]; only affects stereo mixers.
        play_mode mode = play_mode::once;
        std::size_t buffer_frames = 0; // Streamed only: zero requests one second of audio.
    };

    namespace detail {
        struct player_runtime;
    }

    /// Move-only player, owning an output stream or attached to a mixer.
    /// The context/mixer, and for clip playback the clip's storage, must outlive
    /// the player. Standalone streams use the source rate; mixers resample it.
    /// Attached players require default device, sample_rate and period_frames.
    /// Control methods require external synchronization and must not be called
    /// from an audio callback. Queries may be used while playback is running.
    /// Streamed decoding runs on a polling worker; the callback never allocates,
    /// locks or notifies. Buffer capacity is rounded up to a power of two.
    /// At EOF in once mode the logical state becomes stopped; the device keeps
    /// producing silence until pause(), stop() or destruction quiesces it. An
    /// attached voice idles until play(), stop() or destruction.
    /// A default/moved-from player reports stopped and has no source.
    class audio_player {
    public:
        audio_player();
        audio_player(audio_context &, audio_reader source, const audio_player_config & = {});
        audio_player(audio_context &, const_audio_clip_view clip, const audio_player_config & = {});
        audio_player(audio_mixer &, audio_reader source, const audio_player_config & = {});
        audio_player(audio_mixer &, const_audio_clip_view clip, const audio_player_config & = {});
        ~audio_player();
        audio_player(audio_player &&) noexcept;
        audio_player &operator=(audio_player &&) noexcept;
        audio_player(const audio_player &) = delete;
        audio_player &operator=(const audio_player &) = delete;

        /// Start/resume; after a natural end, start again from zero. Rethrows a
        /// stored decoder error; device failures also latch the failed state.
        void play();
        void pause();
        void stop(); // Pause and rewind.
        void restart();
        /// Absolute frame seek, including EOF; throws out_of_range past EOF.
        /// Clip seeks/restarts take effect at the next callback. Streamed seeks
        /// quiesce the callback and wait for the worker to seek and prefill.
        void seek(std::size_t frame);
        void set_gain(float);
        [[nodiscard]] float gain() const;
        void set_pan(float);
        [[nodiscard]] float pan() const;
        void set_play_mode(play_mode);
        [[nodiscard]] play_mode mode() const;
        [[nodiscard]] audio_player_state state() const;
        /// Rendered source frames since the last seek, modulo the known length.
        /// Underflow silence does not advance. MP3 length is learned at first EOF.
        /// Attached positions count frames pulled by the mixer, a few frames
        /// ahead of audible output because of resampling lookahead.
        [[nodiscard]] std::size_t position_frames() const;
        [[nodiscard]] std::optional<std::size_t> frame_count() const;
        [[nodiscard]] int sample_rate() const;

    private:
        std::unique_ptr<detail::player_runtime> runtime_;
    };
} // namespace alia
#endif
