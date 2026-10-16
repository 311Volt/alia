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

file(GLOB_RECURSE alia_headers CONFIGURE_DEPENDS "src/*.hpp" "src/*.h" "src/*.inc")
file(GLOB_RECURSE alia_sources CONFIGURE_DEPENDS "src/*.cpp" "src/*.c")

option(ALIA_STATIC "Build alia as a static library" ON)
if(ALIA_STATIC)
  add_library(alia STATIC ${alia_headers} ${alia_sources})
else()
  add_library(alia SHARED ${alia_headers} ${alia_sources})
endif()

# include("backends.cmake")

option(ALIA_USE_KOKKOS_MDSPAN "Use Kokkos MDSPAN implementation instead of std::mdspan (for libstdc++ <16 and libc++ <18)" ON)
option(ALIA_BUILD_EXAMPLES "Build example programs" ON)
option(ALIA_STATIC_RUNTIME "Statically link the C/C++ runtime (useful with MinGW to avoid DLL mismatch)" OFF)

if(ALIA_STATIC_RUNTIME)
  if(MINGW)
    target_link_options(alia PUBLIC -static-libgcc -static-libstdc++
      -Wl,-Bstatic,-lwinpthread,-Bdynamic)
  elseif(MSVC)
    set_property(TARGET alia PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()

if (NOT ALIA_MDSPAN_FEATURE_TEST_OK)
  set(ALIA_USE_KOKKOS_MDSPAN ON)
endif()

if (ALIA_USE_KOKKOS_MDSPAN)
  find_package(mdspan CONFIG REQUIRED)
  if(TARGET std::mdspan)
    target_link_libraries(alia PUBLIC std::mdspan)
  else()
    target_link_libraries(alia PUBLIC mdspan::mdspan)
  endif()
  target_compile_definitions(alia PUBLIC ALIA_USE_KOKKOS_MDSPAN)
endif()

find_package(Freetype REQUIRED)
target_link_libraries(alia PRIVATE Freetype::Freetype)

find_package(Threads REQUIRED)
target_link_libraries(alia PRIVATE Threads::Threads)

set(ALIA_DR_LIBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/dr_libs")
if(NOT EXISTS "${ALIA_DR_LIBS_DIR}/dr_wav.h")
  message(FATAL_ERROR "dr_libs submodule missing: run 'git submodule update --init external/dr_libs'")
endif()
target_include_directories(alia SYSTEM PRIVATE "${ALIA_DR_LIBS_DIR}")

target_include_directories(alia PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:include>)

# Platform backend options
if(WIN32)
  option(ALIA_COMPILE_PLATFORM_BACKEND_WIN32 "Compile Win32 platform backend" ON)
  option(ALIA_COMPILE_PLATFORM_BACKEND_X11   "Compile X11 platform backend"   OFF)
else()
  option(ALIA_COMPILE_PLATFORM_BACKEND_WIN32 "Compile Win32 platform backend" OFF)
  option(ALIA_COMPILE_PLATFORM_BACKEND_X11   "Compile X11 platform backend"   ON)
endif()

if(ALIA_COMPILE_PLATFORM_BACKEND_WIN32)
  if(NOT WIN32)
    message(FATAL_ERROR "The Win32 OS backend requires Windows")
  endif()
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_PLATFORM_BACKEND_WIN32)
  target_link_libraries(alia PRIVATE user32 gdi32 ole32 shell32 uuid comdlg32)
endif()

if(ALIA_COMPILE_PLATFORM_BACKEND_X11)
  if(NOT UNIX)
    message(FATAL_ERROR "The X11 OS backend requires a Unix-like platform")
  endif()
  find_package(X11 REQUIRED)
  set(alia_x11_Xrandr_header X11/extensions/Xrandr.h)
  set(alia_x11_Xi_header X11/extensions/XInput2.h)
  set(alia_x11_Xcursor_header X11/Xcursor/Xcursor.h)
  set(alia_x11_Xss_header X11/extensions/scrnsaver.h)
  foreach(alia_x11_component Xrandr Xi Xcursor Xss)
    find_path(alia_x11_${alia_x11_component}_include NAMES ${alia_x11_${alia_x11_component}_header} HINTS ${X11_INCLUDE_DIR})
    find_library(alia_x11_${alia_x11_component}_library NAMES ${alia_x11_component})
    if(NOT alia_x11_${alia_x11_component}_include OR NOT alia_x11_${alia_x11_component}_library)
      message(FATAL_ERROR "The X11 OS backend requires ${alia_x11_component} development files")
    endif()
    target_include_directories(alia PRIVATE ${alia_x11_${alia_x11_component}_include})
    target_link_libraries(alia PRIVATE ${alia_x11_${alia_x11_component}_library})
  endforeach()
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_PLATFORM_BACKEND_X11)
  target_include_directories(alia PRIVATE ${X11_INCLUDE_DIR})
  target_link_libraries(alia PRIVATE ${X11_LIBRARIES})
