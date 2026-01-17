#ifndef PIXEL_F150483D_5A18_4194_9A28_C08956350E5B
#define PIXEL_F150483D_5A18_4194_9A28_C08956350E5B

#include <stdint.h>
#include <type_traits>
#include <utility>

namespace alia {

    enum class pixel_format {
        rgba8888,
        rgb888,
        argb8888,
        bgra8888,
        abgr8888,
        rgb565,
        gray8,
        gray16,
        indexed8,
        rgb332,
        bgr565,
        rgba4444,
        argb4444,
        bgra4444,
        abgr4444,
        rgba5551,
        argb1555,
        bgra5551,
        abgr1555,
        gray_f32,
        rgb_f32,
        rgba_f32
    };
    template <pixel_format Format>
    struct pixel_type;

    namespace pixel_components {

        template <int NBits>
        struct int_leastN {
            using type = void;
        };

        template <int NBits>
            requires(NBits <= 8)
        struct int_leastN<NBits> {
            using type = uint8_t;
        };

        template <int NBits>
            requires(NBits > 8 && NBits <= 16)
        struct int_leastN<NBits> {
            using type = uint16_t;
        };

        template <int NBits>
            requires(NBits > 16 && NBits <= 32)
        struct int_leastN<NBits> {
            using type = uint32_t;
        };

        template <int NBits>
            requires(NBits > 32 && NBits <= 64)
        struct int_leastN<NBits> {
            using type = uint64_t;
        };

        template <int NBits>
        using int_leastN_t = int_leastN<NBits>::type;

        template <int NBits>
        struct integer_pixel_component {
            int_leastN_t<NBits> value : NBits;
        };

        struct float_pixel_component {
            float value;
        };

        template <class T>
        struct is_integer_pixel_component : std::false_type {};
        template <int N>
        struct is_integer_pixel_component<integer_pixel_component<N>> : std::true_type {};

        template <class T>
        struct is_float_pixel_component : std::false_type {};
        template <>
        struct is_float_pixel_component<float_pixel_component> : std::true_type {};

        template <class T>
        struct component_bit_depth;
        template <int N>

        struct component_bit_depth<integer_pixel_component<N>> {
            static constexpr int value = N;
        };

        template <int DestNBits, int SrcNBits>
        integer_pixel_component<DestNBits> convert_integer_high_quality(integer_pixel_component<SrcNBits> src) {
            if constexpr (DestNBits == SrcNBits) {
                return integer_pixel_component<DestNBits>{src.value};
            } else if constexpr (DestNBits > SrcNBits) {
                // high-quality widening conversion (abc -> abcabcab)
                using dest_int_t = int_leastN_t<DestNBits>;
                dest_int_t result = 0;
                dest_int_t val = src.value;
                for (int i = DestNBits; i > 0; i -= SrcNBits) {
                    result |= (i > SrcNBits) ? (val << (i - SrcNBits)) : (val >> (SrcNBits - i));
                }
                return integer_pixel_component<DestNBits>{result};
            } else { // DestNBits < SrcNBits
                // narrowing conversion
                static_assert(
                    SrcNBits + DestNBits <= 64,
                    "This conversion might overflow for large component sizes "
                    "on compilers without 128-bit integer support."
                );
                using wide_int_t = uint64_t;
                static constexpr wide_int_t max_src = (wide_int_t(1) << SrcNBits) - 1;
                static constexpr wide_int_t max_dest = (wide_int_t(1) << DestNBits) - 1;
                auto result = (wide_int_t(src.value) * max_dest + max_src / 2) / max_src;
                return integer_pixel_component<DestNBits>{static_cast<int_leastN_t<DestNBits>>(result)};
            }
        }

        template <int DestNBits>
        integer_pixel_component<DestNBits> convert_float_to_integer_high_quality(float_pixel_component src) {
            static_assert(DestNBits <= 32, "int components wider than 32-bit are not supported");
            static constexpr int_leastN_t<DestNBits> max_val = (static_cast<int_leastN_t<DestNBits + 1>>(1) << DestNBits) - 1;
            float scaled = src.value * max_val;
            if (scaled < 0)
                scaled = 0;
            if (scaled > max_val)
                scaled = max_val;
            return integer_pixel_component<DestNBits>{static_cast<int_leastN_t<DestNBits>>(scaled + 0.5f)};
        }

        template <int SrcNBits>
        float_pixel_component convert_integer_to_float_high_quality(integer_pixel_component<SrcNBits> src) {
            static_assert(SrcNBits <= 32, "int components wider than 32-bit are not supported");
            static constexpr int_leastN_t<SrcNBits> max_val = (static_cast<int_leastN_t<SrcNBits + 1>>(1) << SrcNBits) - 1;
            return float_pixel_component{static_cast<float>(src.value) / max_val};
        }

