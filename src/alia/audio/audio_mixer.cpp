#include "audio_mixer_access.hpp"
#include "chebyshev2_lowpass.hpp"
#include "hermite_resampler.hpp"
#include "sample_conversions.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>
#include <numbers>
#include <thread>
#include <utility>
#include <vector>

namespace alia::detail {
    namespace {
        constexpr std::size_t chunk_frames = 256, pull_frames = 512;
        enum class slot_state : std::uint8_t { free, suspended, enabled, rendering, rendering_suspend_requested };
        static_assert(std::atomic<slot_state>::is_always_lock_free);
        static_assert(std::atomic<float>::is_always_lock_free);

        void validate_gain(float value) {
            if (!std::isfinite(value))
                throw std::invalid_argument("audio mixer gain must be finite");
        }
        void validate_pan(float value) {
            if (!std::isfinite(value) || value < -1 || value > 1)
                throw std::invalid_argument("audio pan must be in [-1, 1]");
        }
        void validate_mapping(int source, int bus) {
            if (source < 1 || source > 8 || bus < 1 ||
                (source != bus && source > 2 && bus > 2))
                throw std::invalid_argument("unsupported audio mixer channel mapping");
        }

        struct voice_processor {
            audio_stream_format source_format;
            int bus_channels;
            std::optional<chebyshev2_lowpass> lowpass;
            std::optional<hermite_resampler> resampler;
            std::vector<float> pulled, filtered, converted, matrix;
            std::size_t offset = 0, leftover = 0, flush_remaining = 0;
            enum class phase { running, flushing, idle } current = phase::running;
            float cached_pan = 0, left_gain = 1, right_gain = 1;

            voice_processor(audio_stream_format source, audio_stream_format bus)
                : source_format(source), bus_channels(bus.frame.channels) {
                validate_mapping(source.frame.channels, bus_channels);
                const auto channels = source.frame.channels;
                const sample_frame_format f32{sample_point_formats::f32, channels};
                pulled.resize(audio_byte_count(f32, pull_frames) / sizeof(float));
                filtered.resize(pulled.size());
                converted.resize(audio_byte_count(f32, chunk_frames) / sizeof(float));
                if (source.sample_rate != bus.sample_rate) {
                    auto filter = chebyshev2_lowpass::for_downsampling(channels, source.sample_rate, bus.sample_rate);
                    if (filter.order())
                        lowpass.emplace(std::move(filter));
                    resampler.emplace(channels, source.sample_rate, bus.sample_rate);
                }
                if (channels == bus_channels)
                    return;
                matrix.resize(static_cast<std::size_t>(channels) * bus_channels, 0.0f);
                auto coefficient = [&](int dst, int src, float value) {
                    matrix[static_cast<std::size_t>(dst) * channels + src] = value;
                };
                if (bus_channels > 2) {
                    coefficient(0, 0, 1);
                    coefficient(1, channels == 1 ? 0 : 1, 1);
                    return;
                }
                // Construct the stereo downmix first, then average it for mono.
                float left[8]{}, right[8]{};
                left[0] = 1;
                right[channels == 1 ? 0 : 1] = 1;
                constexpr float k = std::numbers::sqrt2_v<float> / 2;
                if (channels >= 3 && channels != 4)
                    left[2] = right[2] = k;
                const int back = channels == 4 ? 2 : channels == 5 ? 3 : channels >= 6 ? 4 : -1;
                if (back >= 0) {
                    left[back] = k;
                    right[back + 1] = k;
                }
                if (channels == 7)
                    left[6] = right[6] = 0.5f;
                if (channels == 8) {
                    left[6] = k;
                    right[7] = k;
                }
                for (int src = 0; src < channels; ++src) {
                    if (bus_channels == 1)
                        coefficient(0, src, 0.5f * (left[src] + right[src]));
                    else {
                        coefficient(0, src, left[src]);
                        coefficient(1, src, right[src]);
                    }
                }
            }

            void reset() noexcept {
                if (lowpass)
                    lowpass->reset();
                if (resampler)
                    resampler->reset();
                offset = leftover = flush_remaining = 0;
                current = phase::running;
            }
            bool idle() const noexcept { return current == phase::idle; }

