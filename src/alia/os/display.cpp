#include "display.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

namespace alia {
    namespace {
        struct indexed_mode {
            display_mode mode;
            std::size_t index = 0;
        };

        double aspect_difference(vec2i a, vec2i b) {
            if (a.y <= 0 || b.y <= 0)
                return 0.0;
            return std::abs(
                static_cast<double>(a.x) / a.y -
                static_cast<double>(b.x) / b.y);
        }
    }

    std::optional<display_mode> choose_closest_mode(
        std::span<const display_mode> modes,
        vec2i size,
        int refresh_rate,
        int color_depth,
        const display_mode &desktop) {
        if (modes.empty())
            return std::nullopt;

        const int desired_depth = color_depth > 0 ? color_depth : desktop.color_depth;
        std::vector<indexed_mode> candidates;
        candidates.reserve(modes.size());
        for (std::size_t i = 0; i < modes.size(); ++i) {
            if (modes[i].color_depth == desired_depth)
                candidates.push_back({modes[i], i});
        }
        if (candidates.empty()) {
            int closest_depth = (std::numeric_limits<int>::max)();
            for (const auto &mode : modes)
                closest_depth = (std::min)(closest_depth, std::abs(mode.color_depth - desired_depth));
            for (std::size_t i = 0; i < modes.size(); ++i) {
                if (std::abs(modes[i].color_depth - desired_depth) == closest_depth)
                    candidates.push_back({modes[i], i});
            }
        }

        const auto retain = [&candidates](auto predicate) {
            candidates.erase(
                std::remove_if(candidates.begin(), candidates.end(),
                    [&](const indexed_mode &value) { return !predicate(value.mode); }),
                candidates.end());
        };

        const bool has_exact_size = std::ranges::any_of(
            candidates, [&](const indexed_mode &value) { return value.mode.size == size; });
        if (has_exact_size) {
            retain([&](const display_mode &mode) { return mode.size == size; });
        } else {
            const bool has_larger = std::ranges::any_of(candidates, [&](const indexed_mode &value) {
                return value.mode.size.x >= size.x && value.mode.size.y >= size.y;
            });
            if (has_larger) {
                retain([&](const display_mode &mode) {
                    return mode.size.x >= size.x && mode.size.y >= size.y;
                });
                const long long requested_area = static_cast<long long>(size.x) * size.y;
                long long best_overshoot = (std::numeric_limits<long long>::max)();
                for (const auto &candidate : candidates) {
                    const long long area = static_cast<long long>(candidate.mode.size.x) * candidate.mode.size.y;
                    best_overshoot = (std::min)(best_overshoot, area - requested_area);
                }
                retain([&](const display_mode &mode) {
                    return static_cast<long long>(mode.size.x) * mode.size.y - requested_area == best_overshoot;
                });
            } else {
                long long largest_area = 0;
                for (const auto &candidate : candidates)
                    largest_area = (std::max)(largest_area,
                        static_cast<long long>(candidate.mode.size.x) * candidate.mode.size.y);
                retain([&](const display_mode &mode) {
                    return static_cast<long long>(mode.size.x) * mode.size.y == largest_area;
                });
            }

            double best_aspect = (std::numeric_limits<double>::max)();
            for (const auto &candidate : candidates)
                best_aspect = (std::min)(best_aspect, aspect_difference(candidate.mode.size, size));
            retain([&](const display_mode &mode) {
                return aspect_difference(mode.size, size) == best_aspect;
            });
        }

        if (refresh_rate > 0) {
            int best_delta = (std::numeric_limits<int>::max)();
            for (const auto &candidate : candidates)
                best_delta = (std::min)(best_delta, std::abs(candidate.mode.refresh_rate - refresh_rate));
            retain([&](const display_mode &mode) {
                return std::abs(mode.refresh_rate - refresh_rate) == best_delta;
            });
        } else {
            const bool has_desktop_rate = std::ranges::any_of(candidates, [&](const indexed_mode &value) {
                return value.mode.refresh_rate == desktop.refresh_rate;
            });
            if (has_desktop_rate) {
                retain([&](const display_mode &mode) {
                    return mode.refresh_rate == desktop.refresh_rate;
                });
            } else {
                int highest_rate = 0;
                for (const auto &candidate : candidates)
                    highest_rate = (std::max)(highest_rate, candidate.mode.refresh_rate);
                retain([&](const display_mode &mode) {
                    return mode.refresh_rate == highest_rate;
                });
            }
        }

        return std::min_element(
            candidates.begin(), candidates.end(),
            [](const indexed_mode &a, const indexed_mode &b) { return a.index < b.index; })->mode;
    }

    display_mode find_closest_mode(
        int monitor_index,
        vec2i size,
        int refresh_rate,
        int color_depth) {
        const display_mode desktop = get_desktop_mode(monitor_index);
        const auto modes = get_display_modes(monitor_index);
        return choose_closest_mode(modes, size, refresh_rate, color_depth, desktop).value_or(desktop);
    }

    display_mode find_closest_mode(vec2i size, int refresh_rate) {
        return find_closest_mode(0, size, refresh_rate, 0);
    }
} // namespace alia
