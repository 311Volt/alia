#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "ogl_ops.hpp"

// GL_BGR, GL_BGRA, GL_R8, GL_R32F, framebuffer object tokens, etc.
#include <GL/glext.h>

#include <algorithm>
#include <cstring>

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#endif

namespace alia {

    PFNGLGENERATEMIPMAPPROC ogl_s_glGenerateMipmap = nullptr;
    PFNGLGENFRAMEBUFFERSPROC ogl_s_glGenFramebuffers = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC ogl_s_glDeleteFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC ogl_s_glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC ogl_s_glFramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC ogl_s_glCheckFramebufferStatus = nullptr;

    namespace {

        struct ogl_pixel_fmt {
            GLenum internal_format;
            GLenum external_format;
            GLenum type;
        };

        ogl_pixel_fmt to_ogl_format(pixel_format fmt, texture_role role) noexcept {
            switch (fmt) {
            case pixel_format::rgba8888: return {GL_RGBA8,   GL_RGBA,            GL_UNSIGNED_BYTE};
            case pixel_format::rgb888:   return {GL_RGB8,    GL_RGB,             GL_UNSIGNED_BYTE};
            case pixel_format::bgra8888: return {GL_RGBA8,   GL_BGRA,            GL_UNSIGNED_BYTE};
            case pixel_format::bgr888:   return {GL_RGB8,    GL_BGR,             GL_UNSIGNED_BYTE};
            case pixel_format::rgb565:   return {GL_RGB,     GL_RGB,             GL_UNSIGNED_SHORT_5_6_5};
            case pixel_format::gray_u8:  return role == texture_role::alpha_mask
                                                    ? ogl_pixel_fmt{GL_ALPHA8, GL_ALPHA, GL_UNSIGNED_BYTE}
                                                    : ogl_pixel_fmt{GL_R8, GL_RED, GL_UNSIGNED_BYTE};
            case pixel_format::gray_f32: return {GL_R32F,    GL_RED,             GL_FLOAT};
            case pixel_format::rgba_f32: return {GL_RGBA32F, GL_RGBA,            GL_FLOAT};
            case pixel_format::rgb_f32:  return {GL_RGB32F,  GL_RGB,             GL_FLOAT};
            default:                     return {0, 0, 0};
            }
        }

        vec2i level_size(const ogl_texture &t, int level) noexcept {
            return {
                std::max(1, t.width >> level),
                std::max(1, t.height >> level)
            };
        }

        bool framebuffer_ops_loaded() noexcept {
            return ogl_s_glGenFramebuffers &&
                   ogl_s_glDeleteFramebuffers &&
                   ogl_s_glBindFramebuffer &&
                   ogl_s_glFramebufferTexture2D &&
                   ogl_s_glCheckFramebufferStatus;
        }

        bool attach_texture_to_framebuffer(const ogl_texture &texture, int face, int level) {
            if (!framebuffer_ops_loaded())
                return false;
            ogl_s_glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                ogl_face_target(texture, face),
                texture.tex_id,
                level
            );
            return ogl_s_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        }

        bool texture_level_is_renderable(const ogl_texture &texture, int face, int level) {
            if (!framebuffer_ops_loaded())
                return false;

            GLint previous = 0;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);