            void update_pan(float pan) noexcept {
                if (pan == cached_pan || bus_channels != 2)
                    return;
                cached_pan = pan;
                if (pan == 0) {
                    left_gain = right_gain = 1;
                } else if (pan == -1) {
                    left_gain = source_format.frame.channels == 1 ? std::numbers::sqrt2_v<float> : 1;
                    right_gain = 0;
                } else if (pan == 1) {
                    left_gain = 0;
                    right_gain = source_format.frame.channels == 1 ? std::numbers::sqrt2_v<float> : 1;
                } else {
                    const auto theta = (pan + 1) * (std::numbers::pi_v<float> / 4);
                    left_gain = std::numbers::sqrt2_v<float> * std::cos(theta);
                    right_gain = std::numbers::sqrt2_v<float> * std::sin(theta);
                    if (source_format.frame.channels != 1) {
                        left_gain = (std::min)(1.0f, left_gain);
                        right_gain = (std::min)(1.0f, right_gain);
                    }
                }
            }

            void accumulate(const float *input, float *output, std::size_t frames, bool add) const noexcept {
                const auto channels = source_format.frame.channels;
                for (std::size_t frame = 0; frame < frames; ++frame) {
                    for (int dst = 0; dst < bus_channels; ++dst) {
                        float value;
                        if (channels == bus_channels) {
                            value = input[frame * channels + dst];
                        } else {
                            value = 0;
                            for (int src = 0; src < channels; ++src) {
                                const auto weight = matrix[static_cast<std::size_t>(dst) * channels + src];
                                if (weight != 0)
                                    value += weight * input[frame * channels + src];
                            }
                        }
                        if (bus_channels == 2) {
                            const auto gain = dst == 0 ? left_gain : right_gain;
                            value = gain == 0 ? 0 : value * gain;
                        }
                        auto &point = output[frame * bus_channels + dst];
                        point = add ? point + value : value;
                    }
                }
            }

            bool mix(mixer_source &source, float pan, float *accum, std::size_t frames, bool add) {
                if (source.discontinuity_pending() || (current != phase::running && source.active()))
                    reset();
                if (idle())
                    return false;
                if (current == phase::running && !source.active()) {
                    reset();
                    current = phase::idle;
                    return false;
                }
                update_pan(pan);
                if (!resampler) {
                    const auto block = source.render(reinterpret_cast<std::byte *>(pulled.data()), frames);
                    accumulate(pulled.data(), accum, frames, add);
                    if (block.ended)
                        current = phase::idle;
                    return true;
                }
                const auto channels = source_format.frame.channels;
                std::size_t done = 0;
                while (done < frames) {
                    const auto need = resampler->input_frames_for_output(frames - done);
                    if (need && !leftover) {
                        std::size_t count = 0;
                        if (current == phase::running) {
                            count = (std::min)(need, pull_frames);
                            const auto block = source.render(reinterpret_cast<std::byte *>(pulled.data()), count);
                            if (block.discontinuity)
                                reset();
                            if (block.ended) {
                                const auto keep = block.frames + hermite_resampler::lookahead_frames;
                                flush_remaining = keep > count ? keep - count : 0;
                                count = (std::min)(count, keep);
                                current = phase::flushing;
                            }
                        } else if (flush_remaining) {
                            count = (std::min)({need, pull_frames, flush_remaining});
                            std::fill_n(pulled.data(), count * channels, 0.0f);
                            flush_remaining -= count;
                        }
                        if (count && lowpass)
                            lowpass->process(
                                std::span<const float>(pulled.data(), count * channels),
                                std::span<float>(filtered.data(), count * channels)
                            );
                        offset = 0;
                        leftover = count;
                    }
                    const auto *input = lowpass ? filtered.data() : pulled.data();
                    const auto progress = resampler->process(
                        std::span<const float>(input + offset * channels, leftover * channels),
                        std::span<float>(converted.data(), (frames - done) * channels)
                    );
                    offset += progress.consumed;
                    leftover -= progress.consumed;
                    accumulate(converted.data(), accum + done * bus_channels, progress.produced, add);
                    done += progress.produced;
                    if (!progress.consumed && !progress.produced) {
                        if (current == phase::flushing && !leftover && !flush_remaining)
                            current = phase::idle;
                        break;
                    }
                }
                if (current == phase::flushing && !leftover && !flush_remaining &&
                    !resampler->output_frames_for_input(0))
                    current = phase::idle;
                return done != 0;
            }
        };

