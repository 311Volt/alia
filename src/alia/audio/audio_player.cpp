#include "audio_player.hpp"
#include "audio_reader_access.hpp"
#include "audio_mixer_access.hpp"
#include "sample_conversions.hpp"
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <exception>
#include <mutex>
#include <thread>
#include <vector>

namespace alia::detail {
    namespace {
        constexpr auto no_seek = (std::numeric_limits<std::size_t>::max)();
        constexpr std::size_t state_mask = 3, ended_bit = 4, generation_step = 8;

        constexpr std::size_t code(audio_player_state state) {
            return static_cast<std::size_t>(state);
        }

        audio_player_state decode_state(std::size_t word) {
            return static_cast<audio_player_state>(word & state_mask);
        }

        void validate_gain(float gain) {
            if (!std::isfinite(gain))
                throw std::invalid_argument("audio player gain must be finite");
        }

        void validate_mode(play_mode mode) {
            if (mode != play_mode::once && mode != play_mode::loop)
                throw std::invalid_argument("invalid audio play mode");
        }

        void validate_pan(float pan) {
            if (!std::isfinite(pan) || pan < -1 || pan > 1)
                throw std::invalid_argument("audio player pan must be in [-1, 1]");
        }

        // These are the only atomic types used on the callback path.
        static_assert(std::atomic<std::size_t>::is_always_lock_free);
        static_assert(std::atomic<float>::is_always_lock_free);
        static_assert(std::atomic<play_mode>::is_always_lock_free);
        static_assert(std::atomic<bool>::is_always_lock_free);
    } // namespace

    struct player_runtime final : mixer_source {
        audio_stream_format source_format;
        bool streamed;
        const_audio_clip_view clip;
        audio_reader reader;
        audio_conversion_fn convert = nullptr;
        std::atomic<float> volume;
        std::atomic<float> balance;
        std::atomic<play_mode> repeat;
        // The generation prevents a finishing callback from overwriting a
        // concurrent clip restart/seek. Only callbacks publish ended_bit.
        std::atomic<std::size_t> transport{code(audio_player_state::stopped)};
        std::atomic<std::size_t> position{0}, pending_seek{no_seek};
        std::atomic<std::size_t> length{0};
        std::atomic<bool> length_known{false}, failed{false}, stream_failed{false};
        std::exception_ptr worker_error; // Published once through failed (release/acquire).
        std::size_t clip_position = 0;   // Callback-owned; controls only post pending_seek.

        // Streamed only: SPSC ring of f32 frames, written by the polling decoder
        // worker and read by the callback.
        std::vector<float> ring;
        std::size_t capacity = 0, scratch_frames = 4096;
        std::unique_ptr<std::byte[]> scratch;
        std::atomic<std::size_t> read_index{0}, write_index{0};
        std::atomic<bool> eof{false};
        std::chrono::milliseconds poll_interval{1};

        // Only the control and decoder threads use these synchronization objects.
        std::mutex command_mutex;
        std::condition_variable command_cv;
        bool command_pending = false, quit = false;
        std::optional<std::size_t> seek_command;
        std::size_t command_id = 0, acknowledged = 0;
        std::exception_ptr command_error;
        std::optional<std::size_t> prepared_end;
        std::stop_token worker_stop;
        std::jthread worker;
        // Last members: even constructor unwinding quiesces callbacks before the
        // worker and before any memory referenced by the callback is destroyed.
        std::unique_ptr<any_output_stream> stream;
        std::unique_ptr<mixer_voice> voice;

