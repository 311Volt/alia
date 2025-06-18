#ifndef DISPLAY_A9ADB9C3_14F7_4737_8F1B_85CF4C0433B4
#define DISPLAY_A9ADB9C3_14F7_4737_8F1B_85CF4C0433B4

#include "../util/vec.hpp"
#include "../util/rect.hpp"
#include <variant>

#include <span>
#include <string>

namespace alia {

	namespace detail {
		struct window_pos_centered_t {};
		struct window_pos_auto_t {};
	} // namespace detail
	constexpr inline detail::window_pos_centered_t window_pos_centered;
	constexpr inline detail::window_pos_auto_t window_pos_auto;

	using window_pos = std::variant<detail::window_pos_auto_t, detail::window_pos_centered_t, vec2i>;

	enum class window_mode { windowed, windowed_fullscreen, fullscreen };
	enum class window_show_mode { shown, automatic, hidden };
	enum class window_graphics_api { undefined, opengl, vulkan, metal };
	enum class window_status { minimized, maximized };
	enum class display_orientation {
		unknown,
		portrait,
		portrait_flipped,
		landscape,
		landscape_flipped
	};

	enum class pixel_format {
		rgba8888,
		rgb888,
		argb8888,
		bgra8888,
		abgr8888,
		rgb565,
		gray8,
		gray16,
		indexed8,
		rgb332,
		bgr565,
		rgba4444,
		argb4444,
		bgra4444,
		abgr4444,
		rgba5551,
		argb1555,
		bgra5551,
		abgr1555
	};

	struct display_mode {
		pixel_format pixel_format;
		vec2i size;
		double refresh_rate;
		void* backend_data;
	};

	struct window_options {
		window_mode mode = window_mode::windowed;
		window_show_mode show_mode = window_show_mode::shown;
		window_graphics_api graphics_api = window_graphics_api::undefined;

		bool borderless = false;
		bool resizable = false;
		bool minimized = false;
		bool maximized = false;
		bool mouse_grabbed = false;
		bool input_focus = false;
		bool mouse_focus = false;
		bool always_on_top = false;
		bool skip_taskbar = false;
		bool utility = false;
		bool tooltip = false;
		bool popup_menu = false;
		bool keyboard_grabbed = false;

	};

	namespace detail {

		class display_backend_interface {
		public:
			virtual ~display_backend_interface() = 0;
			
			virtual int get_closest_display_mode(int displayIndex, void* mode, void* closestMode) = 0;
			virtual int get_current_display_mode(int displayIndex, void* mode) = 0;

			virtual int get_num_display_modes(int displayIndex) = 0;
			virtual int get_num_video_displays() = 0;
			virtual int get_num_video_drivers() = 0;

			virtual int get_closest_display_for_point(vec2i point) = 0;
			virtual int get_closest_display_for_rect(rect_i rect) = 0;

			virtual const char* get_current_video_driver() = 0;
			virtual int get_desktop_display_mode(int displayIndex,void* mode) = 0;
			virtual int get_display_bounds(int displayIndex,void* rect) = 0;
			virtual int get_display_dpi(int displayIndex, float* ddpi, float* hdpi, float* vdpi) = 0;
			virtual int get_display_mode(int displayIndex, int modeIndex, void* mode) = 0;
			virtual display_orientation get_display_orientation(int displayIndex) = 0;
			virtual int get_display_usable_bounds(int displayIndex, void* rect) = 0;
		};

		class window_backend_interface {
		public:
			virtual ~window_backend_interface() = 0;

			virtual void init_window(vec2i size, window_pos pos, window_options options) = 0;
			virtual void init_window_from_native_handle(void* native_handle) = 0;

			virtual bool disable_screen_saver() = 0;
			virtual bool enable_screen_saver() = 0;

			virtual void set_window_title(std::string const &title) = 0;
			virtual std::string get_window_title() = 0;

			virtual void set_window_icon() = 0; // icon needs alia image type

			virtual void set_window_position(vec2i position) = 0;
			virtual vec2i get_window_position() = 0;

			virtual void set_window_size(vec2i size) = 0;
			virtual vec2i get_window_size() = 0;

			virtual void set_window_minimum_size(vec2i size) = 0;
			virtual vec2i get_window_minimum_size() = 0;

