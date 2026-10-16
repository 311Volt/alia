#include "wasapi_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_WASAPI
#include <limits>

namespace alia::detail {
    namespace {
        struct scoped_prop_variant {
            PROPVARIANT value{};

            ~scoped_prop_variant() {
                PropVariantClear(&value);
            }
        };

        std::string to_utf8(const wchar_t *text) {
            if (!text || !*text)
                return {};
            const int count =
                WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, nullptr, 0, nullptr, nullptr);
            if (count <= 0)
                throw std::runtime_error("invalid WASAPI endpoint text");
            std::string result(count, '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, result.data(), count, nullptr, nullptr);
            result.pop_back();
            return result;
        }

        std::wstring to_wide(std::string_view text) {
            if (text.empty())
                return {};
            if (text.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
                throw std::invalid_argument("WASAPI endpoint id is too long");
            const auto length = static_cast<int>(text.size());
            const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
            if (count <= 0 || text.find('\0') != std::string_view::npos)
                throw std::invalid_argument("invalid UTF-8 WASAPI endpoint id");
            std::wstring result(count, L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, result.data(), count);
            return result;
        }

        std::string endpoint_id(IMMDevice *device) {
            LPWSTR raw = nullptr;
            if (FAILED(device->GetId(&raw)))
                return {};
            wasapi_task_ptr<wchar_t> owned(raw);
            return to_utf8(raw);
        }

        audio_stream_format mix_format(const WAVEFORMATEX &wave) {
            sample_encoding encoding;
            if (wave.wFormatTag == WAVE_FORMAT_IEEE_FLOAT)
                encoding = sample_encoding::floating_point;
            else if (wave.wFormatTag == WAVE_FORMAT_PCM)
                encoding = wave.wBitsPerSample == 8 ? sample_encoding::unsigned_int : sample_encoding::signed_int;
            else if (wave.wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                     wave.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX)) {
                const auto &extended = reinterpret_cast<const WAVEFORMATEXTENSIBLE &>(wave);
                if (IsEqualGUID(extended.SubFormat, wasapi_float_subtype))
                    encoding = sample_encoding::floating_point;
                else if (IsEqualGUID(extended.SubFormat, wasapi_pcm_subtype))
                    encoding = wave.wBitsPerSample == 8 ? sample_encoding::unsigned_int : sample_encoding::signed_int;
                else
                    throw std::runtime_error("unsupported WASAPI mix encoding");
            } else
                throw std::runtime_error("unsupported WASAPI mix encoding");
            return {
                {{encoding, static_cast<std::uint8_t>(wave.wBitsPerSample / 8)}, wave.nChannels},
                static_cast<int>(wave.nSamplesPerSec)
            };
        }
    } // namespace

    bool wasapi_enumerator(wasapi_com_ptr<IMMDeviceEnumerator> &enumerator) {
        return SUCCEEDED(CoCreateInstance(
            wasapi_enumerator_class,
            nullptr,
            CLSCTX_INPROC_SERVER,
            wasapi_enumerator_iid,
            reinterpret_cast<void **>(enumerator.put())
        ));
    }

    bool wasapi_endpoint(
        IMMDeviceEnumerator *enumerator, audio_direction direction, std::string_view id,
        wasapi_com_ptr<IMMDevice> &device
    ) {
        const auto flow = direction == audio_direction::output ? eRender : eCapture;
        HRESULT result;
        if (id.empty())
            result = enumerator->GetDefaultAudioEndpoint(flow, eConsole, device.put());
        else
            result = enumerator->GetDevice(to_wide(id).c_str(), device.put());
        if (FAILED(result))
            return false;
        wasapi_com_ptr<IMMEndpoint> endpoint;
        EDataFlow actual;
        DWORD state = 0;
        return SUCCEEDED(device->QueryInterface(wasapi_endpoint_iid, reinterpret_cast<void **>(endpoint.put()))) &&
               SUCCEEDED(endpoint->GetDataFlow(&actual)) && actual == flow && SUCCEEDED(device->GetState(&state)) &&
               (state & DEVICE_STATE_ACTIVE);
    }

    bool wasapi_wave_format(const audio_stream_format &format, WAVEFORMATEXTENSIBLE &wave) {
        const auto point = format.frame.point;
        using namespace sample_point_formats;
        if (point != u8 && point != s16 && point != s32 && point != f32 && point != f64)
            return false;
        if (format.sample_rate <= 0 || format.frame.channels <= 0 ||
            format.frame.channels > (std::numeric_limits<WORD>::max)())
            return false;
        const auto stride = format.frame.bytes_per_frame();
        if (stride > (std::numeric_limits<WORD>::max)())
            return false;
        if (static_cast<std::uint64_t>(stride) * format.sample_rate > (std::numeric_limits<DWORD>::max)())
            return false;
        // Standard masks: mono, stereo, 3.0, quad, 5.0, 5.1, 6.1, 7.1.
        constexpr DWORD masks[] = {0, 0x4, 0x3, 0x7, 0x33, 0x37, 0x3f, 0x13f, 0x63f};
        wave = {};
        wave.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wave.Format.nChannels = static_cast<WORD>(format.frame.channels);
        wave.Format.nSamplesPerSec = format.sample_rate;
        wave.Format.nAvgBytesPerSec = stride * static_cast<DWORD>(format.sample_rate);
        wave.Format.nBlockAlign = static_cast<WORD>(stride);
        wave.Format.wBitsPerSample = point.bytes * 8;
        wave.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        wave.Samples.wValidBitsPerSample = wave.Format.wBitsPerSample;
        wave.dwChannelMask = format.frame.channels <= 8 ? masks[format.frame.channels] : 0;
        wave.SubFormat = point.encoding == sample_encoding::floating_point ? wasapi_float_subtype : wasapi_pcm_subtype;
        return true;
    }

