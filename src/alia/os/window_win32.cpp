#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include "window.hpp"
#include "display.hpp"
#include "monitor_win32.hpp"
#include "window_events.hpp"
#include "../events/event_source_impl.hpp"
#include "../gfx/bitmap/bitmap.hpp"
#include "../io/keycodes.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace alia {
    namespace {
        key vk_to_key(WPARAM vk, LPARAM lp) {
            const bool extended = ((lp >> 24) & 1) != 0;
            switch (vk) {
            case VK_BACK: return key::backspace;
            case VK_TAB: return key::tab;
            case VK_RETURN: return extended ? key::numpad_enter : key::enter;
            case VK_ESCAPE: return key::escape;
            case VK_SPACE: return key::space;
            case VK_PRIOR: return key::page_up;
            case VK_NEXT: return key::page_down;
            case VK_END: return key::end;
            case VK_HOME: return key::home;
            case VK_LEFT: return key::left;
            case VK_UP: return key::up;
            case VK_RIGHT: return key::right;
            case VK_DOWN: return key::down;
            case VK_INSERT: return key::insert;
            case VK_DELETE: return key::del;
            case VK_LWIN: return key::lsuper;
            case VK_RWIN: return key::rsuper;
            case VK_APPS: return key::menu;
            case VK_MULTIPLY: return key::numpad_multiply;
            case VK_ADD: return key::numpad_add;
            case VK_SUBTRACT: return key::numpad_subtract;
            case VK_DECIMAL: return key::numpad_decimal;
            case VK_DIVIDE: return key::numpad_divide;
            case VK_NUMLOCK: return key::num_lock;
            case VK_SCROLL: return key::scroll_lock;
            case VK_LSHIFT: return key::lshift;
            case VK_RSHIFT: return key::rshift;
            case VK_LCONTROL: return key::lctrl;
            case VK_RCONTROL: return key::rctrl;
            case VK_LMENU: return key::lalt;
            case VK_RMENU: return key::ralt;
            case VK_CAPITAL: return key::caps_lock;
            case VK_SNAPSHOT: return key::print_screen;
            case VK_PAUSE: return key::pause;
            case VK_OEM_MINUS: return key::minus;
            case VK_OEM_PLUS: return key::equals;
            case VK_OEM_4: return key::left_bracket;
            case VK_OEM_6: return key::right_bracket;
            case VK_OEM_5: return key::backslash;
            case VK_OEM_1: return key::semicolon;
            case VK_OEM_7: return key::apostrophe;
            case VK_OEM_3: return key::grave;
            case VK_OEM_COMMA: return key::comma;
            case VK_OEM_PERIOD: return key::period;
            case VK_OEM_2: return key::slash;
            default:
                if (vk >= '0' && vk <= '9')
                    return static_cast<key>(static_cast<int>(key::num0) + static_cast<int>(vk - '0'));
                if (vk >= 'A' && vk <= 'Z')
                    return static_cast<key>(static_cast<int>(key::A) + static_cast<int>(vk - 'A'));
                if (vk >= VK_F1 && vk <= VK_F12)
                    return static_cast<key>(static_cast<int>(key::f1) + static_cast<int>(vk - VK_F1));
                if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
                    return static_cast<key>(static_cast<int>(key::numpad0) + static_cast<int>(vk - VK_NUMPAD0));
                return key::unknown;
            }
        }

        key_mod get_modifiers() {
            key_mod modifiers = key_mod::none;
            if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= key_mod::shift;
            if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= key_mod::ctrl;
            if (GetKeyState(VK_MENU) & 0x8000) modifiers |= key_mod::alt;
            if ((GetKeyState(VK_LWIN) & 0x8000) || (GetKeyState(VK_RWIN) & 0x8000))
                modifiers |= key_mod::super;
            if (GetKeyState(VK_CAPITAL) & 0x0001) modifiers |= key_mod::caps_lock;
            if (GetKeyState(VK_NUMLOCK) & 0x0001) modifiers |= key_mod::num_lock;
            return modifiers;
        }

        std::wstring utf8_to_wide(std::string_view text) {
            if (text.empty()) return {};
            const int length = MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
            std::wstring result(static_cast<std::size_t>(length), L'\0');
            MultiByteToWideChar(
                CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                result.data(), length);
            return result;
        }

        bool constraints_present(window_size_constraints value) {
            return value.min.x > 0 || value.min.y > 0 ||
                   value.max.x > 0 || value.max.y > 0;
        }

        RECT monitor_rect(int index) {
            MONITORINFO info{sizeof(info)};
            if (!GetMonitorInfoW(
                    static_cast<HMONITOR>(detail::win32_monitor_handle(index)), &info))
                throw std::runtime_error("window: GetMonitorInfoW failed");
            return info.rcMonitor;
        }

        HICON create_icon(const any_bitmap_view &source) {
            if (source.width() <= 0 || source.height() <= 0)
                return nullptr;
            bitmap converted(source.size(), px_bgra8888{});
            converting_blit(converted.view(), source);
            HBITMAP color = CreateBitmap(
                source.width(), source.height(), 1, 32, converted.line(0));
            const int mask_stride = ((source.width() + 15) / 16) * 2;
            const std::vector<unsigned char> mask_bits(
                static_cast<std::size_t>(mask_stride * source.height()), 0);
            HBITMAP mask = CreateBitmap(
                source.width(), source.height(), 1, 1, mask_bits.data());
            if (!color || !mask) {
                if (color) DeleteObject(color);
                if (mask) DeleteObject(mask);
                return nullptr;
            }
            ICONINFO info{};
            info.fIcon = TRUE;
            info.hbmColor = color;
            info.hbmMask = mask;
            HICON result = CreateIconIndirect(&info);
            DeleteObject(color);
            DeleteObject(mask);
            return result;
        }

        const any_bitmap_view *closest_icon(
            std::span<const any_bitmap_view> icons, int width, int height) {
            if (icons.empty()) return nullptr;
            return &*std::min_element(icons.begin(), icons.end(), [&](const auto &a, const auto &b) {
                const int a_delta = std::abs(a.width() - width) + std::abs(a.height() - height);
                const int b_delta = std::abs(b.width() - width) + std::abs(b.height() - height);
                return a_delta < b_delta;
            });
        }

        constexpr const wchar_t *window_class_name = L"AliaWindow_v1";
    }

    class win32_window_impl : public window_impl {
    public:
        ~win32_window_impl() override {
            restore_display_mode();
            if (hwnd) DestroyWindow(hwnd);
            if (big_icon_) DestroyIcon(big_icon_);
            if (small_icon_) DestroyIcon(small_icon_);
        }

        HWND hwnd = nullptr;
        event_source source;
        vec2i client_size = {};
        std::string title_utf8;
        bool cursor_visible = true;
        bool mouse_grabbed = false;
        key last_key_down = key::unknown;
        WCHAR high_surrogate = 0;

        vec2i size() const override { return client_size; }
        float aspect_ratio() const override {
            return client_size.y
                ? static_cast<float>(client_size.x) / client_size.y : 1.0f;
        }
        cstring_view title() const override { return cstring_view(title_utf8.c_str()); }
        vec2i position() const override {
            if (!hwnd) return {};
            POINT point{0, 0};
            ClientToScreen(hwnd, &point);
            return {point.x, point.y};
        }
        bool is_focused() const override { return hwnd && GetForegroundWindow() == hwnd; }
        bool is_minimized() const override { return hwnd && IsIconic(hwnd); }
        bool is_maximized() const override { return hwnd && IsZoomed(hwnd); }
        window_fullscreen_mode fullscreen_mode() const override { return mode_; }
        bool is_resizable() const override { return resizable_; }
        bool is_borderless() const override { return borderless_; }
        bool generates_expose_events() const override { return expose_; }
        int refresh_rate() const override {
            if (mode_ == window_fullscreen_mode::fullscreen)
                return active_mode_.refresh_rate;
            try { return get_desktop_mode(monitor()).refresh_rate; }
            catch (...) { return 0; }
        }
        int monitor() const override {
            if (!hwnd)
                return monitor_opt_ >= 0 ? monitor_opt_ : 0;
            const int index = detail::win32_monitor_index(
                MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY));
            return index >= 0 ? index : 0;
        }
        window_size_constraints size_constraints() const override { return constraints_; }

        void set_title(cstring_view value) override {
            title_utf8 = value.data();
            if (hwnd) SetWindowTextW(hwnd, utf8_to_wide(title_utf8).c_str());
        }
        void set_position(vec2i value) override {
            if (hwnd) SetWindowPos(
                hwnd, nullptr, value.x, value.y, 0, 0,
                SWP_NOSIZE | SWP_NOZORDER);
        }
        void resize(vec2i value) override {
            if (!hwnd) return;
            RECT rect{0, 0, value.x, value.y};
            AdjustWindowRectEx(
                &rect, static_cast<DWORD>(GetWindowLongW(hwnd, GWL_STYLE)),
                FALSE, static_cast<DWORD>(GetWindowLongW(hwnd, GWL_EXSTYLE)));
            SetWindowPos(
                hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top,
                SWP_NOMOVE | SWP_NOZORDER);
        }
        void minimize() override { if (hwnd) ShowWindow(hwnd, SW_MINIMIZE); }
        void maximize() override { if (hwnd) ShowWindow(hwnd, SW_MAXIMIZE); }
        void restore() override { if (hwnd) ShowWindow(hwnd, SW_RESTORE); }

        void set_fullscreen_mode(
            window_fullscreen_mode requested,
            const display_mode *exclusive_mode) override {
            if (!hwnd) return;
            if (requested == mode_ &&
                (requested != window_fullscreen_mode::fullscreen ||
                 !exclusive_mode || *exclusive_mode == active_mode_))
                return;

            const window_fullscreen_mode previous = mode_;
            if (previous == window_fullscreen_mode::windowed &&
                requested != window_fullscreen_mode::windowed) {
                saved_placement_.length = sizeof(saved_placement_);
                GetWindowPlacement(hwnd, &saved_placement_);
                windowed_client_size_ = client_size;
            }

            if (previous == window_fullscreen_mode::fullscreen)
                restore_display_mode();
            mode_ = window_fullscreen_mode::windowed;
            mode_suspended_ = false;
            if (previous != window_fullscreen_mode::windowed)
                restore_windowed_placement();
            if (requested == window_fullscreen_mode::windowed)
                return;

            const int target_monitor = monitor_opt_ >= 0 ? monitor_opt_ : monitor();
            if (requested == window_fullscreen_mode::fullscreen) {
                const display_mode selected = exclusive_mode
                    ? *exclusive_mode
                    : find_closest_mode(
                        target_monitor,
                        windowed_client_size_.x > 0 ? windowed_client_size_ : client_size,
                        refresh_opt_);
                try {
                    apply_display_mode(target_monitor, selected);
                } catch (...) {
                    restore_windowed_placement();
                    throw;
                }
            }

            mode_ = requested;
            SetWindowLongW(hwnd, GWL_STYLE, static_cast<LONG>(style_with_runtime(WS_POPUP)));
            const RECT bounds = monitor_rect(target_monitor);
            SetWindowPos(
                hwnd, HWND_TOP, bounds.left, bounds.top,
                bounds.right - bounds.left, bounds.bottom - bounds.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }

        void set_resizable(bool value) override {
            if (resizable_ == value) return;
            resizable_ = value;
            if (mode_ == window_fullscreen_mode::windowed)
                apply_windowed_style_preserving_client();
        }
        void set_borderless(bool value) override {
            if (borderless_ == value) return;
            borderless_ = value;
            if (mode_ == window_fullscreen_mode::windowed)
                apply_windowed_style_preserving_client();
        }
        void set_size_constraints(window_size_constraints value) override {
            constraints_ = value;
            if (constraints_enabled_)
                apply_size_constraints(true);
        }
        void apply_size_constraints(bool enable) override {
            constraints_enabled_ = enable && constraints_present(constraints_);
            if (!constraints_enabled_ || mode_ != window_fullscreen_mode::windowed)
                return;
            vec2i clamped = client_size;
            if (constraints_.min.x > 0) clamped.x = (std::max)(clamped.x, constraints_.min.x);
            if (constraints_.min.y > 0) clamped.y = (std::max)(clamped.y, constraints_.min.y);
            if (constraints_.max.x > 0) clamped.x = (std::min)(clamped.x, constraints_.max.x);
            if (constraints_.max.y > 0) clamped.y = (std::min)(clamped.y, constraints_.max.y);
            if (clamped != client_size)
                resize(clamped);
        }
        void set_icons(std::span<const any_bitmap_view> icons) override {
            HICON next_big = nullptr;
            HICON next_small = nullptr;
            if (const auto *view = closest_icon(
                    icons, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON)))
                next_big = create_icon(*view);
            if (const auto *view = closest_icon(
                    icons, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON)))
                next_small = create_icon(*view);
            if (hwnd) {
                SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(next_big));
                SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(next_small));
            }
            if (big_icon_) DestroyIcon(big_icon_);
            if (small_icon_) DestroyIcon(small_icon_);
            big_icon_ = next_big;
            small_icon_ = next_small;
        }

        void show_cursor(bool show) override {
            cursor_visible = show;
            if (hwnd && is_focused())
                SetCursor(show ? LoadCursor(nullptr, IDC_ARROW) : nullptr);
        }
        void set_cursor(cursor value) override {
            if (!hwnd) return;
            LPCTSTR id = IDC_ARROW;
            switch (value) {
            case cursor::ibeam: id = IDC_IBEAM; break;
            case cursor::wait: id = IDC_WAIT; break;
            case cursor::crosshair: id = IDC_CROSS; break;
            case cursor::resize_we: id = IDC_SIZEWE; break;
            case cursor::resize_ns: id = IDC_SIZENS; break;
            case cursor::resize_nwse: id = IDC_SIZENWSE; break;
            case cursor::resize_nesw: id = IDC_SIZENESW; break;
            case cursor::resize_all: id = IDC_SIZEALL; break;
            case cursor::hand: id = IDC_HAND; break;
            case cursor::not_allowed: id = IDC_NO; break;
            case cursor::hidden: cursor_visible = false; SetCursor(nullptr); return;
            default: break;
            }
            cursor_visible = true;
            SetCursor(LoadCursor(nullptr, id));
        }
        void set_cursor_position(vec2i value) override {
            if (!hwnd) return;
            POINT point{value.x, value.y};
            ClientToScreen(hwnd, &point);
            SetCursorPos(point.x, point.y);
        }
        bool is_cursor_visible() const override { return cursor_visible; }
        void set_mouse_grab(bool grab) override {
            if (!hwnd) return;
            mouse_grabbed = grab;
            if (grab) SetCapture(hwnd); else ReleaseCapture();
        }
        bool is_mouse_grabbed() const override { return mouse_grabbed; }
        event_source &get_event_source() override { return source; }
        void poll() override {
            MSG message;
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        void *native_handle() const override { return static_cast<void *>(hwnd); }

        static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
        static bool ensure_class_registered();

        DWORD windowed_style() const {
            if (borderless_) return WS_POPUP;
            DWORD style = WS_OVERLAPPEDWINDOW;
            if (!resizable_)
                style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
            return style;
        }

        DWORD style_with_runtime(DWORD base) const {
            if (!hwnd) return base;
            constexpr DWORD preserved =
                WS_VISIBLE | WS_DISABLED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
            return base | (static_cast<DWORD>(GetWindowLongW(hwnd, GWL_STYLE)) & preserved);
        }

        void apply_windowed_style_preserving_client() {
            if (!hwnd) return;
            POINT client_origin{0, 0};
            ClientToScreen(hwnd, &client_origin);
            RECT adjusted{0, 0, client_size.x, client_size.y};
            const DWORD style = windowed_style();
            const DWORD ex_style = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_EXSTYLE));
            AdjustWindowRectEx(&adjusted, style, FALSE, ex_style);
            SetWindowLongW(hwnd, GWL_STYLE, static_cast<LONG>(style_with_runtime(style)));
            SetWindowPos(
                hwnd, nullptr,
                client_origin.x + adjusted.left,
                client_origin.y + adjusted.top,
                adjusted.right - adjusted.left,
                adjusted.bottom - adjusted.top,
                SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }

        void restore_windowed_placement() {
            if (!hwnd) return;
            SetWindowLongW(
                hwnd, GWL_STYLE,
                static_cast<LONG>(style_with_runtime(windowed_style())));
            saved_placement_.length = sizeof(saved_placement_);
            SetWindowPlacement(hwnd, &saved_placement_);
            SetWindowPos(
                hwnd, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }

        void apply_display_mode(int monitor_index, const display_mode &mode) {
            mode_device_ = detail::win32_monitor_device_name(monitor_index);
            DEVMODEW native{};
            native.dmSize = sizeof(native);
            native.dmPelsWidth = static_cast<DWORD>(mode.size.x);
            native.dmPelsHeight = static_cast<DWORD>(mode.size.y);
            native.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
            if (mode.color_depth > 0) {
                native.dmBitsPerPel = static_cast<DWORD>(mode.color_depth);
                native.dmFields |= DM_BITSPERPEL;
            }
            if (mode.refresh_rate > 0) {
                native.dmDisplayFrequency = static_cast<DWORD>(mode.refresh_rate);
                native.dmFields |= DM_DISPLAYFREQUENCY;
            }
            const LONG result = ChangeDisplaySettingsExW(
                mode_device_.c_str(), &native, nullptr, CDS_FULLSCREEN, nullptr);
            if (result != DISP_CHANGE_SUCCESSFUL)
                throw std::runtime_error(
                    "window::set_fullscreen: ChangeDisplaySettingsExW failed (" +
                    std::to_string(result) + ")");
            active_mode_ = mode;
            mode_applied_ = true;
        }

        void restore_display_mode() noexcept {
            if (!mode_applied_) return;
            ChangeDisplaySettingsExW(
                mode_device_.c_str(), nullptr, nullptr, 0, nullptr);
            mode_applied_ = false;
        }

        bool reapply_display_mode() noexcept {
            try {
                const int target_monitor = monitor_opt_ >= 0 ? monitor_opt_ : monitor();
                apply_display_mode(target_monitor, active_mode_);
                return true;
            } catch (...) {
                return false;
            }
        }

        window_fullscreen_mode mode_ = window_fullscreen_mode::windowed;
        bool resizable_ = false;
        bool borderless_ = false;
        bool expose_ = false;
        int monitor_opt_ = -1;
        int refresh_opt_ = 0;
        display_mode active_mode_{};
        std::wstring mode_device_;
        bool mode_applied_ = false;
        bool mode_suspended_ = false;
        bool activation_transition_ = false;
        WINDOWPLACEMENT saved_placement_{sizeof(WINDOWPLACEMENT)};
        vec2i windowed_client_size_{};
        window_size_constraints constraints_{};
        bool constraints_enabled_ = false;
        HICON big_icon_ = nullptr;
        HICON small_icon_ = nullptr;
    };

    bool win32_window_impl::ensure_class_registered() {
        static std::once_flag flag;
        static bool ok = false;
        std::call_once(flag, [] {
            WNDCLASSEXW wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = win32_window_impl::wnd_proc;
            wc.hInstance = GetModuleHandleW(nullptr);
            wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
            wc.lpszClassName = window_class_name;
            ok = RegisterClassExW(&wc) != 0;
        });
        return ok;
    }

    LRESULT CALLBACK win32_window_impl::wnd_proc(
        HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == WM_NCCREATE) {
            auto *create = reinterpret_cast<CREATESTRUCTW *>(lp);
            auto *impl = reinterpret_cast<win32_window_impl *>(create->lpCreateParams);
            impl->hwnd = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(impl));
            return DefWindowProcW(hwnd, msg, wp, lp);
        }
        auto *impl = reinterpret_cast<win32_window_impl *>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (!impl) return DefWindowProcW(hwnd, msg, wp, lp);

        switch (msg) {
        case WM_CLOSE:
            impl->source.emit(window_close_event{});
            return 0;
        case WM_DESTROY:
            impl->hwnd = nullptr;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            return 0;
        case WM_SIZE: {
            const int width = LOWORD(lp);
            const int height = HIWORD(lp);
            impl->client_size = {width, height};
            if (wp != SIZE_MINIMIZED)
                impl->source.emit(window_resize_event{{width, height}});
            return 0;
        }
        case WM_PAINT:
            if (impl->expose_) {
                RECT update{};
                if (GetUpdateRect(hwnd, &update, FALSE))
                    impl->source.emit(window_expose_event{{
                        {update.left, update.top}, {update.right, update.bottom}}});
                ValidateRect(hwnd, nullptr);
                return 0;
            }
            break;
        case WM_GETMINMAXINFO:
            if (impl->constraints_enabled_) {
                auto *info = reinterpret_cast<MINMAXINFO *>(lp);
                const DWORD style = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_STYLE));
                const DWORD ex_style = static_cast<DWORD>(GetWindowLongW(hwnd, GWL_EXSTYLE));
                if (impl->constraints_.min.x > 0 || impl->constraints_.min.y > 0) {
                    RECT rect{0, 0, impl->constraints_.min.x, impl->constraints_.min.y};
                    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
                    if (impl->constraints_.min.x > 0)
                        info->ptMinTrackSize.x = rect.right - rect.left;
                    if (impl->constraints_.min.y > 0)
                        info->ptMinTrackSize.y = rect.bottom - rect.top;
                }
                if (impl->constraints_.max.x > 0 || impl->constraints_.max.y > 0) {
                    RECT rect{0, 0, impl->constraints_.max.x, impl->constraints_.max.y};
                    AdjustWindowRectEx(&rect, style, FALSE, ex_style);
                    if (impl->constraints_.max.x > 0)
                        info->ptMaxTrackSize.x = rect.right - rect.left;
                    if (impl->constraints_.max.y > 0)
                        info->ptMaxTrackSize.y = rect.bottom - rect.top;
                }
                return 0;
            }
            break;
        case WM_ACTIVATE:
            if (!impl->activation_transition_ &&
                impl->mode_ == window_fullscreen_mode::fullscreen) {
                impl->activation_transition_ = true;
                if (LOWORD(wp) == WA_INACTIVE && impl->mode_applied_) {
                    impl->restore_display_mode();
                    impl->mode_suspended_ = true;
                    ShowWindow(hwnd, SW_MINIMIZE);
                } else if (LOWORD(wp) != WA_INACTIVE && impl->mode_suspended_) {
                    if (impl->reapply_display_mode()) {
                        impl->mode_suspended_ = false;
                        ShowWindow(hwnd, SW_RESTORE);
                    }
                }
                impl->activation_transition_ = false;
            }
            break;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            impl->last_key_down = vk_to_key(wp, lp);
            impl->source.emit(window_key_down_event{impl->last_key_down});
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            impl->source.emit(window_key_up_event{vk_to_key(wp, lp)});
            return 0;
        case WM_CHAR:
        case WM_SYSCHAR: {
            const WCHAR value = static_cast<WCHAR>(wp);
            const bool repeat = ((lp >> 30) & 1) != 0;
            if (value >= 0xD800 && value <= 0xDBFF) {
                impl->high_surrogate = value;
                return 0;
            }
            char32_t codepoint = value;
            if (value >= 0xDC00 && value <= 0xDFFF && impl->high_surrogate)
                codepoint = 0x10000u +
                    ((impl->high_surrogate - 0xD800u) << 10) +
                    (value - 0xDC00u);
            impl->high_surrogate = 0;
            impl->source.emit(window_key_char_event{
                impl->last_key_down, codepoint, get_modifiers(), repeat});
            return 0;
        }
        case WM_MOUSEMOVE:
            impl->source.emit(window_mouse_move_event{{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_LBUTTONDOWN:
            impl->source.emit(window_mouse_button_down_event{
                mouse_button::left, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_LBUTTONUP:
            impl->source.emit(window_mouse_button_up_event{
                mouse_button::left, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_RBUTTONDOWN:
            impl->source.emit(window_mouse_button_down_event{
                mouse_button::right, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_RBUTTONUP:
            impl->source.emit(window_mouse_button_up_event{
                mouse_button::right, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_MBUTTONDOWN:
            impl->source.emit(window_mouse_button_down_event{
                mouse_button::middle, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_MBUTTONUP:
            impl->source.emit(window_mouse_button_up_event{
                mouse_button::middle, {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)}});
            return 0;
        case WM_SETCURSOR:
            if (!impl->cursor_visible && LOWORD(lp) == HTCLIENT) {
                SetCursor(nullptr);
                return TRUE;
            }
            break;
        case WM_SETFOCUS:
            // TODO: update window::current().
            break;
        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    std::unique_ptr<window_impl> create_win32_window(
        vec2i size, window_options options) {
        if (!win32_window_impl::ensure_class_registered())
            return nullptr;

        auto impl = std::make_unique<win32_window_impl>();
        impl->client_size = size;
        impl->windowed_client_size_ = size;
        impl->title_utf8 = options.title.data();
        impl->resizable_ = options.resizable;
        impl->borderless_ = options.borderless;
        impl->expose_ = options.generate_expose_events;
        impl->monitor_opt_ = options.monitor;
        impl->refresh_opt_ = options.refresh_rate;
        impl->constraints_ = options.size_constraints;
        impl->constraints_enabled_ = constraints_present(options.size_constraints);

        const int target_monitor = options.monitor >= 0 ? options.monitor : 0;
        RECT bounds{};
        try {
            bounds = monitor_rect(target_monitor);
        } catch (...) {
            return nullptr;
        }

        DWORD style = impl->windowed_style();
        DWORD ex_style = 0;
        RECT adjusted{0, 0, size.x, size.y};
        AdjustWindowRectEx(&adjusted, style, FALSE, ex_style);
        const int window_width = adjusted.right - adjusted.left;
        const int window_height = adjusted.bottom - adjusted.top;
        int position_x = bounds.left +
            ((bounds.right - bounds.left) - window_width) / 2;
        int position_y = bounds.top +
            ((bounds.bottom - bounds.top) - window_height) / 2;
        if (const auto *point = std::get_if<vec2i>(&options.position)) {
            position_x = point->x;
            position_y = point->y;
        }

        impl->saved_placement_.length = sizeof(impl->saved_placement_);
        impl->saved_placement_.showCmd = SW_SHOWNORMAL;
        impl->saved_placement_.rcNormalPosition = {
            position_x, position_y,
            position_x + window_width, position_y + window_height};

        if (options.mode == window_fullscreen_mode::fullscreen) {
            try {
                const auto mode = find_closest_mode(
                    target_monitor, size, options.refresh_rate);
                impl->apply_display_mode(target_monitor, mode);
                impl->mode_ = window_fullscreen_mode::fullscreen;
                bounds = monitor_rect(target_monitor);
            } catch (...) {
                return nullptr;
            }
        } else if (options.mode == window_fullscreen_mode::fullscreen_window) {
            impl->mode_ = window_fullscreen_mode::fullscreen_window;
        }

        if (impl->mode_ != window_fullscreen_mode::windowed) {
            style = WS_POPUP;
            position_x = bounds.left;
            position_y = bounds.top;
        }
        const int create_width = impl->mode_ == window_fullscreen_mode::windowed
            ? window_width : bounds.right - bounds.left;
        const int create_height = impl->mode_ == window_fullscreen_mode::windowed
            ? window_height : bounds.bottom - bounds.top;
        const std::wstring title = utf8_to_wide(impl->title_utf8);
        HWND hwnd = CreateWindowExW(
            ex_style, window_class_name, title.c_str(), style,
            position_x, position_y, create_width, create_height,
            nullptr, nullptr, GetModuleHandleW(nullptr), impl.get());
        if (!hwnd)
            return nullptr;
        impl->hwnd = hwnd;

        const int show = impl->mode_ != window_fullscreen_mode::windowed
            ? SW_SHOW
            : options.minimized ? SW_MINIMIZE
            : options.maximized ? SW_MAXIMIZE : SW_SHOW;
        ShowWindow(hwnd, show);
        UpdateWindow(hwnd);
        if (impl->constraints_enabled_)
            impl->apply_size_constraints(true);
        if (options.grab_mouse)
            impl->set_mouse_grab(true);
        return impl;
    }

    void register_win32_window_backend() {
        register_window_backend({"win32", create_win32_window});
    }

    bool inhibit_screensaver(bool inhibit) {
        const EXECUTION_STATE state = SetThreadExecutionState(
            inhibit ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED : ES_CONTINUOUS);
        return state != 0;
    }
} // namespace alia

#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
