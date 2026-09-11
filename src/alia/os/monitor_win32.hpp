#ifndef ALIA_OS_MONITOR_WIN32_HPP
#define ALIA_OS_MONITOR_WIN32_HPP

#include <string>

namespace alia::detail {
    [[nodiscard]] void *win32_monitor_handle(int index);
    [[nodiscard]] int win32_monitor_index(void *hmonitor);
    [[nodiscard]] std::wstring win32_monitor_device_name(int index);
}

#endif // ALIA_OS_MONITOR_WIN32_HPP
