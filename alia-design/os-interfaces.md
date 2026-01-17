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

// Window creation flags
enum class WindowFlags : uint32_t {
    None = 0,
    
    // Style flags
    Windowed = 1 << 0,           // Default windowed mode
    Fullscreen = 1 << 1,         // Exclusive fullscreen
    FullscreenWindow = 1 << 2,   // Borderless fullscreen window
    Resizable = 1 << 3,          // Allow resizing
    Maximized = 1 << 4,          // Start maximized
    Minimized = 1 << 5,          // Start minimized
    NoFrame = 1 << 6,            // Borderless window
    
    // Feature flags
    OpenGL = 1 << 8,             // Request OpenGL context
    Direct3D = 1 << 9,           // Request Direct3D (Windows)
    
    // Input flags
    GrabMouse = 1 << 16,         // Confine mouse to window
    HighDPI = 1 << 17,           // Enable High DPI support
};

constexpr WindowFlags operator|(WindowFlags a, WindowFlags b) {
    return static_cast<WindowFlags>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

// Window options for advanced configuration
struct WindowOptions {
    Vec2i position = {-1, -1};   // -1 = centered
    int refresh_rate = 0;        // 0 = desktop rate
    int color_depth = 32;
    int depth_buffer_bits = 24;
    int stencil_buffer_bits = 8;
    int multisamples = 0;        // MSAA samples
    bool vsync = true;
    const char* title = "ALIA Window";
};

// Window class
class Window {
public:
    // Construction
    Window() = default;
    Window(int width, int height, const char* title = "ALIA Window", 
           WindowFlags flags = WindowFlags::Windowed);
    Window(int width, int height, WindowFlags flags);
    Window(int width, int height, WindowFlags flags, 
           const WindowOptions& options);
    Window(Vec2i size, const char* title = "ALIA Window",
           WindowFlags flags = WindowFlags::Windowed);
    
    // Move only
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;
    ~Window();
    
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] explicit operator bool() const { return valid(); }
    
    // Properties
    [[nodiscard]] int width() const;
    [[nodiscard]] int height() const;
    [[nodiscard]] Vec2i size() const;
    [[nodiscard]] RectI rect() const { return RectI{{0, 0}, size()}; }
    [[nodiscard]] float aspect_ratio() const;
    
    [[nodiscard]] Vec2i position() const;
    [[nodiscard]] const char* title() const;
    [[nodiscard]] WindowFlags flags() const;
    
    // State
    [[nodiscard]] bool is_focused() const;
    [[nodiscard]] bool is_minimized() const;
    [[nodiscard]] bool is_maximized() const;
    [[nodiscard]] bool is_fullscreen() const;
    
    // Modification
    void set_title(const char* title);
    void set_position(Vec2i pos);
    void set_size(Vec2i size);
    void resize(int width, int height);
    
    void minimize();
    void maximize();
    void restore();
    
    // Fullscreen
    void set_fullscreen(bool fullscreen);
    void toggle_fullscreen();
    void set_fullscreen_window(bool enable);  // Borderless fullscreen
    
    // Display
    void flip();  // Present backbuffer
    void set_vsync(bool enable);
    [[nodiscard]] bool get_vsync() const;
    
    // Cursor
    void show_cursor(bool show = true);
    void hide_cursor();
    void set_cursor(Cursor cursor);  // See cursor enum below
    void set_cursor_position(Vec2i pos);
    [[nodiscard]] bool is_cursor_visible() const;
    
    // Grab
    void set_mouse_grab(bool grab);
    [[nodiscard]] bool is_mouse_grabbed() const;
    
    // Event source
    EventSource& get_event_source();
    
    // Backend handle (HWND, Window, etc.)
    [[nodiscard]] void* native_handle() const;
    
    // Current window (most recently focused or created)
    [[nodiscard]] static Window* current();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Cursor types
enum class Cursor {
    Default,
    Arrow,
    IBeam,
    Wait,
    Crosshair,
    ResizeNWSE,
    ResizeNESW,
    ResizeWE,
    ResizeNS,
    ResizeAll,
    Hand,
    NotAllowed,
    Hidden,
};

} // namespace alia
```

### Window Events

```cpp
namespace alia {

// Window event types (subset of all events, see Event System section)
namespace events {
    constexpr EventType WindowClose = 1;
    constexpr EventType WindowResize = 2;
    constexpr EventType WindowFocus = 3;
    constexpr EventType WindowUnfocus = 4;
    constexpr EventType WindowMove = 5;
    constexpr EventType WindowMinimize = 6;
    constexpr EventType WindowMaximize = 7;
    constexpr EventType WindowRestore = 8;
    constexpr EventType WindowExpose = 9;
}

// Window resize event data
struct WindowResizeEvent : Event {
    Window* window;
    Vec2i old_size;
    Vec2i new_size;
};

// Window move event data
struct WindowMoveEvent : Event {
    Window* window;
    Vec2i old_position;
    Vec2i new_position;
};

// Window focus event data
struct WindowFocusEvent : Event {
    Window* window;
    bool focused;
};

} // namespace alia
```

### Display Modes

```cpp
namespace alia {

// Display mode (resolution, refresh rate, etc.)
struct DisplayMode {
    Vec2i size;
    int refresh_rate;
    int color_depth;
    