        struct one_shot_source final : mixer_source {
            const_audio_clip_view clip;
            std::size_t cursor = 0;
            audio_conversion_fn convert = nullptr;
            float gain = 1;
            bool active() const noexcept override { return cursor < clip.frame_count(); }
            source_block render(std::byte *out, std::size_t frames) noexcept override {
                const auto channels = clip.format().channels;
                const auto stride = static_cast<std::size_t>(channels) * sizeof(float);
                const auto count = (std::min)(frames, clip.frame_count() - cursor);
                std::memset(out, 0, frames * stride);
                if (count) {
                    const auto *in = clip.bytes().data() + cursor * clip.format().bytes_per_frame();
                    if (convert)
                        convert(out, in, count, channels, channels);
                    else
                        std::memcpy(out, in, count * stride);
                    if (gain != 1) {
                        // Processor storage is aligned float storage.
                        auto *points = reinterpret_cast<float *>(out);
                        for (std::size_t i = 0; i < count * channels; ++i)
                            points[i] *= gain;
                    }
                    cursor += count;
                }
                return {count, !active(), false};
            }
        };
    } // namespace

    struct mixer_slot {
        std::atomic<slot_state> state{slot_state::free};
        mixer_source *source = nullptr;
        std::atomic<float> pan{0};
        std::unique_ptr<voice_processor> processor;
        one_shot_source one_shot;

        void suspend() noexcept {
            auto previous = state.load(std::memory_order_acquire);
            for (;;) {
                if (previous == slot_state::suspended || previous == slot_state::free)
                    return;
                if (previous == slot_state::enabled) {
                    if (state.compare_exchange_weak(previous, slot_state::suspended, std::memory_order_acquire))
                        return;
                } else if (previous == slot_state::rendering) {
                    state.compare_exchange_weak(previous, slot_state::rendering_suspend_requested,
                                               std::memory_order_acquire);
                } else {
                    std::this_thread::yield();
                    previous = state.load(std::memory_order_acquire);
                }
            }
        }
        void resume() noexcept {
            auto previous = slot_state::suspended;
            state.compare_exchange_strong(previous, slot_state::enabled, std::memory_order_release);
        }
    };

    struct mixer_runtime {
        std::unique_ptr<mixer_slot[]> slots;
        std::size_t player_slots, total_slots;
        std::vector<float> accumulator;
        std::mutex control_mutex;
        std::atomic<float> volume;
        std::atomic<bool> start_failed{false};
        std::size_t attachments = 0;
        // Last member: quiesce before destroying any callback storage.
        std::unique_ptr<any_output_stream> stream;

        mixer_runtime(audio_context &context, const audio_mixer_config &config)
            : player_slots(config.max_players), total_slots(config.max_players), volume(config.gain) {
            validate_gain(config.gain);
            if (config.sample_rate < 0 || config.period_frames < 0)
                throw std::invalid_argument("invalid audio mixer rate or period");
            const sample_frame_format format{sample_point_formats::f32, config.channels};
            validate_erased_audio_format(format);
            if (config.one_shot_voices > (std::numeric_limits<std::size_t>::max)() - total_slots)
                throw std::length_error("too many audio mixer slots");
            total_slots += config.one_shot_voices;
            slots = std::make_unique<mixer_slot[]>(total_slots);
            for (std::size_t i = player_slots; i < total_slots; ++i)
                slots[i].state.store(slot_state::suspended, std::memory_order_relaxed);
            accumulator.resize(audio_byte_count(format, chunk_frames) / sizeof(float));
            stream = std::make_unique<any_output_stream>(
                context, format,
                output_stream_config{config.device, config.sample_rate, config.period_frames, false},
                [this](any_audio_clip_view output, const audio_callback_info &) { render(output); }
            );
        }
        ~mixer_runtime() { assert(attachments == 0 && "audio mixer must outlive attached players"); }
        audio_stream_format format() const { return stream->properties().format; }
        bool output_failed() const {
            return start_failed.load(std::memory_order_acquire) || stream->state() == audio_stream_state::failed;
        }
        void rethrow_failure() const {
            if (output_failed())
                throw std::runtime_error("audio mixer output stream failed");
        }

