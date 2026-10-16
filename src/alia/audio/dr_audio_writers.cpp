#include <dr_wav.h>
#include "dr_audio_writers.hpp"
#include "audio_io.hpp"
#include "audio_codec_file.hpp"
#include "sample_conversions.hpp"
#include <string>

namespace alia {
    namespace {
        void write_wav(const_audio_clip_view clip, std::string_view path) {
            using namespace sample_point_formats;
            detail::validate_erased_audio_format(clip.format());
            const auto point = clip.format().point;
            if (point != u8 && point != s16 && point != s32 && point != f32 && point != f64)
                throw std::invalid_argument("save_audio_clip: WAV writer does not support this point format");
            if (clip.sample_rate() <= 0 || clip.format().channels > 65535)
                throw std::invalid_argument("save_audio_clip: invalid WAV rate or channel count");
            const auto stride = static_cast<unsigned int>(clip.format().bytes_per_frame());
            if (stride > 65535 ||
                static_cast<unsigned int>(clip.sample_rate()) > (std::numeric_limits<drwav_uint32>::max)() / stride)
                throw std::invalid_argument("save_audio_clip: WAV block size or byte rate is too large");
            const drwav_data_format format{
                drwav_container_riff,
                static_cast<drwav_uint32>(
                    point.encoding == sample_encoding::floating_point ? DR_WAVE_FORMAT_IEEE_FLOAT : DR_WAVE_FORMAT_PCM
                ),
                static_cast<drwav_uint32>(clip.format().channels),
                static_cast<drwav_uint32>(clip.sample_rate()),
                static_cast<drwav_uint32>(point.bytes * 8)
            };
            // RIFF chunk lengths are 32-bit. Never silently wrap a large write.
            if (clip.bytes().size() > (std::numeric_limits<drwav_uint32>::max)() - 128u)
                throw std::invalid_argument("save_audio_clip: audio is too large for RIFF WAV");
            drwav wav{};
            if (!detail::open_audio_file(
                    path,
                    [&](const char *p) { return drwav_init_file_write(&wav, p, &format, nullptr); },
                    [&](const wchar_t *p) { return drwav_init_file_write_w(&wav, p, &format, nullptr); }
                ))
                throw std::runtime_error("save_audio_clip: dr_wav failed to open '" + std::string(path) + "'");
            const auto written = drwav_write_pcm_frames(&wav, clip.frame_count(), clip.bytes().data());
            // dr_wav owns this FILE; flush while it is still alive so buffered
            // I/O errors cannot be hidden by its final close.
            auto *file = static_cast<FILE *>(wav.pUserData);
            const bool io_failed = std::fflush(file) != 0 || std::ferror(file) != 0;
            const auto result = drwav_uninit(&wav);
            if (written != clip.frame_count() || io_failed || result != DRWAV_SUCCESS)
                throw std::runtime_error("save_audio_clip: dr_wav failed to write '" + std::string(path) + "'");
        }
    } // namespace

    void register_dr_audio_writers() {
        register_audio_writer("wav", write_wav);
    }
} // namespace alia
