#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <mutex>
#include <string>
#include <string_view>
#include <variant>

#include "window.hpp"
#include "window_events.hpp"
#include "../io/keycodes.hpp"
#include "../events/event_source_impl.hpp"  // template instantiation for emit<>

namespace alia {

// ── VK → alia::key mapping ────────────────────────────────────────────

static key vk_to_key(WPARAM vk, LPARAM lp) {
    // Extended key bit (bit 24) distinguishes e.g. numpad Enter from main Enter
    bool extended = (lp >> 24) & 1;
    switch (vk) {
    case VK_BACK:       return key::backspace;
    case VK_TAB:        return key::tab;
    case VK_RETURN:     return extended ? key::numpad_enter : key::enter;
    case VK_ESCAPE:     return key::escape;
    case VK_SPACE:      return key::space;
    case VK_PRIOR:      return key::page_up;
    case VK_NEXT:       return key::page_down;
    case VK_END:        return key::end;
    case VK_HOME:       return key::home;
    case VK_LEFT:       return key::left;
    case VK_UP:         return key::up;
    case VK_RIGHT:      return key::right;
    case VK_DOWN:       return key::down;
    case VK_INSERT:     return key::insert;
    case VK_DELETE:     return key::del;
    case VK_LWIN:       return key::lsuper;
    case VK_RWIN:       return key::rsuper;
    case VK_APPS:       return key::menu;
    case VK_MULTIPLY:   return key::numpad_multiply;
    case VK_ADD:        return key::numpad_add;
    case VK_SUBTRACT:   return key::numpad_subtract;
    case VK_DECIMAL:    return key::numpad_decimal;
    case VK_DIVIDE:     return key::numpad_divide;
    case VK_NUMLOCK:    return key::num_lock;
    case VK_SCROLL:     return key::scroll_lock;
    case VK_LSHIFT:     return key::lshift;
    case VK_RSHIFT:     return key::rshift;
    case VK_LCONTROL:   return key::lctrl;
    case VK_RCONTROL:   return key::rctrl;
    case VK_LMENU:      return key::lalt;
    case VK_RMENU:      return key::ralt;
    case VK_CAPITAL:    return key::caps_lock;
    case VK_SNAPSHOT:   return key::print_screen;
    case VK_PAUSE:      return key::pause;
    case VK_OEM_MINUS:  return key::minus;
    case VK_OEM_PLUS:   return key::equals;
    case VK_OEM_4:      return key::left_bracket;
    case VK_OEM_6:      return key::right_bracket;
    case VK_OEM_5:      return key::backslash;
    case VK_OEM_1:      return key::semicolon;
    case VK_OEM_7:      return key::apostrophe;
    case VK_OEM_3:      return key::grave;
    case VK_OEM_COMMA:  return key::comma;
    case VK_OEM_PERIOD: return key::period;
    case VK_OEM_2:      return key::slash;
    default:
        if (vk >= '0' && vk <= '9')
            return static_cast<key>(static_cast<int>(key::num0) + (int)(vk - '0'));
        if (vk >= 'A' && vk <= 'Z')
            return static_cast<key>(static_cast<int>(key::A) + (int)(vk - 'A'));
        if (vk >= VK_F1 && vk <= VK_F12)
            return static_cast<key>(static_cast<int>(key::f1) + (int)(vk - VK_F1));
        if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
            return static_cast<key>(static_cast<int>(key::numpad0) + (int)(vk - VK_NUMPAD0));
        return key::unknown;
    }
}

// ── Modifier snapshot ─────────────────────────────────────────────────

static key_mod get_modifiers() {
    key_mod m = key_mod::none;
    if (GetKeyState(VK_SHIFT)   & 0x8000) m |= key_mod::shift;
    if (GetKeyState(VK_CONTROL) & 0x8000) m |= key_mod::ctrl;
    if (GetKeyState(VK_MENU)    & 0x8000) m |= key_mod::alt;
    if (GetKeyState(VK_LWIN)    & 0x8000 ||
        GetKeyState(VK_RWIN)    & 0x8000) m |= key_mod::super;
    if (GetKeyState(VK_CAPITAL) & 0x0001) m |= key_mod::caps_lock;
    if (GetKeyState(VK_NUMLOCK) & 0x0001) m |= key_mod::num_lock;
    return m;
}

// ── UTF-8 ↔ UTF-16 helpers ────────────────────────────────────────────

static std::wstring utf8_to_wide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}

// ── Window class name ─────────────────────────────────────────────────

static constexpr const wchar_t* k_wnd_class = L"AliaWindow_v1";

// ── win32_window_impl ─────────────────────────────────────────────────

class win32_window_impl : public window_impl {
public:
    win32_window_impl() = default;

    ~win32_window_impl() override {
        if (hwnd) DestroyWindow(hwnd);
    }

    HWND         hwnd           = nullptr;
    event_source source;
    vec2i        client_size    = {};
    std::string  title_utf8;
    bool         cursor_visible = true;
    bool         mouse_grabbed  = false;
    key          last_key_down  = key::unknown;  // forwarded to WM_CHAR
    WCHAR        high_surrogate = 0;             // UTF-16 surrogate buffer

