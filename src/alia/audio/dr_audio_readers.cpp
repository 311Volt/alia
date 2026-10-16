#define DR_WAV_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#include <dr_wav.h>
#include <dr_flac.h>
#include <dr_mp3.h>

#include "dr_audio_readers.hpp"
#include "audio_io.hpp"
#include "audio_codec_file.hpp"
#include "sample_conversions.hpp"
#include <cstring>
#include <string>

namespace alia {
    namespace {
        std::runtime_error open_error(std::string_view codec, std::string_view path) {
            return std::runtime_error(
                "open_audio_reader: " + std::string(codec) + " failed to open '" + std::string(path) + "'"
            );
        }

        // Aligned scratch also makes the byte-span contract safe for unaligned
        // user storage. It is allocated once, and reads of any size are chunked.
        class dr_decoder : public audio_decoder {
        public:
            audio_stream_format format() const override {
                return format_;
            }

            std::optional<std::size_t> frame_count() const override {
                return count_;
            }

            std::size_t read(std::span<std::byte> frames) override {
                const auto stride = static_cast<std::size_t>(format_.frame.bytes_per_frame());
                if (frames.size() % stride)
                    throw std::invalid_argument("audio decoder: incomplete frame");
                const auto requested = frames.size() / stride;
                std::size_t done = 0;
                while (done < requested) {
                    auto n = (std::min)(chunk_frames, requested - done);
                    if (count_)
                        n = (std::min)(n, *count_ - position_);
                    if (!n)
                        break;
                    const auto read = decode(n, scratch_.get());
                    if (read > n)
                        throw std::runtime_error("audio decoder returned too many frames");
                    std::memcpy(frames.data() + done * stride, scratch_.get(), read * stride);
                    done += read;
                    position_ += read;
                    if (read < n) {
                        if (count_ && position_ != *count_)
                            throw std::runtime_error("audio decoder: truncated or corrupt file");
                        break;
                    }
                }
                return done;
            }

            void seek(std::size_t frame) override {
                if (count_ && frame > *count_)
                    throw std::out_of_range("audio decoder seek past end of stream");
                if (!seek_native(frame)) {
                    if (!seek_native(position_))
                        throw std::runtime_error("audio decoder: seek and position recovery failed");
                    if (!count_)
                        throw std::out_of_range("audio decoder seek past end of stream");
                    throw std::runtime_error("audio decoder: seek failed");
                }
                position_ = frame;
            }

        protected:
            void configure(
                sample_point_format point, unsigned int channels, unsigned int rate, std::optional<std::size_t> count
            ) {
                if (!channels || channels > static_cast<unsigned int>((std::numeric_limits<int>::max)()) || !rate ||
                    rate > static_cast<unsigned int>((std::numeric_limits<int>::max)()))
                    throw std::runtime_error("audio decoder: invalid channel count or sample rate");
                format_ = {{point, static_cast<int>(channels)}, static_cast<int>(rate)};
                detail::validate_erased_audio_format(format_.frame);
                count_ = count;
                scratch_ = std::make_unique<std::byte[]>(detail::audio_byte_count(format_.frame, chunk_frames));
            }

            virtual std::size_t decode(std::size_t frames, std::byte *output) = 0;
            virtual bool seek_native(std::size_t frame) = 0;
            static constexpr std::size_t chunk_frames = 4096;
            audio_stream_format format_{};
            std::optional<std::size_t> count_;
            std::size_t position_ = 0;
            std::unique_ptr<std::byte[]> scratch_;
        };

        struct wav_file {
            drwav value{};

            explicit wav_file(std::string_view path) {
                if (!detail::open_audio_file(
                        path,
                        [&](const char *p) { return drwav_init_file(&value, p, nullptr); },
                        [&](const wchar_t *p) { return drwav_init_file_w(&value, p, nullptr); }
                    ))
                    throw open_error("dr_wav", path);
            }

            ~wav_file() {
                drwav_uninit(&value);
            }
        };

        class wav_decoder final : public dr_decoder {
        public:
            explicit wav_decoder(std::string_view path) : file_(path) {
                using namespace sample_point_formats;
                const auto &wav = file_.value;
                sample_point_format point;
                if (wav.translatedFormatTag == DR_WAVE_FORMAT_PCM) {
                    switch (wav.bitsPerSample) {
                    case 8:  point = u8; break;
                    case 16: point = s16; break;
                    case 24:
                    case 32: point = s32; break;
                    default: throw std::runtime_error("open_audio_reader: unsupported WAV PCM bit depth");
                    }
                } else if (wav.translatedFormatTag == DR_WAVE_FORMAT_IEEE_FLOAT) {
                    if (wav.bitsPerSample != 32 && wav.bitsPerSample != 64)
                        throw std::runtime_error("open_audio_reader: unsupported WAV float bit depth");
                    point = f32;
                } else if (wav.translatedFormatTag == DR_WAVE_FORMAT_ADPCM ||
                           wav.translatedFormatTag == DR_WAVE_FORMAT_DVI_ADPCM ||
                           wav.translatedFormatTag == DR_WAVE_FORMAT_ALAW ||
                           wav.translatedFormatTag == DR_WAVE_FORMAT_MULAW) {
                    point = s16;
                } else {
                    throw std::runtime_error("open_audio_reader: unsupported WAV encoding");
                }
                configure(point, wav.channels, wav.sampleRate, detail::audio_file_frame_count(wav.totalPCMFrameCount));
            }