        template <typename Dest, typename Src>
        Dest convert_high_quality(Src const &src) {
            if constexpr (is_integer_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                static constexpr int SrcNBits = component_bit_depth<Src>::value;
                static constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_integer_high_quality<DestNBits, SrcNBits>(src);
            } else if constexpr (is_float_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                static constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_float_to_integer_high_quality<DestNBits>(src);
            } else if constexpr (is_integer_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                static constexpr int SrcNBits = component_bit_depth<Src>::value;
                return convert_integer_to_float_high_quality<SrcNBits>(src);
            } else if constexpr (is_float_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                return src;
            } else {
                static_assert(!std::is_same_v<Dest, Dest>, "Unsupported pixel component conversion");
            }
        }

        template <bool HighQuality, typename Dest, typename Src>
        Dest convert(Src const &src) {
            if constexpr (HighQuality) {
                return convert_high_quality<Dest>(src);
            } else {
                return convert_fast<Dest>(src);
            }
        }

        // Fast conversion between two integer components (no clamping or bit
        // replication)
        template <int DestNBits, int SrcNBits>
        integer_pixel_component<DestNBits> convert_integer_fast(integer_pixel_component<SrcNBits> src) {
            return integer_pixel_component<DestNBits>{static_cast<int_leastN_t<DestNBits>>(src.value)};
        }

        // Fast conversion from float to integer (no clamping)
        template <int DestNBits>
        integer_pixel_component<DestNBits> convert_float_to_integer_fast(float_pixel_component src) {
            static_assert(DestNBits <= 32, "int components wider than 32-bit are not supported");
            constexpr int_leastN_t<DestNBits> max_val = (static_cast<int_leastN_t<DestNBits + 1>>(1) << DestNBits) - 1;
            return integer_pixel_component<DestNBits>{static_cast<int_leastN_t<DestNBits>>(src.value * max_val)};
        }

        // Fast conversion from integer to float
        template <int SrcNBits>
        float_pixel_component convert_integer_to_float_fast(integer_pixel_component<SrcNBits> src) {
            static_assert(SrcNBits <= 32, "int components wider than 32-bit are not supported");
            constexpr int_leastN_t<SrcNBits> max_val = (static_cast<int_leastN_t<SrcNBits + 1>>(1) << SrcNBits) - 1;
            return float_pixel_component{static_cast<float>(src.value) / max_val};
        }

        // Main fast conversion function
        template <class Dest, class Src>
        Dest convert_fast(Src const &src) {
            if constexpr (is_integer_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int SrcNBits = component_bit_depth<Src>::value;
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_integer_fast<DestNBits, SrcNBits>(src);
            } else if constexpr (is_float_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_float_to_integer_fast<DestNBits>(src);
            } else if constexpr (is_integer_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                constexpr int SrcNBits = component_bit_depth<Src>::value;
                return convert_integer_to_float_fast<SrcNBits>(src);
            } else if constexpr (is_float_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                return src; // float to float is a copy
            } else {
                static_assert(!std::is_same_v<Dest, Dest>, "Unsupported pixel component conversion");
            }
        }

        // Dithering conversion for narrowing integer-to-integer (fast)
        template <int DestNBits, int SrcNBits>
        std::pair<integer_pixel_component<DestNBits>, integer_pixel_component<SrcNBits>>
        convert_for_dithering_integer_fast(integer_pixel_component<SrcNBits> src) {
            static_assert(DestNBits < SrcNBits, "This is for narrowing conversions only.");

            auto converted = convert_integer_fast<DestNBits, SrcNBits>(src);
            auto converted_back = convert_integer_fast<SrcNBits, DestNBits>(converted);
            auto error = integer_pixel_component<SrcNBits>{src.value - converted_back.value};

            return {converted, error};
        }

        // Dithering conversion for float-to-integer (fast)
        template <int DestNBits>
        std::pair<integer_pixel_component<DestNBits>, float_pixel_component> convert_for_dithering_float_fast(float_pixel_component src) {
            auto converted = convert_float_to_integer_fast<DestNBits>(src);
            auto converted_back = convert_integer_to_float_fast<DestNBits>(converted);
            auto error = float_pixel_component{src.value - converted_back.value};

            return {converted, error};
        }