        player_runtime(
            audio_context *context, audio_mixer *mixer, audio_reader source, const_audio_clip_view view,
            bool use_reader, const audio_player_config &config
        )
            : source_format(use_reader ? source.format() : audio_stream_format{view.format(), view.sample_rate()}),
              streamed(use_reader), clip(view), reader(std::move(source)), volume(config.gain), balance(config.pan),
              repeat(config.mode) {
            validate_erased_audio_format(source_format.frame);
            validate_gain(config.gain);
            validate_pan(config.pan);
            validate_mode(config.mode);
            if (source_format.sample_rate <= 0 || config.sample_rate < 0 || config.period_frames < 0)
                throw std::invalid_argument("invalid audio player rate or period");
            if (mixer && (config.device || config.sample_rate != 0 || config.period_frames != 0))
                throw std::invalid_argument("mixer-attached players require default device, rate and period");
            if (!mixer && config.sample_rate && config.sample_rate != source_format.sample_rate) {
                // Mixers resample; standalone output stays at the source rate.
                throw std::invalid_argument(
                    "standalone audio player sample rate must equal the source rate; attach to a mixer to resample"
                );
            }
            const sample_frame_format output_format{sample_point_formats::f32, source_format.frame.channels};
            if (output_format != source_format.frame)
                convert = select_audio_conversion(output_format, source_format.frame);
            const auto count = streamed ? reader.frame_count() : std::optional<std::size_t>(clip.frame_count());
            if (count) {
                length.store(*count, std::memory_order_relaxed);
                length_known.store(true, std::memory_order_relaxed);
            }
            if (streamed) {
                position.store(reader.position(), std::memory_order_relaxed);
                const auto requested =
                    config.buffer_frames ? config.buffer_frames : static_cast<std::size_t>(source_format.sample_rate);
                constexpr auto largest_capacity = std::size_t(1) << (std::numeric_limits<std::size_t>::digits - 1);
                if (requested > largest_capacity)
                    throw std::length_error("audio player buffer is too large");
                capacity = std::bit_ceil(requested);
                ring.resize(audio_byte_count(output_format, capacity) / sizeof(float));
                scratch = std::make_unique<std::byte[]>(audio_byte_count(source_format.frame, scratch_frames));
                const auto period = std::chrono::duration<double>(double(capacity) / source_format.sample_rate / 8);
                poll_interval = (std::clamp)(
                    std::chrono::duration_cast<std::chrono::milliseconds>(period),
                    std::chrono::milliseconds(1),
                    std::chrono::milliseconds(20)
                );
            }
            if (mixer) {
                voice = std::make_unique<mixer_voice>(
                    audio_mixer_access::get(*mixer), *this,
                    audio_stream_format{output_format, source_format.sample_rate}, config.pan
                );
            } else {
                stream = std::make_unique<any_output_stream>(
                    *context, output_format,
                    output_stream_config{config.device, source_format.sample_rate, config.period_frames, false},
                    [this](any_audio_clip_view output, const audio_callback_info &) noexcept {
                        render_block(output.bytes().data(), output.frame_count());
                    }
                );
            }
            if (streamed) {
                worker = std::jthread([this](std::stop_token token) { worker_main(token); });
                // A decoder failure latches failed and is rethrown by play().
                submit_command(std::nullopt, false);
            }
        }

        ~player_runtime() {
            stream.reset();
            voice.reset();
            if (worker.joinable()) {
                {
                    std::lock_guard lock(command_mutex);
                    quit = true;
                }
                command_cv.notify_one();
                worker.request_stop();
                worker.join();
            }
        }

        std::optional<std::size_t> frame_count() const {
            if (!length_known.load(std::memory_order_acquire))
                return std::nullopt;
            return length.load(std::memory_order_relaxed);
        }

        void set_state(audio_player_state state) {
            auto previous = transport.load(std::memory_order_relaxed);
            for (;;) {
                const auto next = ((previous & ~(generation_step - 1)) + generation_step) | code(state);
                if (transport.compare_exchange_weak(
                        previous, next, std::memory_order_release, std::memory_order_relaxed
                    ))
                    return;
            }
        }

        void mark_end(std::size_t word) noexcept {
            const auto stopped = (word & ~(generation_step - 1)) | ended_bit | code(audio_player_state::stopped);
            transport.compare_exchange_strong(word, stopped, std::memory_order_release, std::memory_order_relaxed);
        }

