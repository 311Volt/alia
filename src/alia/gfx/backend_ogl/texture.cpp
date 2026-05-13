#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

// GL_BGR, GL_BGRA, GL_R8, GL_R32F, etc. (OGL 1.2+ / ARB).
#include <GL/glext.h>

#include <algorithm>
#include <cstring>

namespace alia {

    PFNGLGENERATEMIPMAPPROC ogl_s_glGenerateMipmap = nullptr;

    // ── Pixel format table ────────────────────────────────────────────────

    struct ogl_pixel_fmt {
        GLenum internal_format;
        GLenum external_format;
        GLenum type;
    };

    static ogl_pixel_fmt to_ogl_format(pixel_format fmt) noexcept {
        switch (fmt) {
        case pixel_format::rgba8888: return {GL_RGBA8,   GL_RGBA,            GL_UNSIGNED_BYTE};
        case pixel_format::rgb888:   return {GL_RGB8,    GL_RGB,             GL_UNSIGNED_BYTE};
        case pixel_format::bgra8888: return {GL_RGBA8,   GL_BGRA,            GL_UNSIGNED_BYTE};
        case pixel_format::bgr888:   return {GL_RGB8,    GL_BGR,             GL_UNSIGNED_BYTE};
        case pixel_format::rgb565:   return {GL_RGB,     GL_RGB,             GL_UNSIGNED_SHORT_5_6_5};
        case pixel_format::gray_u8:  return {GL_R8,      GL_RED,             GL_UNSIGNED_BYTE};
        case pixel_format::gray_f32: return {GL_R32F,    GL_RED,             GL_FLOAT};
        case pixel_format::rgba_f32: return {GL_RGBA32F, GL_RGBA,            GL_FLOAT};
        case pixel_format::rgb_f32:  return {GL_RGB32F,  GL_RGB,             GL_FLOAT};
        default:                     return {0, 0, 0};
        }
    }