    bool operator==(const DisplayMode& other) const = default;
};

// Get available display modes
[[nodiscard]] std::vector<DisplayMode> get_display_modes();
[[nodiscard]] std::vector<DisplayMode> get_display_modes(int monitor_index);

// Get current desktop mode
[[nodiscard]] DisplayMode get_desktop_mode();
[[nodiscard]] DisplayMode get_desktop_mode(int monitor_index);

// Find closest matching display mode
[[nodiscard]] DisplayMode find_closest_mode(Vec2i size, int refresh_rate = 0);

} // namespace alia
```

---

## Monitor

### Monitor Class

```cpp
namespace alia {

// Monitor information
struct MonitorInfo {
    int index;
    std::string name;
    Vec2i position;              // Position in virtual screen space
    Vec2i size;                  // Physical size in pixels
    Vec2i size_mm;               // Physical size in millimeters
    float dpi;                   // Dots per inch
    float scale_factor;          // DPI scale factor (e.g., 1.0, 1.25, 1.5, 2.0)
    bool is_primary;
    DisplayMode current_mode;
    std::vector<DisplayMode> modes;
};

// Monitor enumeration
[[nodiscard]] std::vector<MonitorInfo> get_monitors();
[[nodiscard]] int get_monitor_count();
[[nodiscard]] MonitorInfo get_primary_monitor();
[[nodiscard]] MonitorInfo get_monitor(int index);

// Get monitor containing a point
[[nodiscard]] MonitorInfo get_monitor_at(Vec2i point);

// Get monitor for a window
[[nodiscard]] MonitorInfo get_window_monitor(const Window& window);

} // namespace alia
```

---

## Native Dialogs

### File Dialogs

```cpp
namespace alia {

// File dialog filter
struct FileFilter {
    std::string description;     // e.g., "Image Files"
    std::string patterns;        // e.g., "*.png;*.jpg;*.bmp"
};

// File dialog options
struct FileDialogOptions {
    std::vector<FileFilter> filters;
    std::string initial_path;
    std::string default_extension;
    bool allow_multiple = false;  // For open dialogs
    bool show_hidden = false;
};

// Open file dialog
[[nodiscard]] std::optional<std::string> show_open_file_dialog(
    const Window* parent = nullptr,
    const char* title = "Open File",
    const FileDialogOptions& options = {}
);

// Open multiple files dialog
[[nodiscard]] std::vector<std::string> show_open_files_dialog(
    const Window* parent = nullptr,
    const char* title = "Open Files",
    const FileDialogOptions& options = {}
);

// Save file dialog
[[nodiscard]] std::optional<std::string> show_save_file_dialog(
    const Window* parent = nullptr,
    const char* title = "Save File",
    const FileDialogOptions& options = {}
);

// Folder selection dialog
[[nodiscard]] std::optional<std::string> show_folder_dialog(
    const Window* parent = nullptr,
    const char* title = "Select Folder",
    const char* initial_path = nullptr
);

} // namespace alia
```

### Message Boxes

```cpp
namespace alia {

// Message box button configurations
enum class MessageBoxButtons {
    OK,
    OKCancel,
    YesNo,
    YesNoCancel,
    RetryCancel,
    AbortRetryIgnore,
};

// Message box icons
enum class MessageBoxIcon {
    None,
    Info,
    Warning,
    Error,
    Question,
};

// Message box result
enum class MessageBoxResult {
    OK,
    Cancel,
    Yes,
    No,
    Retry,
    Abort,
    Ignore,
};

// Show message box
[[nodiscard]] MessageBoxResult show_message_box(
    const char* title,
    const char* message,
    MessageBoxButtons buttons = MessageBoxButtons::OK,
    MessageBoxIcon icon = MessageBoxIcon::None,
    const Window* parent = nullptr
);

// Convenience functions
void show_info_box(const char* title, const char* message, 
                   const Window* parent = nullptr);

void show_warning_box(const char* title, const char* message,
                      const Window* parent = nullptr);

void show_error_box(const char* title, const char* message,
                    const Window* parent = nullptr);

[[nodiscard]] bool show_confirm_box(const char* title, const char* message,
                                     const Window* parent = nullptr);

} // namespace alia
```

### Text Log

```cpp
namespace alia {

// Text log window (for debug output)
class TextLog {
public:
    TextLog() = default;
    TextLog(const char* title, bool monospace = true);
    
