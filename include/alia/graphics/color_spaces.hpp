#ifndef COLOR_SPACES_A474DBBD_C185_400A_833D_956AF02F6F1E
#define COLOR_SPACES_A474DBBD_C185_400A_833D_956AF02F6F1E

#include <math.h>
#include <algorithm>
#include <numbers>
#include <array>
#include <utility>
#include "../util/vec.hpp"
#include "../util/mat.hpp"

//TODO add tests (correctness, extreme input values, precision)

namespace alia {

	enum class color_space {
		srgb,
		linear_rgb,
		cmyk,
		hsl,
		hsv,
		xyz,
		xyy,
		lch,
		cielab,
		ciede2000,
		oklab,
		yuv,
		NUM_COLOR_SPACES
	};
	
	namespace color_space_conversions {

		namespace detail {
			// Generates a lookup table for gamma correction.
			template<size_t N, typename Func>
			auto generate_lut(Func func) {
				std::array<float, N> table{};
				for (size_t i = 0; i < N; ++i) {
					table[i] = func(static_cast<float>(i) / (N - 1));
				}
				return table;
			}

			// Compile-time lookup tables for sRGB/linear conversions.
			inline constexpr size_t GAMMA_LUT_SIZE = 4097;
			inline auto srgb_to_linear_lut = generate_lut<GAMMA_LUT_SIZE>(
				[](float s) { return std::pow((s + 0.055f) / 1.055f, 2.4f); });
			inline auto linear_to_srgb_lut = generate_lut<GAMMA_LUT_SIZE>(
				[](float l) { return 1.055f * std::pow(l, 1.0f / 2.4f) - 0.055f; });

			// Fast gamma correction using LUT with linear interpolation.
			inline float fast_srgb_to_linear(float s) {
				if (s <= 0.04045f) {
					return s / 12.92f;
				}
				float scaled_s = s * (GAMMA_LUT_SIZE - 1);
				int index = static_cast<int>(scaled_s);
				float fraction = scaled_s - index;
				return srgb_to_linear_lut[index] * (1.0f - fraction) + srgb_to_linear_lut[index + 1] * fraction;
			}

			// Fast inverse gamma correction using LUT with linear interpolation.
			inline float fast_linear_to_srgb(float l) {
				if (l <= 0.0031308f) {
					return l * 12.92f;
				}
				float scaled_l = l * (GAMMA_LUT_SIZE - 1);
				int index = static_cast<int>(scaled_l);
				float fraction = scaled_l - index;
				return linear_to_srgb_lut[index] * (1.0f - fraction) + linear_to_srgb_lut[index + 1] * fraction;
			}

			// Helper for HSL to RGB
			inline float hue_to_rgb(float p, float q, float t) {
				if (t < 0.0f) t += 1.0f;
				if (t > 1.0f) t -= 1.0f;
				if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
				if (t < 1.0f / 2.0f) return q;
				if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
				return p;
			}

			// Helper function for XYZ to LAB
			inline float f_lab(float t) {
				constexpr float d_over_e = 6.0f / 29.0f;
				constexpr float d_over_e_cubed = d_over_e * d_over_e * d_over_e;
				if (t > d_over_e_cubed) {
					return std::cbrt(t);
				} else {
					constexpr float e_over_d_squared = (29.0f / 6.0f) * (29.0f / 6.0f);
					return (t * e_over_d_squared * (1.0f / 3.0f)) + (4.0f / 29.0f);
				}
			}

			// Helper function for LAB to XYZ
			inline float inv_f_lab(float t) {
				constexpr float d_over_e = 6.0f / 29.0f;
				if (t > d_over_e) {
					return t * t * t;
				} else {
					constexpr float d_over_e_squared = d_over_e * d_over_e;
					return 3.0f * d_over_e_squared * (t - 4.0f / 29.0f);
				}
			}