    // Saved placement for fullscreen toggle
    WINDOWPLACEMENT saved_placement = {sizeof(WINDOWPLACEMENT)};
    bool is_fs = false;

    // ── window_impl interface ─────────────────────────────────────────

    vec2i size()         const override { return client_size; }
    float aspect_ratio() const override {
        return client_size.y ? static_cast<float>(client_size.x) / client_size.y : 1.0f;
    }
    cstring_view title() const override { return cstring_view(title_utf8.c_str()); }

    vec2i position() const override {
        if (!hwnd) return {};
        POINT pt = {0, 0};
        ClientToScreen(hwnd, &pt);
        return {pt.x, pt.y};
    }

    bool is_focused()    const override { return hwnd && GetForegroundWindow() == hwnd; }
    bool is_minimized()  const override { return hwnd && IsIconic(hwnd); }
    bool is_maximized()  const override { return hwnd && IsZoomed(hwnd); }
    bool is_fullscreen() const override { return is_fs; }

    void set_title(cstring_view t) override {
        if (!hwnd) return;
        title_utf8 = t.data();
        SetWindowTextW(hwnd, utf8_to_wide(title_utf8).c_str());
    }

    void set_position(vec2i pos) override {
        if (!hwnd) return;
        SetWindowPos(hwnd, nullptr, pos.x, pos.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }

    void resize(vec2i sz) override {
        if (!hwnd) return;
        DWORD style    = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
        DWORD ex_style = (DWORD)GetWindowLongW(hwnd, GWL_EXSTYLE);
        RECT wr = {0, 0, sz.x, sz.y};
        AdjustWindowRectEx(&wr, style, FALSE, ex_style);
        SetWindowPos(hwnd, nullptr, 0, 0,
                     wr.right - wr.left, wr.bottom - wr.top,
                     SWP_NOMOVE | SWP_NOZORDER);
    }

    void minimize() override { if (hwnd) ShowWindow(hwnd, SW_MINIMIZE); }
    void maximize() override { if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE); }
    void restore()  override { if (hwnd) ShowWindow(hwnd, SW_RESTORE); }

