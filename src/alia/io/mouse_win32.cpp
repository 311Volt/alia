#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "mouse_impl.hpp"
#include "../events/event_source_impl.hpp"
#include "../gfx/bitmap/bitmap.hpp"
#include "../gfx/bitmap/pixel_types.hpp"
#include "../os/window_events.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace alia::detail {
namespace {

class win32_mouse_cursor final : public mouse_cursor_impl {
public:
    explicit win32_mouse_cursor(HCURSOR cursor) : cursor_(cursor) {}
    ~win32_mouse_cursor() override {
        if (cursor_)
            DestroyCursor(cursor_);
    }

    void *native_handle() const noexcept override {
        return reinterpret_cast<void *>(cursor_);
    }

private:
    HCURSOR cursor_ = nullptr;
};

std::uint8_t float_channel(float value) {
    return static_cast<std::uint8_t>(
        std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

void copy_cursor_pixels(bitmap &destination, const any_bitmap_view &source) {
    if (can_convert_pixel(source.format(), pixel_format::bgra8888)) {
        converting_blit(destination.view(), source);
        return;
    }

    auto output = destination.view_as<px_bgra8888>();
    switch (source.format()) {
    case pixel_format::rgb_f32:
        for (int y = 0; y < source.height(); ++y) {
            const auto *row = static_cast<const px_rgb_f32 *>(source.line(y));
            for (int x = 0; x < source.width(); ++x)
                output[x, y] = {
                    float_channel(row[x].b), float_channel(row[x].g),
                    float_channel(row[x].r), 255};
        }
        return;
    case pixel_format::rgba_f32:
        for (int y = 0; y < source.height(); ++y) {
            const auto *row = static_cast<const px_rgba_f32 *>(source.line(y));
            for (int x = 0; x < source.width(); ++x)
                output[x, y] = {
                    float_channel(row[x].b), float_channel(row[x].g),
                    float_channel(row[x].r), float_channel(row[x].a)};
        }
        return;
    case pixel_format::gray_f32:
        for (int y = 0; y < source.height(); ++y) {
            const auto *row = static_cast<const px_gray_f32 *>(source.line(y));
            for (int x = 0; x < source.width(); ++x) {
                const auto gray = float_channel(row[x].v);
                output[x, y] = {gray, gray, gray, 255};
            }
        }
        return;
    default:
        throw std::invalid_argument(
            "mouse_cursor: bitmap format cannot be converted to RGBA");
    }
}

HCURSOR create_cursor(const any_bitmap_view &source, vec2i hotspot) {
    if (source.width() <= 0 || source.height() <= 0)
        throw std::invalid_argument("mouse_cursor: bitmap must not be empty");
    if (!source.line(0))
        throw std::invalid_argument("mouse_cursor: bitmap has no pixel data");
    if (hotspot.x < 0 || hotspot.y < 0 ||
        hotspot.x >= source.width() || hotspot.y >= source.height())
        throw std::invalid_argument("mouse_cursor: hotspot is outside the bitmap");

    bitmap converted(source.size(), px_bgra8888{});
    copy_cursor_pixels(converted, source);
    auto converted_pixels = converted.view_as<px_bgra8888>();
    for (int y = 0; y < converted_pixels.height(); ++y) {
        for (int x = 0; x < converted_pixels.width(); ++x) {
            auto &pixel = converted_pixels[x, y];
            pixel.b = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.b) * pixel.a + 127u) / 255u);
            pixel.g = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.g) * pixel.a + 127u) / 255u);
            pixel.r = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.r) * pixel.a + 127u) / 255u);
        }
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = source.width();
    info.bmiHeader.biHeight = -source.height();
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void *dib_pixels = nullptr;
    HBITMAP color = CreateDIBSection(
        nullptr, &info, DIB_RGB_COLORS, &dib_pixels, nullptr, 0);
    if (!color || !dib_pixels) {
        if (color)
            DeleteObject(color);
        return nullptr;
    }

    const auto row_bytes = static_cast<std::size_t>(source.width()) * 4;
    for (int y = 0; y < source.height(); ++y)
        std::memcpy(
            static_cast<std::byte *>(dib_pixels) + row_bytes * y,
            converted.line(y), row_bytes);

    const int mask_stride = ((source.width() + 15) / 16) * 2;
    const std::vector<unsigned char> mask_bits(
        static_cast<std::size_t>(mask_stride * source.height()), 0);
    HBITMAP mask = CreateBitmap(
        source.width(), source.height(), 1, 1, mask_bits.data());
    if (!mask) {
        DeleteObject(color);
        return nullptr;
    }

    ICONINFO icon_info{};
    icon_info.fIcon = FALSE;
    icon_info.xHotspot = static_cast<DWORD>(hotspot.x);
    icon_info.yHotspot = static_cast<DWORD>(hotspot.y);
    icon_info.hbmMask = mask;
    icon_info.hbmColor = color;
    HCURSOR cursor = reinterpret_cast<HCURSOR>(CreateIconIndirect(&icon_info));
    DeleteObject(mask);
    DeleteObject(color);
    return cursor;
}