        void rethrow_failure() const {
            if (failed.load(std::memory_order_acquire))
                std::rethrow_exception(worker_error);
            if (output_failed())
                throw std::runtime_error(voice ? "audio mixer output stream failed" : "audio player output stream failed");
        }

        void latch_failure(std::exception_ptr error) noexcept {
            if (!failed.load(std::memory_order_relaxed)) {
                worker_error = std::move(error);
                failed.store(true, std::memory_order_release);
            }
        }

        void apply_gain(std::byte *output, std::size_t frames) noexcept {
            const auto gain = volume.load(std::memory_order_relaxed);
            if (gain == 1.0f)
                return;
            const auto points = frames * source_format.frame.channels;
            for (std::size_t i = 0; i < points; ++i) {
                float point;
                std::memcpy(&point, output + i * sizeof(float), sizeof(float));
                point *= gain;
                std::memcpy(output + i * sizeof(float), &point, sizeof(float));
            }
        }

        bool active() const noexcept override {
            return !failed.load(std::memory_order_acquire) &&
                   decode_state(transport.load(std::memory_order_acquire)) == audio_player_state::playing;
        }

        bool discontinuity_pending() const noexcept override {
            return failed.load(std::memory_order_acquire) || pending_seek.load(std::memory_order_acquire) != no_seek;
        }

        source_block render(std::byte *output, std::size_t frames) noexcept override {
            return render_block(output, frames);
        }

        source_block render_block(std::byte *output, std::size_t frames) noexcept {
            const std::span<std::byte> bytes(output, frames * source_format.frame.channels * sizeof(float));
            if (!bytes.empty())
                std::memset(bytes.data(), 0, bytes.size());
            const auto word = transport.load(std::memory_order_acquire);
            if (failed.load(std::memory_order_acquire) || decode_state(word) != audio_player_state::playing)
                return {0, true, false};
            const auto mode = repeat.load(std::memory_order_relaxed);
            const auto channels = source_format.frame.channels;
            const auto float_stride = static_cast<std::size_t>(channels) * sizeof(float);
            std::size_t done = 0;
            bool ended = false;
            bool discontinuity = false;
            if (!streamed) {
                const auto seek = pending_seek.exchange(no_seek, std::memory_order_acq_rel);
                if (seek != no_seek) {
                    clip_position = seek;
                    discontinuity = true;
                }
                while (done < frames) {
                    if (clip_position == clip.frame_count()) {
                        if (mode == play_mode::once || !clip.frame_count()) {
                            ended = true;
                            break;
                        }
                        clip_position = 0;
                    }
                    const auto n = (std::min)(frames - done, clip.frame_count() - clip_position);
                    const auto *src = clip.bytes().data() + clip_position * source_format.frame.bytes_per_frame();
                    if (convert)
                        convert(bytes.data() + done * float_stride, src, n, channels, channels);
                    else
                        std::memcpy(bytes.data() + done * float_stride, src, n * float_stride);
                    done += n;
                    clip_position += n;
                }
                ended = ended || (mode == play_mode::once && clip_position == clip.frame_count());
                if (mode == play_mode::loop && clip.frame_count() && clip_position == clip.frame_count())
                    clip_position = 0;
                position.store(clip_position, std::memory_order_relaxed);
            } else {
                auto cursor = position.load(std::memory_order_relaxed);
                auto read = read_index.load(std::memory_order_relaxed);
                while (done < frames) {
                    const auto count = frame_count();
                    if (mode == play_mode::once && count && cursor >= *count) {
                        ended = true;
                        break;
                    }
                    const auto written = write_index.load(std::memory_order_acquire);
                    const auto available = written - read;
                    if (!available)
                        break;
                    const auto slot = read & (capacity - 1);
                    auto n = (std::min)({frames - done, available, capacity - slot});
                    if (mode == play_mode::once && count)
                        n = (std::min)(n, *count - cursor);
                    std::memcpy(bytes.data() + done * float_stride, ring.data() + slot * channels, n * float_stride);
                    done += n;
                    read += n;
                    if (mode == play_mode::loop && count && *count) {
                        cursor %= *count;
                        const auto remaining = *count - cursor;
                        cursor = n >= remaining ? (n - remaining) % *count : cursor + n;
                    } else {
                        cursor += n;
                    }
                }
                read_index.store(read, std::memory_order_release);
                position.store(cursor, std::memory_order_relaxed);
                const auto count = frame_count();
                if (mode == play_mode::once && count && cursor >= *count)
                    ended = true;
                // Load EOF before the indices: its release publishes the last
                // write as well as the learned source length.
                if (eof.load(std::memory_order_acquire) && read == write_index.load(std::memory_order_acquire) &&
                    (mode == play_mode::once || (count && !*count)))
                    ended = true;
            }
            apply_gain(bytes.data(), done);
            // A decoder failure always silences the complete output buffer.
            if (failed.load(std::memory_order_acquire) && !bytes.empty())
                std::memset(bytes.data(), 0, bytes.size());
            if (ended)
                mark_end(word); // Last publication: the worker may now rewind/refill.
            return {done, ended, discontinuity};
        }

