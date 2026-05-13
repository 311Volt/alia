#ifndef UTF8_A37983C1_240F_4D73_9156_E0F21C00C154
#define UTF8_A37983C1_240F_4D73_9156_E0F21C00C154

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace alia {

inline constexpr uint32_t utf8_replacement_codepoint = 0xfffd;

[[nodiscard]] inline bool is_utf8_continuation_byte(unsigned char c) noexcept {
    return (c & 0xc0) == 0x80;
}

[[nodiscard]] inline std::optional<uint32_t>
utf8_read_next_codepoint(std::string_view text, std::size_t& offset) noexcept {
    if (offset >= text.size())
        return std::nullopt;

    const auto byte = [](char c) { return static_cast<unsigned char>(c); };
    const unsigned char c0 = byte(text[offset++]);
    if (c0 < 0x80)
        return c0;

    auto fail = []() noexcept -> std::optional<uint32_t> {
        return std::nullopt;
    };

    if ((c0 & 0xe0) == 0xc0) {
        if (offset >= text.size() || !is_utf8_continuation_byte(byte(text[offset])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const uint32_t cp = ((c0 & 0x1f) << 6) | (c1 & 0x3f);
        if (cp < 0x80)
            return std::nullopt;
        return cp;
    }

    if ((c0 & 0xf0) == 0xe0) {
        if (offset + 1 >= text.size()
            || !is_utf8_continuation_byte(byte(text[offset]))
            || !is_utf8_continuation_byte(byte(text[offset + 1])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const unsigned char c2 = byte(text[offset++]);
        const uint32_t cp = ((c0 & 0x0f) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f);
        if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff))
            return std::nullopt;
        return cp;
    }

    if ((c0 & 0xf8) == 0xf0) {
        if (offset + 2 >= text.size()
            || !is_utf8_continuation_byte(byte(text[offset]))
            || !is_utf8_continuation_byte(byte(text[offset + 1]))
            || !is_utf8_continuation_byte(byte(text[offset + 2])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const unsigned char c2 = byte(text[offset++]);
        const unsigned char c3 = byte(text[offset++]);
        const uint32_t cp =
            ((c0 & 0x07) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
        if (cp < 0x10000 || cp > 0x10ffff)
            return std::nullopt;
        return cp;
    }

    return std::nullopt;
}

} // namespace alia

#endif /* UTF8_A37983C1_240F_4D73_9156_E0F21C00C154 */
