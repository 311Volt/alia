#ifndef VEC_E8EC5BFC_970D_4E88_BD3E_E7D6EC5143BA
#define VEC_E8EC5BFC_970D_4E88_BD3E_E7D6EC5143BA

#include <cmath>
#include <type_traits>
#include <stdexcept> // For std::out_of_range
#include <utility>
#include "meta.hpp"

template<typename T> 
	requires requires{typename T::alia_vector;}
struct std::tuple_size<T>
    : public std::integral_constant<std::size_t, T::alia_vector_size> {};

namespace alia {

	template<typename T>
	class vec2 {
	public:
		using alia_vector = void;
		using alia_vector_value = T;
		static constexpr size_t alia_vector_size = 2;

		T x, y;

		vec2() : x(0), y(0) {}
		vec2(T val) : x(val), y(val) {}
		vec2(T x, T y) : x(x), y(y) {}

		[[nodiscard]] constexpr vec2<T> operator+(const vec2<T>& other) const {return vec2<T>(x + other.x, y + other.y);}
		[[nodiscard]] constexpr vec2<T> operator-(const vec2<T>& other) const {return vec2<T>(x - other.x, y - other.y);}
		[[nodiscard]] constexpr vec2<T> operator*(T scalar) const {return vec2<T>(x * scalar, y * scalar);}
		[[nodiscard]] constexpr vec2<T> operator/(T scalar) const {return vec2<T>(x / scalar, y / scalar);}

		constexpr vec2<T>& operator+=(const vec2<T>& other) {x += other.x;y += other.y;return *this;}
		constexpr vec2<T>& operator-=(const vec2<T>& other) {x -= other.x;y -= other.y;return *this;}
		constexpr vec2<T>& operator*=(T scalar) {x *= scalar;y *= scalar;return *this;}
		constexpr vec2<T>& operator/=(T scalar) {x /= scalar;y /= scalar;return *this;}

		[[nodiscard]] constexpr bool operator==(const vec2<T>& other) const {return x == other.x && y == other.y;}
		[[nodiscard]] constexpr bool operator!=(const vec2<T>& other) const {return !(*this == other);}

		[[nodiscard]] constexpr vec2 transposed() const {return vec2{y, x};}
		[[nodiscard]] constexpr vec2 normalized() const requires(std::is_floating_point_v<T>) {return *this / length();}
		[[nodiscard]] constexpr bool equals(const vec2<T>& other, double epsilon = 1e-12) const {
			return (*this - other).sqLength() < epsilon;
		}
		[[nodiscard]] constexpr vec2 hadamard(const vec2<T>& other) const {
			return {x * other.x, y * other.y};
		}
		[[nodiscard]] constexpr T dot(const vec2<T>& other) const {
			return x*other.x + y*other.y;
		}