        void render(any_audio_clip_view output) {
            const auto channels = format().frame.channels;
            for (std::size_t first = 0; first < output.frame_count();) {
                const auto count = (std::min)(chunk_frames, output.frame_count() - first);
                std::fill_n(accumulator.data(), count * channels, 0.0f);
                bool has_voice = false;
                for (std::size_t i = 0; i < total_slots; ++i) {
                    auto &slot = slots[i];
                    auto previous = slot_state::enabled;
                    if (!slot.state.compare_exchange_strong(previous, slot_state::rendering, std::memory_order_acquire))
                        continue;
                    struct release_slot {
                        mixer_slot &slot;
                        bool finished = false;
                        ~release_slot() {
                            if (finished) {
                                slot.state.store(slot_state::suspended, std::memory_order_release);
                            } else {
                                auto previous = slot_state::rendering;
                                if (!slot.state.compare_exchange_strong(previous, slot_state::enabled,
                                                                        std::memory_order_release))
                                    slot.state.store(slot_state::suspended, std::memory_order_release);
                            }
                        }
                    } guard{slot};
                    const auto mixed = slot.processor->mix(
                        *slot.source, slot.pan.load(std::memory_order_relaxed), accumulator.data(), count, has_voice
                    );
                    has_voice = has_voice || mixed;
                    guard.finished = i >= player_slots && slot.processor->idle();
                }
                const auto gain = volume.load(std::memory_order_relaxed);
                if (gain != 1)
                    for (std::size_t i = 0; i < count * channels; ++i)
                        accumulator[i] *= gain;
                std::memcpy(output.bytes().data() + first * channels * sizeof(float), accumulator.data(),
                            count * channels * sizeof(float));
                first += count;
            }
        }
    };

    mixer_voice::mixer_voice(mixer_runtime &mixer, mixer_source &source, audio_stream_format format, float pan) {
        validate_pan(pan);
        validate_mapping(format.frame.channels, mixer.format().frame.channels);
        std::lock_guard lock(mixer.control_mutex);
        mixer.rethrow_failure();
        for (std::size_t i = 0; i < mixer.player_slots; ++i) {
            auto &slot = mixer.slots[i];
            if (slot.state.load(std::memory_order_acquire) != slot_state::free)
                continue;
            auto processor = std::make_unique<voice_processor>(format, mixer.format());
            slot.source = &source;
            slot.processor = std::move(processor);
            slot.pan.store(pan, std::memory_order_relaxed);
            slot.state.store(slot_state::suspended, std::memory_order_release);
            ++mixer.attachments;
            mixer_ = &mixer;
            slot_ = &slot;
            return;
        }
        throw std::runtime_error("audio mixer has no free player slots");
    }
    mixer_voice::~mixer_voice() { detach(); }
    mixer_voice::mixer_voice(mixer_voice &&other) noexcept
        : mixer_(std::exchange(other.mixer_, nullptr)), slot_(std::exchange(other.slot_, nullptr)) {}
    mixer_voice &mixer_voice::operator=(mixer_voice &&other) noexcept {
        if (this != &other) {
            detach();
            mixer_ = std::exchange(other.mixer_, nullptr);
            slot_ = std::exchange(other.slot_, nullptr);
        }
        return *this;
    }
    void mixer_voice::detach() noexcept {
        if (!slot_)
            return;
        suspend();
        std::lock_guard lock(mixer_->control_mutex);
        slot_->source = nullptr;
        slot_->processor.reset();
        slot_->state.store(slot_state::free, std::memory_order_release);
        --mixer_->attachments;
        slot_ = nullptr;
        mixer_ = nullptr;
    }
    void mixer_voice::suspend() noexcept { slot_->suspend(); }
    void mixer_voice::resume() noexcept { slot_->resume(); }
    void mixer_voice::reset() noexcept {
        assert(slot_->state.load(std::memory_order_relaxed) == slot_state::suspended);
        slot_->processor->reset();
    }
    void mixer_voice::set_pan(float pan) noexcept { slot_->pan.store(pan, std::memory_order_relaxed); }
    bool mixer_voice::output_failed() const { return mixer_->output_failed(); }
    audio_stream_format mixer_voice::mix_format() const { return mixer_->format(); }