        void reset_source(std::size_t frame) {
            reader.seek(frame); // On an invalid seek the old buffer is preserved.
            read_index.store(0, std::memory_order_relaxed);
            write_index.store(0, std::memory_order_relaxed);
            eof.store(false, std::memory_order_relaxed);
            position.store(frame, std::memory_order_relaxed);
        }

        void fill_ring() {
            const auto channels = source_format.frame.channels;
            const auto native_stride = static_cast<std::size_t>(source_format.frame.bytes_per_frame());
            while (!failed.load(std::memory_order_relaxed)) {
                if (worker_stop.stop_requested())
                    break;
                {
                    std::lock_guard lock(command_mutex);
                    if (quit || command_pending)
                        break;
                }
                if (eof.load(std::memory_order_relaxed)) {
                    const auto count = frame_count();
                    if (repeat.load(std::memory_order_relaxed) != play_mode::loop || !count || !*count)
                        break;
                    reader.seek(0);
                    eof.store(false, std::memory_order_release);
                }
                const auto written = write_index.load(std::memory_order_relaxed);
                const auto read = read_index.load(std::memory_order_acquire);
                const auto space = capacity - (written - read);
                if (!space)
                    break;
                const auto slot = written & (capacity - 1);
                const auto n = (std::min)({space, capacity - slot, scratch_frames});
                const auto decoded = audio_reader_access::read(reader, {scratch.get(), n * native_stride});
                auto *dst = reinterpret_cast<std::byte *>(ring.data() + slot * channels);
                if (convert)
                    convert(dst, scratch.get(), decoded, channels, channels);
                else if (decoded)
                    std::memcpy(dst, scratch.get(), decoded * channels * sizeof(float));
                write_index.store(written + decoded, std::memory_order_release);
                const auto count = reader.frame_count();
                if (decoded < n || (count && reader.position() == *count)) {
                    if (!length_known.load(std::memory_order_relaxed)) {
                        length.store(reader.position(), std::memory_order_relaxed);
                        length_known.store(true, std::memory_order_release);
                    }
                    eof.store(true, std::memory_order_release);
                }
            }
        }

        void prepare_replay() {
            const auto word = transport.load(std::memory_order_acquire);
            if ((word & ended_bit) && prepared_end != word) {
                reset_source(0);
                fill_ring();
                prepared_end = word;
            }
        }

