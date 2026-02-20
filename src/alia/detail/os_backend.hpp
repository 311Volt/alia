#ifndef ALIA_DETAIL_OS_BACKEND_HPP
#define ALIA_DETAIL_OS_BACKEND_HPP

#include "../os/window.hpp"
#include "../os/monitor.hpp"
#include "../os/display.hpp"
#include "../os/dialog.hpp"
#include "../os/clipboard.hpp"
#include "../io/keyboard.hpp"
#include "../io/mouse.hpp"
#include "../io/joystick.hpp"
#include "../io/touch.hpp"
#include <span>
#include <vector>
#include <string>

namespace alia {
    struct window_options;
}

namespace alia::detail {

// window backend interface
class i_window_backend {
public:
    virtual ~i_window_backend() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // window operations
    virtual void* create_window(int width, int height, const char* title, 
                                 uint32_t flags, const window_options* options) = 0;
    virtual void destroy_window(void* handle) = 0;
    
    virtual void set_window_title(void* handle, const char* title) = 0;
    virtual void set_window_position(void* handle, int x, int y) = 0;
    virtual void set_window_size(void* handle, int width, int height) = 0;
    virtual void get_window_size(void* handle, int* width, int* height) = 0;
    virtual void get_window_position(void* handle, int* x, int* y) = 0;
    
    virtual void show_window(void* handle) = 0;
    virtual void hide_window(void* handle) = 0;
    virtual void minimize_window(void* handle) = 0;
    virtual void maximize_window(void* handle) = 0;
    virtual void restore_window(void* handle) = 0;
    
    virtual void set_fullscreen(void* handle, bool fullscreen) = 0;
    virtual bool is_fullscreen(void* handle) = 0;
    
    virtual void swap_buffers(void* handle) = 0;
    virtual void set_vsync(void* handle, bool enable) = 0;
    
    // cursor
    virtual void show_cursor(void* handle, bool show) = 0;
    virtual void set_cursor(void* handle, int cursor_type) = 0;
    virtual void set_cursor_position(void* handle, int x, int y) = 0;
    virtual void grab_mouse(void* handle, bool grab) = 0;
    
    // Monitor
    virtual int get_monitor_count() = 0;
    virtual void get_monitor_info(int index, monitor_info* info) = 0;
    virtual void get_display_modes(int monitor, std::vector<display_mode>* modes) = 0;
    
    // Events
    virtual void* get_event_source(void* handle) = 0;
    
    // Backend info
    virtual const char* get_name() const = 0;
};

// Input backend interface
class i_input_backend {
public:
    virtual ~i_input_backend() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    
    // Keyboard
    virtual void get_keyboard_state(bool* keys, int count, uint32_t* modifiers) = 0;
    virtual void* get_keyboard_event_source() = 0;
    
    // Mouse
    virtual void get_mouse_state(int* x, int* y, bool* buttons, int count) = 0;
    virtual void set_mouse_position(int x, int y) = 0;
    virtual void* get_mouse_event_source() = 0;
    
    // Joystick
    virtual int get_joystick_count() = 0;
    virtual void get_joystick_info(int index, joystick_info* info) = 0;
    virtual void get_joystick_state(int index, float* axes, bool* buttons, 
                                     uint8_t* hats) = 0;
    virtual void reconfigure_joysticks() = 0;
    virtual void* get_joystick_event_source() = 0;
    
    // Touch
    virtual bool has_touch_input() = 0;
    virtual int get_max_touches() = 0;
    virtual void get_touch_state(touch_point* points, int* count, int max_count) = 0;
    virtual void* get_touch_event_source() = 0;
};

// Dialog backend interface
class i_dialog_backend {
public:
    virtual ~i_dialog_backend() = default;
    
    virtual bool show_file_dialog(bool open, bool multiple, const char* title,
                                   const file_dialog_options* options,
                                   std::vector<std::string>* results) = 0;
    
    virtual bool show_folder_dialog(const char* title, const char* initial,
                                     std::string* result) = 0;
    
    virtual int show_message_box(const char* title, const char* message,
                                  int buttons, int icon, void* parent) = 0;
    
    virtual void* create_text_log(const char* title, bool monospace) = 0;
    virtual void destroy_text_log(void* handle) = 0;
    virtual void text_log_append(void* handle, const char* text) = 0;
    virtual void text_log_show(void* handle, bool show) = 0;
};

// Clipboard backend interface
class i_clipboard_backend {
public:
    virtual ~i_clipboard_backend() = default;
    
    virtual bool has_text() = 0;
    virtual bool get_text(std::string* result) = 0;
    virtual bool set_text(const char* text, size_t length) = 0;
    virtual bool has_image() = 0;
    virtual void clear() = 0;
};

// Backend registration
struct os_backend_info {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    i_window_backend* (*create_window_backend)();
    i_input_backend* (*create_input_backend)();
    i_dialog_backend* (*create_dialog_backend)();
    i_clipboard_backend* (*create_clipboard_backend)();
    void (*destroy)(void*);
};

void register_os_backend(const os_backend_info& info);
std::span<const os_backend_info> get_os_backends();
const os_backend_info* get_current_os_backend();

} // namespace alia::detail

#endif // ALIA_DETAIL_OS_BACKEND_HPP
