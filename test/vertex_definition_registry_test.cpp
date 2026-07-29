#include "vertex_definition_registry_test_shared.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>

namespace alia::vertex_definition_registry_test {

    namespace {

        struct distinct_vertex_with_same_layout {
            float x;
            float y;

            static constexpr auto elements() {
                return std::array<vertex_element, 1>{{
                    {
                        vertex_attr::position,
                        vertex_storage::float_2,
                        offsetof(distinct_vertex_with_same_layout, x),
                    },
                }};
            }
        };

    } // namespace

    TEST(vertex_definition_registry, caches_one_definition_per_vertex_type) {
        const auto &first = detail::vertex_definition_of<shared_vertex>();
        const auto &second = detail::vertex_definition_of<shared_vertex>();

        EXPECT_EQ(&first, &second);
        EXPECT_EQ(first.index, second.index);
        EXPECT_EQ(first.stride, static_cast<int>(sizeof(shared_vertex)));
        ASSERT_EQ(first.elements.size(), 1u);
        EXPECT_EQ(first.elements[0].attribute, vertex_attr::position);
        EXPECT_EQ(first.elements[0].storage, vertex_storage::float_2);
    }

    TEST(vertex_definition_registry, returns_one_index_across_translation_units) {
        EXPECT_EQ(
            detail::vertex_definition_of<shared_vertex>().index,
            shared_vertex_index_from_other_translation_unit()
        );
    }

    TEST(vertex_definition_registry, assigns_distinct_types_distinct_indices) {
        EXPECT_NE(
            detail::vertex_definition_of<shared_vertex>().index,
            detail::vertex_definition_of<distinct_vertex_with_same_layout>().index
        );
    }

} // namespace alia::vertex_definition_registry_test