			virtual void set_window_maximum_size(vec2i size) = 0;
			virtual vec2i get_window_maximum_size() = 0;

			virtual void set_window_bordered(bool bordered) = 0;

			virtual void set_window_resizable(bool resizable) = 0;
			
			virtual void set_window_always_on_top(bool on_top) = 0;

			virtual void set_window_brightness(float brightness) = 0;
			virtual float get_window_brightness() = 0;
			
			virtual int set_window_userdata(const char* name, void* userdata) = 0;
			virtual void* get_window_userdata(const char* name) = 0;

			virtual int set_window_display_mode(void* displayMode) = 0;
			virtual int get_window_display_mode(void* mode) = 0;

			virtual int set_window_gamma_ramp(const unsigned short *red, const unsigned short *green, const unsigned short *blue) = 0;
			virtual int get_window_gamma_ramp(unsigned short *red, unsigned short *green, unsigned short *blue) = 0;
			
			virtual void set_window_grab(bool grabbed) = 0;
			virtual bool get_window_grab() = 0;
			
			virtual int set_window_hit_test(void* callback, void* data) = 0;
			
			virtual void set_window_keyboard_grab(bool grabbed) = 0;
			virtual bool get_window_keyboard_grab() = 0;
			
			virtual void set_window_mouse_grab(bool grabbed) = 0;
			virtual bool get_window_mouse_grab() = 0;
			
			virtual int set_window_mouse_rect(const void* rect) = 0;
			virtual void* get_window_mouse_rect() = 0;

			virtual void set_window_opacity(float opacity) = 0;
			virtual float get_window_opacity() = 0;

			virtual void set_window_modal_for(void* parent_window_handle) = 0;
			virtual void set_window_input_focus() = 0;

			virtual void* get_grabbed_window() = 0;

			
			virtual const char* get_video_driver(int index) = 0;
			virtual int get_window_borders_size(int* top, int* left, int* bottom, int* right) = 0;
			virtual int get_window_display_index() = 0;
			virtual void* get_window_from_id(unsigned int id) = 0;
			virtual void* get_window_icc_profile(size_t* size) = 0;
			virtual unsigned int get_window_pixel_format() = 0;
			virtual void get_window_size_in_pixels(int* w, int* h) = 0;
			virtual void* get_window_surface() = 0;


			virtual void show_window() = 0;
			virtual void hide_window() = 0;
			virtual void raise_window() = 0;
			virtual void maximize_window() = 0;
			virtual void minimize_window() = 0;
			virtual void restore_window() = 0;

			virtual void set_window_fullscreen_mode(window_mode mode) = 0;

			virtual unsigned get_window_flags() = 0;

			virtual void destroy_window() = 0;
			virtual void destroy_window_surface() = 0;

			virtual void flash_window(unsigned int operation) = 0;
			virtual bool has_window_surface() = 0;
			virtual bool is_screen_saver_enabled() = 0;

			virtual int update_window_surface() = 0;
			virtual int update_window_surface_rects(void* rects, int numrects) = 0;

			virtual int video_init(const char* driver_name) = 0;
			virtual void video_quit() = 0;

			// virtual void* gl_create_context() = 0;
			// virtual int gl_delete_context(void* context) = 0;
			// virtual bool gl_extension_supported(const char* extension) = 0;
			// virtual int gl_get_attribute(void* attr, int* value) = 0;
			// virtual void* gl_get_current_context() = 0;
			// virtual void* gl_get_current_window() = 0;
			// virtual void gl_get_drawable_size(int* w, int* h) = 0;
			// virtual void* gl_get_proc_address(const char* proc) = 0;
			// virtual int gl_get_swap_interval() = 0;
			// virtual int gl_load_library(const char* path) = 0;
			// virtual int gl_make_current(void* context) = 0;
			// virtual void gl_reset_attributes() = 0;
			// virtual int gl_set_attribute(void* attr, int value) = 0;
			// virtual int gl_set_swap_interval(int interval) = 0;
			// virtual void gl_swap_window() = 0;
			// virtual void gl_unload_library() = 0;
		};
	} // namespace detail

	

	template<typename TBackend>
	class window {
	public:
		window(vec2i size) {}


	};
} // namespace alia

#endif /* DISPLAY_A9ADB9C3_14F7_4737_8F1B_85CF4C0433B4 */
