#ifndef GFX_BASIC_BA352045_AC18_4A63_8077_7291CD11F6DD
#define GFX_BASIC_BA352045_AC18_4A63_8077_7291CD11F6DD

#include "../util/rect.hpp"
#include "../util/vec.hpp"

#include <algorithm>
#include <cmath>
#include <concepts>
#include <span>
#include <stdexcept>
#include <vector>

#include "../stdx/mdspan.hpp"

#include "color.hpp"
#include "pixel.hpp"

namespace alia {

    class image {
    public:
        image() = default;
        image(pixel_format fmt, vec2i size) : format_(fmt), size_(size), stride_(bytes_per_pixel(fmt) * size.x) { data_.resize(stride_ * size.y); }
        image(pixel_format fmt, vec2i size, int stride, std::vector<std::byte> data) : format_(fmt), size_(size), stride_(stride), data_(std::move(data)) {
            if (stride != bytes_per_pixel(fmt) * size.x) {
                throw std::logic_error("custom strides not supported");
            }
            if (data_.size() < stride_ * size_.y) {
                throw std::logic_error("image data is smaller than specified dimensions");
            }
        }

        pixel_format format() const { return format_; }
        vec2i size() const { return size_; }
        int stride() const { return stride_; }

        auto &data(this auto &self) { return self.data_; }

        template <pixel_format PixelFmt>
        auto view(this auto &self) {
            if (PixelFmt != self.format_) {
                throw std::logic_error("incorrect pixel format for view");
            }
            using pixel_t = pixel<PixelFmt>;
            if constexpr (std::is_const_v<decltype(self)>) {
                return stdx::mdspan<pixel_t const, stdx::dextents<size_t, 2>, stdx::layout_left>(
                    reinterpret_cast<pixel_t const *>(self.data_.data()), self.size_.x, self.size_.y
                );
            } else {
                return stdx::mdspan<pixel_t, stdx::dextents<size_t, 2>, stdx::layout_left>(
                    reinterpret_cast<pixel_t *>(self.data_.data()), self.size_.x, self.size_.y
                );
            }
        }

        template <pixel_format PixelFmt>
        auto view_region(this auto &self, rect_i region) {
            if (PixelFmt != self.format_) {
                throw std::logic_error("incorrect pixel format for view_region");
            }
            auto full_view = self.template view<PixelFmt>();
            return stdx::submdspan(full_view, std::pair{region.a.x, region.b.x}, std::pair{region.a.y, region.b.y});
        }

        template <pixel_format PixelFmt, typename Func>
        void for_each(this auto &self, Func &&fn) {
            if (PixelFmt != self.format_) {
                throw std::logic_error("incorrect pixel format for for_each");
            }
            auto v = self.template view<PixelFmt>();
            for (int y = 0; y < self.size_.y; ++y) {
                for (int x = 0; x < self.size_.x; ++x) {
                    if constexpr (std::is_invocable_v<Func, decltype(v[x, y]), vec2i>) {
                        fn(v[x, y], vec2i(x, y));
                    } else if constexpr (std::is_invocable_v<Func, decltype(v[x, y])>) {
                        fn(v[x, y]);
                    } else {
                        static_assert(false, "invalid for_each function signature");
                    }
                }
            }
        }

        template <pixel_format DstPixelFmt, pixel_format SrcPixelFmt, typename Func>
        image transform(Func &&fn) const {
            if (SrcPixelFmt != format_) {
                throw std::logic_error("incorrect source pixel format for transform");
            }

            using src_pixel_t = pixel<SrcPixelFmt>;
            using dst_pixel_t = pixel<DstPixelFmt>;

            image result(DstPixelFmt, size_);

            auto src_view = view<SrcPixelFmt>();
            auto dst_view = result.view<DstPixelFmt>();

            for (int y = 0; y < size_.y; ++y) {
                for (int x = 0; x < size_.x; ++x) {
                    if constexpr (std::is_invocable_r_v<dst_pixel_t, Func, src_pixel_t const &, vec2i>) {
                        dst_view[x, y] = fn(src_view[x, y], vec2i(x, y));
                    } else if constexpr (std::is_invocable_r_v<dst_pixel_t, Func, src_pixel_t const &>) {
                        dst_view[x, y] = fn(src_view[x, y]);
                    } else {
                        static_assert(false, "invalid transform function signature");
                    }
                }
            }
            return result;
        }

