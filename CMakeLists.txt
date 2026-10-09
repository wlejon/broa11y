cmake_minimum_required(VERSION 3.24)

project(broa11y VERSION 0.1.0 LANGUAGES CXX)

# Dependencies (cmake/bro_deps.cmake): an outer project's target wins, else the
# ../<name> working tree beside the top-level project, else the pinned commit,
# fetched at configure.
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/bro_deps.cmake)

# Targets
#   broa11y::broa11y   accessibility tree + the platform bridge for this OS
#
# Sources:
#   src/common   portable (tree, nodes, events, transactions, terminal text,
#                text indexing shared by the bridges)
#   src/linux    AT-SPI 2 provider over sd-bus (libsystemd)
#   src/win      UI Automation providers (UIAutomationCore)
#   src/mac      NSAccessibility elements (AppKit, Objective-C++)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 20)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
set(CMAKE_OBJCXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(_broa11y_top_level OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_broa11y_top_level ON)
endif()

option(BROA11Y_BUILD_TESTS "Build the broa11y tests" ${_broa11y_top_level})

# Memory-aware Ninja limits for a top-level build. Job pools are a Ninja
# feature: other generators ignore them, so they are only set up there, and
# never for a parent project that add_subdirectory()s broa11y.
if(_broa11y_top_level AND CMAKE_GENERATOR MATCHES "Ninja")
    set(BRO_COMPILE_JOBS "4" CACHE STRING "Ninja compile concurrency limit")
    set(BRO_LINK_JOBS "2" CACHE STRING "Ninja link concurrency limit")
    set_property(GLOBAL PROPERTY JOB_POOLS
        compile_pool=${BRO_COMPILE_JOBS}
        link_pool=${BRO_LINK_JOBS})
    set(CMAKE_JOB_POOL_COMPILE compile_pool)
    set(CMAKE_JOB_POOL_LINK link_pool)
endif()

# Coverage. Scoped to a top-level build: a consumer that add_subdirectory()s
# broa11y has no business having its own build instrumented from in here.
option(BROA11Y_COVERAGE "Instrument the build for gcov coverage (GCC/Clang)" OFF)
if(BROA11Y_COVERAGE AND _broa11y_top_level)
    if(MSVC)
        message(FATAL_ERROR "BROA11Y_COVERAGE requires GCC or Clang (gcov instrumentation)")
    endif()
    add_compile_options(--coverage -fprofile-update=atomic -O0 -g)
    add_link_options(--coverage)
    message(STATUS "broa11y: coverage instrumentation ON (-O0)")
endif()

# The DLL CRT (/MD, /MDd), as bro uses. broa11y_api's tests link
# bronze_runtime_shared, a DLL whose embed boundary passes C++ objects
# (std::string, ...) both ways: with the static CRT the DLL and the test
# executable each get their own heap, and a Debug build asserts
# (__acrt_first_block == header) the first time one frees the other's memory.
if(MSVC AND _broa11y_top_level)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

function(broa11y_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /MP)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

file(GLOB BROA11Y_SOURCES CONFIGURE_DEPENDS src/common/*.cpp)

if(WIN32)
    file(GLOB _win_sources CONFIGURE_DEPENDS src/win/*.cpp)
    list(APPEND BROA11Y_SOURCES ${_win_sources})
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(BROA11Y_SYSTEMD REQUIRED IMPORTED_TARGET libsystemd>=246)
    file(GLOB _linux_sources CONFIGURE_DEPENDS src/linux/*.cpp)
    list(APPEND BROA11Y_SOURCES ${_linux_sources})
elseif(APPLE)
    enable_language(OBJCXX)
    file(GLOB _mac_sources CONFIGURE_DEPENDS src/mac/*.mm)
    list(APPEND BROA11Y_SOURCES ${_mac_sources})
else()
    message(FATAL_ERROR "broa11y implements Windows, Linux and macOS bridges only")
endif()

add_library(broa11y STATIC ${BROA11Y_SOURCES})
add_library(broa11y::broa11y ALIAS broa11y)

target_include_directories(broa11y
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

broa11y_warnings(broa11y)

if(WIN32)
    target_compile_definitions(broa11y PRIVATE
        NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
        # Windows 10 1709: UiaRaiseNotificationEvent.
        _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000004)
    target_link_libraries(broa11y PUBLIC uiautomationcore comctl32 ole32 oleaut32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(broa11y PRIVATE PkgConfig::BROA11Y_SYSTEMD)
elseif(APPLE)
    set_source_files_properties(${_mac_sources} PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(broa11y PUBLIC "-framework AppKit")
endif()

# --- Standalone Bronze JavaScript API ---
# On by default only when broa11y is the top-level project. A library that
# consumes broa11y then never needs bronze for it; bro forces
# BROA11Y_ENABLE_API ON itself when it wants the binding.
option(BROA11Y_ENABLE_API "Build Bronze JavaScript API" ${PROJECT_IS_TOP_LEVEL})
if(BROA11Y_ENABLE_API)
    # bronze (with brass, its code generator) through cmake/bro_deps.cmake: an
    # outer project's target, else ../bronze beside the top-level project, else
    # the pinned commit, fetched at configure.
    bro_dependency(bronze GITHUB wlejon/bronze TARGET bronze_runtime_shared
                   OPTIONS BRONZE_BUILD_SHARED_RUNTIME=ON BRONZE_BUILD_TESTS=OFF)

    add_subdirectory(src/api)
endif()

if(BROA11Y_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