            GLuint fbo = 0;
            ogl_s_glGenFramebuffers(1, &fbo);
            if (!fbo)
                return false;

            ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            const bool complete = attach_texture_to_framebuffer(texture, face, level);
            ogl_s_glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous));
            ogl_s_glDeleteFramebuffers(1, &fbo);
            return complete;
        }

        void apply_sampler(const ogl_texture &texture, const sampler_state &s) noexcept {
            glBindTexture(texture.target, texture.tex_id);

            auto min_filt = [&]() -> GLint {
                if (texture.mip_levels <= 1)
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

            glTexParameteri(texture.target, GL_TEXTURE_MIN_FILTER, min_filt());
            glTexParameteri(texture.target, GL_TEXTURE_MAG_FILTER, mag_filt());
            glTexParameteri(texture.target, GL_TEXTURE_WRAP_S, wrap_mode(s.wrap_u));
            glTexParameteri(texture.target, GL_TEXTURE_WRAP_T, wrap_mode(s.wrap_v));
            if (texture.target == GL_TEXTURE_CUBE_MAP)
                glTexParameteri(texture.target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }

        bool lock_level(
            ogl_texture &texture,
            int face,
            rect_i region,
            int level,
            texture_lock_mode mode,
            texture_lock_info &out
        ) {
            if (face < 0 || face >= (texture.target == GL_TEXTURE_CUBE_MAP ? cube_face_count : 1) ||
                level < 0 || level >= texture.mip_levels)
                return false;

            const int bpp = bytes_per_pixel_for_format(texture.fmt);
            if (bpp == 0)
                return false;

            const vec2i ls = level_size(texture, level);
            const rect_i bounds{{0, 0}, ls};
            const rect_i r = bounds.intersection_with(region);
            if (r.width() <= 0 || r.height() <= 0)
                return false;

            const std::size_t needed = static_cast<std::size_t>(ls.x * bpp) * ls.y;
            if (needed > texture.stage_buf_bytes) {
                texture.stage_buf = std::make_unique<std::byte[]>(needed);
                texture.stage_buf_bytes = needed;
            }

            if (mode != texture_lock_mode::write_only) {
                const ogl_pixel_fmt format = to_ogl_format(texture.fmt, texture.role);
                glBindTexture(texture.target, texture.tex_id);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(
                    ogl_face_target(texture, face), level,
                    format.external_format, format.type,
                    texture.stage_buf.get());
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
            }

            const int stride = ls.x * bpp;
            out.data = texture.stage_buf.get() + r.top() * stride + r.left() * bpp;
            out.stride_bytes = stride;
            out.origin = r.p1;
            out.extent = r.size();
            out.level = level;
            out.face = face;
            return true;
        }

    } // namespace

    texture_handle *ogl_create_texture(
        device_handle *,
        pixel_format fmt,
        vec2i size,
        int mip_levels,
        texture_role role,
        texture_usage usage
    ) {
        const int bpp = bytes_per_pixel_for_format(fmt);
        if (bpp == 0)
            return nullptr;

        const auto [internal, external, type] = to_ogl_format(fmt, role);
        if (internal == 0)
            return nullptr;

        int actual_mips = mip_levels;
        if (actual_mips == 0) {
            actual_mips = 1;
            int w = size.x, h = size.y;
            while (w > 1 || h > 1) {
                w = std::max(1, w >> 1);
                h = std::max(1, h >> 1);
                ++actual_mips;
            }
        }

        auto *t = new ogl_texture;
        t->fmt = fmt;
        t->width = size.x;
        t->height = size.y;
        t->mip_levels = actual_mips;
        t->role = role;
        t->usage = usage;

        glGenTextures(1, &t->tex_id);
        glBindTexture(t->target, t->tex_id);

        int lw = size.x, lh = size.y;
        for (int lv = 0; lv < actual_mips; ++lv) {
            glTexImage2D(GL_TEXTURE_2D, lv, static_cast<GLint>(internal), lw, lh, 0, external, type, nullptr);
            lw = std::max(1, lw >> 1);
            lh = std::max(1, lh >> 1);
        }

        apply_sampler(*t, t->sampler);

        if (usage == texture_usage::render_target && !texture_level_is_renderable(*t, 0, 0)) {
            glDeleteTextures(1, &t->tex_id);
            delete t;
            return nullptr;
        }

        return t;
    }

    texture_handle *ogl_create_cube_texture(
        device_handle *,
        pixel_format fmt,
        int edge,
        int mip_levels,
        texture_usage usage
    ) {
        if (bytes_per_pixel_for_format(fmt) == 0)
            return nullptr;

        const auto [internal, external, type] = to_ogl_format(fmt, texture_role::color);
        if (internal == 0)
            return nullptr;

        int actual_mips = mip_levels;
        if (actual_mips == 0) {
            actual_mips = 1;
            for (int size = edge; size > 1; size = std::max(1, size >> 1))
                ++actual_mips;
        }

        auto *texture = new ogl_texture;
        texture->target = GL_TEXTURE_CUBE_MAP;
        texture->fmt = fmt;
        texture->width = edge;
        texture->height = edge;
        texture->mip_levels = actual_mips;
        texture->role = texture_role::color;
        texture->usage = usage;

        glGenTextures(1, &texture->tex_id);
        glBindTexture(texture->target, texture->tex_id);
        for (int face = 0; face < cube_face_count; ++face) {
            int size = edge;
            for (int level = 0; level < actual_mips; ++level) {
                glTexImage2D(
                    ogl_face_target(*texture, face), level,
                    static_cast<GLint>(internal), size, size, 0,
                    external, type, nullptr);
                size = std::max(1, size >> 1);
            }
        }

        apply_sampler(*texture, texture->sampler);
        if (usage == texture_usage::render_target &&
            !texture_level_is_renderable(*texture, 0, 0)) {
            glDeleteTextures(1, &texture->tex_id);
            delete texture;
            return nullptr;
        }
        return texture;
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
        apply_sampler(*t, s);
    }

    bool ogl_texture_lock(texture_handle *h, rect_i region, int level, texture_lock_mode mode, texture_lock_info &out) {
        return lock_level(*as_ogl_texture(h), 0, region, level, mode, out);
    }

    bool ogl_cube_texture_lock(
        texture_handle *h,
        cube_face face,
        rect_i region,
        int level,
        texture_lock_mode mode,
        texture_lock_info &out
    ) {
        auto *texture = as_ogl_texture(h);
        if (texture->target != GL_TEXTURE_CUBE_MAP)
            return false;
        return lock_level(
            *texture, static_cast<int>(face), region, level, mode, out);
    }

    void ogl_texture_unlock(texture_handle *h, const texture_lock_info &info, bool wrote) {
        auto *t = as_ogl_texture(h);
        if (!wrote || !t->stage_buf)
            return;

        const vec2i ls = level_size(*t, info.level);
        const ogl_pixel_fmt format = to_ogl_format(t->fmt, t->role);

        glBindTexture(t->target, t->tex_id);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, ls.x);
        glTexSubImage2D(
            ogl_face_target(*t, info.face), info.level,
            info.origin.x, info.origin.y, info.extent.x, info.extent.y,
            format.external_format, format.type, info.data
        );
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    }

    void ogl_texture_generate_mipmaps(texture_handle *h) {
        auto *t = as_ogl_texture(h);
        if (t->mip_levels <= 1 || !ogl_s_glGenerateMipmap)
            return;
        glBindTexture(t->target, t->tex_id);
        ogl_s_glGenerateMipmap(t->target);
    }

    texture_handle *ogl_texture_clone(const texture_handle *h) {
        const auto *src = as_ogl_texture(h);
        const int bpp = bytes_per_pixel_for_format(src->fmt);
        const auto [internal, external, type] = to_ogl_format(src->fmt, src->role);

        auto *dst = new ogl_texture;
        dst->fmt = src->fmt;
        dst->target = src->target;
        dst->width = src->width;
        dst->height = src->height;
        dst->mip_levels = src->mip_levels;
        dst->role = src->role;
        dst->usage = src->usage;
        dst->sampler = src->sampler;

        glGenTextures(1, &dst->tex_id);
        glBindTexture(dst->target, dst->tex_id);

        const int face_count = src->target == GL_TEXTURE_CUBE_MAP ? cube_face_count : 1;
        for (int face = 0; face < face_count; ++face) {
            int lw = src->width, lh = src->height;
            for (int lv = 0; lv < src->mip_levels; ++lv) {
                const std::size_t bytes = static_cast<std::size_t>(lw * bpp) * lh;
                auto buf = std::make_unique<std::byte[]>(bytes);
                glBindTexture(src->target, src->tex_id);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(ogl_face_target(*src, face), lv, external, type, buf.get());
                glPixelStorei(GL_PACK_ALIGNMENT, 4);

                glBindTexture(dst->target, dst->tex_id);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(
                    ogl_face_target(*dst, face), lv, static_cast<GLint>(internal),
                    lw, lh, 0, external, type, buf.get());
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

                lw = std::max(1, lw >> 1);
                lh = std::max(1, lh >> 1);
            }
        }

        apply_sampler(*dst, dst->sampler);
        if (dst->usage == texture_usage::render_target &&
            !texture_level_is_renderable(*dst, 0, 0)) {
            glDeleteTextures(1, &dst->tex_id);
            delete dst;
            return nullptr;
        }
        return dst;
    }

    bool ogl_copy_render_target_to_texture(
        device_handle *,
        texture_handle *dst_h,
        rect_i src_rect,
        vec2i src_target_size,
        vec2i dst_pos,
        int dst_level,
        int dst_face
    ) {
        auto *dst = as_ogl_texture(dst_h);
        while (glGetError() != GL_NO_ERROR) {}

        const GLint src_y = src_target_size.y - src_rect.bottom();
        glBindTexture(dst->target, dst->tex_id);
        glCopyTexSubImage2D(
            ogl_face_target(*dst, dst_face),
            dst_level,
            dst_pos.x,
            dst_pos.y,
            src_rect.left(),
            src_y,
            src_rect.width(),
            src_rect.height()
        );
        return glGetError() == GL_NO_ERROR;
    }

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
