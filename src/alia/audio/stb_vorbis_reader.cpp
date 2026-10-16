#include "stb_vorbis_reader.hpp"
#include "audio_io.hpp"
#include "audio_codec_file.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

// Keep stb_vorbis's implementation and its short macros in this translation unit.
#define STB_VORBIS_NO_PUSHDATA_API
#include <stb_vorbis.c>

namespace alia {
    namespace {
        class vorbis_decoder final : public audio_decoder {
        public:
            explicit vorbis_decoder(std::string_view path) : file_(nullptr, &stb_vorbis_close) {
                std::unique_ptr<FILE, decltype(&std::fclose)> input(detail::open_audio_stdio_file(path), &std::fclose);
                int error = VORBIS_file_open_failure;
                stdio_file_ = input.get();
                if (input)
                    file_.reset(stb_vorbis_open_file(input.release(), 1, &error, nullptr));
                if (!file_)
                    throw std::runtime_error(
                        "open_audio_reader: stb_vorbis failed to open '" + std::string(path) + "' (error " +
                        std::to_string(error) + ")"
                    );
                // close_on_free transfers ownership even if opening fails.
                const auto info = stb_vorbis_get_info(file_.get());
                if (info.channels < 1 || info.channels > 8 || !info.sample_rate ||
                    info.sample_rate > static_cast<unsigned int>((std::numeric_limits<int>::max)()))
                    throw std::runtime_error("open_audio_reader: unsupported Vorbis channels or sample rate");
                format_ = {{sample_point_formats::f32, info.channels}, static_cast<int>(info.sample_rate)};
                count_ = stb_vorbis_stream_length_in_samples(file_.get());
                check_error();
                scratch_.resize(chunk_frames * info.channels);
            }

            audio_stream_format format() const override {
                return format_;
            }

            std::optional<std::size_t> frame_count() const override {
                return count_;
            }

            std::size_t read(std::span<std::byte> frames) override {
                const auto stride = static_cast<std::size_t>(format_.frame.bytes_per_frame());
                if (frames.size() % stride)
                    throw std::invalid_argument("Vorbis decoder: incomplete frame");
                const auto requested = frames.size() / stride;
                const auto channels = format_.frame.channels;
                std::size_t done = 0;
                while (done < requested && position_ < count_) {
                    const auto n = (std::min)({chunk_frames, requested - done, count_ - position_});
                    const int read = stb_vorbis_get_samples_float_interleaved(
                        file_.get(), channels, scratch_.data(), static_cast<int>(n * channels)
                    );
                    check_error();
                    if (read < 0 || static_cast<std::size_t>(read) != n)
                        throw std::runtime_error("Vorbis decoder: truncated or corrupt file");
                    const auto &map = channel_maps[channels - 1];
                    for (int f = 0; f < read; ++f) {
                        for (int c = 0; c < channels; ++c) {
                            const auto point = scratch_[f * channels + map[c]];
                            std::memcpy(frames.data() + (done + f) * stride + c * sizeof(float), &point, sizeof(float));
                        }
                    }
                    done += read;
                    position_ += read;
                }
                return done;
            }

            void seek(std::size_t frame) override {
                if (frame > count_)
                    throw std::out_of_range("Vorbis decoder seek past end of stream");
                if (!seek_native(frame)) {
                    (void)stb_vorbis_get_error(file_.get());
                    if (!seek_native(position_))
                        throw std::runtime_error("Vorbis decoder: seek and position recovery failed");
                    throw std::runtime_error("Vorbis decoder: seek failed");
                }
                position_ = frame;
            }

        private:
            bool seek_native(std::size_t frame) {
                if (!frame)
                    return stb_vorbis_seek_start(file_.get()) != 0;
                // Seek to the last sample and consume it to support exactly EOF.
                if (frame == count_) {
                    return stb_vorbis_seek(file_.get(), static_cast<unsigned int>(frame - 1)) &&
                           stb_vorbis_get_samples_float_interleaved(
                               file_.get(), format_.frame.channels, scratch_.data(), format_.frame.channels
                           ) == 1;
                }
                return stb_vorbis_seek(file_.get(), static_cast<unsigned int>(frame)) != 0;
            }

            void check_error() {
                if (std::ferror(stdio_file_))
                    throw std::runtime_error("Vorbis decoder: file read failed");
                const int error = stb_vorbis_get_error(file_.get());
                if (error != VORBIS__no_error)
                    throw std::runtime_error("Vorbis decoder error " + std::to_string(error));
            }

            static constexpr std::size_t chunk_frames = 4096;
            // Vorbis -> WAVEFORMATEXTENSIBLE/SMPTE channel order.
            static constexpr std::array<std::array<int, 8>, 8> channel_maps{
                {{0},
                 {0, 1},
                 {0, 2, 1},
                 {0, 1, 2, 3},
                 {0, 2, 1, 3, 4},
                 {0, 2, 1, 5, 3, 4},
                 {0, 2, 1, 6, 5, 3, 4},
                 {0, 2, 1, 7, 5, 6, 3, 4}}
            };
            std::unique_ptr<stb_vorbis, decltype(&stb_vorbis_close)> file_;
            FILE *stdio_file_ = nullptr; // Non-owning, closed by stb_vorbis.
            audio_stream_format format_{};
            std::size_t count_ = 0, position_ = 0;
            std::vector<float> scratch_;
        };
    } // namespace

    void register_stb_vorbis_reader() {
        register_audio_reader("ogg", [](std::string_view path) { return std::make_unique<vorbis_decoder>(path); });
    }
} // namespace alia