    mixer_runtime &audio_mixer_access::get(audio_mixer &mixer) {
        if (!mixer.runtime_)
            throw std::logic_error("audio mixer is not valid");
        return *mixer.runtime_;
    }
} // namespace alia::detail

namespace alia {
    namespace {
        detail::mixer_runtime &require_mixer(const std::unique_ptr<detail::mixer_runtime> &runtime) {
            if (!runtime)
                throw std::logic_error("audio mixer is not valid");
            return *runtime;
        }
    }
    audio_mixer::audio_mixer() = default;
    audio_mixer::audio_mixer(audio_context &context, const audio_mixer_config &config)
        : runtime_(std::make_unique<detail::mixer_runtime>(context, config)) {}
    audio_mixer::~audio_mixer() = default;
    audio_mixer::audio_mixer(audio_mixer &&) noexcept = default;
    audio_mixer &audio_mixer::operator=(audio_mixer &&) noexcept = default;
    void audio_mixer::start() {
        auto &r = require_mixer(runtime_);
        r.rethrow_failure();
        try {
            r.stream->start();
        } catch (...) {
            r.start_failed.store(true, std::memory_order_release);
            throw;
        }
    }
    void audio_mixer::stop() { require_mixer(runtime_).stream->stop(); }
    audio_stream_state audio_mixer::state() const {
        return runtime_ ? (runtime_->output_failed() ? audio_stream_state::failed : runtime_->stream->state())
                        : audio_stream_state::stopped;
    }
    int audio_mixer::sample_rate() const { return runtime_ ? runtime_->format().sample_rate : 0; }
    int audio_mixer::channels() const { return runtime_ ? runtime_->format().frame.channels : 0; }
    void audio_mixer::set_gain(float gain) {
        auto &r = require_mixer(runtime_);
        detail::validate_gain(gain);
        r.volume.store(gain, std::memory_order_relaxed);
    }
    float audio_mixer::gain() const { return runtime_ ? runtime_->volume.load(std::memory_order_relaxed) : 1.0f; }
    bool audio_mixer::play_one_shot(const_audio_clip_view clip, const audio_one_shot_params &params) {
        auto &r = require_mixer(runtime_);
        std::lock_guard lock(r.control_mutex);
        r.rethrow_failure();
        detail::validate_gain(params.gain);
        detail::validate_pan(params.pan);
        detail::validate_erased_audio_format(clip.format());
        detail::validate_mapping(clip.format().channels, r.format().frame.channels);
        if (!clip.frame_count())
            return true;
        if (clip.sample_rate() <= 0)
            throw std::invalid_argument("audio one-shot sample rate must be positive");
        const sample_frame_format f32{sample_point_formats::f32, clip.format().channels};
        const auto convert = clip.format() == f32 ? nullptr : detail::select_audio_conversion(f32, clip.format());
        const audio_stream_format source{f32, clip.sample_rate()};
        for (std::size_t i = r.player_slots; i < r.total_slots; ++i) {
            auto &slot = r.slots[i];
            if (slot.state.load(std::memory_order_acquire) != detail::slot_state::suspended)
                continue;
            if (!slot.processor || slot.processor->source_format != source)
                slot.processor = std::make_unique<detail::voice_processor>(source, r.format());
            slot.one_shot.clip = clip;
            slot.one_shot.cursor = 0;
            slot.one_shot.convert = convert;
            slot.one_shot.gain = params.gain;
            slot.source = &slot.one_shot;
            slot.pan.store(params.pan, std::memory_order_relaxed);
            slot.processor->reset();
            slot.resume();
            return true;
        }
        return false;
    }
    void audio_mixer::stop_one_shots() {
        auto &r = require_mixer(runtime_);
        std::lock_guard lock(r.control_mutex);
        for (std::size_t i = r.player_slots; i < r.total_slots; ++i) {
            auto &slot = r.slots[i];
            slot.suspend();
            slot.source = nullptr;
            slot.one_shot.clip = {};
            slot.one_shot.cursor = 0;
            if (slot.processor)
                slot.processor->reset();
        }
    }
} // namespace alia