    private:
        pixel_format format_;
        vec2i size_;
        int stride_ = 0;
        std::vector<std::byte> data_;
    };

    template <pixel_format DstFmt, pixel_format SrcFmt>
    image convert_image_pixel_format(image const &src) {
        if (src.format() != SrcFmt) {
            throw std::logic_error("incorrect source pixel format for conversion");
        }

        return src.template transform<DstFmt, SrcFmt>([](pixel<SrcFmt> const &p) { return convert_pixel_high_quality<pixel<DstFmt>>(p); });
    }

    enum class image_filter { NEAREST, LINEAR };

    template <pixel_format PixelFmt>
    image resize_image(image const &src, vec2i new_size, image_filter filter = image_filter::LINEAR) {
        if (src.format() != PixelFmt) {
            throw std::logic_error("incorrect pixel format for resize");
        }

        image dst(PixelFmt, new_size);
        auto src_view = src.view<PixelFmt>();
        auto dst_view = dst.view<PixelFmt>();

        vec2f scale(float(src.size().x) / new_size.x, float(src.size().y) / new_size.y);

        for (int x = 0; x < new_size.x; ++x) {
            for (int y = 0; y < new_size.y; ++y) {
                vec2f src_coord((x + 0.5f) * scale.x - 0.5f, (y + 0.5f) * scale.y - 0.5f);

                if (filter == image_filter::NEAREST) {
                    vec2i nearest_coord(static_cast<int>(std::round(src_coord.x)), static_cast<int>(std::round(src_coord.y)));
                    nearest_coord.x = std::clamp(nearest_coord.x, 0, src.size().x - 1);
                    nearest_coord.y = std::clamp(nearest_coord.y, 0, src.size().y - 1);
                    dst_view(x, y) = src_view(nearest_coord.x, nearest_coord.y);
                } else // LINEAR
                {
                    vec2i top_left_coord(static_cast<int>(std::floor(src_coord.x)), static_cast<int>(std::floor(src_coord.y)));
                    vec2f fract(src_coord.x - top_left_coord.x, src_coord.y - top_left_coord.y);

                    pixel_rgba_f32 samples[4];
                    for (int i = 0; i < 4; ++i) {
                        vec2i sample_coord = top_left_coord + vec2i(i % 2, i / 2);
                        sample_coord.x = std::clamp(sample_coord.x, 0, src.size().x - 1);
                        sample_coord.y = std::clamp(sample_coord.y, 0, src.size().y - 1);
                        samples[i] = convert_pixel_high_quality<pixel_rgba_f32>(src_view(sample_coord.x, sample_coord.y));
                    }

                    auto lerp = [](auto a, auto b, float t) { return a * (1.f - t) + b * t; };

                    pixel_rgba_f32 result;
                    result.r = lerp(lerp(samples[0].r, samples[1].r, fract.x), lerp(samples[2].r, samples[3].r, fract.x), fract.y);
                    result.g = lerp(lerp(samples[0].g, samples[1].g, fract.x), lerp(samples[2].g, samples[3].g, fract.x), fract.y);
                    result.b = lerp(lerp(samples[0].b, samples[1].b, fract.x), lerp(samples[2].b, samples[3].b, fract.x), fract.y);
                    result.a = lerp(lerp(samples[0].a, samples[1].a, fract.x), lerp(samples[2].a, samples[3].a, fract.x), fract.y);

                    dst_view(x, y) = convert_pixel_high_quality<pixel<PixelFmt>>(result);
                }
            }
        }
        return dst;
    }

    struct draw_image_params {
        std::optional<rect_i> src_rect; //entire source image by default
        std::optional<vec2i> dst_pos; //(0,0) by default
        std::optional<vec2i> dst_size; //source image size by default
        std::optional<color> tint; //solid white by default
        std::optional<double> rotation; //0.0 by default
    };

    class graphics_basic_backend {
    public:
        virtual ~graphics_basic_backend() = default;

        virtual vec2i get_canvas_size() const = 0;
        virtual pixel_format get_canvas_pixel_format() const = 0;

        virtual void clear_canvas(color color) = 0;
        virtual void flip_canvas() = 0;

        virtual void draw_point(vec2i point, color color) = 0;
        virtual void draw_points(const std::span<const vec2i> points, color color) = 0;

        virtual void draw_line(vec2i a, vec2i b, color color) = 0;
        virtual void draw_lines(const std::span<const vec2i> points, color color) = 0;

