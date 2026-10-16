#ifndef ALIA_AUDIO_AUDIO_READER_HPP
#define ALIA_AUDIO_AUDIO_READER_HPP

#include "audio_clip.hpp"
#include <optional>

namespace alia {
    /// Codec extension point. Use from one thread at a time. Readers decode to
    /// their native interleaved format and never resample. A newly constructed
    /// decoder starts at frame zero; its reported native format is immutable.
    class audio_decoder {
    public:
        virtual ~audio_decoder() = default;
        /// Positive rate and supported frame format, constant for this decoder.
        [[nodiscard]] virtual audio_stream_format format() const = 0;
        /// Total frames, or nullopt when finding the length requires a full decode.
        [[nodiscard]] virtual std::optional<std::size_t> frame_count() const = 0;
        /// frames.size() must be a multiple of format().frame.bytes_per_frame().
        /// Returns frames written; short reads mean EOF. Throws runtime_error on
        /// detected corruption or I/O failure. An empty span does not advance.
        virtual std::size_t read(std::span<std::byte> frames) = 0;
        /// Absolute frame seek, including exactly EOF. Throws out_of_range past
        /// EOF, runtime_error on other failures. A failed seek preserves position.
        virtual void seek(std::size_t frame) = 0;
    };

    namespace detail {
        struct audio_reader_access;
    }

    /// Move-only decoder owner. All operations require external synchronization.
    /// A default/moved-from reader is invalid; reading or querying its format
    /// throws logic_error. Reads require an exact native frame format match.
    class audio_reader {
    public:
        audio_reader() = default;
        explicit audio_reader(std::unique_ptr<audio_decoder>);
        audio_reader(audio_reader &&) noexcept;
        audio_reader &operator=(audio_reader &&) noexcept;
        audio_reader(const audio_reader &) = delete;
        audio_reader &operator=(const audio_reader &) = delete;

        [[nodiscard]] bool valid() const noexcept {
            return bool(decoder_);
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return valid();
        }

        [[nodiscard]] audio_stream_format format() const;

        [[nodiscard]] int sample_rate() const {
            return format().sample_rate;
        }

        [[nodiscard]] sample_frame_format frame_format() const {
            return format().frame;
        }

        [[nodiscard]] std::optional<std::size_t> frame_count() const;

        [[nodiscard]] std::size_t position() const noexcept {
            return position_;
        }

        template <sample_frame F, std::size_t N>
            requires(!std::is_const_v<F>)
        std::size_t read(std::span<F, N> frames) {
            if (frame_format() != sample_frame_format_of_v<F>)
                throw std::invalid_argument("audio_reader::read: frame format mismatch");
            return read_bytes(std::as_writable_bytes(frames));
        }

        void seek(std::size_t frame);
        /// Decode the remaining native frames. Known lengths allocate clip
        /// storage once; unknown lengths grow a temporary byte buffer.
        audio_clip read_all();

    private:
        friend struct detail::audio_reader_access;
        std::size_t read_bytes(std::span<std::byte>);
        std::unique_ptr<audio_decoder> decoder_;
        audio_stream_format format_{};
        std::optional<std::size_t> count_;
        std::size_t position_ = 0;
    };
} // namespace alia
#endif
