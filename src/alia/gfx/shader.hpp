#ifndef SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8
#define SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8

#include <span>
#include <string_view>

#include "alia/core/vec.hpp"
#include "alia/gfx/transform.hpp"


namespace alia {

	enum class shader_type {
		pixel,
		vertex
	};


	class texture_handle;

	template<typename T>
	concept scalar_shader_constant_value = 
		std::is_integral_v<T> || std::is_floating_point_v<T>;
	
	template<typename T>
	concept vector_shader_constant_value = false; //is instantiation of vec2, vec3, vec4

	template<typename T>
	concept matrix_shader_constant_value = false; //span<float,16> or alia::transform

	template<typename T>
	concept shader_constant_value = 
		   scalar_shader_constant_value<T> 
		|| vector_shader_constant_value<T> 
		|| matrix_shader_constant_value<T>;


	template<shader_constant_value TValue>
	class shader_constant {
	public:
		void set_value(const TValue& value);
		TValue get_value();
	private:
		int location_; //implementation-defined register/uniform location
	};

	class shader_sampler {
	public:
		void set_texture(texture_handle* handle);
	private:
		int location_; //implementation-defined register/uniform location
	};

	class vertex_shader {

		bool attach_source(int backend_id, const std::string_view source);
		bool attach_source_file(int backend_id, const std::string_view source);

		bool use();

		template<shader_constant_value TValue>
		shader_constant<TValue> allocate_constant(const std::string_view identifier);
		shader_sampler allocate_sampler(const std::string_view identifier);

		static int num_available_samplers();

	};

	//class pixel_shader {}

}

#endif /* SHADER_A879FFEC_0EF8_44FE_9368_C7B98EF8ACF8 */
