#include "alsa_ops.hpp"
#ifdef ALIA_COMPILE_AUDIO_BACKEND_ALSA
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace alia::detail {
    namespace {
        struct free_memory {
            void operator()(void *p) const noexcept {
                std::free(p);
            }
        };

        struct free_hints {
            void operator()(void **p) const noexcept {
                if (p)
                    snd_device_name_free_hint(p);
            }
        };

        using hint_text = std::unique_ptr<char, free_memory>;

        audio_stream_format preferred_format(audio_direction direction, std::string_view id) {
            alsa_pcm_ptr pcm;
            if (!alsa_open(direction, id, pcm))
                return {};
            snd_pcm_hw_params_t *params;
            snd_pcm_hw_params_alloca(&params);
            using namespace sample_point_formats;
            for (auto point : {f32, s16, s32, u8, s8, u16, u32, f64}) {
                unsigned channels = direction == audio_direction::output ? 2 : 1;
                unsigned rate = 48000;
                int dir = 0;
                if (snd_pcm_hw_params_any(pcm.get(), params) < 0 ||
                    snd_pcm_hw_params_set_access(pcm.get(), params, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
                    snd_pcm_hw_params_set_format(pcm.get(), params, alsa_point_format(point)) < 0 ||
                    snd_pcm_hw_params_set_channels_near(pcm.get(), params, &channels) < 0 || channels == 0 ||
                    channels > 8 || snd_pcm_hw_params_set_rate_near(pcm.get(), params, &rate, &dir) < 0 || !rate ||
                    rate > unsigned((std::numeric_limits<int>::max)()))
                    continue;
                audio_stream_properties properties;
                snd_pcm_uframes_t capacity = 0;
                std::array<unsigned, 8> map{};
                const audio_stream_format format{{point, static_cast<int>(channels)}, static_cast<int>(rate)};
                if (alsa_configure(pcm.get(), format, 0, properties, capacity) &&
                    alsa_channel_map(pcm.get(), format.frame.channels, map))
                    return format;
                snd_pcm_hw_free(pcm.get());
            }
            return {};
        }
    } // namespace

    snd_pcm_format_t alsa_point_format(sample_point_format point) {
        using namespace sample_point_formats;
        if (point == u8)
            return SND_PCM_FORMAT_U8;
        if (point == s8)
            return SND_PCM_FORMAT_S8;
        if (point == u16)
            return SND_PCM_FORMAT_U16;
        if (point == s16)
            return SND_PCM_FORMAT_S16;
        if (point == u32)
            return SND_PCM_FORMAT_U32;
        if (point == s32)
            return SND_PCM_FORMAT_S32;
        if (point == f32)
            return SND_PCM_FORMAT_FLOAT;
        if (point == f64)
            return SND_PCM_FORMAT_FLOAT64;
        return SND_PCM_FORMAT_UNKNOWN;
    }

    bool alsa_open(audio_direction direction, std::string_view id, alsa_pcm_ptr &pcm) {
        if (id.find('\0') != std::string_view::npos)
            return false;
        const std::string name(id.empty() ? "default" : id);
        snd_pcm_t *raw = nullptr;
        if (snd_pcm_open(
                &raw,
                name.c_str(),
                direction == audio_direction::output ? SND_PCM_STREAM_PLAYBACK : SND_PCM_STREAM_CAPTURE,
                SND_PCM_NONBLOCK
            ) < 0)
            return false;
        pcm.reset(raw);
        return true;
    }

    bool alsa_configure(
        snd_pcm_t *pcm, const audio_stream_format &format, int period_hint, audio_stream_properties &properties,
        snd_pcm_uframes_t &capacity
    ) {
        const auto point = alsa_point_format(format.frame.point);
        if (point == SND_PCM_FORMAT_UNKNOWN || format.sample_rate <= 0 || format.frame.channels <= 0 ||
            format.frame.channels > 8 || period_hint < 0)
            return false;
        snd_pcm_hw_params_t *hw;
        snd_pcm_hw_params_alloca(&hw);
        if (snd_pcm_hw_params_any(pcm, hw) < 0 ||
            snd_pcm_hw_params_set_access(pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED) < 0 ||
            snd_pcm_hw_params_set_format(pcm, hw, point) < 0 ||
            snd_pcm_hw_params_set_channels(pcm, hw, static_cast<unsigned>(format.frame.channels)) < 0 ||
            snd_pcm_hw_params_set_rate(pcm, hw, static_cast<unsigned>(format.sample_rate), 0) < 0)
            return false;
        snd_pcm_uframes_t period = period_hint ? period_hint : (std::max)(1, format.sample_rate / 100);
        int dir = 0;
        if (snd_pcm_hw_params_set_period_size_near(pcm, hw, &period, &dir) < 0 || !period ||
            period > static_cast<snd_pcm_uframes_t>((std::numeric_limits<int>::max)()))
            return false;
        capacity = static_cast<snd_pcm_uframes_t>(
            (std::min)(std::uint64_t(period) * 4, std::uint64_t((std::numeric_limits<int>::max)()))
        );
        if (snd_pcm_hw_params_set_buffer_size_near(pcm, hw, &capacity) < 0 || snd_pcm_hw_params(pcm, hw) < 0 ||
            snd_pcm_get_params(pcm, &capacity, &period) < 0 || snd_pcm_poll_descriptors_count(pcm) <= 0 || !period ||
            capacity < period || capacity > static_cast<snd_pcm_uframes_t>((std::numeric_limits<int>::max)()))
            return false;
        // Only the period and buffer size may be negotiated. Check the final
        // configuration explicitly; never accept a nearest rate or encoding.
        unsigned rate = 0, channels = 0;
        snd_pcm_format_t actual;
        if (snd_pcm_hw_params_current(pcm, hw) < 0 || snd_pcm_hw_params_get_format(hw, &actual) < 0 ||
            actual != point || snd_pcm_hw_params_get_channels(hw, &channels) < 0 ||
            channels != static_cast<unsigned>(format.frame.channels) ||
            snd_pcm_hw_params_get_rate(hw, &rate, &dir) < 0 || rate != static_cast<unsigned>(format.sample_rate) ||
            dir != 0)
            return false;
        snd_pcm_sw_params_t *sw;
        snd_pcm_sw_params_alloca(&sw);
        snd_pcm_uframes_t boundary = 0;
        if (snd_pcm_sw_params_current(pcm, sw) < 0 || snd_pcm_sw_params_get_boundary(sw, &boundary) < 0 ||
            snd_pcm_sw_params_set_avail_min(pcm, sw, period) < 0 ||
            snd_pcm_sw_params_set_start_threshold(pcm, sw, boundary) < 0 ||
            snd_pcm_sw_params_set_stop_threshold(pcm, sw, capacity) < 0 || snd_pcm_sw_params(pcm, sw) < 0)
            return false;
        properties = {
            format, static_cast<int>(period), static_cast<int>(period), double(capacity) / format.sample_rate
        };
        return true;
    }

    bool alsa_channel_map(snd_pcm_t *pcm, int channels, std::array<unsigned, 8> &mapping) {
        for (int i = 0; i < channels; ++i)
            mapping[i] = static_cast<unsigned>(i);
        std::unique_ptr<snd_pcm_chmap_t, free_memory> map(snd_pcm_get_chmap(pcm));
        // Mono/stereo ordering is unambiguous on older drivers without maps.
        // Surround requires reported positions so we can preserve WAVE order.
        if (!map)
            return channels <= 2;
        if (map->channels != static_cast<unsigned>(channels))
            return false;
        if (channels == 1)
            return true;
        constexpr unsigned desired[][8] = {
            {},
            {},
            {SND_CHMAP_FL, SND_CHMAP_FR},
            {SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC},
            {SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_RL, SND_CHMAP_RR},
            {SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_RL, SND_CHMAP_RR},
            {SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RL, SND_CHMAP_RR},
            {SND_CHMAP_FL, SND_CHMAP_FR, SND_CHMAP_FC, SND_CHMAP_LFE, SND_CHMAP_RL, SND_CHMAP_RR, SND_CHMAP_RC},
            {SND_CHMAP_FL,
             SND_CHMAP_FR,
             SND_CHMAP_FC,
             SND_CHMAP_LFE,
             SND_CHMAP_RL,
             SND_CHMAP_RR,
             SND_CHMAP_SL,
             SND_CHMAP_SR}
        };
        for (int i = 0; i < channels; ++i) {
            bool found = false;
            for (int j = 0; j < channels; ++j)
                if (map->pos[j] == desired[channels][i]) {
                    mapping[i] = static_cast<unsigned>(j);
                    found = true;
                    break;
                }
            if (!found)
                return false;
        }
        return true;
    }

    bool alsa_format_supported(
        audio_context_handle *, audio_direction direction, std::string_view id, const audio_stream_format &format
    ) {
        alsa_pcm_ptr pcm;
        audio_stream_properties properties;
        snd_pcm_uframes_t capacity = 0;
        std::array<unsigned, 8> map{};
        return alsa_open(direction, id, pcm) && alsa_configure(pcm.get(), format, 0, properties, capacity) &&
               alsa_channel_map(pcm.get(), format.frame.channels, map);
    }

    std::vector<audio_device_info> alsa_enumerate_devices(audio_context_handle *, audio_direction direction) {
        std::vector<audio_device_info> result;
        auto add = [&](std::string id, std::string name) {
            if (id.empty() || id == "null" ||
                std::any_of(result.begin(), result.end(), [&](const auto &d) { return d.id == id; }))
                return;
            const auto preferred = preferred_format(direction, id);
            if (!preferred.sample_rate)
                return;
            std::replace(name.begin(), name.end(), '\n', ' ');
            const bool is_default = id == "default";
            result.push_back({std::move(id), std::move(name), direction, is_default, preferred});
        };
        add("default", direction == audio_direction::output ? "Default ALSA playback" : "Default ALSA capture");
        void **raw = nullptr;
        if (snd_device_name_hint(-1, "pcm", &raw) < 0)
            return result;
        std::unique_ptr<void *, free_hints> hints(raw);
        if (!raw)
            return result;
        for (auto hint = raw; *hint; ++hint) {
            hint_text name(snd_device_name_get_hint(*hint, "NAME"));
            hint_text description(snd_device_name_get_hint(*hint, "DESC"));
            hint_text io(snd_device_name_get_hint(*hint, "IOID"));
            if (!name ||
                (io && std::string_view(io.get()) != (direction == audio_direction::output ? "Output" : "Input")))
                continue;
            add(name.get(), description ? description.get() : name.get());
        }
        return result;
    }
} // namespace alia::detail
#endif
