# ------------------------------------------------------------------------------
# Unit tests (googletest).
#
# Test sources live in tests/ and use the .cc extension; the entry point is
# tests/main.cpp. Tests link against chameleon_lib.
# ------------------------------------------------------------------------------

include(CTest)

if(NOT BUILD_TESTING)
    return()
endif()

include(GoogleTest)

add_executable(chameleon_tests
    tests/main.cpp
    tests/config_manager_test.cc
    tests/file_utils_test.cc
    tests/app_state_test.cc
    tests/filter_processor_test.cc
    tests/image_viewer_test.cc
    tests/main_window_test.cc
)

target_compile_definitions(chameleon_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_link_libraries(chameleon_tests PRIVATE
    chameleon_lib
    GTest::gtest
    Qt6::Test
)

# Tests inherit the environment from the test preset (ctest --preset),
# which provides Qt and OpenCV DLLs on PATH for both discovery and runs.
# A per-test ENVIRONMENT here would be merged with the preset environment
# and ctest splits PATH at the semicolons during that merge.
gtest_discover_tests(chameleon_tests
    DISCOVERY_MODE PRE_TEST
)