        void worker_main(std::stop_token token) noexcept {
            worker_stop = token;
            while (!token.stop_requested()) {
                std::optional<std::size_t> seek;
                std::size_t id = 0;
                {
                    std::unique_lock lock(command_mutex);
                    if (quit)
                        break;
                    if (command_pending) {
                        seek = seek_command;
                        id = command_id;
                        command_pending = false;
                    }
                }
                std::exception_ptr error;
                try {
                    if (failed.load(std::memory_order_acquire)) {
                        error = worker_error;
                    } else {
                        if (seek) {
                            try {
                                reset_source(*seek);
                            } catch (const std::out_of_range &) {
                                // A rejected control request is recoverable.
                                error = std::current_exception();
                            }
                        }
                        if (!error) {
                            if (!seek)
                                prepare_replay();
                            fill_ring();
                        }
                    }
                } catch (...) {
                    error = std::current_exception();
                    latch_failure(error);
                }
                if (id) {
                    {
                        std::lock_guard lock(command_mutex);
                        command_error = error;
                        acknowledged = id;
                    }
                    command_cv.notify_all();
                }
                std::unique_lock lock(command_mutex);
                command_cv.wait_for(lock, poll_interval, [&] {
                    return quit || command_pending || token.stop_requested();
                });
            }
        }

        void submit_command(std::optional<std::size_t> seek, bool rethrow = true) {
            std::unique_lock lock(command_mutex);
            seek_command = seek;
            command_pending = true;
            const auto id = ++command_id;
            command_cv.notify_one();
            command_cv.wait(lock, [&] { return acknowledged == id; });
            const auto error = command_error;
            lock.unlock();
            if (error && rethrow)
                std::rethrow_exception(error);
        }

        void quiesce_output() {
            if (voice)
                voice->suspend();
            else
                stream->stop();
        }

        bool output_failed() const {
            return voice ? voice->output_failed()
                         : stream_failed.load(std::memory_order_acquire) || stream->state() == audio_stream_state::failed;
        }

        void reset_voice() noexcept {
            if (voice)
                voice->reset();
        }

        void resume_output() {
            if (voice) {
                rethrow_failure();
                voice->resume();
                return;
            }
            try {
                stream->start();
            } catch (...) {
                stream_failed.store(true, std::memory_order_release);
                throw;
            }
        }
    };
} // namespace alia::detail

namespace alia {
    namespace {
        detail::player_runtime &require_player(const std::unique_ptr<detail::player_runtime> &runtime) {
            if (!runtime)
                throw std::logic_error("audio player has no source");
            return *runtime;
        }
    } // namespace

    audio_player::audio_player() = default;

    audio_player::audio_player(audio_context &context, audio_reader source, const audio_player_config &config)
        : runtime_(std::make_unique<detail::player_runtime>(
              &context, nullptr, std::move(source), const_audio_clip_view{}, true, config
          )) {
    }

    audio_player::audio_player(audio_context &context, const_audio_clip_view clip, const audio_player_config &config)
        : runtime_(std::make_unique<detail::player_runtime>(&context, nullptr, audio_reader{}, clip, false, config)) {
    }

    audio_player::audio_player(audio_mixer &mixer, audio_reader source, const audio_player_config &config)
        : runtime_(std::make_unique<detail::player_runtime>(
              nullptr, &mixer, std::move(source), const_audio_clip_view{}, true, config
          )) {
    }

    audio_player::audio_player(audio_mixer &mixer, const_audio_clip_view clip, const audio_player_config &config)
        : runtime_(std::make_unique<detail::player_runtime>(nullptr, &mixer, audio_reader{}, clip, false, config)) {
    }

    audio_player::~audio_player() = default;
    audio_player::audio_player(audio_player &&) noexcept = default;
    audio_player &audio_player::operator=(audio_player &&) noexcept = default;

    void audio_player::play() {
        auto &r = require_player(runtime_);
        r.rethrow_failure();
        const auto word = r.transport.load(std::memory_order_acquire);
        if (detail::decode_state(word) == audio_player_state::playing)
            return;
        if (r.streamed) {
            r.quiesce_output();
            r.submit_command(std::nullopt);
        } else if (word & detail::ended_bit) {
            r.pending_seek.store(0, std::memory_order_release);
        }
        r.set_state(audio_player_state::playing);
        r.resume_output();
    }

