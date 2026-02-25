#ifndef PRIMITIVES_C23762A7_E841_4445_87DF_B0FD558E484E
#define PRIMITIVES_C23762A7_E841_4445_87DF_B0FD558E484E

#include "vertex.hpp"
#include "../core/rect.hpp"
#include <cstdint>
#include <span>

namespace alia {

void clear(color c);
void present();

// Single triangle
void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2);

// Unindexed bulk
void draw_triangles(std::span<const colored_vertex> vertices);
void draw_triangle_strip(std::span<const colored_vertex> vertices);
void draw_triangle_fan(std::span<const colored_vertex> vertices);

// Indexed bulk
void draw_triangles(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);
void draw_triangle_strip(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);
void draw_triangle_fan(std::span<const colored_vertex> vertices, std::span<const uint32_t> indices);

void fill_rect(rect_f r, color c);
void draw_rect(rect_f r, color c, float thickness = 1.0f);
void draw_line(vec2f a, vec2f b, color c, float thickness = 1.0f);

} // namespace alia

#endif /* PRIMITIVES_C23762A7_E841_4445_87DF_B0FD558E484E */