			// Fast approximation of atan2.
			inline float fast_atan2(float y, float x) {
				constexpr float PI = std::numbers::pi_v<float>;
				if (x == 0.0f) {
					if (y > 0.0f) return PI / 2.0f;
					if (y < 0.0f) return -PI / 2.0f;
					return 0.0f;
				}
				float abs_y = std::abs(y);
				float angle;
				if (x >= 0.0f) {
					float r = (x - abs_y) / (x + abs_y);
					angle = PI / 4.0f - (PI / 4.0f) * r;
				} else {
					float r = (x + abs_y) / (abs_y - x);
					angle = 3.0f * PI / 4.0f - (PI / 4.0f) * r;
				}
				return y < 0.0f ? -angle : angle;
			}
		} // namespace detail

	
		inline vec3f srgb_to_linear(const vec3f& srgb) {
			return vec3f(
				detail::fast_srgb_to_linear(srgb[0]),
				detail::fast_srgb_to_linear(srgb[1]),
				detail::fast_srgb_to_linear(srgb[2])
			);
		}


		inline vec3f linear_to_srgb(const vec3f& linear) {
			return vec3f(
				detail::fast_linear_to_srgb(linear[0]),
				detail::fast_linear_to_srgb(linear[1]),
				detail::fast_linear_to_srgb(linear[2])
			);
		}

		// D65 standard illuminant reference white
		inline constexpr float D65_X = 95.047f;
		inline constexpr float D65_Y = 100.0f;
		inline constexpr float D65_Z = 108.883f;

		inline vec3f xyz_to_lab(const vec3f& xyz) {
			float X = xyz[0] * 100.0f; // Scale to 0-100
			float Y = xyz[1] * 100.0f;
			float Z = xyz[2] * 100.0f;

			float Xr = X / D65_X;
			float Yr = Y / D65_Y;
			float Zr = Z / D65_Z;

			float fx = detail::f_lab(Xr);
			float fy = detail::f_lab(Yr);
			float fz = detail::f_lab(Zr);

			float L = 116.0f * fy - 16.0f;
			float a = 500.0f * (fx - fy);
			float b = 200.0f * (fy - fz);

			return vec3f(L, a, b);
		}

		inline vec3f lab_to_xyz(const vec3f& lab) {
			float L = lab[0];
			float a = lab[1];
			float b = lab[2];

			float fy = (L + 16.0f) / 116.0f;
			float fx = a / 500.0f + fy;
			float fz = fy - b / 200.0f;

			float Xr = detail::inv_f_lab(fx);
			float Yr = detail::inv_f_lab(fy);
			float Zr = detail::inv_f_lab(fz);

			float X = Xr * D65_X / 100.0f; // Scale back to 0-1
			float Y = Yr * D65_Y / 100.0f;
			float Z = Zr * D65_Z / 100.0f;

			return vec3f(X, Y, Z);
		}

		inline vec3f lab_to_lch(const vec3f& lab) {
			float L = lab[0];
			float a = lab[1];
			float b = lab[2];

			float C = std::sqrt(a * a + b * b);
			float H = detail::fast_atan2(b, a) * 180.0f / std::numbers::pi_v<float>;

			if (H < 0) {
				H += 360.0f;
			}

			return vec3f(L, C, H);
		}

		inline vec3f lch_to_lab(const vec3f& lch) {
			float L = lch[0];
			float C = lch[1];
			float H = lch[2];

			float h_rad = H * std::numbers::pi / 180.0f;

			float a = C * std::cos(h_rad);
			float b = C * std::sin(h_rad);

			return vec3f(L, a, b);
		}

		inline vec3f xyz_to_xyy(const vec3f& xyz) {
			float sum_xyz = xyz[0] + xyz[1] + xyz[2];
			if (sum_xyz == 0.0f) {
				return vec3f(0.0f, 0.0f, 0.0f);
			}
			float x = xyz[0] / sum_xyz;
			float y = xyz[1] / sum_xyz;
			float Y = xyz[1]; // Y is the luminance
			return vec3f(x, y, Y);
		}

		inline vec3f xyy_to_xyz(const vec3f& xyy) {
			float x = xyy[0];
			float y = xyy[1];
			float Y = xyy[2];

			if (y == 0.0f) {
				return vec3f(0.0f, 0.0f, 0.0f); // Avoid division by zero
			}

			float X = (x / y) * Y;
			float Z = ((1.0f - x - y) / y) * Y;
			return vec3f(X, Y, Z);
		}

		// Oklab conversion constants
		inline constexpr mat3f linear_rgb_to_lms_matrix = mat3f(
			0.819022f, 0.361970f, -0.128860f,
			0.036152f, 0.628699f, 0.004293f,
			0.000482f, 0.006242f, 1.000000f
		);

