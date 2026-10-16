#include "monitor.hpp"
#include "os_context.hpp"
#include <algorithm>

namespace alia {
    std::vector<monitor_info> get_monitors() {
        auto &c = get_os_context();
        return c.backend()->monitors.get_or_throw()(c.handle());
    }

    int get_monitor_count() {
        return static_cast<int>(get_monitors().size());
    }

    monitor_info get_monitor(int index) {
        auto monitors = get_monitors();
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            throw std::out_of_range("monitor index out of range");
        return std::move(monitors[static_cast<std::size_t>(index)]);
    }

    monitor_info get_primary_monitor() {
        auto monitors = get_monitors();
        for (auto &m : monitors)
            if (m.is_primary)
                return std::move(m);
        if (monitors.empty())
            throw std::runtime_error("no monitors available");
        return std::move(monitors.front());
    }

    monitor_info get_monitor_at(vec2i point) {
        auto monitors = get_monitors();
        for (auto &m : monitors) {
            if (point.x >= m.position.x && point.y >= m.position.y && point.x < m.position.x + m.size.x &&
                point.y < m.position.y + m.size.y)
                return std::move(m);
        }
        for (auto &m : monitors)
            if (m.is_primary)
                return std::move(m);
        if (monitors.empty())
            throw std::runtime_error("no monitors available");
        return std::move(monitors.front());
    }

    monitor_info get_window_monitor(const window &w) {
        const int index = w.monitor();
        auto monitors = w.backend()->monitors.get_or_throw()(w.context_handle());
        if (index < 0 || index >= static_cast<int>(monitors.size()))
            throw std::out_of_range("window monitor no longer available");
        return std::move(monitors[static_cast<std::size_t>(index)]);
    }
} // namespace alia