        // Main dithering conversion function (fast)
        template <class Dest, class Src>
        std::pair<Dest, Src> convert_for_dithering_fast(Src const &src) {
            if constexpr (is_integer_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int SrcNBits = component_bit_depth<Src>::value;
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                if constexpr (DestNBits < SrcNBits) {
                    return convert_for_dithering_integer_fast<DestNBits, SrcNBits>(src);
                } else {
                    return {convert_fast(src), Src{0}};
                }
            } else if constexpr (is_float_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_for_dithering_float_fast<DestNBits>(src);
            } else if constexpr (std::is_same_v<Src, Dest>) {
                return {src, Src{0}};
            } else if constexpr (is_integer_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                return {convert_fast(src), Src{0}};
            } else {
                static_assert(!std::is_same_v<Dest, Dest>, "Unsupported pixel component conversion for dithering");
            }
        }

        template <int DestNBits, int SrcNBits>
        std::pair<integer_pixel_component<DestNBits>, integer_pixel_component<SrcNBits>> convert_for_dithering_integer(integer_pixel_component<SrcNBits> src) {
            static_assert(DestNBits < SrcNBits, "This is for narrowing conversions only.");

            auto converted = convert_integer<DestNBits, SrcNBits>(src);
            auto converted_back = convert_integer<SrcNBits, DestNBits>(converted);
            auto error = integer_pixel_component<SrcNBits>{src.value - converted_back.value};

            return {converted, error};
        }

        template <int DestNBits>
        std::pair<integer_pixel_component<DestNBits>, float_pixel_component> convert_for_dithering_float(float_pixel_component src) {
            auto converted = convert_float_to_integer<DestNBits>(src);
            auto converted_back = convert_integer_to_float<DestNBits>(converted);
            auto error = float_pixel_component{src.value - converted_back.value};

            return {converted, error};
        }

        template <typename Dest, typename Src>
        std::pair<Dest, Src> convert_for_dithering_high_quality(Src const &src) {
            if constexpr (is_integer_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int SrcNBits = component_bit_depth<Src>::value;
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                if constexpr (DestNBits < SrcNBits) {
                    return convert_for_dithering_integer_high_quality<DestNBits, SrcNBits>(src);
                } else {
                    return {convert_high_quality<Dest>(src), Src{0}};
                }
            } else if constexpr (is_float_pixel_component<Src>::value && is_integer_pixel_component<Dest>::value) {
                constexpr int DestNBits = component_bit_depth<Dest>::value;
                return convert_for_dithering_float_high_quality<DestNBits>(src);
            } else if constexpr (std::is_same_v<Src, Dest>) {
                return {src, Src{0}};
            } else if constexpr (is_integer_pixel_component<Src>::value && is_float_pixel_component<Dest>::value) {
                return {convert_high_quality<Dest>(src), Src{0}};
            } else {
                static_assert(!std::is_same_v<Dest, Dest>, "Unsupported pixel component conversion for dithering");
            }
        }
    } // namespace pixel_components

    namespace pixel_types {

        struct rgba8888 {
            uint8_t r, g, b, a;

            using component_r = pixel_components::integer_pixel_component<8>;
            using component_g = pixel_components::integer_pixel_component<8>;
            using component_b = pixel_components::integer_pixel_component<8>;
            using component_a = pixel_components::integer_pixel_component<8>;

            auto get_r() const { return pixel_components::integer_pixel_component<8>(r); }
            auto get_g() const { return pixel_components::integer_pixel_component<8>(g); }
            auto get_b() const { return pixel_components::integer_pixel_component<8>(b); }
            auto get_a() const { return pixel_components::integer_pixel_component<8>(a); }

            void set_r(const pixel_components::integer_pixel_component<8> &component) { r = component.value; }
            void set_g(const pixel_components::integer_pixel_component<8> &component) { g = component.value; }
            void set_b(const pixel_components::integer_pixel_component<8> &component) { b = component.value; }
            void set_a(const pixel_components::integer_pixel_component<8> &component) { a = component.value; }
        };
        struct rgb888 {
            uint8_t r, g, b;

            using component_r = pixel_components::integer_pixel_component<8>;
            using component_g = pixel_components::integer_pixel_component<8>;
            using component_b = pixel_components::integer_pixel_component<8>;
            using component_a = void;

            auto get_r() const { return pixel_components::integer_pixel_component<8>(r); }
            auto get_g() const { return pixel_components::integer_pixel_component<8>(g); }
            auto get_b() const { return pixel_components::integer_pixel_component<8>(b); }

            void set_r(const pixel_components::integer_pixel_component<8> &component) { r = component.value; }
            void set_g(const pixel_components::integer_pixel_component<8> &component) { g = component.value; }
            void set_b(const pixel_components::integer_pixel_component<8> &component) { b = component.value; }
        };
        struct argb8888 {
            uint8_t a, r, g, b;

            using component_a = pixel_components::integer_pixel_component<8>;
            using component_r = pixel_components::integer_pixel_component<8>;
            using component_g = pixel_components::integer_pixel_component<8>;
            using component_b = pixel_components::integer_pixel_component<8>;

