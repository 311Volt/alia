#include "alia/audio/audio.hpp"
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {
    std::string utf8_path(const std::filesystem::path &path) {
        const auto text = path.u8string();
        return {reinterpret_cast<const char *>(text.data()), text.size()};
    }
    class AudioIo : public ::testing::Test {
    protected:
        std::filesystem::path directory;
        void SetUp() override {
            static std::atomic<unsigned> sequence{0};
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (;;) {
                directory = std::filesystem::temp_directory_path() /
                    ("alia-audio-" + std::to_string(stamp) + "-" + std::to_string(sequence.fetch_add(1)));
                if (std::filesystem::create_directory(directory))
                    break;
            }
        }
        void TearDown() override {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
        std::string path(std::string_view name) const { return utf8_path(directory / std::string(name)); }

        template <class Point, std::size_t Channels>
        void round_trip() {
            using Frame = std::array<Point, Channels>;
            std::array<Frame, 7> frames;
            for (std::size_t f = 0; f < frames.size(); ++f) {
                for (std::size_t c = 0; c < Channels; ++c) {
                    if constexpr (std::floating_point<Point>)
                        frames[f][c] = Point(double(f * Channels + c) / 17.0 - 0.5);
                    else
                        frames[f][c] = static_cast<Point>(f * 3127 + c * 17);
                }
            }
            const alia::audio_clip original(44100, std::span(frames));
            const auto file = path("roundtrip.WaV");
            alia::save_audio_clip(original.view(), file);
            const auto decoded = alia::load_audio_clip(file);
            EXPECT_EQ(decoded.format(), original.format());
            EXPECT_EQ(decoded.sample_rate(), 44100);
            EXPECT_EQ(decoded.frame_count(), original.frame_count());
            ASSERT_EQ(decoded.bytes().size(), original.bytes().size());
            EXPECT_EQ(std::memcmp(decoded.bytes().data(), original.bytes().data(), original.bytes().size()), 0);
        }
    };

    TEST_F(AudioIo, WavRoundTripsSupportedPointFormatsInMonoAndStereo) {
        round_trip<std::uint8_t, 1>(); round_trip<std::uint8_t, 2>();
        round_trip<std::int16_t, 1>(); round_trip<std::int16_t, 2>();
        round_trip<std::int32_t, 1>(); round_trip<std::int32_t, 2>();
        round_trip<float, 1>(); round_trip<float, 2>();
    }
    TEST_F(AudioIo, DoublePrecisionWavWritesAndReadsAsFloat32) {
        for (int channels : {1, 2}) {
            const std::array<double, 6> points{-0.999999999, -0.333333333333, 0, 0.123456789, 0.5, 0.999999999};
            const alia::audio_clip source({alia::sample_point_formats::f64, channels}, 44100,
                                           std::as_bytes(std::span(points)));
            const auto file = path("double.wav");
            alia::save_audio_clip(source.view(), file);
            const auto decoded = alia::load_audio_clip(file);
            EXPECT_EQ(decoded.format(), (alia::sample_frame_format{alia::sample_point_formats::f32, channels}));
            EXPECT_EQ(decoded.sample_rate(), 44100);
            EXPECT_EQ(decoded.frame_count(), points.size() / channels);
            ASSERT_EQ(decoded.bytes().size(), points.size() * sizeof(float));
            for (std::size_t i = 0; i < points.size(); ++i) {
                float point;
                std::memcpy(&point, decoded.bytes().data() + i * sizeof(float), sizeof(float));
                EXPECT_FLOAT_EQ(point, static_cast<float>(points[i]));
            }
            auto reader = alia::open_audio_reader(file);
            std::array<double, 2> wrong{};
            EXPECT_THROW(reader.read(std::span(wrong)), std::invalid_argument);
        }
    }
    TEST_F(AudioIo, TypedReadsTrackPositionSeekAndDecodeOnlyRemainingFrames) {
        const std::array<std::int16_t, 7> frames{-32768, -16384, 0, 8192, 16384, 24576, 32767};
        const alia::audio_clip clip(32000, std::span(frames));
        const auto file = path("chunks.wav");
        alia::save_audio_clip(clip.view(), file);
        auto reader = alia::open_audio_reader(file);
        EXPECT_TRUE(reader.valid());
        EXPECT_EQ(reader.format(), (alia::audio_stream_format{alia::sample_frame_format_of_v<std::int16_t>, 32000}));
        EXPECT_EQ(reader.frame_count(), 7);
        std::array<std::int16_t, 3> output{};
        EXPECT_EQ(reader.read(std::span(output)), 3);
        EXPECT_EQ(output, (std::array<std::int16_t, 3>{-32768, -16384, 0}));
        EXPECT_EQ(reader.position(), 3);
        EXPECT_EQ(reader.read(std::span(output)), 3);
        EXPECT_EQ(reader.position(), 6);
        EXPECT_EQ(reader.read(std::span(output)), 1);
        EXPECT_EQ(output[0], 32767);
        EXPECT_EQ(reader.read(std::span(output)), 0);
        reader.seek(2);
        auto rest = reader.read_all();
        ASSERT_EQ(rest.frame_count(), 5);
        EXPECT_EQ(rest.view_as<std::int16_t>()[0], 0);
        EXPECT_EQ(reader.position(), 7);
        reader.seek(7);
        EXPECT_EQ(reader.read_all().frame_count(), 0);
        EXPECT_THROW(reader.seek(8), std::out_of_range);
        EXPECT_EQ(reader.position(), 7);
        reader.seek(1);
        auto moved = std::move(reader);
        EXPECT_FALSE(reader);
        EXPECT_EQ(moved.position(), 1);
        EXPECT_EQ(moved.read(std::span(output)), 3);
        EXPECT_EQ(output[0], -16384);
    }
    TEST_F(AudioIo, RejectsMismatchedFrameTypesBeforeAdvancing) {
        alia::audio_clip clip(3, 48000, std::uint8_t{128});
        const auto file = path("unsigned.wav");
        alia::save_audio_clip(clip.view(), file);
        auto reader = alia::open_audio_reader(file);
        EXPECT_EQ(reader.frame_format(), alia::sample_frame_format_of_v<std::uint8_t>);
        std::array<std::int8_t, 2> wrong{};
        EXPECT_THROW(reader.read(std::span(wrong)), std::invalid_argument);
        EXPECT_EQ(reader.position(), 0);
        std::array<std::uint8_t, 2> right{};
        EXPECT_EQ(reader.read(std::span(right)), 2);
        EXPECT_EQ(right[0], 128);
        std::array<std::array<std::uint8_t, 2>, 2> stereo{};
        EXPECT_THROW(reader.read(std::span(stereo)), std::invalid_argument);
    }
    TEST_F(AudioIo, ConversionChangesFormatAndPreservesRate) {
        const std::array<std::int16_t, 3> frames{-32768, 0, 16384};
        const alia::audio_clip clip(22050, std::span(frames));
        const auto file = path("convert.wav");
        alia::save_audio_clip(clip.view(), file);
        auto converted = alia::load_audio_clip(file, alia::sample_frame_format_of_v<alia::vec2f>);
        EXPECT_EQ(converted.sample_rate(), 22050);
        ASSERT_EQ(converted.frame_count(), 3);
        EXPECT_EQ(converted.view_as<alia::vec2f>()[0], (alia::vec2f{-1, -1}));
        EXPECT_EQ(converted.view_as<alia::vec2f>()[2], (alia::vec2f{0.5f, 0.5f}));
    }
    TEST_F(AudioIo, WritersRejectUnsupportedPointFormats) {
        const auto file = path("unsupported.wav");
        for (auto format : {alia::sample_point_formats::s8, alia::sample_point_formats::u16, alia::sample_point_formats::u32}) {
            std::array<std::byte, 4> bytes{};
            const alia::audio_clip clip({format, 1}, 48000, std::span<const std::byte>(bytes.data(), format.bytes));
            EXPECT_THROW(alia::save_audio_clip(clip.view(), file), std::invalid_argument);
        }
        EXPECT_FALSE(std::filesystem::exists(directory / "unsupported.wav"));
    }
    TEST_F(AudioIo, UnknownExtensionsAndMissingFilesThrow) {
        EXPECT_THROW(alia::open_audio_reader(path("missing.unknown_audio")), std::runtime_error);
        EXPECT_THROW(alia::load_audio_clip(path("missing.unknown_audio")), std::runtime_error);
        const alia::audio_clip clip(1, 48000, 0.0f);
        EXPECT_THROW(alia::save_audio_clip(clip.view(), path("missing.unknown_audio")), std::runtime_error);
        for (auto file : {"missing.wav", "missing.flac", "missing.mp3", "missing.ogg"})
            EXPECT_THROW(alia::open_audio_reader(path(file)), std::runtime_error);
    }
    TEST_F(AudioIo, Utf8PathsAndEmptyWav) {
        const auto file = utf8_path(directory / std::filesystem::path(u8"d\u017awi\u0119k-\u97f3.wav"));
        const alia::audio_clip clip(0, 48000, 0.0f);
        alia::save_audio_clip(clip.view(), file);
        auto reader = alia::open_audio_reader(file);
        EXPECT_EQ(reader.frame_count(), 0);
        EXPECT_EQ(reader.read_all().sample_rate(), 48000);
        reader.seek(0);
        EXPECT_THROW(reader.seek(1), std::out_of_range);
    }
    TEST_F(AudioIo, TruncatedWavThrowsDuringDecode) {
        const alia::audio_clip clip(30, 48000, std::int16_t{123});
        const auto file = path("truncated.wav");
        alia::save_audio_clip(clip.view(), file);
        const auto native = directory / "truncated.wav";
        std::filesystem::resize_file(native, std::filesystem::file_size(native) - 8);
        EXPECT_THROW(alia::load_audio_clip(file), std::runtime_error);
    }

    class memory_decoder final : public alia::audio_decoder {
    public:
        explicit memory_decoder(bool known = false) : known_(known) {}
        alia::audio_stream_format format() const override {
            return {alia::sample_frame_format_of_v<std::int16_t>, 24000};
        }
        std::optional<std::size_t> frame_count() const override {
            return known_ ? std::optional<std::size_t>(samples_.size()) : std::nullopt;
        }
        std::size_t read(std::span<std::byte> bytes) override {
            const auto n = (std::min)(bytes.size() / sizeof(std::int16_t), samples_.size() - position_);
            std::memcpy(bytes.data(), samples_.data() + position_, n * sizeof(std::int16_t));
            position_ += n;
            return n;
        }
        void seek(std::size_t frame) override {
            if (frame > samples_.size())
                throw std::out_of_range("memory decoder seek");
            position_ = frame;
        }
    private:
        bool known_;
        std::size_t position_ = 0;
        std::array<std::int16_t, 5> samples_{-32768, -16384, 0, 16384, 32767};
    };
    TEST_F(AudioIo, CustomRegistrationAndUnknownLengthReads) {
        alia::audio_reader_registrar registration("CuStOm_AuDiO", [](std::string_view) {
            return std::make_unique<memory_decoder>();
        });
        auto reader = alia::open_audio_reader("unused.custom_AUDIO");
        EXPECT_FALSE(reader.frame_count());
        std::array<std::int16_t, 2> first{};
        EXPECT_EQ(reader.read(std::span(first)), 2);
        const auto rest = reader.read_all();
        ASSERT_EQ(rest.frame_count(), 3);
        EXPECT_EQ(rest.view_as<std::int16_t>()[1], 16384);
        EXPECT_EQ(rest.sample_rate(), 24000);
        reader.seek(5);
        EXPECT_THROW(reader.seek(6), std::out_of_range);
        EXPECT_EQ(reader.position(), 5);
        const auto whole = alia::load_audio_clip("unused.custom_audio");
        EXPECT_EQ(whole.frame_count(), 5);
        alia::register_audio_reader("custom_audio", [](std::string_view) { return std::make_unique<memory_decoder>(true); });
        EXPECT_EQ(alia::open_audio_reader("unused.CUSTOM_AUDIO").frame_count(), 5);
    }
    TEST_F(AudioIo, CustomWriterRegistrationUsesCaseInsensitiveExtensions) {
        auto calls = std::make_shared<unsigned>(0);
        alia::audio_writer_registrar registration("Custom_Out", [calls](alia::const_audio_clip_view view, std::string_view file) {
            EXPECT_EQ(view.sample_rate(), 48000);
            EXPECT_EQ(view.frame_count(), 2);
            EXPECT_EQ(file, "unused.CUSTOM_OUT");
            ++*calls;
        });
        const alia::audio_clip clip(2, 48000, 0.0f);
        alia::save_audio_clip(clip.view(), "unused.CUSTOM_OUT");
        EXPECT_EQ(*calls, 1);
    }
    TEST(AudioReader, RejectsNullAndInvalidDecoders) {
        EXPECT_THROW(alia::audio_reader(std::unique_ptr<alia::audio_decoder>{}), std::invalid_argument);
        alia::audio_reader empty;
        EXPECT_FALSE(empty);
        EXPECT_THROW(empty.format(), std::logic_error);
        EXPECT_THROW(empty.read_all(), std::logic_error);
        EXPECT_THROW(empty.seek(0), std::logic_error);
        struct invalid_decoder : alia::audio_decoder {
            alia::audio_stream_format format() const override { return {{alia::sample_point_formats::f32, 0}, 48000}; }
            std::optional<std::size_t> frame_count() const override { return 0; }
            std::size_t read(std::span<std::byte>) override { return 0; }
            void seek(std::size_t) override {}
        };
        EXPECT_THROW(alia::audio_reader(std::make_unique<invalid_decoder>()), std::invalid_argument);
    }
} // namespace
