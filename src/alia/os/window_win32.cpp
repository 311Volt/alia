
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM
#include <mutex>
#include <string>
#include <string_view>

#include "window.hpp"
#include "window_events.hpp"
#include "../events/event_source_impl.hpp"  // template instantiation for emit<>

namespace alia {

// ── UTF-8 ↔ UTF-16 helpers ────────────────────────────────────────────

static std::wstring utf8_to_wide(std::string_view s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}

static std::string wide_to_utf8(std::wstring_view s) {
    if (s.empty()) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                                   nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(),
                        out.data(), len, nullptr, nullptr);
    return out;
}

// ── Window class name ─────────────────────────────────────────────────

static constexpr const wchar_t* k_wnd_class = L"AliaWindow_v1";

// ── Window impl ───────────────────────────────────────────────────────

struct window::impl {
    HWND         hwnd           = nullptr;
    event_source source;
    vec2i        client_size    = {};
    std::string  title_utf8;
    window_flags flags          = window_flags::none;
    bool         cursor_visible = true;
    bool         mouse_grabbed  = false;

    // Saved placement for fullscreen toggle
    WINDOWPLACEMENT saved_placement = {sizeof(WINDOWPLACEMENT)};
    bool is_fs = false;

    static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static bool              ensure_class_registered();
};

// ── Current window (most recently focused) ────────────────────────────

static thread_local window* s_current_window = nullptr;

// ── Win32 window class registration ──────────────────────────────────

bool window::impl::ensure_class_registered() {
    static std::once_flag flag;
    static bool ok = false;
    std::call_once(flag, []() {
        WNDCLASSEXW wc   = {};
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = window::impl::wnd_proc;
        wc.hInstance     = GetModuleHandleW(nullptr);
        wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = k_wnd_class;
        ok = (RegisterClassExW(&wc) != 0);
    });
    return ok;
}

// ── Win32 message procedure ───────────────────────────────────────────