		inline constexpr mat3f lms_to_linear_rgb_matrix = mat3f(
			 1.227034f, -0.227034f,  0.000000f,
			-0.055799f,  1.055799f,  0.000000f,
			 0.000000f,  0.000000f,  1.000000f
		);

		inline constexpr mat3f lms_to_oklab_matrix = mat3f(
			0.2104542553f,  0.7936177850f, -0.0040720468f,
			1.9779984951f, -2.4285922059f,  0.4505937108f,
			0.0259040371f,  0.7827717662f, -0.8086757993f
		);

		inline constexpr mat3f oklab_to_lms_matrix = mat3f(
			1.0f,  0.3963377774f,  0.2158037573f,
			1.0f, -0.1055613423f, -0.0638541728f,
			1.0f, -0.0894841775f, -1.2914855480f
		);

		inline vec3f linear_rgb_to_lms(const vec3f& linear_rgb) {
			vec3f lms_linear;
			lms_linear[0] = linear_rgb_to_lms_matrix[0][0] * linear_rgb[0] + linear_rgb_to_lms_matrix[0][1] * linear_rgb[1] + linear_rgb_to_lms_matrix[0][2] * linear_rgb[2];
			lms_linear[1] = linear_rgb_to_lms_matrix[1][0] * linear_rgb[0] + linear_rgb_to_lms_matrix[1][1] * linear_rgb[1] + linear_rgb_to_lms_matrix[1][2] * linear_rgb[2];
			lms_linear[2] = linear_rgb_to_lms_matrix[2][0] * linear_rgb[0] + linear_rgb_to_lms_matrix[2][1] * linear_rgb[1] + linear_rgb_to_lms_matrix[2][2] * linear_rgb[2];
			return lms_linear;
		}

		inline vec3f lms_to_oklab(const vec3f& lms) {
			vec3f lms_cubed;
			lms_cubed[0] = std::cbrt(lms[0]);
			lms_cubed[1] = std::cbrt(lms[1]);
			lms_cubed[2] = std::cbrt(lms[2]);

			vec3f oklab;
			oklab[0] = lms_to_oklab_matrix[0][0] * lms_cubed[0] + lms_to_oklab_matrix[0][1] * lms_cubed[1] + lms_to_oklab_matrix[0][2] * lms_cubed[2];
			oklab[1] = lms_to_oklab_matrix[1][0] * lms_cubed[0] + lms_to_oklab_matrix[1][1] * lms_cubed[1] + lms_to_oklab_matrix[1][2] * lms_cubed[2];
			oklab[2] = lms_to_oklab_matrix[2][0] * lms_cubed[0] + lms_to_oklab_matrix[2][1] * lms_cubed[1] + lms_to_oklab_matrix[2][2] * lms_cubed[2];
			return oklab;
		}

		inline vec3f oklab_to_lms(const vec3f& oklab) {
			vec3f lms_cubed;
			lms_cubed[0] = oklab_to_lms_matrix[0][0] * oklab[0] + oklab_to_lms_matrix[0][1] * oklab[1] + oklab_to_lms_matrix[0][2] * oklab[2];
			lms_cubed[1] = oklab_to_lms_matrix[1][0] * oklab[0] + oklab_to_lms_matrix[1][1] * oklab[1] + oklab_to_lms_matrix[1][2] * oklab[2];
			lms_cubed[2] = oklab_to_lms_matrix[2][0] * oklab[0] + oklab_to_lms_matrix[2][1] * oklab[1] + oklab_to_lms_matrix[2][2] * oklab[2];
			return lms_cubed;
		}

		inline vec3f lms_to_linear_rgb(const vec3f& lms) {
			vec3f lms_linear;
			lms_linear[0] = lms[0] * lms[0] * lms[0];
			lms_linear[1] = lms[1] * lms[1] * lms[1];
			lms_linear[2] = lms[2] * lms[2] * lms[2];

			vec3f linear_rgb;
			linear_rgb[0] = lms_to_linear_rgb_matrix[0][0] * lms_linear[0] + lms_to_linear_rgb_matrix[0][1] * lms_linear[1] + lms_to_linear_rgb_matrix[0][2] * lms_linear[2];
			linear_rgb[1] = lms_to_linear_rgb_matrix[1][0] * lms_linear[0] + lms_to_linear_rgb_matrix[1][1] * lms_linear[1] + lms_to_linear_rgb_matrix[1][2] * lms_linear[2];
			linear_rgb[2] = lms_to_linear_rgb_matrix[2][0] * lms_linear[0] + lms_to_linear_rgb_matrix[2][1] * lms_linear[1] + lms_to_linear_rgb_matrix[2][2] * lms_linear[2];
			return linear_rgb;
		}

