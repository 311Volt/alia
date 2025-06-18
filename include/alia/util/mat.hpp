#ifndef MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA
#define MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA

#include "vec.hpp"

namespace alia {

	template<typename T>
	struct mat2x2 {
		T data[2][2];

		
		mat2x2() {}
		mat2x2(T a, T b, T c, T d) {
			data[0][0] = a;
			data[0][1] = b;
			data[1][0] = c;
			data[1][1] = d;
		}
		mat2x2(const vec2<T>& a, const vec2<T>& b): mat2x2(a.x, a.y, b.x, b.y) {}

		mat2x2 operator+(const mat2x2& other) const {
			return mat2x2(
				data[0][0] + other.data[0][0],
				data[0][1] + other.data[0][1],
				data[1][0] + other.data[1][0],
				data[1][1] + other.data[1][1]
			);
		}

		mat2x2 operator-(const mat2x2& other) const {
			return mat2x2(
				data[0][0] - other.data[0][0],
				data[0][1] - other.data[0][1],
				data[1][0] - other.data[1][0],
				data[1][1] - other.data[1][1]
			);
		}

		mat2x2 operator*(const mat2x2& other) const {
			return mat2x2(
				data[0][0] * other.data[0][0] + data[0][1] * other.data[1][0],
				data[0][0] * other.data[0][1] + data[0][1] * other.data[1][1],
				data[1][0] * other.data[0][0] + data[1][1] * other.data[1][0],
				data[1][0] * other.data[0][1] + data[1][1] * other.data[1][1]
			);
		}

		vec2<T> row(int i) const {
			return vec2<T>(data[i][0], data[i][1]);
		}

		vec2<T> col(int i) const {
			return vec2<T>(data[0][i], data[1][i]);
		}
	};

}

#endif /* MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA */
