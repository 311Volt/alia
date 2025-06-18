#ifndef META_EA2ED3CF_5157_4717_ACE6_F287345A9379
#define META_EA2ED3CF_5157_4717_ACE6_F287345A9379

#include <stdint.h>

namespace alia {
	
	namespace detail {

		template<typename T>
		struct product_result {
			using type = double;
		};

		template<> struct product_result<float> {using type = float;};
		template<> struct product_result<long double> {using type = long double;};

		template<> struct product_result<int8_t> {using type = int16_t;};
		template<> struct product_result<int16_t> {using type = int32_t;};
		template<> struct product_result<int32_t> {using type = int64_t;};
		template<> struct product_result<int64_t> {using type = double;};

		template<> struct product_result<uint8_t> {using type = uint16_t;};
		template<> struct product_result<uint16_t> {using type = uint32_t;};
		template<> struct product_result<uint32_t> {using type = uint64_t;};
		template<> struct product_result<uint64_t> {using type = double;};

		template<typename T>
		using product_result_t = typename product_result<T>::type;
	}

}

#endif /* META_EA2ED3CF_5157_4717_ACE6_F287345A9379 */