    void audio_player::pause() {
        if (!runtime_)
            return;
        auto &r = *runtime_;
        r.quiesce_output();
        if (detail::decode_state(r.transport.load(std::memory_order_acquire)) == audio_player_state::playing)
            r.set_state(audio_player_state::paused);
    }

    void audio_player::stop() {
        if (!runtime_)
            return;
        auto &r = *runtime_;
        r.quiesce_output();
        r.set_state(audio_player_state::stopped);
        r.rethrow_failure();
        if (r.streamed)
            r.submit_command(0);
        else
            r.pending_seek.store(0, std::memory_order_release);
        r.reset_voice();
    }

    void audio_player::restart() {
        auto &r = require_player(runtime_);
        r.rethrow_failure();
        if (r.streamed) {
            seek(0);
            play();
        } else {
            r.pending_seek.store(0, std::memory_order_release);
            r.set_state(audio_player_state::playing);
            r.resume_output();
        }
    }

    void audio_player::seek(std::size_t frame) {
        auto &r = require_player(runtime_);
        r.rethrow_failure();
        const auto count = r.frame_count();
        if (count && frame > *count)
            throw std::out_of_range("audio_player::seek: past end of source");
        const auto previous = detail::decode_state(r.transport.load(std::memory_order_acquire));
        if (!r.streamed) {
            r.pending_seek.store(frame, std::memory_order_release);
            r.set_state(previous);
            return;
        }
        r.quiesce_output();
        r.set_state(previous == audio_player_state::playing ? audio_player_state::paused : previous);
        try {
            r.submit_command(frame);
        } catch (const std::out_of_range &) {
            if (previous == audio_player_state::playing) {
                r.set_state(previous);
                r.resume_output();
            }
            throw;
        }
        r.reset_voice();
        if (previous == audio_player_state::playing) {
            r.set_state(previous);
            r.resume_output();
        }
    }

    void audio_player::set_gain(float gain) {
        detail::validate_gain(gain);
        require_player(runtime_).volume.store(gain, std::memory_order_relaxed);
    }

    float audio_player::gain() const {
        return runtime_ ? runtime_->volume.load(std::memory_order_relaxed) : 1.0f;
    }

    void audio_player::set_pan(float pan) {
        detail::validate_pan(pan);
        auto &r = require_player(runtime_);
        r.balance.store(pan, std::memory_order_relaxed);
        if (r.voice)
            r.voice->set_pan(pan);
    }

    float audio_player::pan() const {
        return runtime_ ? runtime_->balance.load(std::memory_order_relaxed) : 0.0f;
    }

    void audio_player::set_play_mode(play_mode mode) {
        detail::validate_mode(mode);
        require_player(runtime_).repeat.store(mode, std::memory_order_relaxed);
    }

    play_mode audio_player::mode() const {
        return runtime_ ? runtime_->repeat.load(std::memory_order_relaxed) : play_mode::once;
    }

    audio_player_state audio_player::state() const {
        if (!runtime_)
            return audio_player_state::stopped;
        const auto &r = *runtime_;
        if (r.failed.load(std::memory_order_acquire) || r.output_failed())
            return audio_player_state::failed;
        return detail::decode_state(r.transport.load(std::memory_order_acquire));
    }

    std::size_t audio_player::position_frames() const {
        if (!runtime_)
            return 0;
        const auto &r = *runtime_;
        const auto pending = r.pending_seek.load(std::memory_order_acquire);
        const auto position = pending != detail::no_seek ? pending : r.position.load(std::memory_order_relaxed);
        const auto count = r.frame_count();
        return count && *count ? position % *count : position;
    }

    std::optional<std::size_t> audio_player::frame_count() const {
        return runtime_ ? runtime_->frame_count() : std::nullopt;
    }

    int audio_player::sample_rate() const {
        return runtime_ ? runtime_->source_format.sample_rate : 0;
    }
} // namespace alia