		template<typename FnT>
		[[nodiscard]] auto map(FnT&& fn) const {
			using result_element = std::remove_cvref_t<std::invoke_result_t<FnT, T>>;
			return vec2<result_element>(
				fn(x),
				fn(y)
			);
		}

		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn, auto initialValue) const {
			return fn(fn(initialValue, x), y);
		}
		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn) const {
			return fn(x, y);
		}




		template<typename U>
		[[nodiscard]] constexpr vec2<U> as() const {
			return vec2<U>{static_cast<U>(x), static_cast<U>(y)};
		}

		[[nodiscard]] constexpr vec2<int> i() const {return as<int>();}
		[[nodiscard]] constexpr vec2<float> f32() const {return as<float>();}
		[[nodiscard]] constexpr vec2<double> f64() const {return as<double>();}

		[[nodiscard]] constexpr detail::product_result_t<T> length_sqr() const {
			using ret_t = detail::product_result_t<T>;
			auto x1 = static_cast<ret_t>(x);
			auto y1 = static_cast<ret_t>(y);
			return x1*x1 + y1*y1;
		}
		[[nodiscard]] constexpr double length() const {return std::sqrt(length_sqr());}

		constexpr size_t size() const {return alia_vector_size;}
		constexpr auto* begin(this auto&& self) {return &self.x;}
		constexpr auto* end(this auto&& self) {return &self.x + self.size();}
		constexpr auto* data(this auto&& self) {return &self.x;}

		template<int N>
		constexpr auto&& get(this auto&& self) {
			if constexpr(N == 0) {
				return self.x;
			} else if constexpr(N == 1) {
				return self.y;
			} else {
				static_assert(false, "index out of range");
			}
		}

		constexpr auto& operator[](this auto&& self, int i) {
			if (i == 0) return self.x;
			else if (i == 1) return self.y;
			std::unreachable();
		}

		constexpr auto& at(this auto&& self, int i) {
			switch (i) {
				case 0: return self.x;
				case 1: return self.y;
				default: throw std::out_of_range("vec2 index out of range");
			}
		}
	};

	template<typename T>
	class vec3 {
	public:
		using alia_vector = void;
		using alia_vector_value = T;
		static constexpr size_t alia_vector_size = 3;

		T x, y, z;

		vec3() : x(0), y(0), z(0) {}
		vec3(T val) : x(val), y(val), z(val) {}
		vec3(T x, T y, T z) : x(x), y(y), z(z) {}
		vec3(const vec2<T>& a, T z): x(a.x), y(a.y), z(z) {}
		vec3(T x, const vec2<T>& b): x(x), y(b.x), z(b.y) {}

		[[nodiscard]] constexpr vec3<T> operator+(const vec3<T>& other) const {return vec3<T>(x + other.x, y + other.y, z + other.z);}
		[[nodiscard]] constexpr vec3<T> operator-(const vec3<T>& other) const {return vec3<T>(x - other.x, y - other.y, z - other.z);}
		[[nodiscard]] constexpr vec3<T> operator*(T scalar) const {return vec3<T>(x * scalar, y * scalar, z * scalar);}
		[[nodiscard]] constexpr vec3<T> operator/(T scalar) const {return vec3<T>(x / scalar, y / scalar, z / scalar);}

		constexpr vec3<T>& operator+=(const vec3<T>& other) {x += other.x;y += other.y;z += other.z;return *this;}
		constexpr vec3<T>& operator-=(const vec3<T>& other) {x -= other.x;y -= other.y;z -= other.z;return *this;}
		constexpr vec3<T>& operator*=(T scalar) {x *= scalar;y *= scalar;z *= scalar;return *this;}
		constexpr vec3<T>& operator/=(T scalar) {x /= scalar;y /= scalar;z /= scalar;return *this;}

		constexpr bool operator==(const vec3<T>& other) const {return x == other.x && y == other.y && z == other.z;}
		constexpr bool operator!=(const vec3<T>& other) const {return !(*this == other);}

		[[nodiscard]] constexpr vec3 transposed() const {return vec3{y, x, z};}
		[[nodiscard]] constexpr vec3 normalized() const requires(std::is_floating_point_v<T>) {return *this / length();}
		[[nodiscard]] constexpr bool equals(const vec3<T>& other, double epsilon = 1e-12) const {
			return (*this - other).sqLength() < epsilon;
		}
		[[nodiscard]] constexpr vec3 hadamard(const vec3<T>& other) const {
			return {x * other.x, y * other.y, z * other.z};
		}
		[[nodiscard]] constexpr T dot(const vec3<T>& other) const {
			return x*other.x + y*other.y + z*other.z;
		}
		[[nodiscard]] constexpr vec3 cross(const vec3& v) const {
			return {y * v.z - z * v.y,
					z * v.x - x * v.z,
					x * v.y - y * v.x};
		}

		template<typename U>
		[[nodiscard]] constexpr vec3<U> as() const {
			return vec3<U>{static_cast<U>(x), static_cast<U>(y), static_cast<U>(z)};
		}

		[[nodiscard]] constexpr vec3<int> i() const {return as<int>();}
		[[nodiscard]] constexpr vec3<float> f32() const {return as<float>();}
		[[nodiscard]] constexpr vec3<double> f64() const {return as<double>();}

		[[nodiscard]] constexpr detail::product_result_t<T> length_sqr() const {
			using ret_t = detail::product_result_t<T>;
			auto x1 = static_cast<ret_t>(x);
			auto y1 = static_cast<ret_t>(y);
			auto z1 = static_cast<ret_t>(z);
			return x1*x1 + y1*y1 + z1*z1;
		}
		[[nodiscard]] constexpr double length() const {return std::sqrt(length_sqr());}

		template<typename FnT>
		[[nodiscard]] auto map(FnT&& fn) const {
			using result_element = std::remove_cvref_t<std::invoke_result_t<FnT, T>>;
			return vec3<result_element>(
				fn(x),
				fn(y),
				fn(z)
			);
		}

		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn, auto initialValue) const {
			return fn(fn(fn(initialValue, x), y), z);
		}
		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn) const {
			return fn(fn(x, y), z);
		}

		constexpr size_t size() const {return alia_vector_size;}
		constexpr auto* begin(this auto&& self) {return &self.x;}
		constexpr auto* end(this auto&& self) {return &self.x + self.size();}
		constexpr auto* data(this auto&& self) {return &self.x;}

		template<int N>
		constexpr auto&& get(this auto&& self) {
			if constexpr(N == 0) {
				return self.x;
			} else if constexpr(N == 1) {
				return self.y;
			} else if constexpr (N == 2) {
				return self.z;
			} else {
				static_assert(false, "index out of range");
			}
		}

		constexpr auto& operator[](this auto&& self, int i) {
			if (i == 0) return self.x;
			else if (i == 1) return self.y;
			else if (i == 2) return self.z;
			std::unreachable();
		}

		constexpr auto& at(this auto&& self, int i) {
			switch (i) {
				case 0: return self.x;
				case 1: return self.y;
				case 2: return self.z;
				default: throw std::out_of_range("vec3 index out of range");
			}
		}
	};

	template<typename T>
	class vec4 {
	public:
		using alia_vector = void;
		using alia_vector_value = T;
		static constexpr size_t alia_vector_size = 4;

		T x, y, z, w;

		vec4() : x(0), y(0), z(0), w(0) {}
		vec4(T val) : x(val), y(val), z(val), w(val) {}
		vec4(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

		vec4(const vec2<T>& a, T z, T w): x(a.x), y(a.y), z(z), w(w) {}
		vec4(T x, const vec2<T>& b, T w): x(x), y(b.x), z(b.y), w(w) {}
		vec4(T x, T y, const vec2<T>& b): x(x), y(y), z(b.x), w(b.y) {}
		
		vec4(const vec2<T>& a, const vec2<T>& b): x(a.x), y(a.y), z(b.x), w(b.y) {}

		vec4(const vec3<T>& a, T w): x(a.x), y(a.y), z(a.z), w(w) {}
		vec4(T x, const vec3<T>& b): x(x), y(b.x), z(b.y), w(b.z) {}

		[[nodiscard]] constexpr vec4<T> operator+(const vec4<T>& other) const {return vec4<T>(x + other.x, y + other.y, z + other.z, w + other.w);}
		[[nodiscard]] constexpr vec4<T> operator-(const vec4<T>& other) const {return vec4<T>(x - other.x, y - other.y, z - other.z, w - other.w);}
		[[nodiscard]] constexpr vec4<T> operator*(T scalar) const {return vec4<T>(x * scalar, y * scalar, z * scalar, w * scalar);}
		[[nodiscard]] constexpr vec4<T> operator/(T scalar) const {return vec4<T>(x / scalar, y / scalar, z / scalar, w / scalar);}

		constexpr vec4<T>& operator+=(const vec4<T>& other) {x += other.x;y += other.y;z += other.z;w += other.w;return *this;}
		constexpr vec4<T>& operator-=(const vec4<T>& other) {x -= other.x;y -= other.y;z -= other.z;w -= other.w;return *this;}
		constexpr vec4<T>& operator*=(T scalar) {x *= scalar;y *= scalar;z *= scalar;w *= scalar;return *this;}
		constexpr vec4<T>& operator/=(T scalar) {x /= scalar;y /= scalar;z /= scalar;w /= scalar;return *this;}

		constexpr bool operator==(const vec4<T>& other) const {return x == other.x && y == other.y && z == other.z && w == other.w;}
		constexpr bool operator!=(const vec4<T>& other) const {return !(*this == other);}

		[[nodiscard]] constexpr vec4 transposed() const {return vec4{y, x, z, w};}
		[[nodiscard]] constexpr vec4 normalized() const requires(std::is_floating_point_v<T>) {return *this / length();}
		[[nodiscard]] constexpr bool equals(const vec4<T>& other, double epsilon = 1e-12) const {
			return (*this - other).sqLength() < epsilon;
		}
		[[nodiscard]] constexpr vec4 hadamard(const vec4<T>& other) const {
			return {x * other.x, y * other.y, z * other.z, w * other.w};
		}
		[[nodiscard]] constexpr T dot(const vec4<T>& other) const {
			return x*other.x + y*other.y + z*other.z + w*other.w;
		}

		template<typename U>
		[[nodiscard]] constexpr vec4<U> as() const {
			return vec4<U>{static_cast<U>(x), static_cast<U>(y), static_cast<U>(z), static_cast<U>(w)};
		}

		[[nodiscard]] constexpr vec4<int> i() const {return as<int>();}
		[[nodiscard]] constexpr vec4<float> f32() const {return as<float>();}
		[[nodiscard]] constexpr vec4<double> f64() const {return as<double>();}

		[[nodiscard]] constexpr detail::product_result_t<T> length_sqr() const {
			using ret_t = detail::product_result_t<T>;
			auto x1 = static_cast<ret_t>(x);
			auto y1 = static_cast<ret_t>(y);
			auto z1 = static_cast<ret_t>(z);
			auto w1 = static_cast<ret_t>(w);
			return x1*x1 + y1*y1 + z1*z1 + w1*w1;
		}
		[[nodiscard]] constexpr double length() const {return std::sqrt(length_sqr());}

		template<typename FnT>
		[[nodiscard]] auto map(FnT&& fn) const {
			using result_element = std::remove_cvref_t<std::invoke_result_t<FnT, T>>;
			return vec4<result_element>(
				fn(x),
				fn(y),
				fn(z),
				fn(w)
			);
		}

		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn, auto initialValue) const {
			return fn(fn(fn(fn(initialValue, x), y), z), w);
		}
		template<typename FnT>
		[[nodiscard]] auto reduce(FnT&& fn) const {
			return fn(fn(fn(x, y), z), w);
		}

		constexpr size_t size() const {return alia_vector_size;}
		constexpr auto* begin(this auto&& self) {return &self.x;}
		constexpr auto* end(this auto&& self) {return &self.x + self.size();}
		constexpr auto* data(this auto&& self) {return &self.x;}

		template<int N>
		constexpr auto&& get(this auto&& self) {
			if constexpr(N == 0) {
				return self.x;
			} else if constexpr(N == 1) {
				return self.y;
			} else if constexpr (N == 2) {
				return self.z;
			} else if constexpr (N == 3) {
				return self.w;
			} else {
				static_assert(false, "index out of range");
			}
		}

		constexpr auto& operator[](this auto&& self, int i) {
			if (i == 0) return self.x;
			else if (i == 1) return self.y;
			else if (i == 2) return self.z;
			else if (i == 3) return self.w;
			std::unreachable();
			
		}

		constexpr auto& at(this auto&& self, int i) {
			switch (i) {
				case 0: return self.x;
				case 1: return self.y;
				case 2: return self.z;
				case 3: return self.w;
				default: throw std::out_of_range("vec4 index out of range");
			}
		}
	};

	template<typename T> constexpr vec2<T> operator*(T scalar, const vec2<T>& vec) {
		return vec * scalar;
	}
	template<typename T> constexpr vec2<T> operator/(T scalar, const vec2<T>& vec) {
		return vec2<T>(scalar / vec.x, scalar / vec.y);
	}

	template<typename T> 
	[[nodiscard]] constexpr vec3<T> operator*(T scalar, const vec3<T>& vec) {
		return vec * scalar;
	}
	template<typename T> 
	[[nodiscard]] constexpr vec3<T> operator/(T scalar, const vec3<T>& vec) {
		return vec3<T>(scalar / vec.x, scalar / vec.y, scalar / vec.z);
	}

	template<typename T> 
	[[nodiscard]] constexpr vec4<T> operator*(T scalar, const vec4<T>& vec) {
		return vec * scalar;
	}
	template<typename T> 
	[[nodiscard]] constexpr vec4<T> operator/(T scalar, const vec4<T>& vec) {
		return vec4<T>(scalar / vec.x, scalar / vec.y, scalar / vec.z, scalar / vec.w);
	}

	using vec2i = vec2<int>;
	using vec2f = vec2<float>;
	using vec2d = vec2<double>;
	using vec2ld = vec2<long double>;

	using vec2i8 = vec2<int8_t>;
	using vec2u8 = vec2<uint8_t>;
	using vec2i16 = vec2<int16_t>;
	using vec2u16 = vec2<uint16_t>;
	using vec2i32 = vec2<int32_t>;
	using vec2u32 = vec2<uint32_t>;
	using vec2i64 = vec2<int64_t>;
	using vec2u64 = vec2<uint64_t>;

	using vec3i = vec3<int>;
	using vec3f = vec3<float>;
	using vec3d = vec3<double>;
	using vec3ld = vec3<long double>;

	using vec3i8 = vec3<int8_t>;
	using vec3u8 = vec3<uint8_t>;
	using vec3i16 = vec3<int16_t>;
	using vec3u16 = vec3<uint16_t>;
	using vec3i32 = vec3<int32_t>;
	using vec3u32 = vec3<uint32_t>;
	using vec3i64 = vec3<int64_t>;
	using vec3u64 = vec3<uint64_t>;

	using vec4i = vec4<int>;
	using vec4f = vec4<float>;
	using vec4d = vec4<double>;
	using vec4ld = vec4<long double>;

	using vec4i8 = vec4<int8_t>;
	using vec4u8 = vec4<uint8_t>;
	using vec4i16 = vec4<int16_t>;
	using vec4u16 = vec4<uint16_t>;
	using vec4i32 = vec4<int32_t>;
	using vec4u32 = vec4<uint32_t>;
	using vec4i64 = vec4<int64_t>;
	using vec4u64 = vec4<uint64_t>;
}



#endif /* VEC_E8EC5BFC_970D_4E88_BD3E_E7D6EC5143BA */
