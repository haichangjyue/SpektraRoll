cmake_minimum_required(VERSION 3.20)

project(SpektraRoll
    VERSION 0.1.0
    DESCRIPTION "Open-source spectral film simulation for motion picture"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SR_BUILD_TESTS "Build the test binaries" ON)

add_library(sr_core
    src/core/src/lut3d.cpp
    src/core/src/image.cpp
    src/core/src/json.cpp
    src/core/src/profile.cpp
    src/core/src/film_model.cpp
    src/core/src/metrics.cpp
    src/core/src/spectral_tables.cpp
    src/core/src/transfer.cpp
    src/core/src/gamut.cpp)

target_include_directories(sr_core
    PUBLIC src/core/include)

# stb is third-party C code; keep its warnings out of our build.
target_include_directories(sr_core SYSTEM PRIVATE third_party/stb)

target_compile_options(sr_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>)

add_executable(spektra-roll-cli src/cli/main.cpp)
target_link_libraries(spektra-roll-cli PRIVATE sr_core)

# Ship one self-contained .exe: no MinGW runtime DLLs to hunt for.
if(MINGW)
    target_link_options(spektra-roll-cli PRIVATE -static)
endif()

if(SR_BUILD_TESTS)
    enable_testing()

    add_executable(test_lut3d tests/test_lut3d.cpp)
    target_link_libraries(test_lut3d PRIVATE sr_core)
    if(MINGW)
        target_link_options(test_lut3d PRIVATE -static)
    endif()
    add_test(NAME lut3d COMMAND test_lut3d)

    add_executable(test_film_model tests/test_film_model.cpp)
    target_link_libraries(test_film_model PRIVATE sr_core)
    target_compile_definitions(test_film_model PRIVATE
        SR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MINGW)
        target_link_options(test_film_model PRIVATE -static)
    endif()
    add_test(NAME film_model COMMAND test_film_model)

    add_executable(test_image tests/test_image.cpp)
    target_link_libraries(test_image PRIVATE sr_core)
    if(MINGW)
        target_link_options(test_image PRIVATE -static)
    endif()
    add_test(NAME image_io COMMAND test_image)

    add_executable(test_transfer tests/test_transfer.cpp)
    target_link_libraries(test_transfer PRIVATE sr_core)
    if(MINGW)
        target_link_options(test_transfer PRIVATE -static)
    endif()
    add_test(NAME transfer COMMAND test_transfer)

    add_executable(test_gamut tests/test_gamut.cpp)
    target_link_libraries(test_gamut PRIVATE sr_core)
    if(MINGW)
        target_link_options(test_gamut PRIVATE -static)
    endif()
    add_test(NAME gamut COMMAND test_gamut)
endif()

install(TARGETS spektra-roll-cli RUNTIME DESTINATION bin)
