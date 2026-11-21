include(GoogleTest)

set(TEST_SOURCES
    test_order_book.cpp
    test_matching_engine.cpp
    test_object_pool.cpp
    test_ring_buffer.cpp
)

add_executable(aeromatch_tests ${TEST_SOURCES})
target_link_libraries(aeromatch_tests
    PRIVATE
        aeromatch_core
        GTest::gtest_main
)

gtest_discover_tests(aeromatch_tests)
