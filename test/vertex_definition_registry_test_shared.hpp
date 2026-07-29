#ifndef ALIA_TEST_VERTEX_DEFINITION_REGISTRY_SHARED_HPP
#define ALIA_TEST_VERTEX_DEFINITION_REGISTRY_SHARED_HPP

#include "alia/gfx/gfx_device.hpp"

#include <array>
#include <cstddef>

namespace alia::vertex_definition_registry_test {

    struct shared_vertex {
        float x;
        float y;

        static constexpr auto elements() {
            return std::array<vertex_element, 1>{{
                {
                    vertex_attr::position,
                    vertex_storage::float_2,
                    offsetof(shared_vertex, x),
                },
            }};
        }
    };

    std::size_t shared_vertex_index_from_other_translation_unit();

} // namespace alia::vertex_definition_registry_test

#endif // ALIA_TEST_VERTEX_DEFINITION_REGISTRY_SHARED_HPP