    TextLog(TextLog&&) noexcept;
    TextLog& operator=(TextLog&&) noexcept;
    ~TextLog();
    
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
    EventSource& get_event_source();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
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
    Up, Down, Left, Right,
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
    CapsLock, NumLock, ScrollLock,
    
    // Other
    PrintScreen, Pause, Menu,
    
    KeyCount  // Number of keys
};

// Key modifiers (bitmask)
enum class KeyMod : uint32_t {
    None = 0,
    Shift = 1 << 0,
    Ctrl = 1 << 1,
    Alt = 1 << 2,
    Super = 1 << 3,  // Windows/Command key
    CapsLock = 1 << 4,
    NumLock = 1 << 5,
};

constexpr KeyMod operator|(KeyMod a, KeyMod b) {
    return static_cast<KeyMod>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

constexpr KeyMod operator&(KeyMod a, KeyMod b) {
    return static_cast<KeyMod>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

// Keyboard state
class KeyboardState {
public:
    KeyboardState();
    
    [[nodiscard]] bool is_key_down(Key key) const;
    [[nodiscard]] bool operator[](Key key) const { return is_key_down(key); }
    
    [[nodiscard]] KeyMod get_modifiers() const;
    
    // Check specific modifiers
    [[nodiscard]] bool shift() const;
    [[nodiscard]] bool ctrl() const;
    [[nodiscard]] bool alt() const;
    [[nodiscard]] bool super() const;
    
private:
    std::array<bool, static_cast<size_t>(Key::KeyCount)> keys_;
    KeyMod modifiers_;
};

// Get current keyboard state
[[nodiscard]] KeyboardState get_keyboard_state();

// Check if a specific key is currently down
[[nodiscard]] bool is_key_down(Key key);
[[nodiscard]] bool is_key_down(const KeyboardState& state, Key key);

// Get event source for keyboard events
EventSource& get_keyboard_event_source();

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
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    
    ButtonCount
};

// Convenience aliases
inline constexpr MouseButton LMB = MouseButton::Left;
inline constexpr MouseButton RMB = MouseButton::Right;
inline constexpr MouseButton MMB = MouseButton::Middle;

// Mouse state
class MouseState {
public:
    MouseState();
    
    [[nodiscard]] Vec2i position() const;
    [[nodiscard]] Vec2f position_f() const;
    
    [[nodiscard]] bool is_button_down(MouseButton button) const;
    [[nodiscard]] bool operator[](MouseButton button) const { return is_button_down(button); }
    
    [[nodiscard]] int wheel_delta() const;
    [[nodiscard]] int wheel_delta_x() const;  // Horizontal scroll
    
private:
    Vec2i position_;
    std::array<bool, static_cast<size_t>(MouseButton::ButtonCount)> buttons_;
    int wheel_delta_;
    int wheel_delta_x_;
};

// Get current mouse state
[[nodiscard]] MouseState get_mouse_state();

// Get mouse position
[[nodiscard]] Vec2i get_mouse_position();
[[nodiscard]] Vec2f get_mouse_position_f();

// Set mouse position (relative to window or screen)
void set_mouse_position(Vec2i pos);
void set_mouse_position(Vec2i pos, const Window& window);

// Check if a button is down
[[nodiscard]] bool is_mouse_button_down(MouseButton button);

// Get event source for mouse events
EventSource& get_mouse_event_source();

} // namespace alia
```

### Joystick/Gamepad

```cpp
namespace alia {

// Joystick information
struct JoystickInfo {
    int index;
    std::string name;
    int num_axes;
    int num_buttons;
    int num_hats;       // D-pads
    bool is_gamepad;    // Has standard gamepad layout
};

// Hat (D-pad) direction
enum class HatDirection : uint8_t {
    Centered = 0,
    Up = 1,
    Right = 2,
    Down = 4,
    Left = 8,
    UpRight = Up | Right,
    DownRight = Down | Right,
    DownLeft = Down | Left,
    UpLeft = Up | Left,
};

// Standard gamepad buttons (for is_gamepad == true)
enum class GamepadButton {
    A, B, X, Y,
    LeftBumper, RightBumper,
    Back, Start, Guide,
    LeftThumb, RightThumb,
    DPadUp, DPadRight, DPadDown, DPadLeft,
    