LRESULT CALLBACK window::impl::wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    // Attach impl pointer on creation
    if (msg == WM_NCCREATE) {
        auto* cs  = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* imp = reinterpret_cast<window::impl*>(cs->lpCreateParams);
        imp->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(imp));
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    auto* imp = reinterpret_cast<window::impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
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
        imp->source.emit(window_key_event{static_cast<int>(wp), true});
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        imp->source.emit(window_key_event{static_cast<int>(wp), false});
        return 0;

    case WM_MOUSEMOVE:
        imp->source.emit(window_mouse_move_event{{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;

    case WM_LBUTTONDOWN:
        imp->source.emit(window_mouse_button_event{0, true, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_LBUTTONUP:
        imp->source.emit(window_mouse_button_event{0, false, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_RBUTTONDOWN:
        imp->source.emit(window_mouse_button_event{1, true, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_RBUTTONUP:
        imp->source.emit(window_mouse_button_event{1, false, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_MBUTTONDOWN:
        imp->source.emit(window_mouse_button_event{2, true, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;
    case WM_MBUTTONUP:
        imp->source.emit(window_mouse_button_event{2, false, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
        return 0;

    case WM_SETCURSOR:
        if (!imp->cursor_visible && LOWORD(lp) == HTCLIENT) {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_SETFOCUS:
        s_current_window = nullptr;  // updated via the window* stored externally
        break;

    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ── Constructors ──────────────────────────────────────────────────────

window::window(vec2i size, cstring_view title, window_flags flags)
    : window(size, flags, window_options{.title = title})
{}

window::window(vec2i size, window_flags flags)
    : window(size, flags, window_options{})
{}

window::window(vec2i size, window_flags flags, const window_options& opts)
{
    impl::ensure_class_registered();

    auto imp = std::make_unique<impl>();
    imp->client_size = size;
    imp->flags       = flags;
    imp->title_utf8  = opts.title.data();

    DWORD style   = WS_OVERLAPPEDWINDOW;
    DWORD ex_style = 0;

    if (!any(flags & window_flags::resizable))
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    if (any(flags & window_flags::no_frame))
        style = WS_POPUP;

    // Calculate adjusted window rect so the client area matches requested size
    RECT wr = {0, 0, size.x, size.y};
    AdjustWindowRectEx(&wr, style, FALSE, ex_style);
    int win_w = wr.right  - wr.left;
    int win_h = wr.bottom - wr.top;

    int pos_x = CW_USEDEFAULT, pos_y = CW_USEDEFAULT;
    if (opts.position.x >= 0 && opts.position.y >= 0) {
        pos_x = opts.position.x;
        pos_y = opts.position.y;
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

    if (!hwnd) return;  // impl_ stays null → window is invalid

    imp->hwnd = hwnd;
    ShowWindow(hwnd, any(flags & window_flags::minimized) ? SW_MINIMIZE
                   : any(flags & window_flags::maximized) ? SW_MAXIMIZE
                                                          : SW_SHOW);
    UpdateWindow(hwnd);

    impl_ = std::move(imp);
}

// ── Move ──────────────────────────────────────────────────────────────

window::window(window&& o) noexcept : impl_(std::move(o.impl_)) {}

window& window::operator=(window&& o) noexcept {
    if (this != &o) {
        if (impl_ && impl_->hwnd) DestroyWindow(impl_->hwnd);
        impl_ = std::move(o.impl_);
    }
    return *this;
}

// ── Destructor ────────────────────────────────────────────────────────

window::~window() {
    if (impl_ && impl_->hwnd)
        DestroyWindow(impl_->hwnd);
}

// ── Queries ───────────────────────────────────────────────────────────

bool        window::valid()        const { return impl_ && impl_->hwnd != nullptr; }
int         window::width()        const { return impl_ ? impl_->client_size.x : 0; }
int         window::height()       const { return impl_ ? impl_->client_size.y : 0; }
vec2i       window::size()         const { return impl_ ? impl_->client_size : vec2i{}; }
float       window::aspect_ratio() const {
    auto s = size();
    return s.y ? static_cast<float>(s.x) / s.y : 1.0f;
}
window_flags window::flags()       const { return impl_ ? impl_->flags : window_flags::none; }

cstring_view window::title() const {
    static const char* empty = "";
    return impl_ ? cstring_view(impl_->title_utf8.c_str()) : cstring_view(empty);
}

vec2i window::position() const {
    if (!valid()) return {};
    POINT pt = {0, 0};
    ClientToScreen(impl_->hwnd, &pt);
    return {pt.x, pt.y};
}

bool window::is_focused()    const { return valid() && GetForegroundWindow() == impl_->hwnd; }
bool window::is_minimized()  const { return valid() && IsIconic(impl_->hwnd); }
bool window::is_maximized()  const { return valid() && IsZoomed(impl_->hwnd); }
bool window::is_fullscreen() const { return impl_ && impl_->is_fs; }

// ── Modification ──────────────────────────────────────────────────────

void window::set_title(cstring_view t) {
    if (!valid()) return;
    impl_->title_utf8 = t.data();
    SetWindowTextW(impl_->hwnd, utf8_to_wide(impl_->title_utf8).c_str());
}

void window::set_position(vec2i pos) {
    if (!valid()) return;
    SetWindowPos(impl_->hwnd, nullptr, pos.x, pos.y, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER);
}

void window::set_size(vec2i sz)  { resize(sz); }
void window::resize(vec2i sz) {
    if (!valid()) return;
    DWORD style    = (DWORD)GetWindowLongW(impl_->hwnd, GWL_STYLE);
    DWORD ex_style = (DWORD)GetWindowLongW(impl_->hwnd, GWL_EXSTYLE);
    RECT wr = {0, 0, sz.x, sz.y};
    AdjustWindowRectEx(&wr, style, FALSE, ex_style);
    SetWindowPos(impl_->hwnd, nullptr, 0, 0,
                 wr.right - wr.left, wr.bottom - wr.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

void window::minimize() { if (valid()) ShowWindow(impl_->hwnd, SW_MINIMIZE); }
void window::maximize() { if (valid()) ShowWindow(impl_->hwnd, SW_MAXIMIZE); }
void window::restore()  { if (valid()) ShowWindow(impl_->hwnd, SW_RESTORE); }

void window::set_fullscreen(bool fs) {
    if (!valid() || impl_->is_fs == fs) return;
    HWND hwnd = impl_->hwnd;

    if (fs) {
        GetWindowPlacement(hwnd, &impl_->saved_placement);
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
        SetWindowPlacement(hwnd, &impl_->saved_placement);
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    impl_->is_fs = fs;
}

void window::toggle_fullscreen()             { set_fullscreen(!is_fullscreen()); }
void window::set_fullscreen_window(bool e)   { set_fullscreen(e); }  // same for now

// ── Cursor ────────────────────────────────────────────────────────────

void window::show_cursor(bool show) {
    if (!valid()) return;
    impl_->cursor_visible = show;
    // Force WM_SETCURSOR re-evaluation
    if (is_focused()) SetCursor(show ? LoadCursor(nullptr, IDC_ARROW) : nullptr);
}
void window::hide_cursor() { show_cursor(false); }

void window::set_cursor(cursor c) {
    if (!valid()) return;
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
        case cursor::hidden:      impl_->cursor_visible = false; return;
        default:                  break;
    }
    impl_->cursor_visible = true;
    SetCursor(LoadCursor(nullptr, id));
}

void window::set_cursor_position(vec2i pos) {
    if (!valid()) return;
    POINT pt = {pos.x, pos.y};
    ClientToScreen(impl_->hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

bool window::is_cursor_visible() const { return impl_ && impl_->cursor_visible; }

// ── Mouse grab ────────────────────────────────────────────────────────

void window::set_mouse_grab(bool grab) {
    if (!valid()) return;
    impl_->mouse_grabbed = grab;
    if (grab) SetCapture(impl_->hwnd);
    else      ReleaseCapture();
}
bool window::is_mouse_grabbed() const { return impl_ && impl_->mouse_grabbed; }

// ── Events & native handle ────────────────────────────────────────────

event_source& window::get_event_source() { return impl_->source; }

void* window::native_handle() const {
    return impl_ ? static_cast<void*>(impl_->hwnd) : nullptr;
}

// ── Message pump ──────────────────────────────────────────────────────

void window::poll() {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

// ── Static helpers ────────────────────────────────────────────────────

window* window::current() { return s_current_window; }

} // namespace alia


