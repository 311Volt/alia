#include <iostream>
#include <chrono>
#include <string>
#include <type_traits> // For std::invoke_result_t

#include <alia/alia.hpp>
#include <alia/graphics/color_spaces.hpp>
#include <alia/util/vec.hpp> // For vec3f and vec4f

#include <SDL_main.h>

// Define a generic benchmark runner
template<typename InputVec, typename Func>
void run_benchmark(const std::string& name, Func func, const InputVec& input_value) {
    using OutputVec = std::invoke_result_t<Func, InputVec>;
    const long long iterations = 100'000'000;

    // Accumulate results to prevent optimization
    OutputVec total_result{};

    auto start = std::chrono::high_resolution_clock::now();
    for (long long i = 0; i < iterations; ++i) {
        total_result += func(input_value);
    }
    auto end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double> elapsed = end - start;
    std::cout << name << ": " << elapsed.count() << " seconds";
    // Print a small part of the total_result to ensure it's not optimized away
    // This assumes OutputVec has an operator[] or similar for access.
    // For vec3f/vec4f, accessing the first component is sufficient.
    std::cout << "  (Verification: " << total_result[0] << ")" << std::endl;
}

int main(int argc, char** argv) {
    std::cout << "Running color space conversion benchmarks..." << std::endl;

    // Define sample input colors for each conversion type
    alia::vec3f srgb_input(0.5f, 0.5f, 0.5f);
    alia::vec4f cmyk_input(0.5f, 0.5f, 0.5f, 0.5f);
    alia::vec3f hsl_input(180.0f, 0.5f, 0.5f);
    alia::vec3f hsv_input(180.0f, 0.5f, 0.5f);
    alia::vec3f xyz_input(0.5f, 0.5f, 0.5f);
    alia::vec3f xyy_input(0.3127f, 0.3290f, 0.5f);
    alia::vec3f lab_input(50.0f, 0.0f, 0.0f);
    alia::vec3f lch_input(50.0f, 0.0f, 0.0f);
    alia::vec3f yuv_input(0.5f, 0.0f, 0.0f);
    alia::vec3f oklab_input(0.5f, 0.0f, 0.0f);

    // Run benchmarks for each conversion function
    using namespace alia::color_space_conversions;

    run_benchmark("srgb_to_cmyk", srgb_to_cmyk, srgb_input);
    run_benchmark("cmyk_to_srgb", cmyk_to_srgb, cmyk_input);
    run_benchmark("srgb_to_hsl", srgb_to_hsl, srgb_input);
    run_benchmark("hsl_to_srgb", hsl_to_srgb, hsl_input);
    run_benchmark("srgb_to_hsv", srgb_to_hsv, srgb_input);
    run_benchmark("hsv_to_srgb", hsv_to_srgb, hsv_input);
    run_benchmark("srgb_to_xyz", srgb_to_xyz, srgb_input);
    run_benchmark("xyz_to_srgb", xyz_to_srgb, xyz_input);
    run_benchmark("srgb_to_xyy", srgb_to_xyy, srgb_input);
    run_benchmark("xyy_to_srgb", xyy_to_srgb, xyy_input);
    run_benchmark("srgb_to_lab", srgb_to_lab, srgb_input);
    run_benchmark("lab_to_srgb", lab_to_srgb, lab_input);
    run_benchmark("srgb_to_lch", srgb_to_lch, srgb_input);
    run_benchmark("lch_to_srgb", lch_to_srgb, lch_input);
    run_benchmark("srgb_to_yuv", srgb_to_yuv, srgb_input);
    run_benchmark("yuv_to_srgb", yuv_to_srgb, yuv_input);
    run_benchmark("srgb_to_oklab", srgb_to_oklab, srgb_input);
    run_benchmark("oklab_to_srgb", oklab_to_srgb, oklab_input);
    run_benchmark("srgb_to_linear", srgb_to_linear, srgb_input);
    run_benchmark("linear_to_srgb", linear_to_srgb, srgb_input);

    return 0;
}