    ButtonCount
};

// Standard gamepad axes
enum class GamepadAxis {
    LeftX, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger,
    
    AxisCount
};

// Joystick state
class JoystickState {
public:
    JoystickState();
    JoystickState(int index);
    
    [[nodiscard]] bool valid() const;
    [[nodiscard]] int index() const;
    [[nodiscard]] const JoystickInfo& info() const;
    
    // Raw access
    [[nodiscard]] float get_axis(int axis) const;          // -1.0 to 1.0
    [[nodiscard]] bool get_button(int button) const;
    [[nodiscard]] HatDirection get_hat(int hat) const;
    
    // Gamepad access (if is_gamepad)
    [[nodiscard]] float get_axis(GamepadAxis axis) const;
    [[nodiscard]] bool get_button(GamepadButton button) const;
    
private:
    int index_;
    JoystickInfo info_;
    std::vector<float> axes_;
    std::vector<bool> buttons_;
    std::vector<HatDirection> hats_;
};

// Joystick enumeration
[[nodiscard]] int get_joystick_count();
[[nodiscard]] std::vector<JoystickInfo> get_joysticks();
[[nodiscard]] JoystickInfo get_joystick_info(int index);

// Get joystick state
[[nodiscard]] JoystickState get_joystick_state(int index);

// Check if joystick is connected
[[nodiscard]] bool is_joystick_connected(int index);

// Reconfigure joysticks (after connect/disconnect)
void reconfigure_joysticks();

// Get event source for joystick events
EventSource& get_joystick_event_source();

// Deadzone utility
[[nodiscard]] float apply_deadzone(float value, float deadzone);
[[nodiscard]] Vec2f apply_deadzone(Vec2f stick, float deadzone);

} // namespace alia
```

### Touch Input

```cpp
namespace alia {

// Touch point
struct TouchPoint {
    int id;              // Unique identifier for this touch
    Vec2f position;
    Vec2f pressure;      // 0.0 to 1.0, if available
    bool is_primary;     // First touch in a multi-touch sequence
};

// Touch state
class TouchState {
public:
    TouchState();
    
    [[nodiscard]] int touch_count() const;
    [[nodiscard]] const TouchPoint& get_touch(int index) const;
    [[nodiscard]] const TouchPoint* find_touch(int id) const;
    
    // Iterate touches
    [[nodiscard]] auto begin() const { return touches_.begin(); }
    [[nodiscard]] auto end() const { return touches_.end(); }
    
private:
    std::vector<TouchPoint> touches_;
};

// Get current touch state
[[nodiscard]] TouchState get_touch_state();

// Check touch support
[[nodiscard]] bool has_touch_input();
[[nodiscard]] int get_max_touches();

// Get event source for touch events
EventSource& get_touch_event_source();

} // namespace alia
```

---

## Event System

### Event Types

```cpp
namespace alia {

// Event type (32-bit ID)
using EventType = uint32_t;

// Reserved event type ranges
namespace events {
    // System events: 0 - 99
    constexpr EventType SystemBase = 0;
    
    // Window events: 1 - 99 (defined earlier)
    
    // Keyboard events: 100 - 199
    constexpr EventType KeyDown = 100;
    constexpr EventType KeyUp = 101;
    constexpr EventType KeyChar = 102;
    
    // Mouse events: 200 - 299
    constexpr EventType MouseMove = 200;
    constexpr EventType MouseButtonDown = 201;
    constexpr EventType MouseButtonUp = 202;
    constexpr EventType MouseWheel = 203;
    constexpr EventType MouseEnterWindow = 204;
    constexpr EventType MouseLeaveWindow = 205;
    
