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
option(SR_BUILD_QT_UI "Build the Qt 6 desktop UI (D009)" OFF)
option(SR_WITH_OCIO "Link OpenColorIO when it can be found (D062)" ON)

# --- OpenColorIO (D062) -----------------------------------------------------
# Two ways in: a normal CMake package (Homebrew on macOS, an SDK on Windows) or
# a hand-built prefix with headers in include/ and static libs in lib/. The
# static prefix needs its own dependencies (expat, yaml-cpp, pystring, Imath,
# minizip-ng, zlib), so they are linked explicitly. Without any of this the stub
# is compiled and --ocio-* explains that the build has no OCIO.
set(SR_OCIO_FOUND OFF)
if(SR_WITH_OCIO)
    find_package(OpenColorIO CONFIG QUIET)
    if(OpenColorIO_FOUND AND TARGET OpenColorIO::OpenColorIO)
        set(SR_OCIO_FOUND ON)
        message(STATUS "OpenColorIO: package (D062)")
        set(SR_OCIO_LIBS OpenColorIO::OpenColorIO)
    else()
        set(SR_OCIO_ROOT "$ENV{OCIO_ROOT}" CACHE PATH
            "OpenColorIO prefix (include/OpenColorIO + lib/libOpenColorIO.a)")
        if(NOT SR_OCIO_ROOT AND EXISTS "X:/agent/tools/ocio/include/OpenColorIO/OpenColorIO.h")
            set(SR_OCIO_ROOT "X:/agent/tools/ocio")
        endif()
        if(SR_OCIO_ROOT AND EXISTS "${SR_OCIO_ROOT}/include/OpenColorIO/OpenColorIO.h")
            find_library(SR_OCIO_LIBRARY NAMES OpenColorIO HINTS "${SR_OCIO_ROOT}/lib")
            if(SR_OCIO_LIBRARY)
                set(SR_OCIO_FOUND ON)
                set(SR_OCIO_LIBS ${SR_OCIO_LIBRARY})
                foreach(dep expat Imath-3_1 minizip-ng pystring yaml-cpp zlibstatic)
                    find_library(SR_OCIO_DEP_${dep} NAMES ${dep}
                                 HINTS "${SR_OCIO_ROOT}/lib")
                    if(SR_OCIO_DEP_${dep})
                        list(APPEND SR_OCIO_LIBS ${SR_OCIO_DEP_${dep}})
                    endif()
                endforeach()
                message(STATUS "OpenColorIO: ${SR_OCIO_ROOT} (static prefix, D062)")
            endif()
        endif()
    endif()
endif()

