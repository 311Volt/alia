#include "audio_stream.hpp"
#include "sample_conversions.hpp"
#include <atomic>

namespace alia::detail {
    std::vector<sample_frame_format> audio_format_fallbacks(sample_frame_format requested, sample_frame_format preferred) {
        std::vector<sample_frame_format> result;
        auto add = [&](sample_frame_format format) {
            if (format != requested && std::find(result.begin(), result.end(), format) == result.end()) result.push_back(format);
        };
        for (auto point : {preferred.point, sample_point_formats::f32, sample_point_formats::s16}) add({point, requested.channels});
        const int alternate = requested.channels == 1 ? (std::max)(2, preferred.channels) : 1;
        for (auto point : {preferred.point, sample_point_formats::f32, sample_point_formats::s16, requested.point}) add({point, alternate});
        return result;
    }

    struct stream_runtime {
        const audio_backend_interface *backend = nullptr;
        audio_stream_handle *handle = nullptr;
        audio_stream_properties user_properties{}, device_properties{};
        any_output_stream::callback_type render;
        any_input_stream::callback_type capture;
        std::unique_ptr<std::byte[]> scratch;
        audio_conversion_fn convert = nullptr;
        std::atomic<bool> callback_failed{false};

        ~stream_runtime() {
            if (handle) {
                backend->stream_stop.get_or_throw()(handle);
                backend->destroy_stream.get_or_throw()(handle);
            }
        }
        static void render_callback(void *user, std::byte *frames, int count, const audio_callback_info &info) noexcept {
            auto &self = *static_cast<stream_runtime *>(user);
            const auto device_format = self.device_properties.format.frame;
            if (count < 0 || count > self.device_properties.max_callback_frames) {
                self.callback_failed.store(true, std::memory_order_release);
                return;
            }
            silence_audio_bytes(frames, count, device_format);
            if (self.callback_failed.load(std::memory_order_acquire)) return;
            try {
                auto *target = self.convert ? self.scratch.get() : frames;
                if (self.convert) silence_audio_bytes(target, count, self.user_properties.format.frame);
                self.render({self.user_properties.format.frame, self.user_properties.format.sample_rate, static_cast<std::size_t>(count), target}, info);
                if (self.convert) self.convert(frames, target, count, device_format.channels, self.user_properties.format.frame.channels);
            } catch (...) {
                silence_audio_bytes(frames, count, device_format);
                self.callback_failed.store(true, std::memory_order_release);
            }
        }
        static void capture_callback(void *user, const std::byte *frames, int count, const audio_callback_info &info) noexcept {
            auto &self = *static_cast<stream_runtime *>(user);
            if (self.callback_failed.load(std::memory_order_acquire)) return;
            if (count < 0 || count > self.device_properties.max_callback_frames) {
                self.callback_failed.store(true, std::memory_order_release);
                return;
            }
            try {
                const std::byte *source = frames;
                if (self.convert) {
                    self.convert(self.scratch.get(), frames, count, self.user_properties.format.frame.channels, self.device_properties.format.frame.channels);
                    source = self.scratch.get();
                }
                self.capture({self.user_properties.format.frame, self.user_properties.format.sample_rate, static_cast<std::size_t>(count), source}, info);
            } catch (...) { self.callback_failed.store(true, std::memory_order_release); }
        }
    };