        virtual void draw_rect(rect_i rect, color color) = 0;
        virtual void draw_rects(const std::span<const rect_i> rects, color color) = 0;

        virtual void draw_triangle(const std::array<vec2i, 3>& vertices, color color) = 0;
        virtual void draw_triangles(std::span<const std::array<vec2i, 3>> triangles, color color) = 0;

        virtual void fill_rect(rect_i rect, color color) = 0;
        virtual void fill_rects(const std::span<const rect_i> rects, color color) = 0;

        virtual void draw_image(const image& image, vec2i position) = 0;
        virtual void draw_image_advanced(const image& image, const draw_image_params& params) = 0;
    };

    class graphics_context_basic {
    public:
        graphics_context_basic(graphics_basic_backend &backend) : _backend(backend) {}

        vec2i get_canvas_size() const { return _backend.get_canvas_size(); }
        pixel_format get_canvas_pixel_format() const { return _backend.get_canvas_pixel_format(); }

        void clear_canvas(color color = black) { _backend.clear_canvas(color); }
        void flip_canvas() { _backend.flip_canvas(); }

        void draw_point(vec2i point, color color) { _backend.draw_point(point, color); }
        void draw_points(const std::span<const vec2i> points, color color) { _backend.draw_points(points, color); }

        void draw_line(vec2i a, vec2i b, color color) { _backend.draw_line(a, b, color); }
        void draw_lines(const std::span<const vec2i> points, color color) { _backend.draw_lines(points, color); }

        void draw_rect(rect_i rect, color color) { _backend.draw_rect(rect, color); }
        void draw_rects(const std::span<const rect_i> rects, color color) { _backend.draw_rects(rects, color); }

        void draw_triangle(const std::array<vec2i, 3>& vertices, color color) { _backend.draw_triangle(vertices, color); }
        void draw_triangles(std::span<const std::array<vec2i, 3>> triangles, color color) { _backend.draw_triangles(triangles, color); }

        void fill_rect(rect_i rect, color color) { _backend.fill_rect(rect, color); }
        void fill_rects(const std::span<const rect_i> rects, color color) { _backend.fill_rects(rects, color); }

        void draw_image(const image& image, vec2i position) { _backend.draw_image(image, position); }
        void draw_image_advanced(const image& image, const draw_image_params& params) { _backend.draw_image_advanced(image, params); }

    private:
        graphics_basic_backend &_backend;
    };
} // namespace alia

#ifdef ALIA_BACKEND_GFX_BASIC_USE_SDL2

#include <SDL.h>
#include <cstddef>
#include <type_traits>

namespace alia::backend::sdl2 {

    static_assert(
        sizeof(vec2i) == sizeof(SDL_Point) 
        && offsetof(vec2i, x) == offsetof(SDL_Point, x)
        && offsetof(vec2i, y) == offsetof(SDL_Point, y) 
        && std::is_standard_layout_v<vec2i>,
        "alia::vec2i must be layout-compatible with SDL_Point."
    );

    class graphics_basic_backend_sdl2: public graphics_basic_backend {
    public:
        graphics_basic_backend_sdl2(SDL_Renderer* renderer) : renderer_(renderer) {}

        vec2i get_canvas_size() const override {
            int w, h;
            SDL_GetRendererOutputSize(renderer_, &w, &h);
            return vec2i(w, h);
        }
        pixel_format get_canvas_pixel_format() const override {
            return pixel_format::rgba8888;
        }

        void clear_canvas(color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderClear(renderer_);
        }
        void flip_canvas() override {
            SDL_RenderPresent(renderer_);
        }

        void draw_point(vec2i p, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderDrawPoint(renderer_, p.x, p.y);
        }
        void draw_points(std::span<const vec2i> points, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderDrawPoints(renderer_,
                         reinterpret_cast<const SDL_Point*>(points.data()),
                         int(points.size()));
        }

        void draw_line(vec2i a, vec2i b, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderDrawLine(renderer_, a.x, a.y, b.x, b.y);
        }
        void draw_lines(std::span<const vec2i> points, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderDrawLines(renderer_,
                        reinterpret_cast<const SDL_Point*>(points.data()),
                        int(points.size()));
        }