        private:
            std::size_t decode(std::size_t n, std::byte *out) override {
                auto &wav = file_.value;
                const auto point = format_.frame.point;
                if (point == sample_point_formats::u8)
                    return static_cast<std::size_t>(drwav_read_pcm_frames(&wav, n, out));
                if (point == sample_point_formats::s16)
                    return static_cast<std::size_t>(
                        drwav_read_pcm_frames_s16(&wav, n, reinterpret_cast<drwav_int16 *>(out))
                    );
                if (point == sample_point_formats::s32)
                    return static_cast<std::size_t>(
                        drwav_read_pcm_frames_s32(&wav, n, reinterpret_cast<drwav_int32 *>(out))
                    );
                return static_cast<std::size_t>(drwav_read_pcm_frames_f32(&wav, n, reinterpret_cast<float *>(out)));
            }

            bool seek_native(std::size_t frame) override {
                return drwav_seek_to_pcm_frame(&file_.value, frame);
            }

            wav_file file_;
        };

        class flac_decoder final : public dr_decoder {
        public:
            explicit flac_decoder(std::string_view path)
                : file_(
                      detail::open_audio_file(
                          path, [](const char *p) { return drflac_open_file(p, nullptr); },
                          [](const wchar_t *p) { return drflac_open_file_w(p, nullptr); }
                      ),
                      &drflac_close
                  ) {
                if (!file_)
                    throw open_error("dr_flac", path);
                std::optional<std::size_t> count;
                if (file_->totalPCMFrameCount)
                    count = detail::audio_file_frame_count(file_->totalPCMFrameCount);
                configure(
                    file_->bitsPerSample <= 16 ? sample_point_formats::s16 : sample_point_formats::s32,
                    file_->channels,
                    file_->sampleRate,
                    count
                );
            }

        private:
            std::size_t decode(std::size_t n, std::byte *out) override {
                if (format_.frame.point == sample_point_formats::s16)
                    return static_cast<std::size_t>(
                        drflac_read_pcm_frames_s16(file_.get(), n, reinterpret_cast<drflac_int16 *>(out))
                    );
                return static_cast<std::size_t>(
                    drflac_read_pcm_frames_s32(file_.get(), n, reinterpret_cast<drflac_int32 *>(out))
                );
            }

            bool seek_native(std::size_t frame) override {
                // dr_flac clamps a nonzero seek against totalPCMFrameCount,
                // even when STREAMINFO reports zero (unknown). Decode/discard
                // in that case so a past-EOF seek can be detected accurately.
                if (!count_) {
                    if (!drflac_seek_to_pcm_frame(file_.get(), 0))
                        return false;
                    std::size_t skipped = 0;
                    while (skipped < frame) {
                        const auto n = (std::min)(chunk_frames, frame - skipped);
                        const auto read = decode(n, scratch_.get());
                        skipped += read;
                        if (read < n)
                            return false;
                    }
                    return true;
                }
                // dr_flac seeks to a sample rather than the position after it.
                if (count_ && frame == *count_ && frame) {
                    if (!drflac_seek_to_pcm_frame(file_.get(), frame - 1))
                        return false;
                    return decode(1, scratch_.get()) == 1;
                }
                return drflac_seek_to_pcm_frame(file_.get(), frame);
            }

            std::unique_ptr<drflac, decltype(&drflac_close)> file_;
        };

        struct mp3_file {
            drmp3 value{};

            explicit mp3_file(std::string_view path) {
                if (!detail::open_audio_file(
                        path,
                        [&](const char *p) { return drmp3_init_file(&value, p, nullptr); },
                        [&](const wchar_t *p) { return drmp3_init_file_w(&value, p, nullptr); }
                    ))
                    throw open_error("dr_mp3", path);
            }

            ~mp3_file() {
                drmp3_uninit(&value);
            }
        };

        class mp3_decoder final : public dr_decoder {
        public:
            explicit mp3_decoder(std::string_view path) : file_(path) {
                configure(sample_point_formats::f32, file_.value.channels, file_.value.sampleRate, std::nullopt);
            }

        private:
            std::size_t decode(std::size_t n, std::byte *out) override {
                return static_cast<std::size_t>(
                    drmp3_read_pcm_frames_f32(&file_.value, n, reinterpret_cast<float *>(out))
                );
            }

            bool seek_native(std::size_t frame) override {
                return drmp3_seek_to_pcm_frame(&file_.value, frame);
            }

            mp3_file file_;
        };
    } // namespace

    void register_dr_audio_readers() {
        register_audio_reader("wav", [](std::string_view path) { return std::make_unique<wav_decoder>(path); });
        register_audio_reader("flac", [](std::string_view path) { return std::make_unique<flac_decoder>(path); });
        register_audio_reader("mp3", [](std::string_view path) { return std::make_unique<mp3_decoder>(path); });
    }
} // namespace alia
