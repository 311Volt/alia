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


file(GLOB_RECURSE alia_headers "include/*.hpp")
add_library(alia INTERFACE ${alia_headers})

include("backends.cmake")


target_include_directories(alia INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

option(ALIA_BUILD_EXAMPLES "Build example programs" ${ALIA_MASTER_PROJECT})

if(ALIA_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