endif()

# Graphics backend options
if(WIN32)
  option(ALIA_COMPILE_GFX_BACKEND_D3D9   "Compile D3D9 graphics backend"   ON)
  option(ALIA_COMPILE_GFX_BACKEND_OPENGL "Compile OpenGL graphics backend" ON)
else()
  option(ALIA_COMPILE_GFX_BACKEND_D3D9   "Compile D3D9 graphics backend"   OFF)
  option(ALIA_COMPILE_GFX_BACKEND_OPENGL "Compile OpenGL graphics backend" ON)
endif()

if(ALIA_COMPILE_GFX_BACKEND_D3D9)
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_GFX_BACKEND_D3D9)
  target_link_libraries(alia PRIVATE d3d9 d3dcompiler)
endif()

if(ALIA_COMPILE_GFX_BACKEND_OPENGL)
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_GFX_BACKEND_OPENGL)
  if(WIN32)
    target_link_libraries(alia PRIVATE opengl32)
  else()
    target_link_libraries(alia PRIVATE GL)
  endif()
endif()

# Audio backends.
option(ALIA_COMPILE_AUDIO_BACKEND_WASAPI "Compile WASAPI audio backend (Windows)" ${WIN32})
option(ALIA_COMPILE_AUDIO_BACKEND_MME "Compile MME audio backend (Windows)" ${WIN32})
set(alia_alsa_default OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(alia_alsa_default ON)
endif()
option(ALIA_COMPILE_AUDIO_BACKEND_ALSA "Compile ALSA audio backend (Linux)" ${alia_alsa_default})
if(ALIA_COMPILE_AUDIO_BACKEND_WASAPI)
  if(NOT WIN32)
    message(FATAL_ERROR "The WASAPI audio backend requires Windows")
  endif()
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_AUDIO_BACKEND_WASAPI)
  target_link_libraries(alia PRIVATE ole32)
endif()
if(ALIA_COMPILE_AUDIO_BACKEND_MME)
  if(NOT WIN32)
    message(FATAL_ERROR "The MME audio backend requires Windows")
  endif()
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_AUDIO_BACKEND_MME)
  target_link_libraries(alia PRIVATE winmm)
endif()
if(ALIA_COMPILE_AUDIO_BACKEND_ALSA)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The ALSA audio backend requires Linux")
  endif()
  find_package(ALSA 1.0.27 REQUIRED)
  target_compile_definitions(alia PUBLIC ALIA_COMPILE_AUDIO_BACKEND_ALSA)
  target_include_directories(alia PRIVATE ${ALSA_INCLUDE_DIRS})
  target_link_libraries(alia PRIVATE ${ALSA_LIBRARIES})
endif()

include(GNUInstallDirs)
install(TARGETS alia
    EXPORT alia-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

foreach(header ${alia_headers})
    file(RELATIVE_PATH relative_header "${CMAKE_CURRENT_SOURCE_DIR}/src" "${header}")
    get_filename_component(header_dir "${relative_header}" DIRECTORY)
    install(FILES "${header}" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${header_dir}")
endforeach()

install(EXPORT alia-targets
    FILE alia-config.cmake
    NAMESPACE alia::
    DESTINATION lib/cmake/alia
)

if(ALIA_BUILD_EXAMPLES)
  add_custom_target(alia_examples)
  add_subdirectory("examples")
endif()

include(CTest)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Testing
option(ALIA_BUILD_TESTS "Build tests" ON)
if(ALIA_BUILD_TESTS)
  enable_testing()
  find_package(GTest REQUIRED)
  add_subdirectory("test")
endif()