# The GPU backends are per-platform: D3D11 on Windows, Metal on macOS (D056).
# Each platform compiles exactly one implementation of GpuFilm / GpuTexture.
if(APPLE)
    enable_language(OBJCXX)
    set(SR_GPU_FILM_SRC src/cli/gpu_film_metal.mm)
    set(SR_GPU_TEXTURE_SRC src/cli/gpu_texture_metal.mm)
    set_source_files_properties(src/cli/gpu_film_metal.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    set_source_files_properties(src/cli/gpu_texture_metal.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    find_library(SR_METAL_FRAMEWORK Metal REQUIRED)
    find_library(SR_FOUNDATION_FRAMEWORK Foundation REQUIRED)
    set(SR_GPU_LIBS ${SR_METAL_FRAMEWORK} ${SR_FOUNDATION_FRAMEWORK})
else()
    set(SR_GPU_FILM_SRC src/cli/gpu_film.cpp)
    set(SR_GPU_TEXTURE_SRC src/cli/gpu_texture.cpp)
    set(SR_GPU_LIBS "")
endif()

add_library(sr_core
    src/core/src/lut3d.cpp
    src/core/src/image.cpp
    src/core/src/json.cpp
    src/core/src/profile.cpp
    src/core/src/film_model.cpp
    src/core/src/metrics.cpp
    src/core/src/spectral_tables.cpp
    src/core/src/transfer.cpp
    src/core/src/gamut.cpp
    src/core/src/texture.cpp)

if(SR_OCIO_FOUND)
    target_sources(sr_core PRIVATE src/core/src/ocio.cpp)
    target_compile_definitions(sr_core PUBLIC SR_HAVE_OCIO=1)
    target_link_libraries(sr_core PUBLIC ${SR_OCIO_LIBS})
    if(NOT TARGET OpenColorIO::OpenColorIO)
        # A hand-built static prefix: OCIO headers use dllimport unless this is
        # set, and the prefix's own deps have to be linked too (D062).
        target_include_directories(sr_core SYSTEM PUBLIC "${SR_OCIO_ROOT}/include")
        target_compile_definitions(sr_core PUBLIC OpenColorIO_SKIP_IMPORTS)
        if(WIN32)
            target_link_libraries(sr_core PUBLIC user32 gdi32 ole32 uuid)
        endif()
    endif()
else()
    target_sources(sr_core PRIVATE src/core/src/ocio_stub.cpp)
    message(STATUS "OpenColorIO: not found, --ocio-* will report it is unavailable")
endif()

target_include_directories(sr_core
    PUBLIC src/core/include)

# stb is third-party C code; keep its warnings out of our build.
target_include_directories(sr_core SYSTEM PRIVATE third_party/stb)

target_compile_options(sr_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>)

add_executable(spektra-roll-cli src/cli/main.cpp ${SR_GPU_FILM_SRC} ${SR_GPU_TEXTURE_SRC})
target_link_libraries(spektra-roll-cli PRIVATE sr_core ${SR_GPU_LIBS})
if(WIN32)
    target_link_libraries(spektra-roll-cli PRIVATE d3d11 dxgi d3dcompiler)
endif()

# Ship one self-contained .exe: no MinGW runtime DLLs to hunt for.
if(MINGW)
    target_link_options(spektra-roll-cli PRIVATE -static)
endif()

if(SR_BUILD_QT_UI)
    find_package(Qt6 6.5 COMPONENTS Widgets REQUIRED)
    add_executable(spektra-roll-ui WIN32 src/ui-qt/main.cpp)
    target_link_libraries(spektra-roll-ui PRIVATE sr_core Qt6::Widgets)
    target_compile_options(spektra-roll-ui PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>)
    # The UI must not -static link: Qt ships as DLLs and is deployed next to the
    # executable by windeployqt (scripts/build-ui.cmd).
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

    add_executable(test_texture tests/test_texture.cpp)
    target_link_libraries(test_texture PRIVATE sr_core)
    if(MINGW)
        target_link_options(test_texture PRIVATE -static)
    endif()
    add_test(NAME texture COMMAND test_texture)

    # The GPU parity test skips itself (exit 77) when no GPU device exists.
    add_executable(test_gpu_film tests/test_gpu_film.cpp ${SR_GPU_FILM_SRC})
    target_link_libraries(test_gpu_film PRIVATE sr_core ${SR_GPU_LIBS})
    target_compile_definitions(test_gpu_film PRIVATE
        SR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(WIN32)
        target_link_libraries(test_gpu_film PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    if(MINGW)
        target_link_options(test_gpu_film PRIVATE -static)
    endif()
    add_test(NAME gpu_film COMMAND test_gpu_film)
    set_tests_properties(gpu_film PROPERTIES SKIP_RETURN_CODE 77)

    # Same skip contract for the spatial chain (D047).
    add_executable(test_gpu_texture tests/test_gpu_texture.cpp ${SR_GPU_TEXTURE_SRC})
    target_link_libraries(test_gpu_texture PRIVATE sr_core ${SR_GPU_LIBS})
    if(WIN32)
        target_link_libraries(test_gpu_texture PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    if(MINGW)
        target_link_options(test_gpu_texture PRIVATE -static)
    endif()
    add_test(NAME gpu_texture COMMAND test_gpu_texture)
    set_tests_properties(gpu_texture PROPERTIES SKIP_RETURN_CODE 77)

    # Fused colour+texture submit (D049); same skip contract.
    add_executable(test_gpu_chain tests/test_gpu_chain.cpp ${SR_GPU_FILM_SRC}
        ${SR_GPU_TEXTURE_SRC})
    target_link_libraries(test_gpu_chain PRIVATE sr_core ${SR_GPU_LIBS})
    target_compile_definitions(test_gpu_chain PRIVATE
        SR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(WIN32)
        target_link_libraries(test_gpu_chain PRIVATE d3d11 dxgi d3dcompiler)
    endif()
    if(MINGW)
        target_link_options(test_gpu_chain PRIVATE -static)
    endif()
    add_test(NAME gpu_chain COMMAND test_gpu_chain)
    set_tests_properties(gpu_chain PROPERTIES SKIP_RETURN_CODE 77)

    # OpenColorIO (D062) skips itself when the build has no OCIO (exit 77).
    add_executable(test_ocio tests/test_ocio.cpp)
    target_link_libraries(test_ocio PRIVATE sr_core)
    target_compile_definitions(test_ocio PRIVATE
        SR_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(MINGW)
        target_link_options(test_ocio PRIVATE -static)
    endif()
    add_test(NAME ocio COMMAND test_ocio)
    set_tests_properties(ocio PROPERTIES SKIP_RETURN_CODE 77)
endif()

install(TARGETS spektra-roll-cli RUNTIME DESTINATION bin)