    // Joystick events: 300 - 399
    constexpr EventType JoystickAxis = 300;
    constexpr EventType JoystickButton = 301;
    constexpr EventType JoystickHat = 302;
    constexpr EventType JoystickConnected = 303;
    constexpr EventType JoystickDisconnected = 304;
    
    // Touch events: 400 - 499
    constexpr EventType TouchBegin = 400;
    constexpr EventType TouchEnd = 401;
    constexpr EventType TouchMove = 402;
    constexpr EventType TouchCancel = 403;
    
    // Audio events: 500 - 599 (defined in audio-interfaces.md)
    
    // User events: 1048576+ (0x100000+)
    constexpr EventType UserBase = 1048576;
}

// Base event structure
struct Event {
    EventType type;
    double timestamp;
    void* source;
};

// Keyboard events
struct KeyEvent : Event {
    Key keycode;
    int scancode;
    KeyMod modifiers;
    bool repeat;
};

struct CharEvent : Event {
    char32_t codepoint;
    KeyMod modifiers;
};

// Mouse events
struct MouseEvent : Event {
    Vec2f position;
    Vec2f delta;
    MouseButton button;  // For button events
    int wheel_y;         // For wheel events
    int wheel_x;         // Horizontal scroll
    KeyMod modifiers;
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
    HatDirection direction;
};

struct JoystickConnectionEvent : Event {
    int joystick;
    bool connected;
};

// Touch events
struct TouchEvent : Event {
    int touch_id;
    Vec2f position;
    Vec2f delta;
    float pressure;
    bool is_primary;
};

} // namespace alia
```

### Event Queue

```cpp
namespace alia {

// Event wrapper (polymorphic event holder)
class AnyEvent {
public:
    AnyEvent() = default;
    AnyEvent(const Event& event);
    
    AnyEvent(AnyEvent&&) noexcept;
    AnyEvent& operator=(AnyEvent&&) noexcept;
    ~AnyEvent();
    
    [[nodiscard]] EventType type() const;
    [[nodiscard]] double timestamp() const;
    
    // Access raw event
    [[nodiscard]] const Event& get() const;
    
    // Cast to specific event type
    template<typename T>
    [[nodiscard]] const T& as() const;
    
    template<typename T>
    [[nodiscard]] const T* try_as() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Event source (generates events)
class EventSource {
public:
    EventSource();
    EventSource(EventSource&&) noexcept;
    EventSource& operator=(EventSource&&) noexcept;
    ~EventSource();
    
    // For internal use
    void* native_handle() const;
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Event queue (receives and stores events)
class EventQueue {
public:
    EventQueue();
    EventQueue(EventQueue&&) noexcept;
    EventQueue& operator=(EventQueue&&) noexcept;
    ~EventQueue();
    
    // Register event sources
    void register_source(EventSource& source);
    void unregister_source(EventSource& source);
    
    // Poll for events
    [[nodiscard]] std::optional<AnyEvent> poll();
    
    // Wait for events
    [[nodiscard]] AnyEvent wait();
    [[nodiscard]] std::optional<AnyEvent> wait_for(double timeout_seconds);
    
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
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace alia
```

### Event Dispatcher

```cpp
namespace alia {

// Event handler type
using EventHandler = std::function<void(const AnyEvent&)>;

// Event dispatcher (routes events to handlers)
class EventDispatcher {
public:
    EventDispatcher();
    EventDispatcher(EventDispatcher&&) noexcept;
    EventDispatcher& operator=(EventDispatcher&&) noexcept;
    ~EventDispatcher();
    
    // Register generic handler for event type
    void set_handler(EventType type, EventHandler handler);
    
    // Remove handler
    void remove_handler(EventType type);
    
    // Dispatch an event
    void dispatch(const AnyEvent& event);
    
    // Convenience registration methods
    
    // Keyboard
    EventDispatcher& on_key_down(std::function<void(const KeyEvent&)> handler);
    EventDispatcher& on_key_up(std::function<void(const KeyEvent&)> handler);
    EventDispatcher& on_key_char(std::function<void(const CharEvent&)> handler);
    
    // Specific key
    EventDispatcher& on_key_down(Key key, std::function<void()> handler);
    EventDispatcher& on_key_down(Key key, std::function<void(const KeyEvent&)> handler);
    
