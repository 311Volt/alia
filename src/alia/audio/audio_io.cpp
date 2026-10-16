#include "audio_io.hpp"
#include "audio_codec_file.hpp"
#include "dr_audio_readers.hpp"
#include "dr_audio_writers.hpp"
#include "stb_vorbis_reader.hpp"
#include "sample_conversions.hpp"
#include <cctype>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace alia {
    namespace {
        struct codec_registry {
            std::mutex mutex;
            std::unordered_map<std::string, audio_reader_fn> readers;
            std::unordered_map<std::string, audio_writer_fn> writers;
        };

        codec_registry &registry() {
            static codec_registry instance;
            return instance;
        }

        // Built-ins only supply defaults: explicit user registration wins even
        // when it precedes the first open/save. No user callback runs under a lock.
        thread_local bool registering_defaults = false;

        void ensure_builtins() {
            static std::once_flag flag;
            std::call_once(flag, [] {
                struct guard {
                    guard() {
                        registering_defaults = true;
                    }
                    ~guard() {
                        registering_defaults = false;
                    }
                } scope;
                register_dr_audio_readers();
                register_stb_vorbis_reader();
                register_dr_audio_writers();
            });
        }

        std::string normalize_ext(std::string_view ext) {
            std::string result(ext);
            for (char &c : result)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return result;
        }

        std::string path_extension(std::string_view path) {
            auto ext = detail::audio_file_path(path).extension().u8string();
            if (!ext.empty() && ext[0] == u8'.')
                ext.erase(ext.begin());
            return normalize_ext({reinterpret_cast<const char *>(ext.data()), ext.size()});
        }

        audio_reader_fn find_reader(std::string_view path, std::string_view operation) {
            ensure_builtins();
            const auto ext = path_extension(path);
            auto &r = registry();
            std::lock_guard lock(r.mutex);
            const auto it = r.readers.find(ext);
            if (it == r.readers.end())
                throw std::runtime_error(std::string(operation) + ": no reader registered for extension '" + ext + "'");
            return it->second;
        }
    } // namespace

    void register_audio_reader(std::string_view ext, audio_reader_fn fn) {
        if (!fn)
            throw std::invalid_argument("register_audio_reader: empty reader");
        auto &r = registry();
        std::lock_guard lock(r.mutex);
        if (registering_defaults)
            r.readers.try_emplace(normalize_ext(ext), std::move(fn));
        else
            r.readers[normalize_ext(ext)] = std::move(fn);
    }

    void register_audio_writer(std::string_view ext, audio_writer_fn fn) {
        if (!fn)
            throw std::invalid_argument("register_audio_writer: empty writer");
        auto &r = registry();
        std::lock_guard lock(r.mutex);
        if (registering_defaults)
            r.writers.try_emplace(normalize_ext(ext), std::move(fn));
        else
            r.writers[normalize_ext(ext)] = std::move(fn);
    }

    audio_reader open_audio_reader(std::string_view path) {
        return audio_reader(find_reader(path, "open_audio_reader")(path));
    }

    audio_clip load_audio_clip(std::string_view path) {
        return audio_reader(find_reader(path, "load_audio_clip")(path)).read_all();
    }

    audio_clip load_audio_clip(std::string_view path, sample_frame_format format) {
        detail::validate_erased_audio_format(format);
        auto source = load_audio_clip(path);
        if (source.format() == format)
            return source;
        std::vector<std::byte> bytes(detail::audio_byte_count(format, source.frame_count()));
        converting_copy({format, source.sample_rate(), source.frame_count(), bytes.data()}, source.view());
        return audio_clip(format, source.sample_rate(), bytes);
    }

    void save_audio_clip(const_audio_clip_view clip, std::string_view path) {
        ensure_builtins();
        const auto ext = path_extension(path);
        audio_writer_fn writer;
        {
            auto &r = registry();
            std::lock_guard lock(r.mutex);
            const auto it = r.writers.find(ext);
            if (it == r.writers.end())
                throw std::runtime_error("save_audio_clip: no writer registered for extension '" + ext + "'");
            writer = it->second;
        }
        writer(clip, path);
    }
} // namespace alia