		inline vec4f srgb_to_cmyk(const vec3f& srgb) {
			float r = srgb[0];
			float g = srgb[1];
			float b = srgb[2];

			float k = 1.0f - std::max({r, g, b});
			float c = (1.0f - r - k) / (1.0f - k);
			float m = (1.0f - g - k) / (1.0f - k);
			float y = (1.0f - b - k) / (1.0f - k);

			if (1.0f - k < 1e-6) { // Avoid division by zero if K is close to 1
				c = m = y = 0.0f;
			}

			return vec4f(c, m, y, k);
		}

		inline vec3f cmyk_to_srgb(const vec4f& cmyk) {
			float c = cmyk[0];
			float m = cmyk[1];
			float y = cmyk[2];
			float k = cmyk[3];

			float r = (1.0f - c) * (1.0f - k);
			float g = (1.0f - m) * (1.0f - k);
			float b = (1.0f - y) * (1.0f - k);

			return vec3f(r, g, b);
		}

		inline vec3f srgb_to_hsl(const vec3f& srgb) {
			float r = srgb[0];
			float g = srgb[1];
			float b = srgb[2];

			float max_val = std::max({r, g, b});
			float min_val = std::min({r, g, b});
			float h, s, l;

			l = (max_val + min_val) / 2.0f;

			if (max_val == min_val) {
				h = s = 0.0f; // achromatic
			} else {
				float d = max_val - min_val;
				s = l > 0.5f ? d / (2.0f - max_val - min_val) : d / (max_val + min_val);

				if (max_val == r) {
					h = (g - b) / d + (g < b ? 6.0f : 0.0f);
				} else if (max_val == g) {
					h = (b - r) / d + 2.0f;
				} else {
					h = (r - g) / d + 4.0f;
				}
				h /= 6.0f;
			}
			return vec3f(h * 360.0f, s, l);
		}

		inline vec3f hsl_to_srgb(const vec3f& hsl) {
			float h = hsl[0] / 360.0f; // Normalize H to [0, 1]
			float s = hsl[1];
			float l = hsl[2];

			float r, g, b;

			if (s == 0.0f) {
				r = g = b = l; // achromatic
			} else {
				float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
				float p = 2.0f * l - q;
				r = detail::hue_to_rgb(p, q, h + 1.0f / 3.0f);
				g = detail::hue_to_rgb(p, q, h);
				b = detail::hue_to_rgb(p, q, h - 1.0f / 3.0f);
			}
			return vec3f(r, g, b);
		}

		inline vec3f srgb_to_hsv(const vec3f& srgb) {
			float r = srgb[0];
			float g = srgb[1];
			float b = srgb[2];

			float max_val = std::max({r, g, b});
			float min_val = std::min({r, g, b});
			float h, s, v = max_val;

			float d = max_val - min_val;
			s = max_val == 0.0f ? 0.0f : d / max_val;

			if (max_val == min_val) {
				h = 0.0f; // achromatic
			} else {
				if (max_val == r) {
					h = (g - b) / d + (g < b ? 6.0f : 0.0f);
				} else if (max_val == g) {
					h = (b - r) / d + 2.0f;
				} else {
					h = (r - g) / d + 4.0f;
				}
				h /= 6.0f;
			}
			return vec3f(h * 360.0f, s, v);
		}

		inline vec3f hsv_to_srgb(const vec3f& hsv) {
			float h = hsv[0];
			float s = hsv[1];
			float v = hsv[2];

			float r, g, b;

			int i = static_cast<int>(h / 60.0f) % 6;
			float f = h / 60.0f - i;
			float p = v * (1.0f - s);
			float q = v * (1.0f - f * s);
			float t = v * (1.0f - (1.0f - f) * s);

			switch (i) {
				case 0: r = v; g = t; b = p; break;
				case 1: r = q; g = v; b = p; break;
				case 2: r = p; g = v; b = t; break;
				case 3: r = p; g = q; b = v; break;
				case 4: r = t; g = p; b = v; break;
				case 5: r = v; g = p; b = q; break;
				default: r = g = b = v; break; // Should not happen
			}
			return vec3f(r, g, b);
		}

