#ifndef MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA
#define MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA

#include "vec.hpp"

namespace alia {

	template<typename T>
	struct mat2x2 {
		T data[2][2];

		
		constexpr mat2x2() = default;
		constexpr mat2x2(T a, T b, T c, T d)
		{
			data[0][0] = a;
			data[0][1] = b;
			data[1][0] = c;
			data[1][1] = d;
		}
		constexpr mat2x2(const vec2<T>& a, const vec2<T>& b) : mat2x2(a.x, a.y, b.x, b.y) {}

		constexpr mat2x2 operator+(const mat2x2& other) const
		{
			return mat2x2(
				data[0][0] + other.data[0][0],
				data[0][1] + other.data[0][1],
				data[1][0] + other.data[1][0],
				data[1][1] + other.data[1][1]
			);
		}

		constexpr mat2x2 operator-(const mat2x2& other) const
		{
			return mat2x2(
				data[0][0] - other.data[0][0],
				data[0][1] - other.data[0][1],
				data[1][0] - other.data[1][0],
				data[1][1] - other.data[1][1]
			);
		}

		constexpr mat2x2 operator*(const mat2x2& other) const
		{
			return mat2x2(
				data[0][0] * other.data[0][0] + data[0][1] * other.data[1][0],
				data[0][0] * other.data[0][1] + data[0][1] * other.data[1][1],
				data[1][0] * other.data[0][0] + data[1][1] * other.data[1][0],
				data[1][0] * other.data[0][1] + data[1][1] * other.data[1][1]
			);
		}

		constexpr vec2<T> row(int i) const
		{
			return vec2<T>(data[i][0], data[i][1]);
		}

		constexpr vec2<T> col(int i) const
		{
			return vec2<T>(data[0][i], data[1][i]);
		}
	};

	template<typename T>
	struct mat3x3 {
		T data[3][3];

		constexpr mat3x3() = default;
		constexpr mat3x3(T a, T b, T c, T d, T e, T f, T g, T h, T i)
		{
			data[0][0] = a; data[0][1] = b; data[0][2] = c;
			data[1][0] = d; data[1][1] = e; data[1][2] = f;
			data[2][0] = g; data[2][1] = h; data[2][2] = i;
		}
		constexpr mat3x3(const vec3<T>& a, const vec3<T>& b, const vec3<T>& c)
			: mat3x3(a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z) {}

		constexpr mat3x3 operator+(const mat3x3& other) const
		{
			return mat3x3(
				data[0][0] + other.data[0][0], data[0][1] + other.data[0][1], data[0][2] + other.data[0][2],
				data[1][0] + other.data[1][0], data[1][1] + other.data[1][1], data[1][2] + other.data[1][2],
				data[2][0] + other.data[2][0], data[2][1] + other.data[2][1], data[2][2] + other.data[2][2]
			);
		}

		constexpr mat3x3 operator-(const mat3x3& other) const
		{
			return mat3x3(
				data[0][0] - other.data[0][0], data[0][1] - other.data[0][1], data[0][2] - other.data[0][2],
				data[1][0] - other.data[1][0], data[1][1] - other.data[1][1], data[1][2] - other.data[1][2],
				data[2][0] - other.data[2][0], data[2][1] - other.data[2][1], data[2][2] - other.data[2][2]
			);
		}

		constexpr mat3x3 operator*(const mat3x3& other) const
		{
			mat3x3 result;
			for (int i = 0; i < 3; ++i) {
				for (int j = 0; j < 3; ++j) {
					result.data[i][j] = 0;
					for (int k = 0; k < 3; ++k) {
						result.data[i][j] += data[i][k] * other.data[k][j];
					}
				}
			}
			return result;
		}

		constexpr vec3<T> row(int i) const
		{
			return vec3<T>(data[i][0], data[i][1], data[i][2]);
		}

		constexpr vec3<T> col(int i) const
		{
			return vec3<T>(data[0][i], data[1][i], data[2][i]);
		}

		constexpr const T* operator[](int i) const
		{
			return data[i];
		}

