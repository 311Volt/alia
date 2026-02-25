#ifndef VERTEX_B8A7A092_EF86_4889_A837_38B321E0EECF
#define VERTEX_B8A7A092_EF86_4889_A837_38B321E0EECF

#include "../core/vec.hpp"
#include "../core/color.hpp"

namespace alia {

struct colored_vertex {
    vec2f position;
    color col;
};

struct uv_vertex {
    vec2f position;
    vec2f uv;
};

struct full_vertex {
    vec2f position;
    color col;
    vec2f uv;
};

} // namespace alia

#endif /* VERTEX_B8A7A092_EF86_4889_A837_38B321E0EECF */