    // Mouse
    EventDispatcher& on_mouse_move(std::function<void(const MouseEvent&)> handler);
    EventDispatcher& on_mouse_down(std::function<void(const MouseEvent&)> handler);
    EventDispatcher& on_mouse_up(std::function<void(const MouseEvent&)> handler);
    EventDispatcher& on_mouse_wheel(std::function<void(const MouseEvent&)> handler);
    
    // Specific mouse button
    EventDispatcher& on_mouse_down(MouseButton button, std::function<void()> handler);
    EventDispatcher& on_mouse_down(MouseButton button, std::function<void(const MouseEvent&)> handler);
    
    // Window
    EventDispatcher& on_window_close(std::function<void()> handler);
    EventDispatcher& on_window_resize(std::function<void(const WindowResizeEvent&)> handler);
    EventDispatcher& on_window_focus(std::function<void(bool focused)> handler);
    
    // Joystick
    EventDispatcher& on_joystick_axis(std::function<void(const JoystickAxisEvent&)> handler);
    EventDispatcher& on_joystick_button(std::function<void(const JoystickButtonEvent&)> handler);
    EventDispatcher& on_joystick_connected(std::function<void(int index)> handler);
    EventDispatcher& on_joystick_disconnected(std::function<void(int index)> handler);
    
    // User events (see User Events section)
    template<typename T>
    EventDispatcher& on_user_event(std::function<void(const T&)> handler);
    
    template<typename T>
    EventDispatcher& on_user_event(std::function<void(const T&, const AnyEvent&)> handler);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
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
[[nodiscard]] EventType get_user_event_id() {
    static EventType id = detail::allocate_user_event_id();
    return id;
}

// User event source (emits custom events)
class UserEventSource : public EventSource {
public:
    UserEventSource();
    UserEventSource(UserEventSource&&) noexcept;
    UserEventSource& operator=(UserEventSource&&) noexcept;
    ~UserEventSource();
    
    // Emit a user event
    template<typename T>
    void emit(const T& event_data);
    
