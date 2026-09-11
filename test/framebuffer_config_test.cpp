#include "alia/gfx/framebuffer_config.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {
    TEST(FramebufferConfig, DontCareAndSuggestAllowMismatch) {
        alia::framebuffer_properties actual;
        actual.depth_bits = 16;
        alia::framebuffer_config ignored;
        ignored.depth_bits.value = 24;
        EXPECT_FALSE(alia::check_framebuffer_requirements(ignored, actual));

        alia::framebuffer_config suggested;
        suggested.depth_bits = {24, alia::suggest};
        EXPECT_FALSE(alia::check_framebuffer_requirements(suggested, actual));
    }

    TEST(FramebufferConfig, RequireNamesFirstMismatch) {
        alia::framebuffer_properties actual;
        actual.depth_bits = 16;
        alia::framebuffer_config requested;
        requested.depth_bits = {24, alia::require};
        const auto result = alia::check_framebuffer_requirements(requested, actual);
        ASSERT_TRUE(result);
        EXPECT_NE(result->find("depth_bits"), std::string::npos);
    }

    TEST(FramebufferConfig, ChecksBooleanAndEnumOptions) {
        alia::framebuffer_properties actual;
        actual.float_color = false;
        actual.swap = alia::swap_method::undefined;

        alia::framebuffer_config boolean_request;
        boolean_request.float_color = {true, alia::require};
        EXPECT_TRUE(alia::check_framebuffer_requirements(boolean_request, actual));

        alia::framebuffer_config enum_request;
        enum_request.swap = {alia::swap_method::copy, alia::require};
        EXPECT_TRUE(alia::check_framebuffer_requirements(enum_request, actual));
    }
}
