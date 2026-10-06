# broa11y_test(<name> SOURCES ... [LIBS ...] [TIMEOUT s]): one real ctest.
# Exit code 77 is a skip (a service genuinely absent, reason printed),
# anything else nonzero a failure.
function(broa11y_test name)
    cmake_parse_arguments(T "" "TIMEOUT" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${T_SOURCES})
    target_link_libraries(${name} PRIVATE broa11y ${T_LIBS})
    target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${PROJECT_SOURCE_DIR}/src)
    broa11y_warnings(${name})
    if(WIN32)
        target_compile_definitions(${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
            _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000004)
    endif()
    if(NOT T_TIMEOUT)
        set(T_TIMEOUT 60)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${T_TIMEOUT} SKIP_RETURN_CODE 77)
endfunction()

broa11y_test(test_tree SOURCES test_tree.cpp)
broa11y_test(test_transaction SOURCES test_transaction.cpp)
broa11y_test(test_actions SOURCES test_actions.cpp)
broa11y_test(test_text_index SOURCES test_text_index.cpp)
broa11y_test(test_terminal_a11y SOURCES test_terminal_a11y.cpp)

if(WIN32)
    # A hidden window serves the providers; a second process is the UIA client
    # (IUIAutomation, as Narrator uses it) and drives them.
    broa11y_test(test_win_uia SOURCES test_win_uia.cpp test_win_uia_client.cpp
        LIBS uiautomationcore ole32 oleaut32)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # A private session bus and the real AT-SPI bus launcher + registry; a
    # libatspi client process (the library Orca uses) reads and drives the tree.
    # GLib and GObject are named too: atspi-2.pc keeps them private, and the
    # client uses them directly (main context, GValue, object refs).
    pkg_check_modules(BROA11Y_ATSPI IMPORTED_TARGET atspi-2 glib-2.0 gobject-2.0)
    if(BROA11Y_ATSPI_FOUND)
        add_executable(atspi_client atspi_client.cpp)
        target_link_libraries(atspi_client PRIVATE PkgConfig::BROA11Y_ATSPI)
        target_include_directories(atspi_client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        broa11y_warnings(atspi_client)
        set(_atspi_client "$<TARGET_FILE:atspi_client>")
    else()
        set(_atspi_client "")
    endif()
    broa11y_test(test_linux_atspi SOURCES test_linux_atspi.cpp TIMEOUT 120)
    target_compile_definitions(test_linux_atspi PRIVATE BROA11Y_ATSPI_CLIENT="${_atspi_client}")
elseif(APPLE)
    # AppKit's own accessibility attribute API reads the elements back.
    broa11y_test(test_mac_bridge SOURCES test_mac_bridge.mm)
    set_source_files_properties(test_mac_bridge.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

if(BROA11Y_ENABLE_API)
    add_executable(broa11y_test_api
        test_a11y_api.cpp
    )
    target_include_directories(broa11y_test_api PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/api
        ${PROJECT_SOURCE_DIR}/src
    )
    target_link_libraries(broa11y_test_api PRIVATE
        broa11y_api
        broa11y
        bronze_runtime_shared
    )
    broa11y_warnings(broa11y_test_api)
    if(WIN32)
        target_compile_definitions(broa11y_test_api PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE _WIN32_WINNT=0x0A00)
        target_link_libraries(broa11y_test_api PRIVATE user32 shcore)
        add_custom_command(TARGET broa11y_test_api POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broa11y_test_api>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_LINKER_FILE:bronze_runtime_shared>
                $<TARGET_FILE_DIR:broa11y_test_api>
        )
    endif()

    if(NOT BRASS_HOST_BACKEND)
        message(STATUS "broa11y: broa11y_test_api not registered (brass has no code generator for this target)")
    else()
        add_test(NAME broa11y_test_api COMMAND broa11y_test_api)
        set_tests_properties(broa11y_test_api PROPERTIES TIMEOUT 60 SKIP_RETURN_CODE 77)

        add_test(NAME broa11y_test_api_gc_stress COMMAND broa11y_test_api)
        set_tests_properties(broa11y_test_api_gc_stress PROPERTIES
            TIMEOUT 60
            SKIP_RETURN_CODE 77
            ENVIRONMENT "BRONZE_GC_STRESS=1;BRONZE_GC_POISON=1"
        )
        if(WIN32)
            set_tests_properties(broa11y_test_api PROPERTIES
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broa11y_test_api>;$ENV{PATH}")
            set_property(TEST broa11y_test_api_gc_stress APPEND PROPERTY
                ENVIRONMENT "PATH=$<TARGET_FILE_DIR:bronze_runtime_shared>;$<TARGET_FILE_DIR:broa11y_test_api>;$ENV{PATH}")
        endif()
    endif()
endif()

# Every test executable links no_dialogs.cpp (see the file).
if(WIN32)
    get_property(_test_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(_t IN LISTS _test_targets)
        get_target_property(_type ${_t} TYPE)
        if(_type STREQUAL "EXECUTABLE")
            target_sources(${_t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/no_dialogs.cpp)
        endif()
    endforeach()
endif()

