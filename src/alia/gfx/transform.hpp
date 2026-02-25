#ifndef TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA
#define TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA

#include "../core/vec.hpp"
#include <cmath>

namespace alia {

struct transform {
    float m[4][4];

    static transform identity() {
        transform t = {};
        t.m[0][0] = 1; t.m[1][1] = 1; t.m[2][2] = 1; t.m[3][3] = 1;
        return t;
    }

    static transform translate(vec2f t) {
        transform r = identity();
        r.m[3][0] = t.x;
        r.m[3][1] = t.y;
        return r;
    }

    static transform scale(vec2f s) {
        transform r = identity();
        r.m[0][0] = s.x;
        r.m[1][1] = s.y;
        return r;
    }

    static transform scale(float s) {
        return scale(vec2f{s, s});
    }

    static transform rotate(float angle_rad) {
        transform r = identity();
        float c = std::cos(angle_rad);
        float s = std::sin(angle_rad);
        r.m[0][0] = c;  r.m[0][1] = s;
        r.m[1][0] = -s; r.m[1][1] = c;
        return r;
    }

    static transform ortho(float l, float r, float b, float t) {
        transform res = {};
        res.m[0][0] = 2.0f / (r - l);
        res.m[1][1] = 2.0f / (t - b);
        res.m[2][2] = -1.0f;
        res.m[3][0] = -(r + l) / (r - l);
        res.m[3][1] = -(t + b) / (t - b);
        res.m[3][3] = 1.0f;
        return res;
    }

    transform operator*(const transform& o) const {
        transform res = {};
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                res.m[i][j] = m[i][0] * o.m[0][j] +
                              m[i][1] * o.m[1][j] +
                              m[i][2] * o.m[2][j] +
                              m[i][3] * o.m[3][j];
            }
        }
        return res;
    }

    transform& operator*=(const transform& o) {
        *this = *this * o;
        return *this;
    }

    vec2f apply(vec2f p) const {
        return {
            p.x * m[0][0] + p.y * m[1][0] + m[3][0],
            p.x * m[0][1] + p.y * m[1][1] + m[3][1]
        };
    }
};

transform get_current_transform();
void      set_current_transform(const transform& t);
transform get_current_projection();
void      set_current_projection(const transform& t);

struct scoped_transform {
    explicit scoped_transform(const transform& t) {
        saved_ = get_current_transform();
        set_current_transform(saved_ * t);
    }
    ~scoped_transform() {
        set_current_transform(saved_);
    }
private:
    transform saved_;
};

} // namespace alia

#endif /* TRANSFORM_D0BF54D2_38DF_4A32_87FE_50E22A30C9FA */
