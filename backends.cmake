# TODO come up with something smarter (ie more DRY)
# for now we rely on AI to maintain this file

set(ALIA_BACKEND_GFX_BASIC "SDL2" CACHE STRING "Select the basic graphics backend")
set_property(CACHE ALIA_BACKEND_GFX_BASIC PROPERTY STRINGS  "SDL2" "D3D9" "OGL")

set(ALIA_BACKEND_GFX_HW_CLASSIC "OGL" CACHE STRING "Select the classic hardware-accelerated graphics backend")
set_property(CACHE ALIA_BACKEND_GFX_HW_CLASSIC PROPERTY STRINGS  "D3D9" "OGL")

set(ALIA_BACKEND_GFX_HW_MODERN "Vulkan" CACHE STRING "Select the modern hardware-accelerated graphics backend")
set_property(CACHE ALIA_BACKEND_GFX_HW_MODERN PROPERTY STRINGS "D3D12" "Vulkan" "Metal")

set(ALIA_BACKEND_AUDIO "SDL2" CACHE STRING "Select the audio backend")
set_property(CACHE ALIA_BACKEND_AUDIO PROPERTY STRINGS "SDL2" "OpenAL")

set(ALIA_BACKEND_INPUT "SDL2" CACHE STRING "Select the input backend")
set_property(CACHE ALIA_BACKEND_INPUT PROPERTY STRINGS "SDL2" "GLFW")

set(ALIA_BACKEND_PLATFORM "SDL2" CACHE STRING "Select the platform backend")
set_property(CACHE ALIA_BACKEND_PLATFORM PROPERTY STRINGS "SDL2" "GLFW" "Win32" "X11")

set(ALIA_USE_DEP_SDL2 OFF)
set(ALIA_USE_DEP_D3D9 OFF)
set(ALIA_USE_DEP_OGL OFF)
set(ALIA_USE_DEP_GLFW OFF)

if(ALIA_BACKEND_GFX_BASIC STREQUAL "SDL2" OR ALIA_BACKEND_AUDIO STREQUAL "SDL2" OR ALIA_BACKEND_INPUT STREQUAL "SDL2" OR ALIA_BACKEND_PLATFORM STREQUAL "SDL2")
	set(ALIA_USE_DEP_SDL2 ON)
endif()

if(ALIA_BACKEND_GFX_HW_CLASSIC STREQUAL "D3D9")
	set(ALIA_USE_DEP_D3D9 ON)
endif()

if(ALIA_BACKEND_GFX_HW_CLASSIC STREQUAL "OGL")
	set(ALIA_USE_DEP_OGL ON)
endif()

if(ALIA_BACKEND_INPUT STREQUAL "GLFW")
	set(ALIA_USE_DEP_GLFW ON)
endif()

if(ALIA_BACKEND_GFX_BASIC STREQUAL "D3D9")
	message(ERROR "D3D9 is not yet implemented for ALIA_BACKEND_GFX_BASIC")
endif()

if(ALIA_BACKEND_GFX_BASIC STREQUAL "OGL")
	message(ERROR "OGL is not yet implemented for ALIA_BACKEND_GFX_BASIC")
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "D3D12")
	message(ERROR "D3D12 is not yet implemented for ALIA_BACKEND_GFX_HW_MODERN")
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "Vulkan")
	# no existing backend for hw_modern yet
	# TODO change to ERROR
	message(WARNING "Vulkan is not yet implemented for ALIA_BACKEND_GFX_HW_MODERN")
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "Metal")
	message(ERROR "Metal is not yet implemented for ALIA_BACKEND_GFX_HW_MODERN")
endif()

if(ALIA_BACKEND_AUDIO STREQUAL "OpenAL")
	message(ERROR "OpenAL is not yet implemented for ALIA_BACKEND_AUDIO")
endif()

if(ALIA_BACKEND_INPUT STREQUAL "GLFW")
	message(ERROR "GLFW is not yet implemented for ALIA_BACKEND_INPUT")
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "GLFW")
	message(ERROR "GLFW is not yet implemented for ALIA_BACKEND_PLATFORM")
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "Win32")
	message(ERROR "Win32 is not yet implemented for ALIA_BACKEND_PLATFORM")
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "X11")
	message(ERROR "X11 is not yet implemented for ALIA_BACKEND_PLATFORM")
endif()

if(ALIA_BACKEND_GFX_HW_CLASSIC STREQUAL "D3D9")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_HW_CLASSIC_USE_D3D9)
endif()

if(ALIA_BACKEND_GFX_HW_CLASSIC STREQUAL "OGL")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_HW_CLASSIC_USE_OGL)
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "D3D12")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_HW_MODERN_USE_D3D12)
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "Vulkan")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_HW_MODERN_USE_VULKAN)
endif()

if(ALIA_BACKEND_GFX_HW_MODERN STREQUAL "Metal")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_HW_MODERN_USE_METAL)
endif()

if(ALIA_BACKEND_GFX_BASIC STREQUAL "SDL2")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_BASIC_USE_SDL2)
endif()

if(ALIA_BACKEND_GFX_BASIC STREQUAL "D3D9")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_BASIC_USE_D3D9)
endif()

if(ALIA_BACKEND_GFX_BASIC STREQUAL "OGL")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_GFX_BASIC_USE_OGL)
endif()

if(ALIA_BACKEND_AUDIO STREQUAL "SDL2")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_AUDIO_USE_SDL2)
endif()

if(ALIA_BACKEND_AUDIO STREQUAL "OpenAL")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_AUDIO_USE_OPENAL)
endif()

if(ALIA_BACKEND_INPUT STREQUAL "GLFW")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_INPUT_USE_GLFW)
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "SDL2")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_PLATFORM_USE_SDL2)
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "GLFW")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_PLATFORM_USE_GLFW)
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "Win32")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_PLATFORM_USE_WIN32)
endif()

if(ALIA_BACKEND_PLATFORM STREQUAL "X11")
	target_compile_definitions(alia INTERFACE ALIA_BACKEND_PLATFORM_USE_X11)
endif()


if(ALIA_USE_DEP_SDL2)
	find_package(SDL2 REQUIRED CONFIG COMPONENTS SDL2main)

	add_library(alia_backend_sdl2 INTERFACE ${ALIA_BACKEND_SDL2_HEADERS})

	if(TARGET SDL2::SDL2main)
		target_link_libraries(alia_backend_sdl2 INTERFACE SDL2::SDL2main)
	endif()
	target_link_libraries(alia_backend_sdl2 INTERFACE SDL2::SDL2)

	target_link_libraries(alia INTERFACE alia_backend_sdl2)

endif()
