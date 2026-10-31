cmake_minimum_required(VERSION 3.20)

project(spektravideo
    VERSION 0.1.0
    DESCRIPTION "Open-source spectral film simulation for motion picture"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SV_BUILD_TESTS "Build the test binaries" ON)

add_library(sv_core
    src/core/src/lut3d.cpp
    src/core/src/image.cpp)

target_include_directories(sv_core
    PUBLIC src/core/include
    PRIVATE third_party/stb)

# stb is third-party C code; keep its warnings out of our build.
target_compile_options(sv_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>)

add_executable(spektravideo-cli src/cli/main.cpp)
target_link_libraries(spektravideo-cli PRIVATE sv_core)

if(SV_BUILD_TESTS)
    enable_testing()

    add_executable(test_lut3d tests/test_lut3d.cpp)
    target_link_libraries(test_lut3d PRIVATE sv_core)
    add_test(NAME lut3d COMMAND test_lut3d)
endif()

install(TARGETS spektravideo-cli RUNTIME DESTINATION bin)
