#ifdef ALIA_COMPILE_PLATFORM_BACKEND_WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "win32_ops.hpp"
#include "win32_ops.hpp"
#include "../window.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace alia {
    namespace {
        struct native_monitor {
            HMONITOR handle = nullptr;
            MONITORINFOEXW info = {};
        };

        BOOL CALLBACK collect_monitor(HMONITOR handle, HDC, LPRECT, LPARAM data) {
            native_monitor monitor;
            monitor.handle = handle;
            monitor.info.cbSize = sizeof(monitor.info);
            if (GetMonitorInfoW(handle, reinterpret_cast<MONITORINFO *>(&monitor.info)))
                reinterpret_cast<std::vector<native_monitor> *>(data)->push_back(monitor);
            return TRUE;
        }

        std::vector<native_monitor> enumerate_native_monitors() {
            std::vector<native_monitor> result;
            EnumDisplayMonitors(nullptr, nullptr, collect_monitor, reinterpret_cast<LPARAM>(&result));
            std::stable_sort(result.begin(), result.end(), [](const native_monitor &a, const native_monitor &b) {
                const bool a_primary = (a.info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                const bool b_primary = (b.info.dwFlags & MONITORINFOF_PRIMARY) != 0;
                return std::tuple(!a_primary, a.info.rcMonitor.left, a.info.rcMonitor.top) <
                       std::tuple(!b_primary, b.info.rcMonitor.left, b.info.rcMonitor.top);
            });
            return result;
        }

        const native_monitor &checked_monitor(const std::vector<native_monitor> &monitors, int index) {
            if (index < 0 || index >= static_cast<int>(monitors.size()))
                throw std::out_of_range("monitor index is out of range");
            return monitors[static_cast<std::size_t>(index)];
        }

        display_mode mode_from_devmode(const DEVMODEW &mode) {
            return {
                {static_cast<int>(mode.dmPelsWidth), static_cast<int>(mode.dmPelsHeight)},
                static_cast<int>(mode.dmDisplayFrequency),
                static_cast<int>(mode.dmBitsPerPel)
            };
        }

        vec2i physical_size_mm(const wchar_t *device) {
            HDC dc = CreateDCW(L"DISPLAY", device, nullptr, nullptr);
            if (!dc)
                return {};
            const vec2i result{GetDeviceCaps(dc, HORZSIZE), GetDeviceCaps(dc, VERTSIZE)};
            DeleteDC(dc);
            return result;
        }

        float monitor_dpi(HMONITOR handle, const wchar_t *device) {
            using get_dpi_for_monitor_fn = HRESULT(WINAPI *)(HMONITOR, int, UINT *, UINT *);
            if (HMODULE shcore = LoadLibraryW(L"Shcore.dll")) {
                const auto get_dpi =
                    reinterpret_cast<get_dpi_for_monitor_fn>(GetProcAddress(shcore, "GetDpiForMonitor"));
                UINT x = 0, y = 0;
                const bool ok = get_dpi && SUCCEEDED(get_dpi(handle, 0, &x, &y));
                FreeLibrary(shcore);
                if (ok)
                    return static_cast<float>(x);
            }
            HDC dc = CreateDCW(L"DISPLAY", device, nullptr, nullptr);
            if (!dc)
                return 96.0f;
            const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
            DeleteDC(dc);
            return dpi > 0 ? static_cast<float>(dpi) : 96.0f;
        }

        std::string utf8_from_wide(const wchar_t *text) {
            if (!text || !*text)
                return {};
            const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
            if (length <= 1)
                return {};
            std::string result(static_cast<std::size_t>(length), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
            result.pop_back();
            return result;
        }
    } // namespace

    namespace detail {
        void *win32_monitor_handle(int index) {
            const auto monitors = enumerate_native_monitors();
            return static_cast<void *>(checked_monitor(monitors, index).handle);
        }

        int win32_monitor_index(void *hmonitor) {
            const auto monitors = enumerate_native_monitors();
            for (std::size_t i = 0; i < monitors.size(); ++i) {
                if (monitors[i].handle == static_cast<HMONITOR>(hmonitor))
                    return static_cast<int>(i);
            }
            return -1;
        }

        std::wstring win32_monitor_device_name(int index) {
            const auto monitors = enumerate_native_monitors();
            return checked_monitor(monitors, index).info.szDevice;
        }
    } // namespace detail

    std::vector<display_mode> win32_display_modes(os_context_handle *, int monitor_index) {
        const std::wstring device = detail::win32_monitor_device_name(monitor_index);
        std::vector<display_mode> modes;
        DEVMODEW native = {};
        native.dmSize = sizeof(native);
        for (DWORD i = 0; EnumDisplaySettingsExW(device.c_str(), i, &native, 0); ++i) {
            modes.push_back(mode_from_devmode(native));
            native = {};
            native.dmSize = sizeof(native);
        }
        std::sort(modes.begin(), modes.end(), [](const display_mode &a, const display_mode &b) {
            return std::tuple(a.size.x, a.size.y, a.refresh_rate, a.color_depth) <
                   std::tuple(b.size.x, b.size.y, b.refresh_rate, b.color_depth);
        });
        modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
        return modes;
    }

    display_mode win32_desktop_mode(os_context_handle *, int monitor_index) {
        const std::wstring device = detail::win32_monitor_device_name(monitor_index);
        DEVMODEW native = {};
        native.dmSize = sizeof(native);
        if (!EnumDisplaySettingsExW(device.c_str(), ENUM_CURRENT_SETTINGS, &native, 0))
            throw std::runtime_error("get_desktop_mode: EnumDisplaySettingsExW failed");
        return mode_from_devmode(native);
    }

    std::vector<monitor_info> win32_monitors(os_context_handle *) {
        const auto native = enumerate_native_monitors();
        std::vector<monitor_info> result;
        result.reserve(native.size());
        for (std::size_t i = 0; i < native.size(); ++i) {
            const auto &entry = native[i];
            DISPLAY_DEVICEW display = {};
            display.cb = sizeof(display);
            const bool have_name = EnumDisplayDevicesW(entry.info.szDevice, 0, &display, 0) == TRUE;
            const float dpi = monitor_dpi(entry.handle, entry.info.szDevice);
            monitor_info info;
            info.index = static_cast<int>(i);
            info.name = utf8_from_wide(have_name ? display.DeviceString : entry.info.szDevice);
            info.position = {entry.info.rcMonitor.left, entry.info.rcMonitor.top};
            info.size = {
                entry.info.rcMonitor.right - entry.info.rcMonitor.left,
                entry.info.rcMonitor.bottom - entry.info.rcMonitor.top
            };
            info.size_mm = physical_size_mm(entry.info.szDevice);
            info.dpi = dpi;
            info.scale_factor = dpi / 96.0f;
            info.is_primary = (entry.info.dwFlags & MONITORINFOF_PRIMARY) != 0;
            info.current_mode = win32_desktop_mode(nullptr, info.index);
            info.modes = win32_display_modes(nullptr, info.index);
            result.push_back(std::move(info));
        }
        return result;
    }

} // namespace alia

#endif // ALIA_COMPILE_PLATFORM_BACKEND_WIN32