		constexpr T* operator[](int i)
		{
			return data[i];
		}
	};

	template<typename T>
	constexpr vec3<T> operator*(const mat3x3<T>& m, const vec3<T>& v)
	{
		return vec3<T>(
			m.data[0][0] * v.x + m.data[0][1] * v.y + m.data[0][2] * v.z,
			m.data[1][0] * v.x + m.data[1][1] * v.y + m.data[1][2] * v.z,
			m.data[2][0] * v.x + m.data[2][1] * v.y + m.data[2][2] * v.z
		);
	}

	template<typename T>
	struct mat4x4 {
		T data[4][4];

		constexpr mat4x4() = default;
		constexpr mat4x4(T a, T b, T c, T d, T e, T f, T g, T h, T i, T j, T k, T l, T m, T n, T o, T p)
		{
			data[0][0] = a; data[0][1] = b; data[0][2] = c; data[0][3] = d;
			data[1][0] = e; data[1][1] = f; data[1][2] = g; data[1][3] = h;
			data[2][0] = i; data[2][1] = j; data[2][2] = k; data[2][3] = l;
			data[3][0] = m; data[3][1] = n; data[3][2] = o; data[3][3] = p;
		}
		constexpr mat4x4(const vec4<T>& a, const vec4<T>& b, const vec4<T>& c, const vec4<T>& d)
			: mat4x4(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w, c.x, c.y, c.z, c.w, d.x, d.y, d.z, d.w) {}

		constexpr mat4x4 operator+(const mat4x4& other) const
		{
			return mat4x4(
				data[0][0] + other.data[0][0], data[0][1] + other.data[0][1], data[0][2] + other.data[0][2], data[0][3] + other.data[0][3],
				data[1][0] + other.data[1][0], data[1][1] + other.data[1][1], data[1][2] + other.data[1][2], data[1][3] + other.data[1][3],
				data[2][0] + other.data[2][0], data[2][1] + other.data[2][1], data[2][2] + other.data[2][2], data[2][3] + other.data[2][3],
				data[3][0] + other.data[3][0], data[3][1] + other.data[3][1], data[3][2] + other.data[3][2], data[3][3] + other.data[3][3]
			);
		}

		constexpr mat4x4 operator-(const mat4x4& other) const
		{
			return mat4x4(
				data[0][0] - other.data[0][0], data[0][1] - other.data[0][1], data[0][2] - other.data[0][2], data[0][3] - other.data[0][3],
				data[1][0] - other.data[1][0], data[1][1] - other.data[1][1], data[1][2] - other.data[1][2], data[1][3] - other.data[1][3],
				data[2][0] - other.data[2][0], data[2][1] - other.data[2][1], data[2][2] - other.data[2][2], data[2][3] - other.data[2][3],
				data[3][0] - other.data[3][0], data[3][1] - other.data[3][1], data[3][2] - other.data[3][2], data[3][3] - other.data[3][3]
			);
		}

		constexpr mat4x4 operator*(const mat4x4& other) const
		{
			mat4x4 result;
			for (int i = 0; i < 4; ++i) {
				for (int j = 0; j < 4; ++j) {
					result.data[i][j] = 0;
					for (int k = 0; k < 4; ++k) {
						result.data[i][j] += data[i][k] * other.data[k][j];
					}
				}
			}
			return result;
		}

		constexpr vec4<T> row(int i) const
		{
			return vec4<T>(data[i][0], data[i][1], data[i][2], data[i][3]);
		}

		constexpr vec4<T> col(int i) const
		{
			return vec4<T>(data[0][i], data[1][i], data[2][i], data[3][i]);
		}
	};

	using mat2f = mat2x2<float>;
	using mat3f = mat3x3<float>;
	using mat4f = mat4x4<float>;
	using mat2d = mat2x2<double>;
	using mat3d = mat3x3<double>;
	using mat4d = mat4x4<double>;
	
	

}

#endif /* MAT_F30BF9F8_4604_4DDE_A2B8_56842EF624BA */
