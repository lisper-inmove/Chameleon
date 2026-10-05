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
)

target_compile_definitions(chameleon_tests PRIVATE
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

target_link_libraries(chameleon_tests PRIVATE
    chameleon_lib
    GTest::gtest
)

gtest_discover_tests(chameleon_tests
    DISCOVERY_MODE PRE_TEST
    PROPERTIES
        ENVIRONMENT "PATH=${CHAMELEON_QT_ROOT}/bin;${CHAMELEON_THIRD_PARTY_DIR}/opencv/x64/vc18/bin"
)
