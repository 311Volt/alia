#ifndef ALIA_AUDIO_AUDIO_CODEC_FILE_HPP
#define ALIA_AUDIO_AUDIO_CODEC_FILE_HPP

#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace alia::detail {
    inline std::filesystem::path audio_file_path(std::string_view utf8) {
        if (utf8.find('\0') != std::string_view::npos)
            throw std::invalid_argument("audio file path contains a null character");
        return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t *>(utf8.data()), utf8.size()));
    }

    template <class Path = std::filesystem::path, class Narrow, class Wide>
    decltype(auto) open_audio_file(std::string_view utf8, Narrow &&narrow, Wide &&wide) {
        const Path path = audio_file_path(utf8);
        if constexpr (std::is_same_v<typename Path::value_type, wchar_t>)
            return std::forward<Wide>(wide)(path.c_str());
        else
            return std::forward<Narrow>(narrow)(path.c_str());
    }

    inline FILE *open_audio_stdio_file(std::string_view path) {
        return open_audio_file(
            path,
            [](const char *p) { return std::fopen(p, "rb"); },
            [](const wchar_t *p) -> FILE * {
#ifdef _WIN32
                return _wfopen(p, L"rb");
#else
                (void)p;
                return nullptr;
#endif
            }
        );
    }

    template <class T>
    std::size_t audio_file_frame_count(T count) {
        if (count > (std::numeric_limits<std::size_t>::max)())
            throw std::length_error("audio file has too many frames");
        return static_cast<std::size_t>(count);
    }
} // namespace alia::detail
#endif
