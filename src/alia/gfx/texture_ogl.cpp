#ifdef ALIA_COMPILE_GFX_BACKEND_OPENGL

#include "detail/ogl_backend.hpp"

// <GL/glext.h> supplies GL_BGR, GL_BGRA, GL_R8, GL_R32F, etc. (OGL 1.2+ / ARB).
// It must be included before any other GL header on some platforms, so we include
// it here explicitly rather than in the shared ogl_backend.hpp.
#include <GL/glext.h>

// wglGetProcAddress / glXGetProcAddressARB are needed to load GL 3.0+ entry
// points at runtime, since opengl32.lib / libGL.so only export GL 1.1 directly.
#ifdef _WIN32
#  include <GL/wgl.h>
#else
#  include <GL/glx.h>
#endif

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace alia {

// ── Pixel format table ────────────────────────────────────────────────

struct ogl_pixel_fmt {
    GLenum internal_format;
    GLenum external_format;
    GLenum type;
};

static ogl_pixel_fmt to_ogl_format(pixel_format fmt) noexcept {
    switch (fmt) {
        case pixel_format::rgba8888: return {GL_RGBA8,   GL_RGBA, GL_UNSIGNED_BYTE};
        case pixel_format::rgb888:   return {GL_RGB8,    GL_RGB,  GL_UNSIGNED_BYTE};
        case pixel_format::bgra8888: return {GL_RGBA8,   GL_BGRA, GL_UNSIGNED_BYTE};
        case pixel_format::bgr888:   return {GL_RGB8,    GL_BGR,  GL_UNSIGNED_BYTE};
        case pixel_format::rgb565:   return {GL_RGB,     GL_RGB,  GL_UNSIGNED_SHORT_5_6_5};
        case pixel_format::gray_u8:  return {GL_R8,      GL_RED,  GL_UNSIGNED_BYTE};
        case pixel_format::gray_f32: return {GL_R32F,    GL_RED,  GL_FLOAT};
        case pixel_format::rgba_f32: return {GL_RGBA32F, GL_RGBA, GL_FLOAT};
        case pixel_format::rgb_f32:  return {GL_RGB32F,  GL_RGB,  GL_FLOAT};
        default:                     return {0, 0, 0};
    }
}

// ── ogl_texture_impl ──────────────────────────────────────────────────

ogl_texture_impl::~ogl_texture_impl() {
    if (tex_id) glDeleteTextures(1, &tex_id);
}

void ogl_texture_impl::apply_sampler() noexcept {
    auto min_filt = [&]() -> GLint {
        if (mip_levels_ <= 1) {
            return (sampler_.min_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
        }
        bool mn = (sampler_.min_filter == texture_filter::nearest);
        bool mp = (sampler_.mip_filter == texture_filter::nearest);
        if ( mn &&  mp) return GL_NEAREST_MIPMAP_NEAREST;
        if ( mn && !mp) return GL_NEAREST_MIPMAP_LINEAR;
        if (!mn &&  mp) return GL_LINEAR_MIPMAP_NEAREST;
        return GL_LINEAR_MIPMAP_LINEAR;
    };
    auto mag_filt = [&]() -> GLint {
        return (sampler_.mag_filter == texture_filter::nearest) ? GL_NEAREST : GL_LINEAR;
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_mode(sampler_.wrap_u));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_mode(sampler_.wrap_v));
}

void ogl_texture_impl::set_sampler(const sampler_state& s) {
    sampler_ = s;
    glBindTexture(GL_TEXTURE_2D, tex_id);
    apply_sampler();
}

bool ogl_texture_impl::lock(rect_i region, int level, texture_lock_info& out) {
    if (level < 0 || level >= mip_levels_) return false;

    const int bpp = bytes_per_pixel_for_format(fmt_);
    if (bpp == 0) return false;

    const int lw = std::max(1, width_  >> level);
    const int lh = std::max(1, height_ >> level);

    // Clamp region to this level's bounds.
    const rect_i bounds{{0, 0}, {lw, lh}};
    const rect_i r = bounds.intersection_with(region);
    if (r.width() <= 0 || r.height() <= 0) return false;

    // Ensure staging buffer is large enough for the full level.
    const std::size_t needed = static_cast<std::size_t>(lw * bpp) * lh;
    if (needed > stage_buf_bytes_) {
        stage_buf_       = std::make_unique<std::byte[]>(needed);
        stage_buf_bytes_ = needed;
    }

    // Download entire level from GPU into the staging buffer.
    const auto [internal, external, type] = to_ogl_format(fmt_);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glGetTexImage(GL_TEXTURE_2D, level, external, type, stage_buf_.get());

    const int stride = lw * bpp;
    out.data         = stage_buf_.get() + r.top() * stride + r.left() * bpp;
    out.stride_bytes = stride;
    out.origin       = r.p1;
    out.extent       = r.size();
    out.level        = level;
    return true;
}

void ogl_texture_impl::unlock(const texture_lock_info& info, bool wrote) {
    if (!wrote || !stage_buf_) return;

    const int bpp = bytes_per_pixel_for_format(fmt_);
    const int lw  = std::max(1, width_ >> info.level);
    const auto [internal, external, type] = to_ogl_format(fmt_);

    glBindTexture(GL_TEXTURE_2D, tex_id);
    // GL_UNPACK_ROW_LENGTH tells GL the row stride of the source buffer in pixels.
    glPixelStorei(GL_UNPACK_ROW_LENGTH, lw);
    glTexSubImage2D(GL_TEXTURE_2D, info.level,
                    info.origin.x, info.origin.y,
                    info.extent.x, info.extent.y,
                    external, type, info.data);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void ogl_texture_impl::generate_mipmaps() {
    if (mip_levels_ <= 1) return;
    glBindTexture(GL_TEXTURE_2D, tex_id);
#ifdef _WIN32
    static auto fn = reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(
        wglGetProcAddress("glGenerateMipmap"));
#else
    static auto fn = reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glGenerateMipmap")));
#endif
    fn(GL_TEXTURE_2D);
}

std::unique_ptr<texture_impl> ogl_texture_impl::clone() const {
    const int bpp = bytes_per_pixel_for_format(fmt_);
    const auto [internal, external, type] = to_ogl_format(fmt_);

    auto t = std::make_unique<ogl_texture_impl>();
    t->fmt_        = fmt_;
    t->width_      = width_;
    t->height_     = height_;
    t->mip_levels_ = mip_levels_;
    t->sampler_    = sampler_;

    glGenTextures(1, &t->tex_id);
    glBindTexture(GL_TEXTURE_2D, t->tex_id);

    int lw = width_, lh = height_;
    for (int lv = 0; lv < mip_levels_; ++lv) {
        // Download this level from source texture.
        const std::size_t bytes = static_cast<std::size_t>(lw * bpp) * lh;
        auto buf = std::make_unique<std::byte[]>(bytes);
        glBindTexture(GL_TEXTURE_2D, tex_id);
        glGetTexImage(GL_TEXTURE_2D, lv, external, type, buf.get());

        // Upload to the new texture.
        glBindTexture(GL_TEXTURE_2D, t->tex_id);
        glTexImage2D(GL_TEXTURE_2D, lv, static_cast<GLint>(internal),
                     lw, lh, 0, external, type, buf.get());

        lw = std::max(1, lw >> 1);
        lh = std::max(1, lh >> 1);
    }

    t->apply_sampler();
    return t;
}

// ── ogl_device_impl::create_texture ──────────────────────────────────

std::unique_ptr<texture_impl>
ogl_device_impl::create_texture(pixel_format fmt, vec2i size, int mip_levels) {
    const int bpp = bytes_per_pixel_for_format(fmt);
    if (bpp == 0) return nullptr;

    const auto [internal, external, type] = to_ogl_format(fmt);
    if (internal == 0) return nullptr;

    // Compute actual mip level count.
    int actual_mips = mip_levels;
    if (actual_mips == 0) {
        actual_mips = 1;
        int w = size.x, h = size.y;
        while (w > 1 || h > 1) { w >>= 1; h >>= 1; ++actual_mips; }
    }

    auto t = std::make_unique<ogl_texture_impl>();
    t->fmt_        = fmt;
    t->width_      = size.x;
    t->height_     = size.y;
    t->mip_levels_ = actual_mips;

    glGenTextures(1, &t->tex_id);
    glBindTexture(GL_TEXTURE_2D, t->tex_id);

    int lw = size.x, lh = size.y;
    for (int lv = 0; lv < actual_mips; ++lv) {
        glTexImage2D(GL_TEXTURE_2D, lv, static_cast<GLint>(internal),
                     lw, lh, 0, external, type, nullptr);
        lw = std::max(1, lw >> 1);
        lh = std::max(1, lh >> 1);
    }

    t->apply_sampler();
    return t;
}

} // namespace alia

#endif // ALIA_COMPILE_GFX_BACKEND_OPENGL
