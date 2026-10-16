#ifndef ALIA_AUDIO_AUDIO_READER_ACCESS_HPP
#define ALIA_AUDIO_AUDIO_READER_ACCESS_HPP
#include "audio_reader.hpp"

namespace alia::detail {
    /// Internal erased read for the player's worker; advances reader position.
    struct audio_reader_access {
        static std::size_t read(audio_reader &reader, std::span<std::byte> frames) {
            return reader.read_bytes(frames);
        }
    };
} // namespace alia::detail
#endif
