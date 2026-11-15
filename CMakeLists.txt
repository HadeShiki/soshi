cmake_minimum_required(VERSION 3.24)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake"
      CACHE FILEPATH "vcpkg toolchain file")
  set(VCPKG_TARGET_TRIPLET x64-windows CACHE STRING "vcpkg triplet")
endif()

project(soshi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL3 CONFIG REQUIRED)

find_program(ZIG_EXECUTABLE NAMES zig)
if(NOT ZIG_EXECUTABLE)
  file(GLOB ZIG_INSTALLS "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/zig.zig_*/zig-*/zig.exe")
  if(ZIG_INSTALLS)
    list(SORT ZIG_INSTALLS)
    list(GET ZIG_INSTALLS -1 ZIG_EXECUTABLE)
  endif()
endif()
if(NOT ZIG_EXECUTABLE)
  message(FATAL_ERROR "zig not found: install Zig or configure with -DZIG_EXECUTABLE=<path to zig.exe>")
endif()

file(GLOB ZIG_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/zig/*.zig")
set(SOSHI_CORE_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/soshi_core.lib")

add_custom_command(
  OUTPUT "${SOSHI_CORE_LIBRARY}"
  COMMAND "${ZIG_EXECUTABLE}" build-lib "${CMAKE_CURRENT_SOURCE_DIR}/src/zig/soshi_core.zig"
          -target native -O ReleaseFast -mcpu=baseline -fstrip -femit-bin=${SOSHI_CORE_LIBRARY}
  DEPENDS ${ZIG_SOURCES}
  COMMENT "Compiling soshi_core.lib with Zig"
  VERBATIM)

add_custom_target(soshi_core DEPENDS "${SOSHI_CORE_LIBRARY}")

add_library(soshi SHARED
  src/cpp/audio.cpp
  src/cpp/capi.cpp
  src/cpp/crt_shim.cpp
  src/cpp/sdl_context.cpp
  src/cpp/texture_bank.cpp)

add_dependencies(soshi soshi_core)
target_link_libraries(soshi PRIVATE SDL3::SDL3 "${SOSHI_CORE_LIBRARY}")

set_target_properties(soshi PROPERTIES
  PREFIX ""
  OUTPUT_NAME "soshi")

set(SOSHI_PYTHON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/python/soshi")
add_custom_command(TARGET soshi POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${SOSHI_PYTHON_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:soshi>" "${SOSHI_PYTHON_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:soshi> "${SOSHI_PYTHON_DIR}"
  COMMAND_EXPAND_LISTS)
