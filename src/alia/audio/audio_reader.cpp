#include "audio_reader.hpp"
#include "sample_conversions.hpp"
#include <vector>

namespace alia {
    audio_reader::audio_reader(std::unique_ptr<audio_decoder> decoder) : decoder_(std::move(decoder)) {
        if (!decoder_)
            throw std::invalid_argument("audio_reader: null decoder");
        format_ = decoder_->format();
        detail::validate_erased_audio_format(format_.frame);
        if (format_.sample_rate <= 0)
            throw std::invalid_argument("audio_reader: sample rate must be positive");
        count_ = decoder_->frame_count();
    }

    audio_reader::audio_reader(audio_reader &&other) noexcept
        : decoder_(std::move(other.decoder_)), format_(other.format_), count_(std::exchange(other.count_, {})),
          position_(std::exchange(other.position_, 0)) {
    }

    audio_reader &audio_reader::operator=(audio_reader &&other) noexcept {
        if (this != &other) {
            decoder_ = std::move(other.decoder_);
            format_ = other.format_;
            count_ = std::exchange(other.count_, {});
            position_ = std::exchange(other.position_, 0);
        }
        return *this;
    }

    audio_stream_format audio_reader::format() const {
        if (!valid())
            throw std::logic_error("audio reader is not valid");
        return format_;
    }

    std::optional<std::size_t> audio_reader::frame_count() const {
        (void)format();
        return count_;
    }

    std::size_t audio_reader::read_bytes(std::span<std::byte> bytes) {
        const auto stride = static_cast<std::size_t>(frame_format().bytes_per_frame());
        if (bytes.size() % stride)
            throw std::invalid_argument("audio_reader::read: incomplete frame");
        if (bytes.empty())
            return 0;
        const auto requested = bytes.size() / stride;
        if (requested > (std::numeric_limits<std::size_t>::max)() - position_)
            throw std::length_error("audio reader position overflow");
        const auto read = decoder_->read(bytes);
        if (read > requested || (count_ && read > *count_ - position_))
            throw std::runtime_error("audio decoder returned an invalid frame count");
        position_ += read;
        if (read < requested && count_ && position_ != *count_)
            throw std::runtime_error("audio decoder reached EOF before its advertised frame count");
        return read;
    }

    void audio_reader::seek(std::size_t frame) {
        (void)format();
        if (count_ && frame > *count_)
            throw std::out_of_range("audio_reader::seek: past end of stream");
        decoder_->seek(frame);
        position_ = frame;
    }

    audio_clip audio_reader::read_all() {
        const auto fmt = format();
        const auto stride = static_cast<std::size_t>(fmt.frame.bytes_per_frame());
        constexpr std::size_t chunk_frames = 4096;
        if (count_) {
            audio_clip result(fmt.frame, fmt.sample_rate, *count_ - position_);
            auto bytes = result.bytes();
            std::size_t offset = 0;
            while (offset < result.count_) {
                const auto count = (std::min)(chunk_frames, result.count_ - offset);
                offset += read_bytes(bytes.subspan(offset * stride, count * stride));
            }
            return result;
        }
        std::vector<std::byte> bytes;
        std::size_t count = 0;
        for (;;) {
            if (count > (std::numeric_limits<std::size_t>::max)() - chunk_frames)
                throw std::length_error("decoded audio is too large");
            bytes.resize(detail::audio_byte_count(fmt.frame, count + chunk_frames));
            const auto read = read_bytes(std::span(bytes).subspan(count * stride, chunk_frames * stride));
            count += read;
            if (read < chunk_frames)
                break;
        }
        return audio_clip(fmt.frame, fmt.sample_rate, std::span<const std::byte>(bytes.data(), count * stride));
    }
} // namespace alia
