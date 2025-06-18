#ifndef RECT_C1E10D99_656D_46FE_806B_06F324305523
#define RECT_C1E10D99_656D_46FE_806B_06F324305523

#include "vec.hpp"
#include "meta.hpp"

namespace alia {

	template<typename T>
	struct rect {
		vec2<T> a, b;

		rect() : a(0), b(0) {}
		rect(const vec2<T>& a, const vec2<T>& b) : a(a), b(b) {}
		rect(T x1, T y1, T x2, T y2): a(x1, y1), b(x2, y2) {}

		static constexpr rect xywh(T x, T y, T w, T h) {
			return {x, y, x+w, x+h};
		}

		rect<T> operator+(const vec2<T>& other) const {
			return rect<T>(a + other, b + other);
		}
		rect<T> operator-(const vec2<T>& other) const {
			return rect<T>(a - other, b - other);
		}

		rect<T>& operator+=(const vec2<T>& other) {
			a += other;
			b += other;
			return *this;
		}
		rect<T>& operator-=(const vec2<T>& other) {
			a -= other;
			b -= other;
			return *this;
		}

		T width() const {
			return b.x - a.x;
		}
		T height() const {
			return b.y - a.y;
		}
		bool valid() const {
			return b.x >= a.x && b.y >= a.y;
		}
		detail::product_result_t<T> area() {
			using ret_t = detail::product_result_t<T>;
			return static_cast<ret_t>(width()) * static_cast<ret_t>(height());
		}

		bool contains(const vec2<T>& point) const requires(std::is_integral_v<T>) {
			return point.x >= a.x && point.x < b.x && point.y >= a.y && point.y < b.y;
		}
		bool contains(const vec2<T>& point) const requires(not std::is_integral_v<T>) {
			return point.x >= a.x && point.x <= b.x && point.y >= a.y && point.y <= b.y;
		}

		rect<T> intersection_with(const rect<T>& other) const {
			vec2<T> v1(std::max(a.x, other.a.x), std::max(a.y, other.a.y));
			vec2<T> v2(std::min(b.x, other.b.x), std::min(b.y, other.b.y));
			return rect<T>(v1, v2);
		}

		rect<T> union_with(const rect<T>& other) const {
			vec2<T> v1(std::min(a.x, other.a.x), std::min(a.y, other.a.y));
			vec2<T> v2(std::max(b.x, other.b.x), std::max(b.y, other.b.y));
			return rect<T>(v1, v2);
		}

		vec2<T> topLeft() const { return a; }
		vec2<T> topRight() const { return {b.x, a.y}; }
		vec2<T> bottomLeft() const { return {a.x, b.y}; }
		vec2<T> bottomRight() const { return b; }
	};

	using rect_i = rect<int>;
	using rect_f = rect<float>;
	using rect_d = rect<double>;

	using rect_i8 = rect<int8_t>;
	using rect_u8 = rect<uint8_t>;
	using rect_i16 = rect<int16_t>;
	using rect_u16 = rect<uint16_t>;
	using rect_i32 = rect<int32_t>;
	using rect_u32 = rect<uint32_t>;
	using rect_i64 = rect<int64_t>;
	using rect_u64 = rect<uint64_t>;

}

#endif /* RECT_C1E10D99_656D_46FE_806B_06F324305523 */