            auto get_a() const { return pixel_components::integer_pixel_component<8>(a); }
            auto get_r() const { return pixel_components::integer_pixel_component<8>(r); }
            auto get_g() const { return pixel_components::integer_pixel_component<8>(g); }
            auto get_b() const { return pixel_components::integer_pixel_component<8>(b); }

            void set_a(const pixel_components::integer_pixel_component<8> &component) { a = component.value; }
            void set_r(const pixel_components::integer_pixel_component<8> &component) { r = component.value; }
            void set_g(const pixel_components::integer_pixel_component<8> &component) { g = component.value; }
            void set_b(const pixel_components::integer_pixel_component<8> &component) { b = component.value; }
        };
        struct bgra8888 {
            uint8_t b, g, r, a;

            using component_b = pixel_components::integer_pixel_component<8>;
            using component_g = pixel_components::integer_pixel_component<8>;
            using component_r = pixel_components::integer_pixel_component<8>;
            using component_a = pixel_components::integer_pixel_component<8>;

            auto get_b() const { return pixel_components::integer_pixel_component<8>(b); }
            auto get_g() const { return pixel_components::integer_pixel_component<8>(g); }
            auto get_r() const { return pixel_components::integer_pixel_component<8>(r); }
            auto get_a() const { return pixel_components::integer_pixel_component<8>(a); }

            void set_b(const pixel_components::integer_pixel_component<8> &component) { b = component.value; }
            void set_g(const pixel_components::integer_pixel_component<8> &component) { g = component.value; }
            void set_r(const pixel_components::integer_pixel_component<8> &component) { r = component.value; }
            void set_a(const pixel_components::integer_pixel_component<8> &component) { a = component.value; }
        };
        struct abgr8888 {
            uint8_t a, b, g, r;

            using component_a = pixel_components::integer_pixel_component<8>;
            using component_b = pixel_components::integer_pixel_component<8>;
            using component_g = pixel_components::integer_pixel_component<8>;
            using component_r = pixel_components::integer_pixel_component<8>;

            auto get_a() const { return pixel_components::integer_pixel_component<8>(a); }
            auto get_b() const { return pixel_components::integer_pixel_component<8>(b); }
            auto get_g() const { return pixel_components::integer_pixel_component<8>(g); }
            auto get_r() const { return pixel_components::integer_pixel_component<8>(r); }

            void set_a(const pixel_components::integer_pixel_component<8> &component) { a = component.value; }
            void set_b(const pixel_components::integer_pixel_component<8> &component) { b = component.value; }
            void set_g(const pixel_components::integer_pixel_component<8> &component) { g = component.value; }
            void set_r(const pixel_components::integer_pixel_component<8> &component) { r = component.value; }
        };
        struct rgb565 {
            uint16_t value;

