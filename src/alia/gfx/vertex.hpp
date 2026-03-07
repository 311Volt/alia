#ifndef VERTEX_E06CED84_9AE6_43E0_AC21_06A13D20CF2C
#define VERTEX_E06CED84_9AE6_43E0_AC21_06A13D20CF2C

#include "../core/vec.hpp"
#include "../core/color.hpp"

#include <array>
#include <cstddef>
#include <type_traits>

namespace alia {

// ── Vertex element descriptors ───────────────────────────────────────

enum class vertex_attr : int {
    position = 1,
    color_attr,
    tex_coord,
};

enum class vertex_storage : int {
    float_2,
    float_3,
    float_4,
};

struct vertex_element {
    vertex_attr    attribute;
    vertex_storage storage;
    int            offset;
};

// ── Concept: a standard-layout type with static constexpr elements() ─

namespace detail {
    template<typename T, typename = void>
    struct has_elements : std::false_type {};

    template<typename T>
    struct has_elements<T, std::void_t<decltype(T::elements())>> : std::true_type {};
}

template<typename T>
concept vertex_type = std::is_standard_layout_v<T> && detail::has_elements<T>::value;

// ── Built-in vertex types ────────────────────────────────────────────

struct colored_vertex {
    vec2f position;
    color col;

    static constexpr auto elements() {
        return std::array<vertex_element, 2>{{
            {vertex_attr::position,   vertex_storage::float_2, offsetof(colored_vertex, position)},
            {vertex_attr::color_attr, vertex_storage::float_4, offsetof(colored_vertex, col)},
        }};
    }
};

struct uv_vertex {
    vec2f position;
    vec2f uv;

    static constexpr auto elements() {
        return std::array<vertex_element, 2>{{
            {vertex_attr::position,  vertex_storage::float_2, offsetof(uv_vertex, position)},
            {vertex_attr::tex_coord, vertex_storage::float_2, offsetof(uv_vertex, uv)},
        }};
    }
};

struct full_vertex {
    vec2f position;
    color col;
    vec2f uv;

    static constexpr auto elements() {
        return std::array<vertex_element, 3>{{
            {vertex_attr::position,   vertex_storage::float_2, offsetof(full_vertex, position)},
            {vertex_attr::color_attr, vertex_storage::float_4, offsetof(full_vertex, col)},
            {vertex_attr::tex_coord,  vertex_storage::float_2, offsetof(full_vertex, uv)},
        }};
    }
};

} // namespace alia

#endif /* VERTEX_E06CED84_9AE6_43E0_AC21_06A13D20CF2C */
