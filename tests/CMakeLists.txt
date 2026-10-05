set(TEST_TARGETS
    test_tree
    test_transaction
    test_actions
    test_terminal_a11y
    test_atspi_serializer
    test_linux_bridge
    test_win_bridge
    test_mac_bridge
)

foreach(t ${TEST_TARGETS})
    add_executable(${t} ${t}.cpp)
    target_link_libraries(${t} PRIVATE broa11y)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME ${t} COMMAND ${t})
endforeach()