            using component_r = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<6>;
            using component_b = pixel_components::integer_pixel_component<5>;
            using component_a = void;

            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 11) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<6>((value >> 5) & 0x3F); }
            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 0) & 0x1F); }
            void set_r(const pixel_components::integer_pixel_component<5> &component) {
                value &= 0x07FF;
                value |= component.value << 11;
            }
            void set_g(const pixel_components::integer_pixel_component<6> &component) {
                value &= 0xF81F;
                value |= component.value << 5;
            }
            void set_b(const pixel_components::integer_pixel_component<5> &component) {
                value &= 0xFFE0;
                value |= component.value << 0;
            }
        };
        struct gray8 {
            uint8_t value;
        };
        struct gray16 {
            uint16_t value;
        };
        struct indexed8 {
            uint8_t index;
        };
        struct rgb332 {
            uint8_t value;

            using component_r = pixel_components::integer_pixel_component<3>;
            using component_g = pixel_components::integer_pixel_component<3>;
            using component_b = pixel_components::integer_pixel_component<2>;
            using component_a = void;

            auto get_r() const { return pixel_components::integer_pixel_component<3>((value >> 5) & 0x07); }
            auto get_g() const { return pixel_components::integer_pixel_component<3>((value >> 2) & 0x07); }
            auto get_b() const { return pixel_components::integer_pixel_component<2>((value >> 0) & 0x03); }

            void set_r(const pixel_components::integer_pixel_component<3> &component) { value = (value & 0x1F) | (component.value << 5); }
            void set_g(const pixel_components::integer_pixel_component<3> &component) { value = (value & 0xE3) | (component.value << 2); }
            void set_b(const pixel_components::integer_pixel_component<2> &component) { value = (value & 0xFC) | (component.value << 0); }
        };
        struct bgr565 {
            uint16_t value;

            using component_b = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<6>;
            using component_r = pixel_components::integer_pixel_component<5>;
            using component_a = void;

            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 11) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<6>((value >> 5) & 0x3F); }
            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 0) & 0x1F); }

            void set_b(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0x07FF) | (component.value << 11); }
            void set_g(const pixel_components::integer_pixel_component<6> &component) { value = (value & 0xF81F) | (component.value << 5); }
            void set_r(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFFE0) | (component.value << 0); }
        };
        struct rgba4444 {
            uint16_t value;

            using component_r = pixel_components::integer_pixel_component<4>;
            using component_g = pixel_components::integer_pixel_component<4>;
            using component_b = pixel_components::integer_pixel_component<4>;
            using component_a = pixel_components::integer_pixel_component<4>;

            auto get_r() const { return pixel_components::integer_pixel_component<4>((value >> 12) & 0x0F); }
            auto get_g() const { return pixel_components::integer_pixel_component<4>((value >> 8) & 0x0F); }
            auto get_b() const { return pixel_components::integer_pixel_component<4>((value >> 4) & 0x0F); }
            auto get_a() const { return pixel_components::integer_pixel_component<4>((value >> 0) & 0x0F); }

            void set_r(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0x0FFF) | (component.value << 12); }
            void set_g(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xF0FF) | (component.value << 8); }
            void set_b(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFF0F) | (component.value << 4); }
            void set_a(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFFF0) | (component.value << 0); }
        };
        struct argb4444 {
            uint16_t value;

            using component_a = pixel_components::integer_pixel_component<4>;
            using component_r = pixel_components::integer_pixel_component<4>;
            using component_g = pixel_components::integer_pixel_component<4>;
            using component_b = pixel_components::integer_pixel_component<4>;

            auto get_a() const { return pixel_components::integer_pixel_component<4>((value >> 12) & 0x0F); }
            auto get_r() const { return pixel_components::integer_pixel_component<4>((value >> 8) & 0x0F); }
            auto get_g() const { return pixel_components::integer_pixel_component<4>((value >> 4) & 0x0F); }
            auto get_b() const { return pixel_components::integer_pixel_component<4>((value >> 0) & 0x0F); }

            void set_a(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0x0FFF) | (component.value << 12); }
            void set_r(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xF0FF) | (component.value << 8); }
            void set_g(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFF0F) | (component.value << 4); }
            void set_b(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFFF0) | (component.value << 0); }
        };
        struct bgra4444 {
            uint16_t value;

            using component_b = pixel_components::integer_pixel_component<4>;
            using component_g = pixel_components::integer_pixel_component<4>;
            using component_r = pixel_components::integer_pixel_component<4>;
            using component_a = pixel_components::integer_pixel_component<4>;

            auto get_b() const { return pixel_components::integer_pixel_component<4>((value >> 12) & 0x0F); }
            auto get_g() const { return pixel_components::integer_pixel_component<4>((value >> 8) & 0x0F); }
            auto get_r() const { return pixel_components::integer_pixel_component<4>((value >> 4) & 0x0F); }
            auto get_a() const { return pixel_components::integer_pixel_component<4>((value >> 0) & 0x0F); }

            void set_b(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0x0FFF) | (component.value << 12); }
            void set_g(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xF0FF) | (component.value << 8); }
            void set_r(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFF0F) | (component.value << 4); }
            void set_a(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFFF0) | (component.value << 0); }
        };
        struct abgr4444 {
            uint16_t value;

            using component_a = pixel_components::integer_pixel_component<4>;
            using component_b = pixel_components::integer_pixel_component<4>;
            using component_g = pixel_components::integer_pixel_component<4>;
            using component_r = pixel_components::integer_pixel_component<4>;

            auto get_a() const { return pixel_components::integer_pixel_component<4>((value >> 12) & 0x0F); }
            auto get_b() const { return pixel_components::integer_pixel_component<4>((value >> 8) & 0x0F); }
            auto get_g() const { return pixel_components::integer_pixel_component<4>((value >> 4) & 0x0F); }
            auto get_r() const { return pixel_components::integer_pixel_component<4>((value >> 0) & 0x0F); }

            void set_a(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0x0FFF) | (component.value << 12); }
            void set_b(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xF0FF) | (component.value << 8); }
            void set_g(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFF0F) | (component.value << 4); }
            void set_r(const pixel_components::integer_pixel_component<4> &component) { value = (value & 0xFFF0) | (component.value << 0); }
        };
        struct rgba5551 {
            uint16_t value;

            using component_r = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<5>;
            using component_b = pixel_components::integer_pixel_component<5>;
            using component_a = pixel_components::integer_pixel_component<1>;

            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 11) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<5>((value >> 6) & 0x1F); }
            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 1) & 0x1F); }
            auto get_a() const { return pixel_components::integer_pixel_component<1>((value >> 0) & 0x01); }

            void set_r(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0x07FF) | (component.value << 11); }
            void set_g(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xF83F) | (component.value << 6); }
            void set_b(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFFC1) | (component.value << 1); }
            void set_a(const pixel_components::integer_pixel_component<1> &component) { value = (value & 0xFFFE) | (component.value << 0); }
        };
        struct argb1555 {
            uint16_t value;

            using component_a = pixel_components::integer_pixel_component<1>;
            using component_r = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<5>;
            using component_b = pixel_components::integer_pixel_component<5>;

            auto get_a() const { return pixel_components::integer_pixel_component<1>((value >> 15) & 0x01); }
            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 10) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<5>((value >> 5) & 0x1F); }
            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 0) & 0x1F); }

            void set_a(const pixel_components::integer_pixel_component<1> &component) { value = (value & 0x7FFF) | (component.value << 15); }
            void set_r(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0x83FF) | (component.value << 10); }
            void set_g(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFC1F) | (component.value << 5); }
            void set_b(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFFE0) | (component.value << 0); }
        };
        struct bgra5551 {
            uint16_t value;

            using component_b = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<5>;
            using component_r = pixel_components::integer_pixel_component<5>;
            using component_a = pixel_components::integer_pixel_component<1>;

            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 11) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<5>((value >> 6) & 0x1F); }
            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 1) & 0x1F); }
            auto get_a() const { return pixel_components::integer_pixel_component<1>((value >> 0) & 0x01); }

            void set_b(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0x07FF) | (component.value << 11); }
            void set_g(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xF83F) | (component.value << 6); }
            void set_r(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFFC1) | (component.value << 1); }
            void set_a(const pixel_components::integer_pixel_component<1> &component) { value = (value & 0xFFFE) | (component.value << 0); }
        };
        struct abgr1555 {
            uint16_t value;

            using component_a = pixel_components::integer_pixel_component<1>;
            using component_b = pixel_components::integer_pixel_component<5>;
            using component_g = pixel_components::integer_pixel_component<5>;
            using component_r = pixel_components::integer_pixel_component<5>;

            auto get_a() const { return pixel_components::integer_pixel_component<1>((value >> 15) & 0x01); }
            auto get_b() const { return pixel_components::integer_pixel_component<5>((value >> 10) & 0x1F); }
            auto get_g() const { return pixel_components::integer_pixel_component<5>((value >> 5) & 0x1F); }
            auto get_r() const { return pixel_components::integer_pixel_component<5>((value >> 0) & 0x1F); }

            void set_a(const pixel_components::integer_pixel_component<1> &component) { value = (value & 0x7FFF) | (component.value << 15); }
            void set_b(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0x83FF) | (component.value << 10); }
            void set_g(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFC1F) | (component.value << 5); }
            void set_r(const pixel_components::integer_pixel_component<5> &component) { value = (value & 0xFFE0) | (component.value << 0); }
        };
        struct gray_f32 {
            float value;

            using component_gray = pixel_components::float_pixel_component;
            using component_r = void;
            using component_g = void;
            using component_b = void;
            using component_a = void;

            auto get_gray() const { return pixel_components::float_pixel_component{value}; }
            void set_gray(const pixel_components::float_pixel_component &component) { value = component.value; }
        };
        struct rgb_f32 {
            float r, g, b;

            using component_r = pixel_components::float_pixel_component;
            using component_g = pixel_components::float_pixel_component;
            using component_b = pixel_components::float_pixel_component;
            using component_a = void;

            auto get_r() const { return pixel_components::float_pixel_component{r}; }
            auto get_g() const { return pixel_components::float_pixel_component{g}; }
            auto get_b() const { return pixel_components::float_pixel_component{b}; }

            void set_r(const pixel_components::float_pixel_component &component) { r = component.value; }
            void set_g(const pixel_components::float_pixel_component &component) { g = component.value; }
            void set_b(const pixel_components::float_pixel_component &component) { b = component.value; }
        };
        struct rgba_f32 {
            float r, g, b, a;

            using component_r = pixel_components::float_pixel_component;
            using component_g = pixel_components::float_pixel_component;
            using component_b = pixel_components::float_pixel_component;
            using component_a = pixel_components::float_pixel_component;

            auto get_r() const { return pixel_components::float_pixel_component{r}; }
            auto get_g() const { return pixel_components::float_pixel_component{g}; }
            auto get_b() const { return pixel_components::float_pixel_component{b}; }
            auto get_a() const { return pixel_components::float_pixel_component{a}; }

            void set_r(const pixel_components::float_pixel_component &component) { r = component.value; }
            void set_g(const pixel_components::float_pixel_component &component) { g = component.value; }
            void set_b(const pixel_components::float_pixel_component &component) { b = component.value; }
            void set_a(const pixel_components::float_pixel_component &component) { a = component.value; }
        };

    } // namespace pixel_types

    namespace detail {

        template <pixel_format Fmt>
        struct pixel_type {
            using type = void;
        };
        template <>
        struct pixel_type<pixel_format::rgba8888> {
            using type = pixel_types::rgba8888;
        };
        template <>
        struct pixel_type<pixel_format::rgb888> {
            using type = pixel_types::rgb888;
        };
        template <>
        struct pixel_type<pixel_format::argb8888> {
            using type = pixel_types::argb8888;
        };
        template <>
        struct pixel_type<pixel_format::bgra8888> {
            using type = pixel_types::bgra8888;
        };
        template <>
        struct pixel_type<pixel_format::abgr8888> {
            using type = pixel_types::abgr8888;
        };
        template <>
        struct pixel_type<pixel_format::rgb565> {
            using type = pixel_types::rgb565;
        };
        template <>
        struct pixel_type<pixel_format::gray8> {
            using type = pixel_types::gray8;
        };
        template <>
        struct pixel_type<pixel_format::gray16> {
            using type = pixel_types::gray16;
        };
        template <>
        struct pixel_type<pixel_format::indexed8> {
            using type = pixel_types::indexed8;
        };
        template <>
        struct pixel_type<pixel_format::rgb332> {
            using type = pixel_types::rgb332;
        };
        template <>
        struct pixel_type<pixel_format::bgr565> {
            using type = pixel_types::bgr565;
        };
        template <>
        struct pixel_type<pixel_format::rgba4444> {
            using type = pixel_types::rgba4444;
        };
        template <>
        struct pixel_type<pixel_format::argb4444> {
            using type = pixel_types::argb4444;
        };
        template <>
        struct pixel_type<pixel_format::bgra4444> {
            using type = pixel_types::bgra4444;
        };
        template <>
        struct pixel_type<pixel_format::abgr4444> {
            using type = pixel_types::abgr4444;
        };
        template <>
        struct pixel_type<pixel_format::rgba5551> {
            using type = pixel_types::rgba5551;
        };
        template <>
        struct pixel_type<pixel_format::argb1555> {
            using type = pixel_types::argb1555;
        };
        template <>
        struct pixel_type<pixel_format::bgra5551> {
            using type = pixel_types::bgra5551;
        };
        template <>
        struct pixel_type<pixel_format::abgr1555> {
            using type = pixel_types::abgr1555;
        };
        template <>
        struct pixel_type<pixel_format::gray_f32> {
            using type = pixel_types::gray_f32;
        };
        template <>
        struct pixel_type<pixel_format::rgb_f32> {
            using type = pixel_types::rgb_f32;
        };
        template <>
        struct pixel_type<pixel_format::rgba_f32> {
            using type = pixel_types::rgba_f32;
        };
    } // namespace detail

    template <pixel_format Fmt>
    using pixel = detail::pixel_type<Fmt>::type;

    using pixel_rgb888 = pixel<pixel_format::rgb888>;
    using pixel_rgba8888 = pixel<pixel_format::rgba8888>;
    using pixel_argb8888 = pixel<pixel_format::argb8888>;
    using pixel_bgra8888 = pixel<pixel_format::bgra8888>;
    using pixel_abgr8888 = pixel<pixel_format::abgr8888>;
    using pixel_rgb565 = pixel<pixel_format::rgb565>;
    using pixel_gray8 = pixel<pixel_format::gray8>;
    using pixel_gray16 = pixel<pixel_format::gray16>;
    using pixel_indexed8 = pixel<pixel_format::indexed8>;
    using pixel_rgb332 = pixel<pixel_format::rgb332>;
    using pixel_bgr565 = pixel<pixel_format::bgr565>;
    using pixel_rgba4444 = pixel<pixel_format::rgba4444>;
    using pixel_argb4444 = pixel<pixel_format::argb4444>;
    using pixel_bgra4444 = pixel<pixel_format::bgra4444>;
    using pixel_abgr4444 = pixel<pixel_format::abgr4444>;
    using pixel_rgba5551 = pixel<pixel_format::rgba5551>;
    using pixel_argb1555 = pixel<pixel_format::argb1555>;
    using pixel_bgra5551 = pixel<pixel_format::bgra5551>;
    using pixel_abgr1555 = pixel<pixel_format::abgr1555>;
    using pixel_gray_f32 = pixel<pixel_format::gray_f32>;
    using pixel_rgb_f32 = pixel<pixel_format::rgb_f32>;
    using pixel_rgba_f32 = pixel<pixel_format::rgba_f32>;

    namespace detail {
        template <bool HighQuality, typename TPixelResult, typename TPixel>
        TPixelResult convert_pixel_impl(const TPixel &pixel) {
            TPixelResult result;

            static constexpr bool src_has_r = not std::is_same_v<typename TPixel::component_r, void>;
            static constexpr bool src_has_g = not std::is_same_v<typename TPixel::component_g, void>;
            static constexpr bool src_has_b = not std::is_same_v<typename TPixel::component_b, void>;
            static constexpr bool src_has_a = not std::is_same_v<typename TPixel::component_a, void>;
            static constexpr bool dst_has_r = not std::is_same_v<typename TPixelResult::component_r, void>;
            static constexpr bool dst_has_g = not std::is_same_v<typename TPixelResult::component_g, void>;
            static constexpr bool dst_has_b = not std::is_same_v<typename TPixelResult::component_b, void>;
            static constexpr bool dst_has_a = not std::is_same_v<typename TPixelResult::component_a, void>;

            if constexpr (src_has_r && dst_has_r) {
                result.set_r(pixel_components::convert<HighQuality, typename TPixelResult::component_r>(pixel.get_r()));
            }

            if constexpr (src_has_g && dst_has_g) {
                result.set_g(pixel_components::convert<HighQuality, typename TPixelResult::component_g>(pixel.get_g()));
            }

            if constexpr (src_has_b && dst_has_b) {
                result.set_b(pixel_components::convert<HighQuality, typename TPixelResult::component_b>(pixel.get_b()));
            }

            if constexpr (src_has_a && dst_has_a) {
                result.set_a(pixel_components::convert<HighQuality, typename TPixelResult::component_a>(pixel.get_a()));
            } else if constexpr (!src_has_a && dst_has_a) {
                // source has alpha but dst does not - set to opaque
                using DestAComponent = typename TPixelResult::component_a;

                if constexpr (pixel_components::is_integer_pixel_component<DestAComponent>::value) {
                    constexpr int NBits = pixel_components::component_bit_depth<DestAComponent>::value;
                    result.set_a(pixel_components::integer_pixel_component<NBits>{(1 << NBits) - 1});
                } else if constexpr (pixel_components::is_float_pixel_component<DestAComponent>::value) {
                    result.set_a(pixel_components::float_pixel_component{1.0f});
                }

            } else {
                // dst has no alpha - do nothing
            }
            return result;
        }
    } // namespace detail

    template <typename TPixelResult, typename TPixel>
    TPixelResult convert_pixel_high_quality(const TPixel &pixel) {
        return detail::convert_pixel_impl<true, TPixelResult>(pixel);
    }

    template <typename TPixelResult, typename TPixel>
    TPixelResult convert_pixel_fast(const TPixel &pixel) {
        return detail::convert_pixel_impl<false, TPixelResult>(pixel);
    }

    inline size_t bytes_per_pixel(pixel_format fmt) {
        switch (fmt) {
        case pixel_format::rgba8888: return sizeof(pixel_rgba8888);
        case pixel_format::rgb888: return sizeof(pixel_rgb888);
        case pixel_format::argb8888: return sizeof(pixel_argb8888);
        case pixel_format::bgra8888: return sizeof(pixel_bgra8888);
        case pixel_format::abgr8888: return sizeof(pixel_abgr8888);
        case pixel_format::rgb565: return sizeof(pixel_rgb565);
        case pixel_format::gray8: return sizeof(pixel_gray8);
        case pixel_format::gray16: return sizeof(pixel_gray16);
        case pixel_format::indexed8: return sizeof(pixel_indexed8);
        case pixel_format::rgb332: return sizeof(pixel_rgb332);
        case pixel_format::bgr565: return sizeof(pixel_bgr565);
        case pixel_format::rgba4444: return sizeof(pixel_rgba4444);
        case pixel_format::argb4444: return sizeof(pixel_argb4444);
        case pixel_format::bgra4444: return sizeof(pixel_bgra4444);
        case pixel_format::abgr4444: return sizeof(pixel_abgr4444);
        case pixel_format::rgba5551: return sizeof(pixel_rgba5551);
        case pixel_format::argb1555: return sizeof(pixel_argb1555);
        case pixel_format::bgra5551: return sizeof(pixel_bgra5551);
        case pixel_format::abgr1555: return sizeof(pixel_abgr1555);
        case pixel_format::gray_f32: return sizeof(pixel_gray_f32);
        case pixel_format::rgb_f32: return sizeof(pixel_rgb_f32);
        case pixel_format::rgba_f32: return sizeof(pixel_rgba_f32);
        default: return 0;
        }
    }

    // TODO convert_for_dithering

} // namespace alia

#endif /* PIXEL_F150483D_5A18_4194_9A28_C08956350E5B */
