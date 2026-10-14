#include "mme_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_MME
#include <charconv>
#include <limits>

namespace alia::detail {
    namespace {
        std::string to_utf8(const wchar_t *text) {
            const int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (count <= 0) return {};
            std::string result(count, '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), count, nullptr, nullptr);
            result.pop_back();
            return result;
        }
        audio_stream_format preferred_format(audio_context_handle *context, audio_direction direction, std::string_view id) {
            // MME has no mix-format query. Publish a format the mapper/device
            // actually accepts, preferring common PCM rates and channel counts.
            for (int rate : {48000, 44100, 22050, 11025, 8000, 96000})
                for (int channels : {2, 1})
                    for (auto point : {sample_point_formats::s16, sample_point_formats::u8, sample_point_formats::f32}) {
                        const audio_stream_format format{{point, channels}, rate};
                        if (mme_format_supported(context, direction, id, format)) return format;
                    }
            return {};
        }
    }
    bool mme_device_id(audio_direction direction, std::string_view id, UINT &result) {
        if (id.empty() || id == "default") { result = WAVE_MAPPER; return true; }
        const std::string_view prefix = direction == audio_direction::output ? "out:" : "in:";
        if (!id.starts_with(prefix)) return false;
        id.remove_prefix(prefix.size());
        if (id.empty()) return false;
        const auto parsed = std::from_chars(id.data(), id.data() + id.size(), result);
        return parsed.ec == std::errc{} && parsed.ptr == id.data() + id.size() &&
            result < (direction == audio_direction::output ? waveOutGetNumDevs() : waveInGetNumDevs());
    }
    bool mme_wave_format(const audio_stream_format &format, WAVEFORMATEXTENSIBLE &wave) {
        using namespace sample_point_formats;
        const auto point = format.frame.point;
        if (point != u8 && point != s16 && point != s32 && point != f32 && point != f64) return false;
        if (format.sample_rate <= 0 || format.frame.channels <= 0 || format.frame.channels > 8) return false;
        const auto stride = format.frame.bytes_per_frame();
        if (static_cast<std::uint64_t>(stride) * format.sample_rate > (std::numeric_limits<DWORD>::max)()) return false;
        constexpr DWORD masks[] = {0, 0x4, 0x3, 0x7, 0x33, 0x37, 0x3f, 0x13f, 0x63f};
        const bool floating = point.encoding == sample_encoding::floating_point;
        wave = {};
        // Keep legacy headers for the usual mono/stereo PCM and float formats.
        const bool extended = format.frame.channels > 2 || (floating ? point.bytes > 4 : point.bytes > 2);
        wave.Format.wFormatTag = extended ? WAVE_FORMAT_EXTENSIBLE : (floating ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM);
        wave.Format.nChannels = static_cast<WORD>(format.frame.channels);
        wave.Format.nSamplesPerSec = static_cast<DWORD>(format.sample_rate);
        wave.Format.nAvgBytesPerSec = static_cast<DWORD>(stride) * format.sample_rate;
        wave.Format.nBlockAlign = static_cast<WORD>(stride);
        wave.Format.wBitsPerSample = point.bytes * 8;
        if (wave.Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            wave.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
            wave.Samples.wValidBitsPerSample = wave.Format.wBitsPerSample;
            wave.dwChannelMask = masks[format.frame.channels];
            wave.SubFormat = GUID{floating ? 3u : 1u, 0, 0x0010, {0x80, 0, 0, 0xaa, 0, 0x38, 0x9b, 0x71}};
        }
        return true;
    }
    bool mme_format_supported(audio_context_handle *, audio_direction direction, std::string_view id, const audio_stream_format &format) {
        UINT device = 0;
        WAVEFORMATEXTENSIBLE wave;
        if (!mme_device_id(direction, id, device) || !mme_wave_format(format, wave)) return false;
        const MMRESULT result = direction == audio_direction::output ?
            waveOutOpen(nullptr, device, &wave.Format, 0, 0, WAVE_FORMAT_QUERY) :
            waveInOpen(nullptr, device, &wave.Format, 0, 0, WAVE_FORMAT_QUERY);
        return result == MMSYSERR_NOERROR;
    }
    std::vector<audio_device_info> mme_enumerate_devices(audio_context_handle *context, audio_direction direction) {
        std::vector<audio_device_info> result;
        const bool output = direction == audio_direction::output;
        const UINT count = output ? waveOutGetNumDevs() : waveInGetNumDevs();
        if (!count) return result;
        // The mapper resolves only when opening. The selected handle is kept
        // for the stream's lifetime, including after stop/start and device loss.
        const auto preferred = preferred_format(context, direction, "default");
        if (preferred.sample_rate > 0)
            result.push_back({"default", output ? "Default MME output" : "Default MME input", direction, true, preferred});
        for (UINT i = 0; i < count; ++i) {
            std::string name;
            if (output) {
                WAVEOUTCAPSW caps{};
                if (waveOutGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
                name = to_utf8(caps.szPname);
            } else {
                WAVEINCAPSW caps{};
                if (waveInGetDevCapsW(i, &caps, sizeof(caps)) != MMSYSERR_NOERROR) continue;
                name = to_utf8(caps.szPname);
            }
            auto id = std::string(output ? "out:" : "in:") + std::to_string(i);
            const auto format = preferred_format(context, direction, id);
            if (format.sample_rate > 0) result.push_back({std::move(id), std::move(name), direction, false, format});
        }
        return result;
    }
}
#endif