		// D65 standard illuminant
		inline constexpr mat3f rgb_to_xyz_matrix = mat3f(
			0.4124564f, 0.3575761f, 0.1804375f,
			0.2126729f, 0.7151522f, 0.0721750f,
			0.0193339f, 0.1191920f, 0.9503041f
		);

		inline constexpr mat3f xyz_to_rgb_matrix = mat3f(
			 3.2404542f, -1.5371385f, -0.4985314f,
			-0.9692660f,  1.8760108f,  0.0415560f,
			 0.0556434f, -0.2040259f,  1.0572252f
		);

	}


	namespace detail {

		struct conversion_definition {
			color_space from;
			color_space to;
			double weight;
		};

		inline constexpr std::array available_conversions {
			conversion_definition {color_space::srgb, color_space::cmyk, 0.0698941},
			conversion_definition {color_space::cmyk, color_space::srgb, 0.0702469},
			conversion_definition {color_space::srgb, color_space::hsl, 0.327715},
			conversion_definition {color_space::hsl, color_space::srgb, 0.545194},
			conversion_definition {color_space::srgb, color_space::hsv, 0.275804},
			conversion_definition {color_space::hsv, color_space::srgb, 0.397545},
			conversion_definition {color_space::linear_rgb, color_space::xyz, 0.76818},
			conversion_definition {color_space::xyz, color_space::linear_rgb, 0.592736},
			conversion_definition {color_space::xyz, color_space::xyy, 0.990133},
			conversion_definition {color_space::xyy, color_space::xyz, 0.860196},
			conversion_definition {color_space::xyz, color_space::cielab, 16.2545},
			conversion_definition {color_space::cielab, color_space::xyz, 0.069827},
			conversion_definition {color_space::cielab, color_space::lch, 5.46548},
			conversion_definition {color_space::lch, color_space::cielab, 0.0694048},
			conversion_definition {color_space::linear_rgb, color_space::oklab, 15.9883},
			conversion_definition {color_space::oklab, color_space::linear_rgb, 0.0694654},
			conversion_definition {color_space::srgb, color_space::linear_rgb, 0.0705963},
			conversion_definition {color_space::linear_rgb, color_space::srgb, 0.0693519},
			conversion_definition {color_space::srgb, color_space::yuv, 0.1}, // Placeholder weight
			conversion_definition {color_space::yuv, color_space::srgb, 0.1}  // Placeholder weight
		};

		using color_space_conversion_function = vec3f(*)(const vec3f&);



		// Gets the integer index of a color_space enum value.
		consteval int to_int(color_space cs) {
			return static_cast<int>(cs);
		}

		// Gets the color_space enum value from an integer index.
		consteval color_space to_color_space(int i) {
			return static_cast<color_space>(i);
		}

		// Gets the total number of color spaces.
		consteval int color_space_count() {
			return to_int(color_space::NUM_COLOR_SPACES);
		}

		// Creates an adjacency matrix for the conversion graph.
		consteval auto make_adjacency_matrix() {
			constexpr int num_spaces = color_space_count();
			std::array<std::array<double, num_spaces>, num_spaces> matrix{};
			for (int i = 0; i < num_spaces; ++i) {
				for (int j = 0; j < num_spaces; ++j) {
					matrix[i][j] = -1.0; // Use -1 to signify no direct path
				}
			}
			for (const auto& conv : available_conversions) {
				matrix[to_int(conv.from)][to_int(conv.to)] = conv.weight;
			}
			return matrix;
		}

