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

find_package(Qt6 REQUIRED COMPONENTS Widgets Test Concurrent)
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(GTest REQUIRED)

# Root of the Qt kit (needed to locate runtime DLLs for running tests)
get_filename_component(CHAMELEON_QT_ROOT "${Qt6_DIR}/../../.." ABSOLUTE)

# ------------------------------------------------------------------------------
# OpenGL: GLFW (built from source) + GLAD (generated 4.6 core loader).
# Override the paths with -DCHAMELEON_GLFW_SOURCE_DIR=... -DCHAMELEON_GLAD_DIR=...
# ------------------------------------------------------------------------------
enable_language(C)  # GLFW and glad.c are C sources

find_package(OpenGL REQUIRED)

set(CHAMELEON_GLFW_SOURCE_DIR "F:/codes/sources/glfw-3.5.1" CACHE PATH
    "GLFW source tree")
set(CHAMELEON_GLAD_DIR "F:/codes/sources/glad" CACHE PATH
    "GLAD loader directory (glad.c / glad.h / khrplatform.h)")
if(NOT EXISTS "${CHAMELEON_GLFW_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "GLFW sources not found at ${CHAMELEON_GLFW_SOURCE_DIR}")
endif()
if(NOT EXISTS "${CHAMELEON_GLAD_DIR}/glad.c")
    message(FATAL_ERROR "GLAD loader not found at ${CHAMELEON_GLAD_DIR}")
endif()

set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory("${CHAMELEON_GLFW_SOURCE_DIR}"
                 "${CMAKE_BINARY_DIR}/third_party/glfw" EXCLUDE_FROM_ALL)

add_library(glad STATIC "${CHAMELEON_GLAD_DIR}/glad.c")
target_include_directories(glad SYSTEM PUBLIC "${CHAMELEON_GLAD_DIR}")
target_link_libraries(glad PUBLIC OpenGL::GL)