    // Emit with custom timestamp
    template<typename T>
    void emit(const T& event_data, double timestamp);
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Get event data from a user event
template<typename T>
[[nodiscard]] const T& get_event_data(const AnyEvent& event);

// Get the source that emitted a user event
[[nodiscard]] UserEventSource& get_user_event_source(const AnyEvent& event);

} // namespace alia
```

### Example User Event

```cpp
// Example: Define a custom event
struct PlayerDiedEvent {
    int player_id;
    Vec2f position;
    int killer_id;
};

// Usage:
void setup_events() {
    UserEventSource source;
    EventQueue queue;
    EventDispatcher dispatcher;
    
    queue.register_source(source);
    
    // Register handler
    dispatcher.on_user_event<PlayerDiedEvent>([](const PlayerDiedEvent& ev) {
        std::cout << "Player " << ev.player_id << " died at " 
                  << ev.position.x << ", " << ev.position.y << "\n";
    });
    
    // Emit event
    source.emit(PlayerDiedEvent{
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

// Window backend interface
class IWindowBackend {
public:
    virtual ~IWindowBackend() = default;
    
    // Lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool is_valid() const = 0;
    
    // Window operations
    virtual void* create_window(int width, int height, const char* title, 
                                 uint32_t flags, const WindowOptions* options) = 0;
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
    
    // Cursor
    virtual void show_cursor(void* handle, bool show) = 0;
    virtual void set_cursor(void* handle, int cursor_type) = 0;
    virtual void set_cursor_position(void* handle, int x, int y) = 0;
    virtual void grab_mouse(void* handle, bool grab) = 0;
    
    // Monitor
    virtual int get_monitor_count() = 0;
    virtual void get_monitor_info(int index, MonitorInfo* info) = 0;
    virtual void get_display_modes(int monitor, std::vector<DisplayMode>* modes) = 0;
    
    // Events
    virtual void* get_event_source(void* handle) = 0;
    
    // Backend info
    virtual const char* get_name() const = 0;
};

// Input backend interface
class IInputBackend {
public:
    virtual ~IInputBackend() = default;
    
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
    virtual void get_joystick_info(int index, JoystickInfo* info) = 0;
    virtual void get_joystick_state(int index, float* axes, bool* buttons, 
                                     uint8_t* hats) = 0;
    virtual void reconfigure_joysticks() = 0;
    virtual void* get_joystick_event_source() = 0;
    
    // Touch
    virtual bool has_touch_input() = 0;
    virtual int get_max_touches() = 0;
    virtual void get_touch_state(TouchPoint* points, int* count, int max_count) = 0;
    virtual void* get_touch_event_source() = 0;
};

// Dialog backend interface
class IDialogBackend {
public:
    virtual ~IDialogBackend() = default;
    
    virtual bool show_file_dialog(bool open, bool multiple, const char* title,
                                   const FileDialogOptions* options,
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
class IClipboardBackend {
public:
    virtual ~IClipboardBackend() = default;
    
    virtual bool has_text() = 0;
    virtual bool get_text(std::string* result) = 0;
    virtual bool set_text(const char* text, size_t length) = 0;
    virtual bool has_image() = 0;
    virtual void clear() = 0;
};

// Backend registration
struct OSBackendInfo {
    const char* name;
    uint32_t id;
    bool (*is_available)();
    IWindowBackend* (*create_window_backend)();
    IInputBackend* (*create_input_backend)();
    IDialogBackend* (*create_dialog_backend)();
    IClipboardBackend* (*create_clipboard_backend)();
    void (*destroy)(void*);
};

void register_os_backend(const OSBackendInfo& info);
std::span<const OSBackendInfo> get_os_backends();
const OSBackendInfo* get_current_os_backend();

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
    alia::Window window(800, 600, "My Application");
    
    // Event handling
    alia::EventQueue queue;
    alia::EventDispatcher dispatcher;
    
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
        alia::target::clear(alia::Color::CornflowerBlue);
        
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
    alia::Window window(1024, 768, "Game");
    
    alia::EventQueue queue;
    alia::EventDispatcher dispatcher;
    
    queue.register_source(window.get_event_source());
    queue.register_source(alia::get_keyboard_event_source());
    queue.register_source(alia::get_mouse_event_source());
    queue.register_source(alia::get_joystick_event_source());
    
    alia::Vec2f player_pos{400, 300};
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
        alia::Vec2f move{0, 0};
        
        if (kb[alia::Key::W] || kb[alia::Key::Up])    move.y -= 1;
        if (kb[alia::Key::S] || kb[alia::Key::Down])  move.y += 1;
        if (kb[alia::Key::A] || kb[alia::Key::Left])  move.x -= 1;
        if (kb[alia::Key::D] || kb[alia::Key::Right]) move.x += 1;
        
        if (move.length_squared() > 0) {
            player_pos += move.normalized() * player_speed * dt;
        }
        
        // Gamepad input
        if (alia::get_joystick_count() > 0) {
            auto joy = alia::get_joystick_state(0);
            if (joy.info().is_gamepad) {
                alia::Vec2f stick{
                    joy.get_axis(alia::GamepadAxis::LeftX),
                    joy.get_axis(alia::GamepadAxis::LeftY)
                };
                stick = alia::apply_deadzone(stick, 0.15f);
                player_pos += stick * player_speed * dt;
            }
        }
        
        // Render
        alia::target::clear();
        alia::draw_filled_circle(player_pos, 20.0f, alia::Color::Red);
        window.flip();
    }
}
```

### File Dialog Example

```cpp
#include <alia/alia.hpp>

void load_image() {
    alia::FileDialogOptions options;
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
        auto bitmap = alia::Bitmap::load(*result);
        // ... use bitmap
    }
}

void save_document() {
    alia::FileDialogOptions options;
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
        main_window_ = alia::Window(1024, 768, "Main Window");
        
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
            // Ctrl+N: New tool window
            if (alia::get_keyboard_state().ctrl()) {
                create_tool_window();
            }
        });
    }
    
    void create_tool_window() {
        auto& tool_win = tool_windows_.emplace_back(
            400, 300, "Tool Window",
            alia::WindowFlags::Windowed | alia::WindowFlags::Resizable
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
        alia::target::clear(alia::Color::Black);
        // ... main content
        main_window_.flip();
        
        // Render tool windows
        for (auto& tool : tool_windows_) {
            // ... render tool window content
            tool.flip();
        }
    }
    
    bool running_ = true;
    alia::Window main_window_;
    std::vector<alia::Window> tool_windows_;
    alia::EventQueue queue_;
    alia::EventDispatcher dispatcher_;
};

int main() {
    alia::init();
    Application app;
    app.run();
    return 0;
}
```