    static void apply_sampler(GLuint tex_id, const sampler_state &s, int mip_levels) noexcept {
        glBindTexture(GL_TEXTURE_2D, tex_id);

        auto min_filt = [&]() -> GLint {
            if (mip_levels <= 1)
                return (s.min_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
            bool mn = (s.min_filter == texture_filter::nearest);
            bool mp = (s.mip_filter == texture_filter::nearest);
            if (mn && mp)  return GL_NEAREST_MIPMAP_NEAREST;
            if (mn && !mp) return GL_NEAREST_MIPMAP_LINEAR;
            if (!mn && mp) return GL_LINEAR_MIPMAP_NEAREST;
            return GL_LINEAR_MIPMAP_LINEAR;
        };
        auto mag_filt = [&]() -> GLint {
            return (s.mag_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
        };
        auto wrap_mode = [](texture_wrap w) -> GLint {
            switch (w) {
            case texture_wrap::clamp:  return GL_CLAMP_TO_EDGE;
            case texture_wrap::repeat: return GL_REPEAT;
            case texture_wrap::mirror: return GL_MIRRORED_REPEAT;
            }
            return GL_CLAMP_TO_EDGE;
        };

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filt());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filt());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode(s.wrap_u));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode(s.wrap_v));
    }

    // ── Texture ops ───────────────────────────────────────────────────────

    texture_handle *ogl_create_texture(device_handle * /*dev*/, pixel_format fmt, vec2i size, int mip_levels) {
        const int bpp = bytes_per_pixel_for_format(fmt);
        if (bpp == 0)
            return nullptr;

        const auto [internal, external, type] = to_ogl_format(fmt);
        if (internal == 0)
            return nullptr;

        int actual_mips = mip_levels;
        if (actual_mips == 0) {
            actual_mips = 1;
            int w = size.x, h = size.y;
            while (w > 1 || h > 1) {
                w >>= 1;
                h >>= 1;
                ++actual_mips;
            }
        }

        auto *t = new ogl_texture;
        t->fmt = fmt;
        t->width = size.x;
        t->height = size.y;
        t->mip_levels = actual_mips;

        glGenTextures(1, &t->tex_id);
        glBindTexture(GL_TEXTURE_2D, t->tex_id);

        int lw = size.x, lh = size.y;
        for (int lv = 0; lv < actual_mips; ++lv) {
            glTexImage2D(GL_TEXTURE_2D, lv, static_cast<GLint>(internal), lw, lh, 0, external, type, nullptr);
            lw = std::max(1, lw >> 1);
            lh = std::max(1, lh >> 1);
        }

        apply_sampler(t->tex_id, t->sampler, actual_mips);
        return t;
    }

    void ogl_destroy_texture(texture_handle *h) {
        auto *t = as_ogl_texture(h);
        if (t->tex_id)
            glDeleteTextures(1, &t->tex_id);
        delete t;
    }

    pixel_format ogl_texture_format(const texture_handle *h) { return as_ogl_texture(h)->fmt; }
    int ogl_texture_width(const texture_handle *h)          { return as_ogl_texture(h)->width; }
    int ogl_texture_height(const texture_handle *h)         { return as_ogl_texture(h)->height; }
    int ogl_texture_mip_levels(const texture_handle *h)     { return as_ogl_texture(h)->mip_levels; }
    sampler_state ogl_texture_sampler(const texture_handle *h) { return as_ogl_texture(h)->sampler; }

    void ogl_texture_set_sampler(texture_handle *h, const sampler_state &s) {
        auto *t = as_ogl_texture(h);
        t->sampler = s;
        glBindTexture(GL_TEXTURE_2D, t->tex_id);
        apply_sampler(t->tex_id, s, t->mip_levels);
    }

    bool ogl_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_info &out) {
        auto *t = as_ogl_texture(h);
        if (level < 0 || level >= t->mip_levels)
            return false;

        const int bpp = bytes_per_pixel_for_format(t->fmt);
        if (bpp == 0)
            return false;

        const int lw = std::max(1, t->width >> level);
        const int lh = std::max(1, t->height >> level);

        const rect_i bounds{{0, 0}, {lw, lh}};
        const rect_i r = bounds.intersection_with(region);
        if (r.width() <= 0 || r.height() <= 0)
            return false;

        const std::size_t needed = static_cast<std::size_t>(lw * bpp) * lh;
        if (needed > t->stage_buf_bytes) {
            t->stage_buf = std::make_unique<std::byte[]>(needed);
            t->stage_buf_bytes = needed;
        }

        const auto [internal, external, type] = to_ogl_format(t->fmt);
        glBindTexture(GL_TEXTURE_2D, t->tex_id);
        glGetTexImage(GL_TEXTURE_2D, level, external, type, t->stage_buf.get());

        const int stride = lw * bpp;
        out.data = t->stage_buf.get() + r.top() * stride + r.left() * bpp;
        out.stride_bytes = stride;
        out.origin = r.p1;
        out.extent = r.size();
        out.level = level;
        return true;
    }

    void ogl_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote) {
        auto *t = as_ogl_texture(h);
        if (!wrote || !t->stage_buf)
            return;

        const int bpp = bytes_per_pixel_for_format(t->fmt);
        const int lw = std::max(1, t->width >> info.level);
        const auto [internal, external, type] = to_ogl_format(t->fmt);

        glBindTexture(GL_TEXTURE_2D, t->tex_id);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, lw);
        glTexSubImage2D(
            GL_TEXTURE_2D, info.level,
            info.origin.x, info.origin.y, info.extent.x, info.extent.y,
            external, type, info.data
        );
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    }

    void ogl_texture_generate_mipmaps(texture_handle *h) {
        auto *t = as_ogl_texture(h);
        if (t->mip_levels <= 1 || !ogl_s_glGenerateMipmap)
            return;
        glBindTexture(GL_TEXTURE_2D, t->tex_id);
        ogl_s_glGenerateMipmap(GL_TEXTURE_2D);
    }

    texture_handle *ogl_texture_clone(const texture_handle *h) {
        const auto *src = as_ogl_texture(h);
        const int bpp = bytes_per_pixel_for_format(src->fmt);
        const auto [internal, external, type] = to_ogl_format(src->fmt);

        auto *dst = new ogl_texture;
        dst->fmt = src->fmt;
        dst->width = src->width;
        dst->height = src->height;
        dst->mip_levels = src->mip_levels;
        dst->sampler = src->sampler;

        glGenTextures(1, &dst->tex_id);
        glBindTexture(GL_TEXTURE_2D, dst->tex_id);

        int lw = src->width, lh = src->height;
        for (int lv = 0; lv < src->mip_levels; ++lv) {
            const std::size_t bytes = static_cast<std::size_t>(lw * bpp) * lh;
            auto buf = std::make_unique<std::byte[]>(bytes);
            glBindTexture(GL_TEXTURE_2D, src->tex_id);
            glGetTexImage(GL_TEXTURE_2D, lv, external, type, buf.get());

            glBindTexture(GL_TEXTURE_2D, dst->tex_id);
            glTexImage2D(GL_TEXTURE_2D, lv, static_cast<GLint>(internal), lw, lh, 0, external, type, buf.get());

            lw = std::max(1, lw >> 1);
            lh = std::max(1, lh >> 1);
        }

        apply_sampler(dst->tex_id, dst->sampler, dst->mip_levels);
        return dst;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
