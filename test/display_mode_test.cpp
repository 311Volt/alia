#include "alia/os/display.hpp"

#include <gtest/gtest.h>

#include <array>

namespace {
    constexpr alia::display_mode desktop{{1920, 1080}, 60, 32};

    TEST(DisplayMode, ExactSizeWins) {
        constexpr std::array modes{
            alia::display_mode{{1280, 720}, 60, 32},
            alia::display_mode{{1920, 1080}, 60, 32},
        };
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1280, 720}, 60, 32, desktop),
            modes[0]);
    }

    TEST(DisplayMode, ChoosesLeastLargerModeThenLargestFallback) {
        constexpr std::array modes{
            alia::display_mode{{1024, 768}, 60, 32},
            alia::display_mode{{1600, 900}, 60, 32},
            alia::display_mode{{1920, 1080}, 60, 32},
        };
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1400, 800}, 60, 32, desktop),
            modes[1]);
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {2560, 1440}, 60, 32, desktop),
            modes[2]);
    }

    TEST(DisplayMode, RefreshZeroPrefersDesktopThenHighest) {
        constexpr std::array modes{
            alia::display_mode{{1280, 720}, 75, 32},
            alia::display_mode{{1280, 720}, 60, 32},
            alia::display_mode{{1280, 720}, 144, 32},
        };
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1280, 720}, 0, 32, desktop),
            modes[1]);
        constexpr alia::display_mode desktop50{{1920, 1080}, 50, 32};
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1280, 720}, 0, 32, desktop50),
            modes[2]);
    }

    TEST(DisplayMode, RequestedRefreshChoosesClosest) {
        constexpr std::array modes{
            alia::display_mode{{1280, 720}, 60, 32},
            alia::display_mode{{1280, 720}, 120, 32},
        };
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1280, 720}, 100, 32, desktop),
            modes[1]);
    }

    TEST(DisplayMode, FiltersByRequestedDepth) {
        constexpr std::array modes{
            alia::display_mode{{1920, 1080}, 60, 16},
            alia::display_mode{{1920, 1080}, 60, 32},
        };
        EXPECT_EQ(
            alia::choose_closest_mode(modes, {1920, 1080}, 60, 16, desktop),
            modes[0]);
    }

    TEST(DisplayMode, EmptyListHasNoChoice) {
        EXPECT_FALSE(alia::choose_closest_mode({}, {800, 600}, 0, 0, desktop));
    }
}