struct mouse_mode_coordinator {
    void *owner = nullptr;
    HWND hwnd = nullptr;
    event_source *source = nullptr;
    mouse_mode requested = mouse_mode::normal;
    mouse_mode active = mouse_mode::normal;
    bool raw_registered = false;
    DWORD ui_thread = 0;
    std::unordered_map<HANDLE, POINT> absolute_baselines;
};

mouse_mode_coordinator &coordinator() {
    static mouse_mode_coordinator value;
    return value;
}

bool on_ui_thread(const mouse_mode_coordinator &value) {
    return value.ui_thread == 0 || value.ui_thread == GetCurrentThreadId();
}

void refresh_native_cursor(HWND hwnd) {
    if (!hwnd)
        return;
    POINT point{};
    if (GetCursorPos(&point) && ScreenToClient(hwnd, &point)) {
        RECT client{};
        if (GetClientRect(hwnd, &client) && PtInRect(&client, point))
            SendMessageW(
                hwnd, WM_SETCURSOR, reinterpret_cast<WPARAM>(hwnd),
                MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    }
}

bool clip_to_client(HWND hwnd) {
    RECT bounds{};
    if (!hwnd || !GetClientRect(hwnd, &bounds))
        return false;
    POINT top_left{bounds.left, bounds.top};
    POINT bottom_right{bounds.right, bounds.bottom};
    if (!ClientToScreen(hwnd, &top_left) ||
        !ClientToScreen(hwnd, &bottom_right))
        return false;
    bounds = {top_left.x, top_left.y, bottom_right.x, bottom_right.y};
    return ClipCursor(&bounds) != FALSE;
}

bool has_conflicting_raw_mouse_registration() {
    UINT count = 0;
    if (GetRegisteredRawInputDevices(
            nullptr, &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1))
        return true;
    if (count == 0)
        return false;
    std::vector<RAWINPUTDEVICE> devices(count);
    UINT actual = count;
    if (GetRegisteredRawInputDevices(
            devices.data(), &actual, sizeof(RAWINPUTDEVICE)) ==
        static_cast<UINT>(-1))
        return true;
    for (UINT i = 0; i < actual; ++i) {
        if (devices[i].usUsagePage == 0x01 && devices[i].usUsage == 0x02)
            return true;
    }
    return false;
}

bool register_raw_mouse(HWND hwnd) {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = 0; // foreground-only input
    device.hwndTarget = hwnd;
    return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

bool unregister_raw_mouse() {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    return RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
}

bool unregister_raw_mouse_if_owned(HWND hwnd) {
    UINT count = 0;
    if (GetRegisteredRawInputDevices(
            nullptr, &count, sizeof(RAWINPUTDEVICE)) == static_cast<UINT>(-1))
        return false;
    if (count == 0)
        return true;
    std::vector<RAWINPUTDEVICE> devices(count);
    UINT actual = count;
    if (GetRegisteredRawInputDevices(
            devices.data(), &actual, sizeof(RAWINPUTDEVICE)) ==
        static_cast<UINT>(-1))
        return false;
    for (UINT i = 0; i < actual; ++i) {
        if (devices[i].usUsagePage == 0x01 && devices[i].usUsage == 0x02) {
            // Another component can replace a process registration after alia.
            // Never remove a target/flag combination that is no longer ours.
            if (devices[i].hwndTarget != hwnd || devices[i].dwFlags != 0)
                return true;
            return unregister_raw_mouse();
        }
    }
    return true;
}

void emit_mode_changed(mouse_mode_coordinator &value) {
    if (value.source)
        value.source->emit(mouse_mode_changed_event{
            value.requested, value.active});
}

bool cancel_owner(bool emit_event) {
    auto &value = coordinator();
    if (!value.owner)
        return true;
    const HWND old_hwnd = value.hwnd;
    const bool clip_ok = value.active == mouse_mode::normal ||
        ClipCursor(nullptr) != FALSE;
    bool raw_ok = true;
    if (value.raw_registered)
        raw_ok = unregister_raw_mouse_if_owned(value.hwnd);
    value.raw_registered = false;
    value.absolute_baselines.clear();
    value.requested = mouse_mode::normal;
    value.active = mouse_mode::normal;
    if (emit_event)
        emit_mode_changed(value);
    value.owner = nullptr;
    value.hwnd = nullptr;
    value.source = nullptr;
    value.ui_thread = 0;
    refresh_native_cursor(old_hwnd);
    return clip_ok && raw_ok;
}

void set_effective_mode(mouse_mode active) {
    auto &value = coordinator();
    if (value.active == active)
        return;
    value.active = active;
    value.absolute_baselines.clear();
    emit_mode_changed(value);
    refresh_native_cursor(value.hwnd);
}

POINT raw_absolute_position(const RAWMOUSE &mouse) {
    const bool virtual_desktop =
        (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
    const int origin_x = virtual_desktop
        ? GetSystemMetrics(SM_XVIRTUALSCREEN) : 0;
    const int origin_y = virtual_desktop
        ? GetSystemMetrics(SM_YVIRTUALSCREEN) : 0;
    const int width = GetSystemMetrics(
        virtual_desktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
    const int height = GetSystemMetrics(
        virtual_desktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
    return {
        origin_x + static_cast<LONG>(
            static_cast<std::int64_t>(mouse.lLastX) * (std::max)(0, width - 1) /
            65535),
        origin_y + static_cast<LONG>(
            static_cast<std::int64_t>(mouse.lLastY) * (std::max)(0, height - 1) /
            65535),
    };
}

} // namespace

std::shared_ptr<const mouse_cursor_impl> create_platform_mouse_cursor(
    const any_bitmap_view &bitmap, vec2i hotspot) {
    HCURSOR cursor = create_cursor(bitmap, hotspot);
    if (!cursor)
        throw std::runtime_error("mouse_cursor: CreateIconIndirect failed");
    return std::make_shared<win32_mouse_cursor>(cursor);
}

unsigned platform_mouse_num_axes() {
    return GetSystemMetrics(SM_MOUSEPRESENT) ? 4u : 0u;
}

unsigned platform_mouse_num_buttons() {
    if (!GetSystemMetrics(SM_MOUSEPRESENT))
        return 0;
    return static_cast<unsigned>((std::max)(0, GetSystemMetrics(SM_CMOUSEBUTTONS)));
}

bool platform_can_get_mouse_cursor_position() { return true; }

std::optional<vec2i> platform_get_mouse_cursor_position() {
    POINT point{};
    if (!GetCursorPos(&point))
        return std::nullopt;
    return vec2i{point.x, point.y};
}

bool platform_set_mouse_mode(
    void *owner_token, void *native_window, event_source &source,
    mouse_mode requested) {
    auto &value = coordinator();
    const auto hwnd = static_cast<HWND>(native_window);
    if (!owner_token || !hwnd ||
        GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId())
        return false;

    if (value.owner && !on_ui_thread(value))
        return false;

    if (requested == mouse_mode::normal) {
        if (value.owner != owner_token)
            return true;
        return cancel_owner(true);
    }

    if (value.owner == owner_token && value.requested == requested) {
        platform_refresh_mouse_bounds(owner_token);
        return true;
    }

    if (value.owner)
        cancel_owner(true);

    bool raw_registered = false;
    if (requested == mouse_mode::relative) {
        if (has_conflicting_raw_mouse_registration() || !register_raw_mouse(hwnd))
            return false;
        raw_registered = true;
    }

    const bool should_activate =
        GetForegroundWindow() == hwnd && !IsIconic(hwnd);
    if (should_activate && !clip_to_client(hwnd)) {
        if (raw_registered)
            unregister_raw_mouse_if_owned(hwnd);
        return false;
    }

    value.owner = owner_token;
    value.hwnd = hwnd;
    value.source = &source;
    value.requested = requested;
    value.active = should_activate ? requested : mouse_mode::normal;
    value.raw_registered = raw_registered;
    value.ui_thread = GetCurrentThreadId();
    value.absolute_baselines.clear();
    emit_mode_changed(value);
    refresh_native_cursor(hwnd);
    return true;
}

mouse_mode platform_requested_mouse_mode(void *owner_token) {
    const auto &value = coordinator();
    return value.owner == owner_token ? value.requested : mouse_mode::normal;
}

mouse_mode platform_active_mouse_mode(void *owner_token) {
    const auto &value = coordinator();
    return value.owner == owner_token ? value.active : mouse_mode::normal;
}

void platform_mouse_window_focus_changed(
    void *owner_token, bool focused, bool minimized) {
    auto &value = coordinator();
    if (value.owner != owner_token || !on_ui_thread(value))
        return;
    if (!focused || minimized) {
        if (value.active != mouse_mode::normal)
            ClipCursor(nullptr);
        set_effective_mode(mouse_mode::normal);
        return;
    }
    if (value.requested != mouse_mode::normal && clip_to_client(value.hwnd))
        set_effective_mode(value.requested);
}

void platform_refresh_mouse_bounds(void *owner_token) {
    auto &value = coordinator();
    if (value.owner != owner_token ||
        value.active == mouse_mode::normal || !on_ui_thread(value))
        return;
    if (!clip_to_client(value.hwnd)) {
        ClipCursor(nullptr);
        set_effective_mode(mouse_mode::normal);
    }
}

void platform_mouse_window_destroyed(void *owner_token) {
    auto &value = coordinator();
    if (value.owner == owner_token)
        cancel_owner(false);
}

bool platform_ungrab_mouse() {
    auto &value = coordinator();
    if (!value.owner)
        return true;
    if (!on_ui_thread(value))
        return false;
    return cancel_owner(true);
}

bool platform_mouse_handle_raw_input(void *owner_token, void *raw_input) {
    auto &value = coordinator();
    if (value.owner != owner_token ||
        value.active != mouse_mode::relative || !value.source)
        return false;

    UINT size = 0;
    const auto handle = static_cast<HRAWINPUT>(raw_input);
    if (GetRawInputData(
            handle, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) ==
        static_cast<UINT>(-1))
        return false;
    if (size < sizeof(RAWINPUTHEADER))
        return false;
    std::vector<std::uint64_t> storage((size + sizeof(std::uint64_t) - 1) /
                                       sizeof(std::uint64_t));
    if (GetRawInputData(
            handle, RID_INPUT, storage.data(), &size,
            sizeof(RAWINPUTHEADER)) != size)
        return false;
    const auto &input = *reinterpret_cast<const RAWINPUT *>(storage.data());
    if (input.header.dwType != RIM_TYPEMOUSE)
        return false;

    vec2i delta{};
    const RAWMOUSE &mouse = input.data.mouse;
    if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
        delta = {mouse.lLastX, mouse.lLastY};
    } else {
        const POINT position = raw_absolute_position(mouse);
        const auto existing = value.absolute_baselines.find(input.header.hDevice);
        if (existing == value.absolute_baselines.end()) {
            value.absolute_baselines.emplace(input.header.hDevice, position);
        } else {
            const POINT previous = existing->second;
            existing->second = position;
            delta = {position.x - previous.x, position.y - previous.y};
        }
    }
    mouse_handle_relative(*value.source, delta);
    return true;
}

} // namespace alia::detail

#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
