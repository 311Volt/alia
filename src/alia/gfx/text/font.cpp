#include "font.hpp"

#include "alia/gfx/pixel_types.hpp"
#include "alia/gfx/primitives.hpp"
#include "alia/gfx/texture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace alia {

namespace {

[[nodiscard]] float from_26_6(FT_Pos value) noexcept {
    return static_cast<float>(value) / 64.0f;
}

[[nodiscard]] int ceil_26_6(FT_Pos value) noexcept {
    return static_cast<int>(std::ceil(from_26_6(value)));
}

[[nodiscard]] bool is_continuation(unsigned char c) noexcept {
    return (c & 0xc0) == 0x80;
}

[[nodiscard]] uint32_t next_codepoint(std::string_view text, std::size_t& offset) noexcept {
    constexpr uint32_t replacement = 0xfffd;
    if (offset >= text.size())
        return 0;

    const auto byte = [](char c) { return static_cast<unsigned char>(c); };
    const unsigned char c0 = byte(text[offset++]);
    if (c0 < 0x80)
        return c0;

    auto fail = [&]() noexcept {
        return replacement;
    };

    if ((c0 & 0xe0) == 0xc0) {
        if (offset >= text.size() || !is_continuation(byte(text[offset])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const uint32_t cp = ((c0 & 0x1f) << 6) | (c1 & 0x3f);
        return cp >= 0x80 ? cp : replacement;
    }

    if ((c0 & 0xf0) == 0xe0) {
        if (offset + 1 >= text.size()
            || !is_continuation(byte(text[offset]))
            || !is_continuation(byte(text[offset + 1])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const unsigned char c2 = byte(text[offset++]);
        const uint32_t cp = ((c0 & 0x0f) << 12) | ((c1 & 0x3f) << 6) | (c2 & 0x3f);
        if (cp < 0x800 || (cp >= 0xd800 && cp <= 0xdfff))
            return replacement;
        return cp;
    }

    if ((c0 & 0xf8) == 0xf0) {
        if (offset + 2 >= text.size()
            || !is_continuation(byte(text[offset]))
            || !is_continuation(byte(text[offset + 1]))
            || !is_continuation(byte(text[offset + 2])))
            return fail();
        const unsigned char c1 = byte(text[offset++]);
        const unsigned char c2 = byte(text[offset++]);
        const unsigned char c3 = byte(text[offset++]);
        const uint32_t cp =
            ((c0 & 0x07) << 18) | ((c1 & 0x3f) << 12) | ((c2 & 0x3f) << 6) | (c3 & 0x3f);
        return cp >= 0x10000 && cp <= 0x10ffff ? cp : replacement;
    }

    return replacement;
}

void copy_bitmap_coverage(const FT_Bitmap& bitmap, std::vector<unsigned char>& out) {
    const int width = static_cast<int>(bitmap.width);
    const int height = static_cast<int>(bitmap.rows);
    out.assign(static_cast<std::size_t>(width) * height, 0);
    if (width <= 0 || height <= 0 || !bitmap.buffer)
        return;

    const int pitch = bitmap.pitch;
    const int abs_pitch = pitch < 0 ? -pitch : pitch;

    for (int y = 0; y < height; ++y) {
        const unsigned char* row =
            pitch >= 0
                ? bitmap.buffer + y * abs_pitch
                : bitmap.buffer + (height - 1 - y) * abs_pitch;

        if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
            for (int x = 0; x < width; ++x) {
                unsigned value = row[x];
                if (bitmap.num_grays > 1 && bitmap.num_grays != 256)
                    value = value * 255u / static_cast<unsigned>(bitmap.num_grays - 1);
                out[static_cast<std::size_t>(y) * width + x] = static_cast<unsigned char>(value);
            }
        } else if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
            for (int x = 0; x < width; ++x) {
                const unsigned char mask = static_cast<unsigned char>(0x80u >> (x & 7));
                out[static_cast<std::size_t>(y) * width + x] = (row[x >> 3] & mask) ? 255 : 0;
            }
        } else {
            throw std::runtime_error("font: unsupported FreeType bitmap pixel mode");
        }
    }
}

} // namespace

namespace detail {

struct ttf_font_impl {
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    font_metrics metrics;

    ~ttf_font_impl() {
        if (face)
            FT_Done_Face(face);
        if (library)
            FT_Done_FreeType(library);
    }
};

struct cached_glyph {
    glyph_metrics metrics;
    int page = -1;
    rect_i atlas_rect;
};

struct glyph_page {
    texture atlas;
    int cursor_x = 1;
    int cursor_y = 1;
    int row_height = 0;
};

struct hardware_glyph_buffer_impl {
    font* source = nullptr;
    vec2i page_size = {1024, 1024};
    std::unordered_map<uint32_t, cached_glyph> glyphs;
    std::vector<glyph_page> pages;

    hardware_glyph_buffer_impl(font& source, vec2i page_size)
        : source(&source), page_size(page_size)
    {
        if (page_size.x <= 2 || page_size.y <= 2)
            throw std::invalid_argument("hardware_glyph_buffer: page size is too small");
    }

    void clear() {
        glyphs.clear();
        pages.clear();
    }

    glyph_page& add_page() {
        bitmap blank(page_size, px_bgra8888{255, 255, 255, 0});
        texture atlas(current_device(), blank);
        if (!atlas)
            throw std::runtime_error("hardware_glyph_buffer: failed to create atlas texture");

        atlas.set_sampler({
            .min_filter = texture_filter::linear,
            .mag_filter = texture_filter::linear,
            .mip_filter = texture_filter::linear,
            .wrap_u = texture_wrap::clamp,
            .wrap_v = texture_wrap::clamp,
        });

        pages.push_back(glyph_page{std::move(atlas)});
        return pages.back();
    }

    glyph_page& page_with_space(vec2i size) {
        constexpr int padding = 1;
        if (size.x + padding * 2 > page_size.x || size.y + padding * 2 > page_size.y)
            throw std::runtime_error("hardware_glyph_buffer: glyph is larger than the atlas page");

        if (pages.empty())
            return add_page();

        glyph_page* page = &pages.back();
        if (page->cursor_x + size.x + padding > page_size.x) {
            page->cursor_x = padding;
            page->cursor_y += page->row_height + padding;
            page->row_height = 0;
        }

        if (page->cursor_y + size.y + padding > page_size.y)
            return add_page();

        return *page;
    }

    cached_glyph& get(uint32_t codepoint) {
        if (auto it = glyphs.find(codepoint); it != glyphs.end())
            return it->second;

        rendered_glyph rendered = source->render_glyph(codepoint);
        cached_glyph glyph;
        glyph.metrics = rendered.metrics;

        if (rendered.has_bitmap()) {
            constexpr int padding = 1;
            glyph_page& page = page_with_space(rendered.metrics.bitmap_size);
            glyph.page = static_cast<int>(pages.size()) - 1;
            glyph.atlas_rect = rect_i::pos_size(
                {page.cursor_x, page.cursor_y},
                rendered.metrics.bitmap_size);

            if (auto region = page.atlas.lock<px_bgra8888>(glyph.atlas_rect)) {
                auto view = region.view();
                for (int y = 0; y < rendered.metrics.bitmap_size.y; ++y) {
                    for (int x = 0; x < rendered.metrics.bitmap_size.x; ++x) {
                        const auto alpha =
                            rendered.coverage[static_cast<std::size_t>(y) * rendered.metrics.bitmap_size.x + x];
                        view[x, y] = px_bgra8888{255, 255, 255, alpha};
                    }
                }
            } else {
                throw std::runtime_error("hardware_glyph_buffer: failed to lock atlas texture");
            }

            page.cursor_x += rendered.metrics.bitmap_size.x + padding;
            page.row_height = (std::max)(page.row_height, rendered.metrics.bitmap_size.y);
        }

        auto [it, inserted] = glyphs.emplace(codepoint, std::move(glyph));
        return it->second;
    }
};

} // namespace detail

static detail::ttf_font_impl& require_impl(const std::unique_ptr<detail::ttf_font_impl>& impl) {
    if (!impl)
        throw std::runtime_error("ttf_font: no font loaded");
    return *impl;
}

ttf_font::ttf_font(std::unique_ptr<detail::ttf_font_impl> impl) noexcept
    : impl_(std::move(impl))
{}

ttf_font::~ttf_font() = default;
ttf_font::ttf_font(ttf_font&&) noexcept = default;
ttf_font& ttf_font::operator=(ttf_font&&) noexcept = default;

font_metrics ttf_font::metrics() const {
    return require_impl(impl_).metrics;
}

glyph_metrics ttf_font::get_glyph_metrics(uint32_t codepoint) {
    auto& impl = require_impl(impl_);
    if (FT_Load_Char(impl.face, codepoint, FT_LOAD_DEFAULT))
        throw std::runtime_error("ttf_font: failed to load glyph metrics");

    const FT_GlyphSlot glyph = impl.face->glyph;
    return {
        .bitmap_size = {
            ceil_26_6(glyph->metrics.width),
            ceil_26_6(glyph->metrics.height),
        },
        .bearing = {
            static_cast<int>(std::floor(from_26_6(glyph->metrics.horiBearingX))),
            static_cast<int>(std::ceil(from_26_6(glyph->metrics.horiBearingY))),
        },
        .advance = from_26_6(glyph->advance.x),
    };
}

rendered_glyph ttf_font::render_glyph(uint32_t codepoint) {
    auto& impl = require_impl(impl_);
    if (FT_Load_Char(impl.face, codepoint, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
        throw std::runtime_error("ttf_font: failed to render glyph");

    const FT_GlyphSlot glyph = impl.face->glyph;
    rendered_glyph result;
    result.metrics = {
        .bitmap_size = {
            static_cast<int>(glyph->bitmap.width),
            static_cast<int>(glyph->bitmap.rows),
        },
        .bearing = {glyph->bitmap_left, glyph->bitmap_top},
        .advance = from_26_6(glyph->advance.x),
    };
    copy_bitmap_coverage(glyph->bitmap, result.coverage);
    return result;
}

float ttf_font::kerning(uint32_t left, uint32_t right) const {
    auto& impl = require_impl(impl_);
    if (!FT_HAS_KERNING(impl.face) || left == 0 || right == 0)
        return 0.0f;

    const FT_UInt left_index = FT_Get_Char_Index(impl.face, left);
    const FT_UInt right_index = FT_Get_Char_Index(impl.face, right);
    if (left_index == 0 || right_index == 0)
        return 0.0f;

    FT_Vector delta{};
    if (FT_Get_Kerning(impl.face, left_index, right_index, FT_KERNING_DEFAULT, &delta))
        return 0.0f;
    return from_26_6(delta.x);
}

hardware_glyph_buffer::hardware_glyph_buffer(font& source, vec2i page_size)
    : impl_(std::make_unique<detail::hardware_glyph_buffer_impl>(source, page_size))
{}

hardware_glyph_buffer::~hardware_glyph_buffer() = default;
hardware_glyph_buffer::hardware_glyph_buffer(hardware_glyph_buffer&&) noexcept = default;
hardware_glyph_buffer& hardware_glyph_buffer::operator=(hardware_glyph_buffer&&) noexcept = default;

font& hardware_glyph_buffer::source_font() const noexcept {
    return *impl_->source;
}

void hardware_glyph_buffer::clear() {
    impl_->clear();
}

ttf_font load_ttf_font(std::string_view filename, int pixel_height) {
    if (pixel_height <= 0)
        throw std::invalid_argument("load_ttf_font: pixel height must be positive");

    auto impl = std::make_unique<detail::ttf_font_impl>();
    if (FT_Init_FreeType(&impl->library))
        throw std::runtime_error("load_ttf_font: failed to initialize FreeType");

    const std::string path(filename);
    if (FT_New_Face(impl->library, path.c_str(), 0, &impl->face))
        throw std::runtime_error("load_ttf_font: failed to load '" + path + "'");

    if (FT_Set_Pixel_Sizes(impl->face, 0, static_cast<FT_UInt>(pixel_height)))
        throw std::runtime_error("load_ttf_font: failed to set pixel size for '" + path + "'");

    const FT_Size_Metrics& ftm = impl->face->size->metrics;
    impl->metrics = {
        .ascender = from_26_6(ftm.ascender),
        .descender = from_26_6(ftm.descender),
        .line_height = from_26_6(ftm.height),
    };

    return ttf_font(std::move(impl));
}

vec2f measure_text(font& source, std::string_view text) {
    if (text.empty())
        return {};

    const font_metrics metrics = source.metrics();
    float pen_x = 0.0f;
    float max_x = 0.0f;
    int line_count = 1;
    uint32_t previous = 0;

    std::size_t offset = 0;
    while (offset < text.size()) {
        const uint32_t cp = next_codepoint(text, offset);
        if (cp == '\r')
            continue;
        if (cp == '\n') {
            max_x = (std::max)(max_x, pen_x);
            pen_x = 0.0f;
            previous = 0;
            ++line_count;
            continue;
        }

        if (cp == '\t') {
            const glyph_metrics space = source.get_glyph_metrics(' ');
            pen_x += space.advance * 4.0f;
            previous = 0;
            continue;
        }

        pen_x += source.kerning(previous, cp);
        pen_x += source.get_glyph_metrics(cp).advance;
        max_x = (std::max)(max_x, pen_x);
        previous = cp;
    }

    return {max_x, metrics.line_height * static_cast<float>(line_count)};
}

void draw_text(
    font& source,
    std::string_view text,
    color text_color,
    vec2f position,
    hardware_glyph_buffer* glyph_buffer)
{
    if (text.empty())
        return;

    hardware_glyph_buffer local_buffer(source);
    hardware_glyph_buffer& buffer = glyph_buffer ? *glyph_buffer : local_buffer;
    if (&buffer.source_font() != &source)
        throw std::invalid_argument("draw_text: glyph buffer belongs to a different font");

    const font_metrics metrics = source.metrics();
    float pen_x = 0.0f;
    float baseline = position.y + metrics.ascender;
    uint32_t previous = 0;

    std::size_t offset = 0;
    while (offset < text.size()) {
        const uint32_t cp = next_codepoint(text, offset);
        if (cp == '\r')
            continue;
        if (cp == '\n') {
            pen_x = 0.0f;
            baseline += metrics.line_height;
            previous = 0;
            continue;
        }
        if (cp == '\t') {
            const glyph_metrics space = source.get_glyph_metrics(' ');
            pen_x += space.advance * 4.0f;
            previous = 0;
            continue;
        }

        pen_x += source.kerning(previous, cp);

        detail::cached_glyph& glyph = buffer.impl_->get(cp);
        if (glyph.page >= 0) {
            detail::glyph_page& page = buffer.impl_->pages[static_cast<std::size_t>(glyph.page)];
            const rect_i r = glyph.atlas_rect;
            const float x0 = position.x + pen_x + static_cast<float>(glyph.metrics.bearing.x);
            const float y0 = baseline - static_cast<float>(glyph.metrics.bearing.y);
            const float x1 = x0 + static_cast<float>(glyph.metrics.bitmap_size.x);
            const float y1 = y0 + static_cast<float>(glyph.metrics.bitmap_size.y);
            const float u0 = static_cast<float>(r.left()) / static_cast<float>(buffer.impl_->page_size.x);
            const float v0 = static_cast<float>(r.top()) / static_cast<float>(buffer.impl_->page_size.y);
            const float u1 = static_cast<float>(r.right()) / static_cast<float>(buffer.impl_->page_size.x);
            const float v1 = static_cast<float>(r.bottom()) / static_cast<float>(buffer.impl_->page_size.y);

            full_vertex v0tx{{x0, y0}, text_color, {u0, v0}};
            full_vertex v1tx{{x1, y0}, text_color, {u1, v0}};
            full_vertex v2tx{{x0, y1}, text_color, {u0, v1}};
            full_vertex v3tx{{x1, y1}, text_color, {u1, v1}};
            full_vertex vertices[6] = {v0tx, v1tx, v2tx, v1tx, v3tx, v2tx};
            draw_textured_triangles<full_vertex>(page.atlas, vertices);
        }

        pen_x += glyph.metrics.advance;
        previous = cp;
    }
}

} // namespace alia
