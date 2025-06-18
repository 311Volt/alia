#ifndef GFX_BASIC_C0F38D90_5333_452B_876F_54D9A56A7B50
#define GFX_BASIC_C0F38D90_5333_452B_876F_54D9A56A7B50

#include "../util/rect.hpp"
#include "../util/vec.hpp"

#include <span>


namespace alia {

	struct color {
		float r,g,b,a;

		static constexpr color of(uint8_t r, uint8_t g, uint8_t b) {
			return {r/255.f, g/255.f, b/255.f, 1.0};
		}

		static constexpr color of(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
			return {r/255.f, g/255.f, b/255.f, 1.0};
		}

		static constexpr color of_rgb24(uint32_t color) {
			uint8_t r = (color >> 16) & 0xFF;
			uint8_t g = (color >> 8) & 0xFF;
			uint8_t b = (color >> 0) & 0xFF;
			return of(r, g, b);
		}

		static constexpr color of_rgba32(uint32_t color) {
			uint8_t r = (color >> 24) & 0xFF;
			uint8_t g = (color >> 16) & 0xFF;
			uint8_t b = (color >> 8) & 0xFF;
			uint8_t a = (color >> 0) & 0xFF;
			return of(r, g, b, a);
		}

		static constexpr color cga(int index) {
			switch(index) {
				case 0: return of_rgb24(0x000000);
				case 1: return of_rgb24(0x0000AA);
				case 2: return of_rgb24(0x00AA00);
				case 3: return of_rgb24(0x00AAAA);
				case 4: return of_rgb24(0xAA0000);
				case 5: return of_rgb24(0xAA00AA);
				case 6: return of_rgb24(0xAA5500);
				case 7: return of_rgb24(0xAAAAAA);
				case 8: return of_rgb24(0x555555);
				case 9: return of_rgb24(0x5555FF);
				case 10: return of_rgb24(0x55FF55);
				case 11: return of_rgb24(0x55FFFF);
				case 12: return of_rgb24(0xFF5555);
				case 13: return of_rgb24(0xFF55FF);
				case 14: return of_rgb24(0xFFFF55);
				case 15: return of_rgb24(0xFFFFFF);
				default: return of_rgb24(0x000000);
			};
		}
	};

	static constexpr color black = color::cga(0);
	static constexpr color blue = color::cga(1);
	static constexpr color green = color::cga(2);
	static constexpr color cyan = color::cga(3);
	static constexpr color red = color::cga(4);
	static constexpr color magenta = color::cga(5);
	static constexpr color brown = color::cga(6);
	static constexpr color light_gray = color::cga(7);
	static constexpr color dark_gray = color::cga(8);
	static constexpr color light_blue = color::cga(9);
	static constexpr color light_green = color::cga(10);
	static constexpr color light_cyan = color::cga(11);
	static constexpr color light_red = color::cga(12);
	static constexpr color light_magenta = color::cga(13);
	static constexpr color yellow = color::cga(14);
	static constexpr color white = color::cga(15);

	static constexpr color pure_blue = color::of_rgb24(0x0000FF);
	static constexpr color pure_green = color::of_rgb24(0x00FF00);
	static constexpr color pure_red = color::of_rgb24(0xFF0000);
	static constexpr color pure_cyan = color::of_rgb24(0x00FFFF);
	static constexpr color pure_magenta = color::of_rgb24(0xFF00FF);
	static constexpr color pure_yellow = color::of_rgb24(0xFFFF00);


	class basic_graphics_context {
	public:

		bool clear() {

		}

		bool draw_point(vec2i point, color color) {

		}

		bool draw_points(const std::span<const vec2i> points, color color) {

		}

		bool draw_line(vec2i a, vec2i b, color color) {

		}

		bool draw_lines(const std::span<const vec2i> points, color color) {

		}

		bool draw_rect(rect_i rect) {

		}

		bool draw_rects(const std::span<const rect_i> rects) {

		}

		bool fill_rect(rect_i rect) {

		}

		bool fill_rects(const std::span<const rect_i> rects) {

		}

		bool copy_texture(void *texture, rect_i src_rect, rect_i dst_rect) {

		}

		bool copy_texture_ex(void *texture, rect_i src_rect, rect_i dst_rect, double angle, vec2i center, int flip)  {
			
		}

		bool set_draw_color(int r, int g, int b, int a) {

		}

		bool get_draw_color(int &r, int &g, int &b, int &a) {

		}

		bool set_render_target(void *texture) {

		}

		void *get_render_target() { 
			return nullptr; 
		}

		bool read_pixels(rect_i rect, unsigned int format, void *pixels, int pitch) {

		}
	private:
		
	};
}

#ifdef ALIA_BACKEND_GFX_BASIC_USE_SDL2

namespace alia::backend::sdl2 {

	

}

#else
	#error "no backend for basic graphics specified"
#endif

#endif /* GFX_BASIC_C0F38D90_5333_452B_876F_54D9A56A7B50 */
