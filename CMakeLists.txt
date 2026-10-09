cmake_minimum_required(VERSION 3.18)
project(seedlessds_libretro C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64"
   AND NOT CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    if(SEEDLESS_ALLOW_NON_AARCH64)
        message(WARNING
            "Non-AArch64 build: GPU NEON / JIT will not work. "
            "Stubs are for link experiments only.")
    else()
        message(FATAL_ERROR
            "SeedlessDS-Core requires AArch64 (JIT + NEON).\n"
            "Cross-compile example:\n"
            "  make CROSS=aarch64-linux-gnu-\n"
            "Or Android NDK arm64-v8a. "
            "Override with -DSEEDLESS_ALLOW_NON_AARCH64=ON.")
    endif()
endif()

set(SEEDLESS_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/core" CACHE PATH
    "Path to SeedlessDS-Core")
set(SEEDLESS_LIBRETRO ON CACHE BOOL "Build libretro frontend" FORCE)
set(SEEDLESS_SKIP_ANDROID_PLATFORM ON CACHE BOOL "Skip Android JNI platform" FORCE)

if(NOT EXISTS "${SEEDLESS_CORE_DIR}/include/seedlessds/nds.h")
    message(FATAL_ERROR "SeedlessDS-Core not found at ${SEEDLESS_CORE_DIR}. "
            "Run: git submodule update --init --recursive")
endif()

find_package(Threads REQUIRED)

# Cross toolchains often lack a target libz; prefer a bundled static zlib then.
set(SEEDLESS_USE_BUNDLED_ZLIB OFF)
if(CMAKE_CROSSCOMPILING OR SEEDLESS_FORCE_BUNDLED_ZLIB)
    set(SEEDLESS_USE_BUNDLED_ZLIB ON)
else()
    find_package(ZLIB QUIET)
    if(NOT ZLIB_FOUND)
        set(SEEDLESS_USE_BUNDLED_ZLIB ON)
    endif()
endif()

if(SEEDLESS_USE_BUNDLED_ZLIB)
    message(STATUS "Building bundled zlib 1.3.1")
    include(FetchContent)
    set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    FetchContent_Declare(zlib
        URL https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz
        URL_HASH SHA256=9a93b2b7dfdac77ceba5a558a580e74667dd6fede4585b91eefb60f03b72df23
    )
    FetchContent_MakeAvailable(zlib)
    if(TARGET zlibstatic)
        add_library(ZLIB::ZLIB ALIAS zlibstatic)
        target_include_directories(zlibstatic PUBLIC
            $<BUILD_INTERFACE:${zlib_SOURCE_DIR}>
            $<BUILD_INTERFACE:${zlib_BINARY_DIR}>)
        set(SEEDLESS_ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
    elseif(TARGET zlib)
        add_library(ZLIB::ZLIB ALIAS zlib)
        set(SEEDLESS_ZLIB_INCLUDE_DIRS "${zlib_SOURCE_DIR}" "${zlib_BINARY_DIR}")
    else()
        message(FATAL_ERROR "Bundled zlib did not create an expected target")
    endif()
endif()

add_subdirectory("${SEEDLESS_CORE_DIR}/third_party" third_party)

# Core sources land on this OBJECT target (name expected by core CMakeLists).
add_library(recon_fn OBJECT)
target_include_directories(recon_fn PRIVATE
    "${SEEDLESS_CORE_DIR}/include"
    "${SEEDLESS_CORE_DIR}/src"
    "${SEEDLESS_CORE_DIR}/third_party/lua"
    "${CMAKE_CURRENT_BINARY_DIR}"
    ${SEEDLESS_ZLIB_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS})
target_compile_definitions(recon_fn PRIVATE _GNU_SOURCE SEEDLESS_LIBRETRO=1)
target_compile_options(recon_fn PRIVATE
    -O2 -fno-strict-aliasing -fPIC
    -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable)

include("${SEEDLESS_CORE_DIR}/src/CMakeLists.txt")

# Keep Android audio mixer feed path; replace OpenSL with libretro/opensl_stub.c.
target_sources(recon_fn PRIVATE
    "${SEEDLESS_CORE_DIR}/src/platform/android/audio.c")

# Propagate third_party / zlib include dirs onto OBJECT sources.
target_link_libraries(recon_fn PUBLIC seedless_lua seedless_unrar seedless_lzma ZLIB::ZLIB)

set(SEEDLESS_LIBRETRO_SOURCES
    libretro/libretro.c
    libretro/opensl_stub.c
    libretro/gl_stubs.c
    libretro/android_compat.c
    "${SEEDLESS_CORE_DIR}/src/platform/posix/files_posix.c"
)

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64"
   AND NOT CMAKE_C_COMPILER_TARGET MATCHES "aarch64")
    list(APPEND SEEDLESS_LIBRETRO_SOURCES
        libretro/adpcm_c.c
        libretro/cpu_stub.c)
endif()

add_library(seedlessds_libretro SHARED
    ${SEEDLESS_LIBRETRO_SOURCES}
    $<TARGET_OBJECTS:recon_fn>
)

set_target_properties(seedlessds_libretro PROPERTIES
    PREFIX ""
    OUTPUT_NAME "seedlessds_libretro"
)

target_include_directories(seedlessds_libretro PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/deps"
    "${CMAKE_CURRENT_SOURCE_DIR}/libretro"
    "${SEEDLESS_CORE_DIR}/include"
    "${SEEDLESS_CORE_DIR}/src"
    "${SEEDLESS_CORE_DIR}/third_party/lua"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_compile_definitions(seedlessds_libretro PRIVATE
    _GNU_SOURCE
    SEEDLESS_LIBRETRO=1
)

target_compile_options(seedlessds_libretro PRIVATE
    -O2
    -fno-strict-aliasing
    -Wall
    -Wno-unused-parameter
)

target_link_libraries(seedlessds_libretro PRIVATE
    seedless_lua
    seedless_unrar
    seedless_lzma
    ZLIB::ZLIB
    Threads::Threads
    m
    dl
)

if(NOT APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/link.T")
    target_link_options(seedlessds_libretro PRIVATE
        -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/link.T
        -Wl,-Bsymbolic)
endif()

install(TARGETS seedlessds_libretro LIBRARY DESTINATION lib/libretro)
install(FILES seedlessds_libretro.info DESTINATION share/libretro/info OPTIONAL)