    void set_fullscreen(bool fs) override {
        if (!hwnd || is_fs == fs) return;
        if (fs) {
            GetWindowPlacement(hwnd, &saved_placement);
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
            SetWindowLongW(hwnd, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);

            MONITORINFO mi = {sizeof(mi)};
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right  - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        } else {
            DWORD style = (DWORD)GetWindowLongW(hwnd, GWL_STYLE);
            SetWindowLongW(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(hwnd, &saved_placement);
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
        is_fs = fs;
    }

    void set_fullscreen_window(bool e) override { set_fullscreen(e); }

    void show_cursor(bool show) override {
        if (!hwnd) return;
        cursor_visible = show;
        if (is_focused()) SetCursor(show ? LoadCursor(nullptr, IDC_ARROW) : nullptr);
    }

    void set_cursor(cursor c) override {
        if (!hwnd) return;
        LPCTSTR id = IDC_ARROW;
        switch (c) {
            case cursor::ibeam:       id = IDC_IBEAM;    break;
            case cursor::wait:        id = IDC_WAIT;     break;
            case cursor::crosshair:   id = IDC_CROSS;    break;
            case cursor::resize_we:   id = IDC_SIZEWE;   break;
            case cursor::resize_ns:   id = IDC_SIZENS;   break;
            case cursor::resize_nwse: id = IDC_SIZENWSE; break;
            case cursor::resize_nesw: id = IDC_SIZENESW; break;
            case cursor::resize_all:  id = IDC_SIZEALL;  break;
            case cursor::hand:        id = IDC_HAND;     break;
            case cursor::not_allowed: id = IDC_NO;       break;
            case cursor::hidden:      cursor_visible = false; return;
            default:                  break;
        }
        cursor_visible = true;
        SetCursor(LoadCursor(nullptr, id));
    }

    void set_cursor_position(vec2i pos) override {
        if (!hwnd) return;
        POINT pt = {pos.x, pos.y};
        ClientToScreen(hwnd, &pt);
        SetCursorPos(pt.x, pt.y);
    }

    bool is_cursor_visible() const override { return cursor_visible; }

    void set_mouse_grab(bool grab) override {
        if (!hwnd) return;
        mouse_grabbed = grab;
        if (grab) SetCapture(hwnd);
        else      ReleaseCapture();
    }

    bool is_mouse_grabbed() const override { return mouse_grabbed; }

    event_source& get_event_source() override { return source; }

    void poll() override {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void* native_handle() const override { return static_cast<void*>(hwnd); }

    // ── Win32 internals ───────────────────────────────────────────────

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static bool             ensure_class_registered();
};

// ── Win32 window class registration ──────────────────────────────────

bool win32_window_impl::ensure_class_registered() {
    static std::once_flag flag;
    static bool ok = false;
    std::call_once(flag, []() {
        WNDCLASSEXW wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = win32_window_impl::wnd_proc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = k_wnd_class;
        ok = (RegisterClassExW(&wc) != 0);
    });
    return ok;
}

// ── Win32 message procedure ───────────────────────────────────────────

LRESULT CALLBACK win32_window_impl::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Attach impl pointer on creation
    if (msg == WM_NCCREATE) {
        auto* cs  = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* imp = reinterpret_cast<win32_window_impl*>(cs->lpCreateParams);
        imp->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(imp));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    auto* imp = reinterpret_cast<win32_window_impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!imp) return DefWindowProcW(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_CLOSE:
        imp->source.emit(window_close_event{});
        return 0;  // Don't destroy; let the user decide

    case WM_DESTROY:
        imp->hwnd = nullptr;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        imp->client_size = {w, h};
        if (wp != SIZE_MINIMIZED)
            imp->source.emit(window_resize_event{{w, h}});
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        imp->last_key_down = vk_to_key(wp, lp);
        imp->source.emit(window_key_down_event{imp->last_key_down});
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        imp->source.emit(window_key_up_event{vk_to_key(wp, lp)});
        return 0;

    case WM_CHAR:
    case WM_SYSCHAR: {
        WCHAR ch     = static_cast<WCHAR>(wp);
        bool  repeat = (lp >> 30) & 1;
        if (ch >= 0xD800 && ch <= 0xDBFF) {
            imp->high_surrogate = ch;  // buffer high surrogate, wait for low
            return 0;
        }
        char32_t cp;
        if (ch >= 0xDC00 && ch <= 0xDFFF && imp->high_surrogate != 0) {
            cp = 0x10000u + ((imp->high_surrogate - 0xD800u) << 10) + (ch - 0xDC00u);
        } else {
            cp = ch;
        }
        imp->high_surrogate = 0;
        imp->source.emit(window_key_char_event{imp->last_key_down, cp, get_modifiers(), repeat});
        return 0;
    }

    case WM_MOUSEMOVE:
        imp->source.emit(window_mouse_move_event{{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;

    case WM_LBUTTONDOWN:
        imp->source.emit(window_mouse_button_down_event{mouse_button::left,  {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_LBUTTONUP:
        imp->source.emit(window_mouse_button_up_event{mouse_button::left,    {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_RBUTTONDOWN:
        imp->source.emit(window_mouse_button_down_event{mouse_button::right,  {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_RBUTTONUP:
        imp->source.emit(window_mouse_button_up_event{mouse_button::right,    {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_MBUTTONDOWN:
        imp->source.emit(window_mouse_button_down_event{mouse_button::middle, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_MBUTTONUP:
        imp->source.emit(window_mouse_button_up_event{mouse_button::middle,   {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;

    case WM_SETCURSOR:
        if (!imp->cursor_visible && LOWORD(lp) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_SETFOCUS:
        // TODO: update window::current()
        break;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Factory ───────────────────────────────────────────────────────────

static std::unique_ptr<window_impl> create_win32_window(vec2i size, window_options opts) {
    if (!win32_window_impl::ensure_class_registered())
        return nullptr;

    auto imp = std::make_unique<win32_window_impl>();
    imp->client_size = size;
    imp->title_utf8  = opts.title.data();

    DWORD style    = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = 0;

    if (!opts.resizable)
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    if (opts.borderless)
        style = WS_POPUP;

    // Calculate adjusted window rect so the client area matches requested size
    RECT wr = {0, 0, size.x, size.y};
    AdjustWindowRectEx(&wr, style, FALSE, ex_style);
    int win_w = wr.right  - wr.left;
    int win_h = wr.bottom - wr.top;

    int pos_x = CW_USEDEFAULT, pos_y = CW_USEDEFAULT;
    if (auto* pt = std::get_if<vec2i>(&opts.position)) {
        pos_x = pt->x;
        pos_y = pt->y;
    } else {
        // Center on primary monitor
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        pos_x = (sw - win_w) / 2;
        pos_y = (sh - win_h) / 2;
    }

    std::wstring wtitle = utf8_to_wide(imp->title_utf8);

    // Pass imp.get() as lpParam; WM_NCCREATE sets GWLP_USERDATA
    HWND hwnd = CreateWindowExW(
        ex_style, k_wnd_class, wtitle.c_str(), style,
        pos_x, pos_y, win_w, win_h,
        nullptr, nullptr, GetModuleHandleW(nullptr),
        imp.get());

    if (!hwnd)
        return nullptr;

    imp->hwnd = hwnd;
    ShowWindow(hwnd, opts.minimized ? SW_MINIMIZE
                   : opts.maximized ? SW_MAXIMIZE
                                    : SW_SHOW);
    UpdateWindow(hwnd);

    return imp;
}

// ── Registration ──────────────────────────────────────────────────────

void register_win32_window_backend() {
    register_window_backend({"win32", create_win32_window});
}

} // namespace alia

#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