    bool wasapi_initialize(
        IMMDevice *device, const audio_stream_format &format, int period_frames, wasapi_com_ptr<IAudioClient> &client
    ) {
        WAVEFORMATEXTENSIBLE wave;
        if (!wasapi_wave_format(format, wave) || period_frames < 0)
            return false;
        const auto duration = static_cast<REFERENCE_TIME>(10000000LL * period_frames / format.sample_rate);
        auto activate = [&] {
            return SUCCEEDED(device->Activate(
                wasapi_client_iid, CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void **>(client.put())
            ));
        };
        if (!activate())
            return false;
        WAVEFORMATEX *closest = nullptr;
        const HRESULT support = client->IsFormatSupported(AUDCLNT_SHAREMODE_SHARED, &wave.Format, &closest);
        CoTaskMemFree(closest); // S_FALSE is not exact support.
        // Probe the conversion flags by initializing the exact requested format.
        // Older systems reject these flags; no version-dependent DLL imports.
        // https://learn.microsoft.com/windows/win32/coreaudio/audclnt-streamflags-xxx-constants
        constexpr DWORD conversion_flags = 0x80000000u | 0x08000000u; // AUTOCONVERTPCM | SRC_DEFAULT_QUALITY
        HRESULT result = client->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | conversion_flags,
            duration,
            0,
            &wave.Format,
            nullptr
        );
        if (SUCCEEDED(result))
            return true;
        if (support != S_OK || !activate())
            return false;
        return SUCCEEDED(client->Initialize(
            AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, duration, 0, &wave.Format, nullptr
        ));
    }

    bool wasapi_format_supported(
        audio_context_handle *, audio_direction direction, std::string_view id, const audio_stream_format &format
    ) {
        wasapi_com_scope apartment;
        if (!apartment.usable())
            return false;
        wasapi_com_ptr<IMMDeviceEnumerator> enumerator;
        wasapi_com_ptr<IMMDevice> device;
        wasapi_com_ptr<IAudioClient> client;
        return wasapi_enumerator(enumerator) && wasapi_endpoint(enumerator.get(), direction, id, device) &&
               wasapi_initialize(device.get(), format, 0, client);
    }

    std::vector<audio_device_info> wasapi_enumerate_devices(audio_context_handle *, audio_direction direction) {
        wasapi_com_scope apartment;
        if (!apartment.usable())
            throw std::runtime_error("WASAPI COM initialization failed");
        wasapi_com_ptr<IMMDeviceEnumerator> enumerator;
        if (!wasapi_enumerator(enumerator))
            throw std::runtime_error("WASAPI endpoint enumeration unavailable");
        wasapi_com_ptr<IMMDevice> default_device;
        std::string default_id;
        if (wasapi_endpoint(enumerator.get(), direction, {}, default_device))
            default_id = endpoint_id(default_device.get());
        wasapi_com_ptr<IMMDeviceCollection> collection;
        if (FAILED(enumerator->EnumAudioEndpoints(
                direction == audio_direction::output ? eRender : eCapture, DEVICE_STATE_ACTIVE, collection.put()
            )))
            throw std::runtime_error("WASAPI endpoint enumeration failed");
        UINT count = 0;
        if (FAILED(collection->GetCount(&count)))
            throw std::runtime_error("WASAPI endpoint count failed");
        std::vector<audio_device_info> result;
        for (UINT i = 0; i < count; ++i) {
            wasapi_com_ptr<IMMDevice> device;
            wasapi_com_ptr<IAudioClient> client;
            if (FAILED(collection->Item(i, device.put())) ||
                FAILED(device->Activate(
                    wasapi_client_iid, CLSCTX_INPROC_SERVER, nullptr, reinterpret_cast<void **>(client.put())
                )))
                continue;
            WAVEFORMATEX *raw = nullptr;
            if (FAILED(client->GetMixFormat(&raw)))
                continue;
            wasapi_task_ptr<WAVEFORMATEX> wave(raw);
            audio_device_info info;
            info.id = endpoint_id(device.get());
            if (info.id.empty())
                continue;
            info.name = info.id;
            info.direction = direction;
            info.is_default = info.id == default_id;
            info.preferred = mix_format(*wave);
            wasapi_com_ptr<IPropertyStore> properties;
            if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, properties.put()))) {
                scoped_prop_variant name;
                if (SUCCEEDED(properties->GetValue(wasapi_friendly_name, &name.value)) && name.value.vt == VT_LPWSTR &&
                    name.value.pwszVal)
                    info.name = to_utf8(name.value.pwszVal);
            }
            result.push_back(std::move(info));
        }
        return result;
    }
} // namespace alia::detail
#endif
