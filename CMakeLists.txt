cmake_minimum_required(VERSION 3.10.0)
project(alia VERSION 0.1.0)

if (NOT DEFINED ALIA_MASTER_PROJECT)
	set(ALIA_MASTER_PROJECT OFF)
	if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
		set(ALIA_MASTER_PROJECT ON)
	endif ()
endif ()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)


include(CheckCXXSourceCompiles)
check_cxx_source_compiles([[
  #if __cpp_lib_mdspan < 202207L
  #error "__cpp_lib_mdspan is too old"
  #endif
  #if __cpp_lib_submdspan < 202403L
  #error "__cpp_lib_submdspan is too old"
  #endif
  int main() { return 0; }
]] ALIA_MDSPAN_FEATURE_TEST_OK)

file(GLOB_RECURSE alia_headers "include/*.hpp")
add_library(alia INTERFACE ${alia_headers})

include("backends.cmake")

add_subdirectory(mdspan)

option(ALIA_USE_KOKKOS_MDSPAN "Use Kokkos MDSPAN implementation instead of std::mdspan" ON)

if (NOT ALIA_MDSPAN_FEATURE_TEST_OK)
  set(ALIA_USE_KOKKOS_MDSPAN ON)
endif()

if (ALIA_USE_KOKKOS_MDSPAN)
  target_link_libraries(alia INTERFACE mdspan)
  target_compile_definitions(alia INTERFACE ALIA_USE_KOKKOS_MDSPAN)
endif()

target_include_directories(alia INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

option(ALIA_BUILD_EXAMPLES "Build example programs" ${ALIA_MASTER_PROJECT})

if(ALIA_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

