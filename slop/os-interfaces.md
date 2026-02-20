# ALIA OS Interfaces

This document defines the C++ concepts, types, and functions for the ALIA operating system interfaces, including windowing, dialogs, and input handling.

---

## Table of Contents

- [ALIA OS Interfaces](#alia-os-interfaces)
  - [Table of Contents](#table-of-contents)
  - [Window System](#window-system)
    - [Window Class](#window-class)
    - [Window Events](#window-events)
    - [Display Modes](#display-modes)
  - [Monitor](#monitor)
    - [Monitor Class](#monitor-class)
  - [Native Dialogs](#native-dialogs)
    - [File Dialogs](#file-dialogs)
    - [Message Boxes](#message-boxes)
    - [Text Log](#text-log)
  - [Clipboard](#clipboard)
  - [Input System](#input-system)
    - [Keyboard](#keyboard)
    - [Mouse](#mouse)
    - [Joystick/Gamepad](#joystickgamepad)
    - [Touch Input](#touch-input)
  - [Event System](#event-system)
    - [Event Types](#event-types)
    - [Event Queue](#event-queue)
    - [Event Dispatcher](#event-dispatcher)
  - [User Events](#user-events)
    - [Custom User Events](#custom-user-events)
    - [Example User Event](#example-user-event)
  - [Backend Interface](#backend-interface)
    - [OS Backend Interface](#os-backend-interface)
  - [Example Usage](#example-usage)
    - [Basic Window and Event Loop](#basic-window-and-event-loop)
    - [Input Handling](#input-handling)
    - [File Dialog Example](#file-dialog-example)
    - [Multi-Window Application](#multi-window-application)

---

## Window System

### Window Class

```cpp
namespace alia {

// window creation flags
enum class window_flags : uint32_t {
    none = 0,
    
    // Style flags
    windowed = 1 << 0,           // default windowed mode
    fullscreen = 1 << 1,         // Exclusive fullscreen
    fullscreen_window = 1 << 2,   // Borderless fullscreen window
    resizable = 1 << 3,          // Allow resizing
    maximized = 1 << 4,          // start maximized
    minimized = 1 << 5,          // start minimized
    no_frame = 1 << 6,            // Borderless window
    
    // Feature flags
    open_gl = 1 << 8,             // Request open_gl context
    Direct3D = 1 << 9,           // Request Direct3D (Windows)
    
    // Input flags
    grab_mouse = 1 << 16,         // Confine mouse to window
    high_dpi = 1 << 17,           // Enable high DPI support
};

constexpr window_flags operator|(window_flags a, window_flags b) {
    return static_cast<window_flags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

// window options for advanced configuration
struct window_options {
    vec2i position = {-1, -1};   // -1 = centered
    int refresh_rate = 0;        // 0 = desktop rate
    int color_depth = 32;
    int depth_buffer_bits = 24;
    int stencil_buffer_bits = 8;
    int multisamples = 0;        // MSAA samples
    bool vsync = true;
    const char* title = "ALIA window";
};

// window class
class window {
public:
    // Construction
    window() = default;
    window(int width, int height, const char* title = "ALIA window", 
           window_flags flags = window_flags::windowed);
    window(int width, int height, window_flags flags);
    window(int width, int height, window_flags flags, 
           const window_options& options);
    window(vec2i size, const char* title = "ALIA window",
           window_flags flags = window_flags::windowed);
    
    // Move only
    window(window&&) noexcept;
    window& operator=(window&&) noexcept;
    ~window();
    
    window(const window&) = delete;
    window& operator=(const window&) = delete;
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] vec2i size() const;
    [[nodiscard]] rect_i rect() const { return rect_i{{0, 0}, size()}; }
    [[nodiscard]] float aspect_ratio() const;
    
    [[nodiscard]] vec2i position() const;
    [[nodiscard]] const char* title() const;
    [[nodiscard]] window_flags flags() const;
    
    // State
    [[nodiscard]] bool is_focused() const;
    [[nodiscard]] bool is_minimized() const;
    [[nodiscard]] bool is_maximized() const;
    [[nodiscard]] bool is_fullscreen() const;
    
    // Modification
    void set_title(const char* title);
    void set_position(vec2i pos);
    void set_size(vec2i size);
    void resize(int width, int height);
    
    void minimize();
    void maximize();
    void restore();
    
    // fullscreen
    void set_fullscreen(bool fullscreen);
    void toggle_fullscreen();
    void set_fullscreen_window(bool enable);  // Borderless fullscreen
    
    // Display
    void flip();  // Present backbuffer
    void set_vsync(bool enable);
    [[nodiscard]] bool get_vsync() const;
    
    // cursor
    void show_cursor(bool show = true);
    void hide_cursor();
    void set_cursor(cursor cursor);  // See cursor enum below
    void set_cursor_position(vec2i pos);
    [[nodiscard]] bool is_cursor_visible() const;
    
    // Grab
    void set_mouse_grab(bool grab);
    [[nodiscard]] bool is_mouse_grabbed() const;
    
    // Event source
    event_source& get_event_source();
    
    // Backend handle (HWND, window, etc.)
    [[nodiscard]] void* native_handle() const;
    
    // Current window (most recently focused or created)
    [[nodiscard]] static window* current();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// cursor types
enum class cursor {
    default,
    arrow,
    i_beam,
    wait,
    crosshair,
    resize_nwse,
    resize_nesw,
    resize_we,
    resize_ns,
    resize_all,
    hand,
    not_allowed,
    hidden,
};

} // namespace alia
```

### Window Events

```cpp
namespace alia {

// window event types (subset of all events, see Event System section)
namespace events {
    constexpr event_type WindowClose = 1;
    constexpr event_type WindowResize = 2;
    constexpr event_type WindowFocus = 3;
    constexpr event_type WindowUnfocus = 4;
    constexpr event_type WindowMove = 5;
    constexpr event_type WindowMinimize = 6;
    constexpr event_type WindowMaximize = 7;
    constexpr event_type WindowRestore = 8;
    constexpr event_type WindowExpose = 9;
}

// window resize event data
struct window_resize_event : Event {
    window* window;
    vec2i old_size;
    vec2i new_size;
};

// window move event data
struct window_move_event : Event {
    window* window;
    vec2i old_position;
    vec2i new_position;
};

// window focus event data
struct window_focus_event : Event {
    window* window;
    bool focused;
};

} // namespace alia
```

### Display Modes

```cpp
namespace alia {

// Display mode (resolution, refresh rate, etc.)
struct display_mode {
    vec2i size;
    int refresh_rate;
    int color_depth;
    
    bool operator==(const display_mode& other) const = default;
};

// Get available display modes
[[nodiscard]] std::vector<display_mode> get_display_modes();
[[nodiscard]] std::vector<display_mode> get_display_modes(int monitor_index);

// Get current desktop mode
[[nodiscard]] display_mode get_desktop_mode();
[[nodiscard]] display_mode get_desktop_mode(int monitor_index);

// Find closest matching display mode
[[nodiscard]] display_mode find_closest_mode(vec2i size, int refresh_rate = 0);

} // namespace alia
```

---

## Monitor

### Monitor Class

```cpp
namespace alia {

// Monitor information
struct monitor_info {
    int index;
    std::string name;
    vec2i position;              // position in virtual screen space
    vec2i size;                  // Physical size in pixels
    vec2i size_mm;               // Physical size in millimeters
    float dpi;                   // Dots per inch
    float scale_factor;          // DPI scale factor (e.g., 1.0, 1.25, 1.5, 2.0)
    bool is_primary;
    display_mode current_mode;
    std::vector<display_mode> modes;
};

// Monitor enumeration
[[nodiscard]] std::vector<monitor_info> get_monitors();
[[nodiscard]] int get_monitor_count();
[[nodiscard]] monitor_info get_primary_monitor();
[[nodiscard]] monitor_info get_monitor(int index);

// Get monitor containing a point
[[nodiscard]] monitor_info get_monitor_at(vec2i point);

// Get monitor for a window
[[nodiscard]] monitor_info get_window_monitor(const window& window);

} // namespace alia
```

---

## Native Dialogs

### File Dialogs

```cpp
namespace alia {

// File dialog filter
struct file_filter {
    std::string description;     // e.g., "Image Files"
    std::string patterns;        // e.g., "*.png;*.jpg;*.bmp"
};

// File dialog options
struct file_dialog_options {
    std::vector<file_filter> filters;
    std::string initial_path;
    std::string default_extension;
    bool allow_multiple = false;  // For open dialogs
    bool show_hidden = false;
};

// Open file dialog
[[nodiscard]] std::optional<std::string> show_open_file_dialog(
    const window* parent = nullptr,
    const char* title = "Open File",
    const file_dialog_options& options = {}
);

// Open multiple files dialog
[[nodiscard]] std::vector<std::string> show_open_files_dialog(
    const window* parent = nullptr,
    const char* title = "Open Files",
    const file_dialog_options& options = {}
);

// Save file dialog
[[nodiscard]] std::optional<std::string> show_save_file_dialog(
    const window* parent = nullptr,
    const char* title = "Save File",
    const file_dialog_options& options = {}
);

// Folder selection dialog
[[nodiscard]] std::optional<std::string> show_folder_dialog(
    const window* parent = nullptr,
    const char* title = "Select Folder",
    const char* initial_path = nullptr
);

} // namespace alia
```

### Message Boxes

```cpp
namespace alia {

// Message box button configurations
enum class message_box_buttons {
    ok,
    ok_cancel,
    yes_no,
    yes_no_cancel,
    retry_cancel,
    abort_retry_ignore,
};

// Message box icons
enum class message_box_icon {
    none,
    info,
    warning,
    error,
    question,
};

// Message box result
enum class message_box_result {
    ok,
    Cancel,
    Yes,
    No,
    retry,
    abort,
    ignore,
};

// Show message box
[[nodiscard]] message_box_result show_message_box(
    const char* title,
    const char* message,
    message_box_buttons buttons = message_box_buttons::ok,
    message_box_icon icon = message_box_icon::none,
    const window* parent = nullptr
);

// Convenience functions
void show_info_box(const char* title, const char* message, 
                   const window* parent = nullptr);

void show_warning_box(const char* title, const char* message,
                      const window* parent = nullptr);

void show_error_box(const char* title, const char* message,
                    const window* parent = nullptr);

[[nodiscard]] bool show_confirm_box(const char* title, const char* message,
                                     const window* parent = nullptr);

} // namespace alia
```

### Text Log

```cpp
namespace alia {

// Text log window (for debug output)
class text_log {
public:
    text_log() = default;
    text_log(const char* title, bool monospace = true);
    
    text_log(text_log&&) noexcept;
    text_log& operator=(text_log&&) noexcept;
    ~text_log();
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Append text
    void append(const char* text);
    void append(std::string_view text);
    
    template<typename... Args>
    void printf(const char* fmt, Args&&... args) {
        append(std::format(fmt, std::forward<Args>(args)...));
    }
    
    // Clear contents
    void clear();
    
    // Show/hide
    void show();
    void hide();
    [[nodiscard]] bool is_visible() const;
    
    // Get event source for close events
    event_source& get_event_source();
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia
```

---

## Clipboard

```cpp
namespace alia {

// Clipboard operations
namespace clipboard {
    // Text
    [[nodiscard]] bool has_text();
    [[nodiscard]] std::string get_text();
    bool set_text(std::string_view text);
    
    // Check if clipboard has image data
    [[nodiscard]] bool has_image();
    
    // Clear clipboard
    void clear();
}

} // namespace alia
```

---

## Input System

### Keyboard

```cpp
namespace alia {

// Key codes (platform-independent)
enum class Key : int {
    Unknown = 0,
    
    // Letters
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    
    // Numbers
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    
    // Function keys
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    
    // Modifiers
    LShift, RShift, LCtrl, RCtrl, LAlt, RAlt, LSuper, RSuper,
    
    // Navigation
    up, down, left, right,
    Home, End, PageUp, PageDown,
    Insert, Delete,
    
    // Editing
    Backspace, Tab, Enter, Escape, Space,
    
    // Punctuation
    Minus, Equals, LeftBracket, RightBracket, Backslash,
    Semicolon, Apostrophe, Grave, Comma, Period, Slash,
    
    // Numpad
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadAdd, NumpadSubtract, NumpadMultiply, NumpadDivide,
    NumpadEnter, NumpadDecimal,
    
    // Lock keys
    caps_lock, num_lock, ScrollLock,
    
    // Other
    PrintScreen, Pause, Menu,
    
    KeyCount  // Number of keys
};

// Key modifiers (bitmask)
enum class key_mod : uint32_t {
    none = 0,
    shift = 1 << 0,
    ctrl = 1 << 1,
    alt = 1 << 2,
    super = 1 << 3,  // Windows/Command key
    caps_lock = 1 << 4,
    num_lock = 1 << 5,
};

constexpr key_mod operator|(key_mod a, key_mod b) {
    return static_cast<key_mod>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr key_mod operator&(key_mod a, key_mod b) {
    return static_cast<key_mod>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Keyboard state
class keyboard_state {
public:
    keyboard_state();
    
    [[nodiscard]] bool is_key_down(Key key) const;
    [[nodiscard]] bool operator[](Key key) const { return is_key_down(key); }
    
    [[nodiscard]] key_mod get_modifiers() const;
    
    // Check specific modifiers
    [[nodiscard]] bool shift() const;
    [[nodiscard]] bool ctrl() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool super() const;
    
private:
    std::array<bool, static_cast<size_t>(Key::KeyCount)> keys_;
    key_mod modifiers_;
};

// Get current keyboard state
[[nodiscard]] keyboard_state get_keyboard_state();

// Check if a specific key is currently down
[[nodiscard]] bool is_key_down(Key key);
[[nodiscard]] bool is_key_down(const keyboard_state& state, Key key);

// Get event source for keyboard events
event_source& get_keyboard_event_source();

// Key name utilities
[[nodiscard]] const char* get_key_name(Key key);
[[nodiscard]] Key get_key_from_name(const char* name);

} // namespace alia
```

### Mouse

```cpp
namespace alia {

// Mouse buttons
enum class MouseButton : int {
    left = 0,
    right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    
    ButtonCount
};

// Convenience aliases
inline constexpr MouseButton lmb = MouseButton::left;
inline constexpr MouseButton rmb = MouseButton::right;
inline constexpr MouseButton mmb = MouseButton::Middle;

// Mouse state
class mouse_state {
public:
    mouse_state();
    
    [[nodiscard]] vec2i position() const;
    [[nodiscard]] vec2f position_f() const;
    
    [[nodiscard]] bool is_button_down(MouseButton button) const;
    [[nodiscard]] bool operator[](MouseButton button) const { return is_button_down(button); }
    
    [[nodiscard]] int wheel_delta() const;
    [[nodiscard]] int wheel_delta_x() const;  // Horizontal scroll
    
private:
    vec2i position_;
    std::array<bool, static_cast<size_t>(MouseButton::ButtonCount)> buttons_;
    int wheel_delta_;
    int wheel_delta_x_;
};

// Get current mouse state
[[nodiscard]] mouse_state get_mouse_state();

// Get mouse position
[[nodiscard]] vec2i get_mouse_position();
[[nodiscard]] vec2f get_mouse_position_f();

// Set mouse position (relative to window or screen)
void set_mouse_position(vec2i pos);
void set_mouse_position(vec2i pos, const window& window);

// Check if a button is down
[[nodiscard]] bool is_mouse_button_down(MouseButton button);

// Get event source for mouse events
event_source& get_mouse_event_source();

} // namespace alia
```

### Joystick/Gamepad

```cpp
namespace alia {

// Joystick information
struct joystick_info {
    int index;
    std::string name;
    int num_axes;
    int num_buttons;
    int num_hats;       // D-pads
    bool is_gamepad;    // Has standard gamepad layout
};

// Hat (D-pad) direction
enum class hat_direction : uint8_t {
    centered = 0,
    up = 1,
    right = 2,
    down = 4,
    left = 8,
    up_right = up | right,
    down_right = down | right,
    down_left = down | left,
    up_left = up | left,
};

// Standard gamepad buttons (for is_gamepad == true)
enum class gamepad_button {
    A, B, X, Y,
    left_bumper, right_bumper,
    back, start, guide,
    left_thumb, right_thumb,
    d_pad_up, d_pad_right, d_pad_down, d_pad_left,
    
    ButtonCount
};

// Standard gamepad axes
enum class gamepad_axis {
    left_x, left_y,
    right_x, right_y,
    left_trigger, right_trigger,
    
    AxisCount
};

// Joystick state
class joystick_state {
public:
    joystick_state();
    joystick_state(int index);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] const joystick_info& info() const;
    
    // Raw access
    [[nodiscard]] float get_axis(int axis) const;          // -1.0 to 1.0
    [[nodiscard]] bool get_button(int button) const;
    [[nodiscard]] hat_direction get_hat(int hat) const;
    
    // Gamepad access (if is_gamepad)
    [[nodiscard]] float get_axis(gamepad_axis axis) const;
    [[nodiscard]] bool get_button(gamepad_button button) const;
    
private:
    int index_;
    joystick_info info_;
    std::vector<float> axes_;
    std::vector<bool> buttons_;
    std::vector<hat_direction> hats_;
};

// Joystick enumeration
[[nodiscard]] int get_joystick_count();
[[nodiscard]] std::vector<joystick_info> get_joysticks();
[[nodiscard]] joystick_info get_joystick_info(int index);

// Get joystick state
[[nodiscard]] joystick_state get_joystick_state(int index);

// Check if joystick is connected
[[nodiscard]] bool is_joystick_connected(int index);

// Reconfigure joysticks (after connect/disconnect)
void reconfigure_joysticks();

// Get event source for joystick events
event_source& get_joystick_event_source();

// Deadzone utility
[[nodiscard]] float apply_deadzone(float value, float deadzone);
[[nodiscard]] vec2f apply_deadzone(vec2f stick, float deadzone);

} // namespace alia
```

### Touch Input

```cpp
namespace alia {

// Touch point
struct touch_point {
    int id;              // Unique identifier for this touch
    vec2f position;
    vec2f pressure;      // 0.0 to 1.0, if available
    bool is_primary;     // First touch in a multi-touch sequence
};

// Touch state
class touch_state {
public:
    touch_state();
    
    [[nodiscard]] int touch_count() const;
    [[nodiscard]] const touch_point& get_touch(int index) const;
    [[nodiscard]] const touch_point* find_touch(int id) const;
    
    // Iterate touches
    [[nodiscard]] auto begin() const { return touches_.begin(); }
    [[nodiscard]] auto end() const { return touches_.end(); }
    
private:
    std::vector<touch_point> touches_;
};

// Get current touch state
[[nodiscard]] touch_state get_touch_state();

// Check touch support
[[nodiscard]] bool has_touch_input();
[[nodiscard]] int get_max_touches();

// Get event source for touch events
event_source& get_touch_event_source();

} // namespace alia
```

---

## Event System

### Event Types

```cpp
namespace alia {

// Event type (32-bit ID)
using event_type = uint32_t;

// Reserved event type ranges
namespace events {
    // System events: 0 - 99
    constexpr event_type SystemBase = 0;
    
    // window events: 1 - 99 (defined earlier)
    
    // Keyboard events: 100 - 199
    constexpr event_type KeyDown = 100;
    constexpr event_type KeyUp = 101;
    constexpr event_type KeyChar = 102;
    
    // Mouse events: 200 - 299
    constexpr event_type MouseMove = 200;
    constexpr event_type MouseButtonDown = 201;
    constexpr event_type MouseButtonUp = 202;
    constexpr event_type MouseWheel = 203;
    constexpr event_type MouseEnterWindow = 204;
    constexpr event_type MouseLeaveWindow = 205;
    
    // Joystick events: 300 - 399
    constexpr event_type JoystickAxis = 300;
    constexpr event_type JoystickButton = 301;
    constexpr event_type JoystickHat = 302;
    constexpr event_type JoystickConnected = 303;
    constexpr event_type JoystickDisconnected = 304;
    
    // Touch events: 400 - 499
    constexpr event_type TouchBegin = 400;
    constexpr event_type TouchEnd = 401;
    constexpr event_type TouchMove = 402;
    constexpr event_type TouchCancel = 403;
    
    // Audio events: 500 - 599 (defined in audio-interfaces.md)
    
    // User events: 1048576+ (0x100000+)
    constexpr event_type UserBase = 1048576;
}

// Base event structure
struct Event {
    event_type type;
    double timestamp;
    void* source;
};

// Keyboard events
struct KeyEvent : Event {
    Key keycode;
    int scancode;
    key_mod modifiers;
    bool repeat;
};

struct char_event : Event {
    char32_t codepoint;
    key_mod modifiers;
};

// Mouse events
struct MouseEvent : Event {
    vec2f position;
    vec2f delta;
    MouseButton button;  // For button events
    int wheel_y;         // For wheel events
    int wheel_x;         // Horizontal scroll
    key_mod modifiers;
};

// Joystick events
struct JoystickAxisEvent : Event {
    int joystick;
    int axis;
    float value;
};

struct JoystickButtonEvent : Event {
    int joystick;
    int button;
    bool pressed;
};

struct JoystickHatEvent : Event {
    int joystick;
    int hat;
    hat_direction direction;
};

struct JoystickConnectionEvent : Event {
    int joystick;
    bool connected;
};

// Touch events
struct TouchEvent : Event {
    int touch_id;
    vec2f position;
    vec2f delta;
    float pressure;
    bool is_primary;
};

} // namespace alia
```

### Event Queue

```cpp
namespace alia {

// Event wrapper (polymorphic event holder)
class any_event {
public:
    any_event() = default;
    any_event(const Event& event);
    
    any_event(any_event&&) noexcept;
    any_event& operator=(any_event&&) noexcept;
    ~any_event();
    
    [[nodiscard]] event_type type() const;
    [[nodiscard]] double timestamp() const;
    
    // Access raw event
    [[nodiscard]] const Event& get() const;
    
    // Cast to specific event type
    template<typename T>
    [[nodiscard]] const T& as() const;
    
    template<typename T>
    [[nodiscard]] const T* try_as() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Event source (generates events)
class event_source {
public:
    event_source();
    event_source(event_source&&) noexcept;
    event_source& operator=(event_source&&) noexcept;
    ~event_source();
    
    // For internal use
    void* native_handle() const;
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Event queue (receives and stores events)
class event_queue {
public:
    event_queue();
    event_queue(event_queue&&) noexcept;
    event_queue& operator=(event_queue&&) noexcept;
    ~event_queue();
    
    // Register event sources
    void register_source(event_source& source);
    void unregister_source(event_source& source);
    
    // Poll for events
    [[nodiscard]] std::optional<any_event> poll();
    
    // wait for events
    [[nodiscard]] any_event wait();
    [[nodiscard]] std::optional<any_event> wait_for(double timeout_seconds);
    
    // Check if events are available
    [[nodiscard]] bool has_events() const;
    [[nodiscard]] bool empty() const { return !has_events(); }
    
    // Drain all events
    void clear();
    
    // Process all pending events with a callback
    template<typename F>
    void process_all(F&& handler) {
        while (auto event = poll()) {
            handler(*event);
        }
    }
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia
```

### Event Dispatcher

```cpp
namespace alia {

// Event handler type
using event_handler = std::function<void(const any_event&)>;

// Event dispatcher (routes events to handlers)
class event_dispatcher {
public:
    event_dispatcher();
    event_dispatcher(event_dispatcher&&) noexcept;
    event_dispatcher& operator=(event_dispatcher&&) noexcept;
    ~event_dispatcher();
    
    // Register generic handler for event type
    void set_handler(event_type type, event_handler handler);
    
    // Remove handler
    void remove_handler(event_type type);
    
    // Dispatch an event
    void dispatch(const any_event& event);
    
    // Convenience registration methods
    
    // Keyboard
    event_dispatcher& on_key_down(std::function<void(const KeyEvent&)> handler);
    event_dispatcher& on_key_up(std::function<void(const KeyEvent&)> handler);
    event_dispatcher& on_key_char(std::function<void(const char_event&)> handler);
    
    // Specific key
    event_dispatcher& on_key_down(Key key, std::function<void()> handler);
    event_dispatcher& on_key_down(Key key, std::function<void(const KeyEvent&)> handler);
    
    // Mouse
    event_dispatcher& on_mouse_move(std::function<void(const MouseEvent&)> handler);
    event_dispatcher& on_mouse_down(std::function<void(const MouseEvent&)> handler);
    event_dispatcher& on_mouse_up(std::function<void(const MouseEvent&)> handler);
    event_dispatcher& on_mouse_wheel(std::function<void(const MouseEvent&)> handler);
    
    // Specific mouse button
    event_dispatcher& on_mouse_down(MouseButton button, std::function<void()> handler);
    event_dispatcher& on_mouse_down(MouseButton button, std::function<void(const MouseEvent&)> handler);
    
    // window
    event_dispatcher& on_window_close(std::function<void()> handler);
    event_dispatcher& on_window_resize(std::function<void(const window_resize_event&)> handler);
    event_dispatcher& on_window_focus(std::function<void(bool focused)> handler);
    
    // Joystick
    event_dispatcher& on_joystick_axis(std::function<void(const JoystickAxisEvent&)> handler);
    event_dispatcher& on_joystick_button(std::function<void(const JoystickButtonEvent&)> handler);
    event_dispatcher& on_joystick_connected(std::function<void(int index)> handler);
    event_dispatcher& on_joystick_disconnected(std::function<void(int index)> handler);
    
    // User events (see User Events section)
    template<typename T>
    event_dispatcher& on_user_event(std::function<void(const T&)> handler);
    
    template<typename T>
    event_dispatcher& on_user_event(std::function<void(const T&, const any_event&)> handler);
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // namespace alia
```

---

## User Events

### Custom User Events

```cpp
namespace alia {

// Get unique event type ID for a user type
// IDs are assigned from UserBase (1048576) upward using an atomic counter
template<typename T>
[[nodiscard]] event_type get_user_event_id() {
    static event_type id = detail::allocate_user_event_id();
    return id;
}

// User event source (emits custom events)
class user_event_source : public event_source {
public:
    user_event_source();
    user_event_source(user_event_source&&) noexcept;
    user_event_source& operator=(user_event_source&&) noexcept;
    ~user_event_source();
    
    // Emit a user event
    template<typename T>
    void emit(const T& event_data);
    
    // Emit with custom timestamp
    template<typename T>
    void emit(const T& event_data, double timestamp);
    
private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

// Get event data from a user event
template<typename T>
[[nodiscard]] const T& get_event_data(const any_event& event);

// Get the source that emitted a user event
[[nodiscard]] user_event_source& get_user_event_source(const any_event& event);

} // namespace alia
```

### Example User Event

```cpp
// Example: Define a custom event
struct player_died_event {
    int player_id;
    vec2f position;
    int killer_id;
};

// Usage:
void setup_events() {
    user_event_source source;
    event_queue queue;
    event_dispatcher dispatcher;
    
    queue.register_source(source);
    
    // Register handler
    dispatcher.on_user_event<player_died_event>([](const player_died_event& ev) {
        std::cout << "Player " << ev.player_id << " died at " 
                  << ev.position.x << ", " << ev.position.y << "\n";
    });
    
    // Emit event
    source.emit(player_died_event{
        .player_id = 1,
        .position = {100.0f, 200.0f},
        .killer_id = 2
    });
    
    // Process events
    while (auto event = queue.poll()) {
        dispatcher.dispatch(*event);
    }
}
```

---

## Backend Interface

### OS Backend Interface

```cpp
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
```

---

## Example Usage

### Basic Window and Event Loop

```cpp
#include <alia/alia.hpp>

int main() {
    // Initialize ALIA
    alia::init();
    
    // Create window (99% use case - simple!)
    alia::window window(800, 600, "My Application");
    
    // Event handling
    alia::event_queue queue;
    alia::event_dispatcher dispatcher;
    
    queue.register_source(window.get_event_source());
    queue.register_source(alia::get_keyboard_event_source());
    queue.register_source(alia::get_mouse_event_source());
    
    bool running = true;
    
    dispatcher.on_window_close([&]() {
        running = false;
    });
    
    dispatcher.on_key_down(alia::Key::Escape, [&]() {
        running = false;
    });
    
    dispatcher.on_key_down(alia::Key::F11, [&]() {
        window.toggle_fullscreen();
    });
    
    // Main loop
    while (running) {
        // Process all pending events
        while (auto event = queue.poll()) {
            dispatcher.dispatch(*event);
        }
        
        // Clear and render
        alia::target::clear(alia::color::CornflowerBlue);
        
        // ... rendering code ...
        
        window.flip();
    }
    
    return 0;
}
```

### Input Handling

```cpp
#include <alia/alia.hpp>

void game_loop() {
    alia::window window(1024, 768, "Game");
    
    alia::event_queue queue;
    alia::event_dispatcher dispatcher;
    
    queue.register_source(window.get_event_source());
    queue.register_source(alia::get_keyboard_event_source());
    queue.register_source(alia::get_mouse_event_source());
    queue.register_source(alia::get_joystick_event_source());
    
    alia::vec2f player_pos{400, 300};
    float player_speed = 200.0f;  // pixels per second
    
    double last_time = alia::get_time();
    bool running = true;
    
    dispatcher.on_window_close([&]() { running = false; });
    
    while (running) {
        double now = alia::get_time();
        float dt = static_cast<float>(now - last_time);
        last_time = now;
        
        // Process events
        while (auto event = queue.poll()) {
            dispatcher.dispatch(*event);
        }
        
        // Keyboard movement (polling-based)
        auto kb = alia::get_keyboard_state();
        alia::vec2f move{0, 0};
        
        if (kb[alia::Key::W] || kb[alia::Key::up])    move.y -= 1;
        if (kb[alia::Key::S] || kb[alia::Key::down])  move.y += 1;
        if (kb[alia::Key::A] || kb[alia::Key::left])  move.x -= 1;
        if (kb[alia::Key::D] || kb[alia::Key::right]) move.x += 1;
        
        if (move.length_squared() > 0) {
            player_pos += move.normalized() * player_speed * dt;
        }
        
        // Gamepad input
        if (alia::get_joystick_count() > 0) {
            auto joy = alia::get_joystick_state(0);
            if (joy.info().is_gamepad) {
                alia::vec2f stick{
                    joy.get_axis(alia::gamepad_axis::left_x),
                    joy.get_axis(alia::gamepad_axis::left_y)
                };
                stick = alia::apply_deadzone(stick, 0.15f);
                player_pos += stick * player_speed * dt;
            }
        }
        
        // Render
        alia::target::clear();
        alia::draw_filled_circle(player_pos, 20.0f, alia::color::Red);
        window.flip();
    }
}
```

### File Dialog Example

```cpp
#include <alia/alia.hpp>

void load_image() {
    alia::file_dialog_options options;
    options.filters = {
        {"Image Files", "*.png;*.jpg;*.jpeg;*.bmp;*.gif"},
        {"PNG Images", "*.png"},
        {"JPEG Images", "*.jpg;*.jpeg"},
        {"All Files", "*.*"}
    };
    options.initial_path = "~/Pictures";
    
    auto result = alia::show_open_file_dialog(
        nullptr,
        "Select an Image",
        options
    );
    
    if (result) {
        std::cout << "Selected: " << *result << "\n";
        auto bitmap = alia::bitmap::load(*result);
        // ... use bitmap
    }
}

void save_document() {
    alia::file_dialog_options options;
    options.filters = {
        {"Text Documents", "*.txt"},
        {"All Files", "*.*"}
    };
    options.default_extension = "txt";
    
    auto result = alia::show_save_file_dialog(
        nullptr,
        "Save Document",
        options
    );
    
    if (result) {
        std::cout << "Saving to: " << *result << "\n";
        // ... save file
    }
}
```

### Multi-Window Application

```cpp
#include <alia/alia.hpp>

class Application {
public:
    void run() {
        main_window_ = alia::window(1024, 768, "Main window");
        
        queue_.register_source(main_window_.get_event_source());
        queue_.register_source(alia::get_keyboard_event_source());
        
        setup_handlers();
        
        while (running_) {
            process_events();
            render();
        }
    }
    
private:
    void setup_handlers() {
        dispatcher_.on_window_close([this]() {
            running_ = false;
        });
        
        dispatcher_.on_key_down(alia::Key::N, [this]() {
            // ctrl+N: New tool window
            if (alia::get_keyboard_state().ctrl()) {
                create_tool_window();
            }
        });
    }
    
    void create_tool_window() {
        auto& tool_win = tool_windows_.emplace_back(
            400, 300, "Tool window",
            alia::window_flags::windowed | alia::window_flags::resizable
        );
        queue_.register_source(tool_win.get_event_source());
    }
    
    void process_events() {
        while (auto event = queue_.poll()) {
            dispatcher_.dispatch(*event);
        }
    }
    
    void render() {
        // Render main window
        // (set as target implicitly or explicitly)
        alia::target::clear(alia::color::Black);
        // ... main content
        main_window_.flip();
        
        // Render tool windows
        for (auto& tool : tool_windows_) {
            // ... render tool window content
            tool.flip();
        }
    }
    
    bool running_ = true;
    alia::window main_window_;
    std::vector<alia::window> tool_windows_;
    alia::event_queue queue_;
    alia::event_dispatcher dispatcher_;
};

int main() {
    alia::init();
    Application app;
    app.run();
    return 0;
}
```