    template <class Config>
    std::unique_ptr<stream_runtime> open_stream(audio_context &context, sample_frame_format requested,
                                               const Config &config, any_output_stream::callback_type render,
                                               any_input_stream::callback_type capture) {
        if (!context) throw std::invalid_argument("audio context is not valid");
        (void)requested.bytes_per_frame();
        if (config.sample_rate < 0 || config.period_frames < 0) throw std::invalid_argument("invalid audio stream configuration");
        constexpr bool output = std::same_as<Config, output_stream_config>;
        const auto direction = output ? audio_direction::output : audio_direction::input;
        auto device = config.device;
        if (!device) {
            if constexpr (output) device = context.default_output_device();
            else device = context.default_input_device();
        }
        if (!device) throw std::runtime_error(output ? "no default audio output device" : "no default audio input device");
        const int rate = config.sample_rate ? config.sample_rate : device->preferred.sample_rate;
        if (rate <= 0) throw std::invalid_argument("audio sample rate must be positive");
        auto runtime = std::make_unique<stream_runtime>();
        runtime->backend = context.backend();
        runtime->render = std::move(render);
        runtime->capture = std::move(capture);
        auto &backend = *runtime->backend;
        if constexpr (output) (void)backend.create_output_stream.get_or_throw();
        else (void)backend.create_input_stream.get_or_throw();
        std::vector<sample_frame_format> candidates{requested};
        if (!config.exact_format) {
            auto fallbacks = audio_format_fallbacks(requested, device->preferred.frame);
            candidates.insert(candidates.end(), fallbacks.begin(), fallbacks.end());
        }
        for (auto candidate : candidates) {
            audio_conversion_fn convert = nullptr;
            if (candidate != requested) {
                try { convert = output ? select_audio_conversion(candidate, requested) : select_audio_conversion(requested, candidate); }
                catch (const std::invalid_argument &) { continue; }
            }
            const audio_stream_desc desc{device->id, {candidate, rate}, config.period_frames, runtime.get()};
            if (!backend.stream_format_supported.get_or_throw()(context.handle(), direction, device->id, desc.format)) continue;
            audio_stream_handle *handle;
            if constexpr (output) handle = backend.create_output_stream.get_or_throw()(context.handle(), desc, &stream_runtime::render_callback);
            else handle = backend.create_input_stream.get_or_throw()(context.handle(), desc, &stream_runtime::capture_callback);
            if (!handle) continue;
            runtime->handle = handle;
            const auto props = backend.stream_properties.get_or_throw()(handle);
            if (props.format != desc.format || props.max_callback_frames <= 0 || props.period_frames <= 0)
                throw std::runtime_error("audio backend returned invalid stream properties or substituted a format");
            runtime->device_properties = runtime->user_properties = props;
            runtime->user_properties.format.frame = requested;
            runtime->convert = convert;
            if (convert) runtime->scratch = std::make_unique<std::byte[]>(audio_byte_count(requested, props.max_callback_frames));
            return runtime;
        }
        throw std::runtime_error("cannot open audio stream on '" + device->name + "' at " + std::to_string(rate) +
            " Hz (device preferred rate: " + std::to_string(device->preferred.sample_rate) +
            " Hz); no supported callback format, and L3 resampling is unavailable");
    }
    void start(stream_runtime *runtime) {
        if (!runtime) throw std::logic_error("audio stream is not valid");
        if (runtime->callback_failed.load(std::memory_order_acquire) || !runtime->backend->stream_start.get_or_throw()(runtime->handle))
            throw std::runtime_error("audio stream failed to start");
    }
    void stop(stream_runtime *runtime) { if (runtime) runtime->backend->stream_stop.get_or_throw()(runtime->handle); }
    audio_stream_state state(const stream_runtime *runtime) {
        if (!runtime) return audio_stream_state::stopped;
        if (runtime->callback_failed.load(std::memory_order_acquire)) return audio_stream_state::failed;
        return runtime->backend->stream_state.get_or_throw()(runtime->handle);
    }
    const audio_stream_properties &properties(const stream_runtime *runtime) {
        if (!runtime) throw std::logic_error("audio stream is not valid");
        return runtime->user_properties;
    }
    std::int64_t position(const stream_runtime *runtime) { return runtime ? runtime->backend->stream_position.get_or_throw()(runtime->handle) : 0; }
}

namespace alia {
    any_output_stream::any_output_stream() = default;
    any_output_stream::any_output_stream(audio_context &context, sample_frame_format format, const output_stream_config &config, callback_type callback) {
        if (!callback) throw std::invalid_argument("empty audio callback");
        runtime_ = detail::open_stream(context, format, config, std::move(callback), {});
    }
    any_output_stream::~any_output_stream() = default;
    any_output_stream::any_output_stream(any_output_stream &&) noexcept = default;
    any_output_stream &any_output_stream::operator=(any_output_stream &&) noexcept = default;
    void any_output_stream::start() { detail::start(runtime_.get()); }
    void any_output_stream::stop() { detail::stop(runtime_.get()); }
    audio_stream_state any_output_stream::state() const { return detail::state(runtime_.get()); }
    const audio_stream_properties &any_output_stream::properties() const { return detail::properties(runtime_.get()); }
    std::int64_t any_output_stream::position_frames() const { return detail::position(runtime_.get()); }
    any_input_stream::any_input_stream() = default;
    any_input_stream::any_input_stream(audio_context &context, sample_frame_format format, const input_stream_config &config, callback_type callback) {
        if (!callback) throw std::invalid_argument("empty audio callback");
        runtime_ = detail::open_stream(context, format, config, {}, std::move(callback));
    }
    any_input_stream::~any_input_stream() = default;
    any_input_stream::any_input_stream(any_input_stream &&) noexcept = default;
    any_input_stream &any_input_stream::operator=(any_input_stream &&) noexcept = default;
    void any_input_stream::start() { detail::start(runtime_.get()); }
    void any_input_stream::stop() { detail::stop(runtime_.get()); }
    audio_stream_state any_input_stream::state() const { return detail::state(runtime_.get()); }
    const audio_stream_properties &any_input_stream::properties() const { return detail::properties(runtime_.get()); }
    std::int64_t any_input_stream::position_frames() const { return detail::position(runtime_.get()); }
}
