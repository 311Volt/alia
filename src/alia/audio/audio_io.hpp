#ifndef ALIA_AUDIO_AUDIO_IO_HPP
#define ALIA_AUDIO_AUDIO_IO_HPP

#include "audio_reader.hpp"
#include <functional>
#include <string_view>

namespace alia {
    using audio_reader_fn = std::function<std::unique_ptr<audio_decoder>(std::string_view path)>;
    using audio_writer_fn = std::function<void(const_audio_clip_view, std::string_view path)>;

    /// Register an extension without its leading dot. Matching is case-insensitive;
    /// later registrations replace earlier ones. Registration/lookup is thread-safe.
    void register_audio_reader(std::string_view extension, audio_reader_fn);
    void register_audio_writer(std::string_view extension, audio_writer_fn);

    /// Paths are UTF-8. Built-ins read WAV, FLAC, MP3 and Ogg Vorbis.
    /// Floating-point WAVs (including f64 files), MP3 and Vorbis decode to f32.
    /// Throws runtime_error on an unknown extension, open or decode failure.
    audio_reader open_audio_reader(std::string_view path);
    audio_clip load_audio_clip(std::string_view path);
    /// Convert point/channel format after decoding. The sample rate is unchanged.
    audio_clip load_audio_clip(std::string_view path, sample_frame_format);
    /// The built-in WAV writer accepts u8, s16, s32, f32 and f64.
    /// Unsupported formats throw invalid_argument; unknown extensions and I/O
    /// failures throw runtime_error.
    void save_audio_clip(const_audio_clip_view, std::string_view path);

    struct audio_reader_registrar {
        audio_reader_registrar(std::string_view ext, audio_reader_fn fn) {
            register_audio_reader(ext, std::move(fn));
        }
    };

    struct audio_writer_registrar {
        audio_writer_registrar(std::string_view ext, audio_writer_fn fn) {
            register_audio_writer(ext, std::move(fn));
        }
    };
} // namespace alia
#endif
