cmake_minimum_required(VERSION 3.21)

project(Chameleon VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# All project logic lives in cmakes/, included in order:
#   Dependencies     - find_package for Qt and third-party libraries
#   CompilerOptions  - global compile options and warnings
#   Targets          - application library and executable
#   Tests            - unit tests (googletest)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmakes")

include(Dependencies)
include(CompilerOptions)
include(Targets)
include(Tests)

qt_finalize_project()