		// Dijkstra's algorithm to find the shortest path.
		consteval auto dijkstra(color_space from, color_space to) {
			constexpr int num_spaces = color_space_count();
			constexpr auto adj_matrix = make_adjacency_matrix();
			
			std::array<double, num_spaces> dist;
			std::array<int, num_spaces> prev;
			std::array<bool, num_spaces> visited{};

			for (int i = 0; i < num_spaces; ++i) {
				dist[i] = std::numeric_limits<double>::max();
				prev[i] = -1;
			}

			dist[to_int(from)] = 0;

			for (int count = 0; count < num_spaces - 1; ++count) {
				double min_dist = std::numeric_limits<double>::max();
				int u = -1;

				for (int i = 0; i < num_spaces; ++i) {
					if (!visited[i] && dist[i] <= min_dist) {
						min_dist = dist[i];
						u = i;
					}
				}

				if (u == -1) break;

				visited[u] = true;

				for (int v = 0; v < num_spaces; ++v) {
					if (!visited[v] && adj_matrix[u][v] != -1.0 && dist[u] != std::numeric_limits<double>::max() && dist[u] + adj_matrix[u][v] < dist[v]) {
						dist[v] = dist[u] + adj_matrix[u][v];
						prev[v] = u;
					}
				}
			}
			return prev;
		}

		consteval color_space_conversion_function get_conversion_function(color_space from, color_space to) {
			if (from == color_space::srgb && to == color_space::linear_rgb) return color_space_conversions::srgb_to_linear;
			if (from == color_space::linear_rgb && to == color_space::srgb) return color_space_conversions::linear_to_srgb;
			// CMYK is unsupported for now
			// if (from == color_space::srgb && to == color_space::cmyk) return color_space_conversions::srgb_to_cmyk;
			// if (from == color_space::cmyk && to == color_space::srgb) return color_space_conversions::cmyk_to_srgb;
			if (from == color_space::srgb && to == color_space::hsl) return color_space_conversions::srgb_to_hsl;
			if (from == color_space::hsl && to == color_space::srgb) return color_space_conversions::hsl_to_srgb;
			if (from == color_space::srgb && to == color_space::hsv) return color_space_conversions::srgb_to_hsv;
			if (from == color_space::hsv && to == color_space::srgb) return color_space_conversions::hsv_to_srgb;
			if (from == color_space::xyz && to == color_space::cielab) return color_space_conversions::xyz_to_lab;
			if (from == color_space::cielab && to == color_space::xyz) return color_space_conversions::lab_to_xyz;
			if (from == color_space::cielab && to == color_space::lch) return color_space_conversions::lab_to_lch;
			if (from == color_space::lch && to == color_space::cielab) return color_space_conversions::lch_to_lab;
			if (from == color_space::xyz && to == color_space::xyy) return color_space_conversions::xyz_to_xyy;
			if (from == color_space::xyy && to == color_space::xyz) return color_space_conversions::xyy_to_xyz;
			if (from == color_space::linear_rgb && to == color_space::oklab) return color_space_conversions::linear_rgb_to_lms;
			if (from == color_space::oklab && to == color_space::linear_rgb) return color_space_conversions::lms_to_linear_rgb;
			if (from == color_space::oklab && to == color_space::lch) return color_space_conversions::lms_to_oklab;
			if (from == color_space::lch && to == color_space::oklab) return color_space_conversions::oklab_to_lms;
			return nullptr;
		}

		template<color_space From, color_space To>
		consteval auto determine_conversion_sequence() {
			constexpr auto prev = dijkstra(From, To);
			constexpr int to_idx = to_int(To);

			static_assert(prev[to_idx] != -1 || From == To, "No conversion path found");

			constexpr int max_depth = color_space_count();
			std::array<color_space, max_depth> path;
			int path_len = 0;
			
			if (From != To) {
				int at = to_idx;
				while (at != -1) {
					path[path_len++] = to_color_space(at);
					at = prev[at];
				}
			}

			std::array<color_space, max_depth> reversed_path;
			for(int i = 0; i < path_len; ++i) {
				reversed_path[i] = path[path_len - 1 - i];
			}

			std::array<color_space_conversion_function, max_depth -1> func_sequence{};
			if (path_len > 1) {
				for (int i = 0; i < path_len - 1; ++i) {
					func_sequence[i] = get_conversion_function(reversed_path[i], reversed_path[i+1]);
				}
			}
			
			return func_sequence;
		}
	}

	template<color_space From, color_space To>
	auto convert_color_space(const vec3f& color) {
		static constexpr auto conversion_sequence = detail::determine_conversion_sequence<From, To>();
		vec3f result = color;
		for(auto&& fn: conversion_sequence) {
			result = fn(result);
		}
		return result;
	}


}

#endif /* COLOR_SPACES_A474DBBD_C185_400A_833D_956AF02F6F1E */
