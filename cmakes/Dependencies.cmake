# ------------------------------------------------------------------------------
# Dependencies: Qt + third-party libraries.
#
# Third-party libraries (googletest, OpenCV, spdlog, yaml-cpp) are prebuilt
# with MSVC and installed under
#   F:/codes/third_party        - Release builds
#   F:/codes/third_party_debug  - Debug builds
#
# CHAMELEON_THIRD_PARTY_DIR selects the tree to use. It is set per preset in
# CMakePresets.json (Debug -> third_party_debug, Release -> third_party); the
# fallback below derives it from CMAKE_BUILD_TYPE when configuring outside of
# presets (e.g. Qt Creator).
# ------------------------------------------------------------------------------

if(NOT CHAMELEON_THIRD_PARTY_DIR)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(CHAMELEON_THIRD_PARTY_DIR "F:/codes/third_party_debug" CACHE PATH
            "Root directory of prebuilt third-party libraries (Debug or Release tree)")
    else()
        set(CHAMELEON_THIRD_PARTY_DIR "F:/codes/third_party" CACHE PATH
            "Root directory of prebuilt third-party libraries (Debug or Release tree)")
    endif()
endif()

list(APPEND CMAKE_PREFIX_PATH
    "${CHAMELEON_THIRD_PARTY_DIR}/opencv"
    "${CHAMELEON_THIRD_PARTY_DIR}/spdlog"
    "${CHAMELEON_THIRD_PARTY_DIR}/yaml-cpp"
    "${CHAMELEON_THIRD_PARTY_DIR}/googletest")

find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(GTest REQUIRED)

# Root of the Qt kit (needed to locate runtime DLLs for running tests)
get_filename_component(CHAMELEON_QT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)