        void draw_rect(rect_i r, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_Rect sdl_rect = {r.a.x, r.a.y, r.b.x - r.a.x, r.b.y - r.a.y};
            SDL_RenderDrawRect(renderer_, &sdl_rect);
        }
        void draw_rects(std::span<const rect_i> rects, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            std::vector<SDL_Rect> sdl_rects; 
            sdl_rects.reserve(rects.size());
            for (auto const& r : rects) {
                sdl_rects.push_back({r.a.x, r.a.y, r.b.x - r.a.x, r.b.y - r.a.y});
            }
            SDL_RenderDrawRects(renderer_, sdl_rects.data(), int(sdl_rects.size()));
        }

        void draw_triangle(const std::array<vec2i, 3>& vertices, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_RenderDrawLine(renderer_, vertices[0].x, vertices[0].y, vertices[1].x, vertices[1].y);
            SDL_RenderDrawLine(renderer_, vertices[1].x, vertices[1].y, vertices[2].x, vertices[2].y);
            SDL_RenderDrawLine(renderer_, vertices[2].x, vertices[2].y, vertices[0].x, vertices[0].y);
        }
        void draw_triangles(std::span<const std::array<vec2i, 3>> triangles, color c) override {
            for (auto const& vertices : triangles) {
                draw_triangle(vertices, c);
            }
        }

        void fill_rect(rect_i r, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            SDL_Rect sdl_rect = {r.a.x, r.a.y, r.b.x - r.a.x, r.b.y - r.a.y};
            SDL_RenderFillRect(renderer_, &sdl_rect);
        }
        void fill_rects(std::span<const rect_i> rects, color c) override {
            SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, c.a);
            std::vector<SDL_Rect> sdl_rects;
            sdl_rects.reserve(rects.size());
            for (auto const& r : rects) {
                sdl_rects.push_back({r.a.x, r.a.y, r.b.x - r.a.x, r.b.y - r.a.y});
            }
            SDL_RenderFillRects(renderer_, sdl_rects.data(), int(sdl_rects.size()));
        }

        void draw_image(const image& img, vec2i position) override {
            draw_image_advanced(img, { .dst_pos = position });
        }
        void draw_image_advanced(const image& img, const draw_image_params& params) override;

    private:
        SDL_Renderer* renderer_;
    };

    inline void graphics_basic_backend_sdl2::draw_image_advanced(const image& img, const draw_image_params& params) {
        if (!renderer_) return;

        Uint32 sdl_format;
        switch (img.format()) {
            case pixel_format::rgb888:
                sdl_format = SDL_PIXELFORMAT_RGB24;
                break;
            case pixel_format::rgba8888:
                sdl_format = SDL_PIXELFORMAT_RGBA8888;
                break;
            default:
                return; // Unsupported format
        }

        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
            reinterpret_cast<void*>(const_cast<std::byte*>(img.data().data())),
            img.size().x,
            img.size().y,
            int(bytes_per_pixel(img.format())) * 8,
            img.stride(),
            sdl_format);

        if (!surface) {
            return;
        }

        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);

        if (!texture) {
            return;
        }

        SDL_Rect src_sdl_rect;
        if (params.src_rect) {
            src_sdl_rect = {params.src_rect->a.x, params.src_rect->a.y, params.src_rect->b.x - params.src_rect->a.x, params.src_rect->b.y - params.src_rect->a.y};
        }
        SDL_Rect* src_rect_ptr = params.src_rect ? &src_sdl_rect : NULL;

        vec2i dst_pos = params.dst_pos.value_or(vec2i(0,0));
        vec2i dst_size = params.dst_size.value_or(img.size());
        SDL_Rect dst_sdl_rect = {dst_pos.x, dst_pos.y, dst_size.x, dst_size.y};

        if (params.tint) {
            auto const& tint = *params.tint;
            SDL_SetTextureColorMod(texture, 255.0f * tint.r, 255.0f * tint.g, 255.0f * tint.b);
            SDL_SetTextureAlphaMod(texture, 255.f * tint.a);
        }

        double angle_degrees = params.rotation.value_or(0.0) * 180.0 / 3.14159265358979323846;

        SDL_RenderCopyEx(renderer_, texture, src_rect_ptr, &dst_sdl_rect, angle_degrees, NULL, SDL_FLIP_NONE);

        SDL_DestroyTexture(texture);
    }

}

#else
#error "no backend for basic graphics specified"
#endif

#endif /* GFX_BASIC_BA352045_AC18_4A63_8077_7291CD11F6DD */
