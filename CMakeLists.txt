cmake_minimum_required(VERSION 3.24)

project(broa11y VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Memory-aware defaults
set(BRO_COMPILE_JOBS "4" CACHE STRING "Ninja compile concurrency limit")
set(BRO_LINK_JOBS "2" CACHE STRING "Ninja link concurrency limit")
set_property(GLOBAL PROPERTY JOB_POOLS
    compile_pool=${BRO_COMPILE_JOBS}
    link_pool=${BRO_LINK_JOBS}
)
set(CMAKE_JOB_POOL_COMPILE compile_pool)
set(CMAKE_JOB_POOL_LINK link_pool)

if(MSVC)
    add_compile_options(
        $<$<COMPILE_LANGUAGE:CXX>:/MP>
        $<$<COMPILE_LANGUAGE:CXX>:/permissive->
        $<$<COMPILE_LANGUAGE:CXX>:/utf-8>
    )
    if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(PkgConfig QUIET)
set(BROA11Y_HAVE_DBUS FALSE)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(DBUS QUIET dbus-1)
    if(DBUS_FOUND)
        set(BROA11Y_HAVE_DBUS TRUE)
    endif()
endif()

set(BROA11Y_SOURCES
    src/common/types.cpp
    src/common/role.cpp
    src/common/state.cpp
    src/common/action.cpp
    src/common/events.cpp
    src/common/node.cpp
    src/common/tree.cpp
    src/common/tree_transaction.cpp
    src/common/terminal_accessibility.cpp
    src/common/terminal_navigation.cpp
    src/common/bridge.cpp
    src/linux/atspi_serializer.cpp
    src/linux/atspi_node_adaptor.cpp
    src/linux/dbus_connection.cpp
    src/linux/linux_bridge.cpp
    src/win/uia_node_provider.cpp
    src/win/win_bridge.cpp
    src/mac/mac_accessible_element.cpp
    src/mac/mac_bridge.cpp
)

add_library(broa11y STATIC ${BROA11Y_SOURCES})
add_library(broa11y::broa11y ALIAS broa11y)

target_include_directories(broa11y
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(BROA11Y_HAVE_DBUS)
    target_compile_definitions(broa11y PRIVATE BROA11Y_HAVE_DBUS=1)
    target_include_directories(broa11y PRIVATE ${DBUS_INCLUDE_DIRS})
    target_link_libraries(broa11y PRIVATE ${DBUS_LIBRARIES})
endif()

option(BROA11Y_BUILD_TESTS "Build broa11y test suite" ON)
if(BROA11Y_BUILD_TESTS AND (PROJECT_IS_TOP_LEVEL OR BRO_BUILD_TESTS))
    enable_testing()
    add_subdirectory(tests)
endif()
