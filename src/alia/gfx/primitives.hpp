#ifndef PRIMITIVES_A4864BEA_2370_4C26_B44F_E8AEF0E14311
#define PRIMITIVES_A4864BEA_2370_4C26_B44F_E8AEF0E14311

#include "vertex.hpp"
#include "../core/rect.hpp"

namespace alia {

void clear(color c);
void present();

void draw_triangle(colored_vertex v0, colored_vertex v1, colored_vertex v2);
void fill_rect(rect_f r, color c);
void draw_rect(rect_f r, color c, float thickness = 1.0f);
void draw_line(vec2f a, vec2f b, color c, float thickness = 1.0f);

} // namespace alia

#endif /* PRIMITIVES_A4864BEA_2370_4C26_B44F_E8AEF0E14311 */
