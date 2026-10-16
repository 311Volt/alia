#ifndef ALIA_AUDIO_STREAM_HPP
#define ALIA_AUDIO_STREAM_HPP

#include "audio_context.hpp"
#include "audio_clip.hpp"
#include <functional>
#include <utility>

namespace alia {
    struct output_stream_config {
        std::optional<audio_output_device> device;
        int sample_rate = 0;       // Zero chooses the endpoint's preferred rate. Streams never resample.
        int period_frames = 0;     // Hint; properties() reports the actual period.
        bool exact_format = false; // Throw rather than converting point/channel formats in L3.
    };

    struct input_stream_config {
        std::optional<audio_input_device> device;
        int sample_rate = 0;
        int period_frames = 0;
        bool exact_format = false;
    };

    namespace detail {
        struct stream_runtime;
        std::vector<sample_frame_format>
        audio_format_fallbacks(sample_frame_format requested, sample_frame_format preferred);
    } // namespace detail

    /// A stopped, move-only output stream. The context must outlive it. Callback
    /// views expire on return. Callbacks must not allocate, block, lock, emit
    /// events, or stop/destroy the stream. Control methods require external
    /// synchronization; state()/position_frames() may be read while running.
    /// Device loss or a throwing callback latches failed and silences output;
    /// the stream is never rerouted to another endpoint.
    class any_output_stream {
    public:
        using callback_type = std::move_only_function<void(any_audio_clip_view, const audio_callback_info &)>;
        any_output_stream();
        any_output_stream(audio_context &, sample_frame_format, const output_stream_config &, callback_type);
        ~any_output_stream();
        any_output_stream(any_output_stream &&) noexcept;
        any_output_stream &operator=(any_output_stream &&) noexcept;
        any_output_stream(const any_output_stream &) = delete;
        any_output_stream &operator=(const any_output_stream &) = delete;
        /// Throws if starting fails or a callback has already failed.
        void start();
        /// Synchronous: no callback remains active after return.
        void stop();
        [[nodiscard]] audio_stream_state state() const;
        /// User-facing callback format; the backend may use another point/channel format.
        [[nodiscard]] const audio_stream_properties &properties() const;

        [[nodiscard]] int sample_rate() const {
            return properties().format.sample_rate;
        }

        [[nodiscard]] std::int64_t position_frames() const;

    private:
        std::unique_ptr<detail::stream_runtime> runtime_;
    };

    /// Capture counterpart. The callback receives read-only frame storage.
    class any_input_stream {
    public:
        using callback_type = std::move_only_function<void(const_audio_clip_view, const audio_callback_info &)>;
        any_input_stream();
        any_input_stream(audio_context &, sample_frame_format, const input_stream_config &, callback_type);
        ~any_input_stream();
        any_input_stream(any_input_stream &&) noexcept;
        any_input_stream &operator=(any_input_stream &&) noexcept;
        any_input_stream(const any_input_stream &) = delete;
        any_input_stream &operator=(const any_input_stream &) = delete;
        void start();
        void stop();
        [[nodiscard]] audio_stream_state state() const;
        [[nodiscard]] const audio_stream_properties &properties() const;

        [[nodiscard]] int sample_rate() const {
            return properties().format.sample_rate;
        }

        [[nodiscard]] std::int64_t position_frames() const;

    private:
        std::unique_ptr<detail::stream_runtime> runtime_;
    };

    /// Typed L2 wrapper. The callback accepts span<F>, optionally followed by
    /// audio_callback_info. Partially written output is initialized to silence.
    template <sample_frame F>
        requires(!std::is_const_v<F>)
    class output_stream : public any_output_stream {
    public:
        output_stream() = default;

        template <class Callback>
            requires(std::invocable<Callback &, std::span<F>> || std::invocable<Callback &, std::span<F>, const audio_callback_info &>)
        output_stream(audio_context &context, const output_stream_config &config, Callback &&callback)
            : any_output_stream(
                  context, sample_frame_format_of_v<F>, config,
                  [fn = std::forward<Callback>(callback
                   )](any_audio_clip_view view, const audio_callback_info &info) mutable {
                      auto frames = view.template as<F>().frames();
                      if constexpr (std::invocable<decltype(fn) &, std::span<F>, const audio_callback_info &>)
                          std::invoke(fn, frames, info);
                      else
                          std::invoke(fn, frames);
                  }
              ) {
        }
    };

    template <sample_frame F>
        requires(!std::is_const_v<F>)
    class input_stream : public any_input_stream {
    public:
        input_stream() = default;

        template <class Callback>
            requires(std::invocable<Callback &, std::span<const F>> || std::invocable<Callback &, std::span<const F>, const audio_callback_info &>)
        input_stream(audio_context &context, const input_stream_config &config, Callback &&callback)
            : any_input_stream(
                  context, sample_frame_format_of_v<F>, config,
                  [fn = std::forward<Callback>(callback
                   )](const_audio_clip_view view, const audio_callback_info &info) mutable {
                      auto frames = view.template as<F>().frames();
                      if constexpr (std::invocable<decltype(fn) &, std::span<const F>, const audio_callback_info &>)
                          std::invoke(fn, frames, info);
                      else
                          std::invoke(fn, frames);
                  }
              ) {
        }
    };
} // namespace alia
#endif
