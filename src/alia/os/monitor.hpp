#ifndef ALIA_OS_MONITOR_HPP
#define ALIA_OS_MONITOR_HPP

#include "../core/vec.hpp"
#include "display.hpp"
#include <string>
#include <vector>

namespace alia {

class window;

// Monitor information
struct monitor_info {
    int index;
    std::string name;
    vec2i position;              // Position in virtual screen space
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

#endif // ALIA_OS_MONITOR_HPP
