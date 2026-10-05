# ------------------------------------------------------------------------------
# Application targets.
#
#   chameleon_lib  - static library with the application sources, shared by
#                    the executable and the unit tests
#   appChameleon   - the application executable (entry point: src/main.cpp)
# ------------------------------------------------------------------------------

include(GNUInstallDirs)

# Content of downloaded design from Figma to Qt or similar
if(EXISTS "${CMAKE_SOURCE_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory("${CMAKE_SOURCE_DIR}/importedcontent")
endif()

qt_add_library(chameleon_lib STATIC)

# OUTPUT_DIRECTORY must end with the module's target path, otherwise qmllint
# and other QML tooling cannot locate the module
qt_add_qml_module(chameleon_lib
    URI Chameleon
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Chameleon"
    QML_FILES
        src/Main.qml
        src/app/AppState.qml
        src/components/ImageViewer.qml
        src/components/ThumbnailBar.qml
)

target_sources(chameleon_lib PRIVATE
    include/config/config_manager.h
    include/utils/file_utils.h
    src/config/config_manager.cc
    src/utils/file_utils.cc
)

target_include_directories(chameleon_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

# OpenCV 5 exports its modules without the OpenCV:: namespace
target_link_libraries(chameleon_lib PUBLIC
    Qt6::Quick
    Qt6::QuickControls2
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    spdlog::spdlog
    yaml-cpp::yaml-cpp
)

# The prebuilt spdlog is a compiled library (spdlog.lib), but its installed
# package does not export the SPDLOG_COMPILED_LIB define, so add it here.
target_compile_definitions(chameleon_lib PUBLIC SPDLOG_COMPILED_LIB)

# Generated header with the source configs dir path; during development the
# config is read from there so edits take effect without redeploying.
set(CHAMELEON_CONFIG_SOURCE_DIR "${CMAKE_SOURCE_DIR}/configs")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmakes/config_paths.h.in"
    "${CMAKE_BINARY_DIR}/generated/chameleon/config_paths.h"
    @ONLY)
target_include_directories(chameleon_lib PUBLIC "${CMAKE_BINARY_DIR}/generated")

qt_add_executable(appChameleon
    src/main.cpp
)

target_link_libraries(appChameleon PRIVATE chameleon_lib)

# The QML module is backed by a static library, so its static plugin must be
# linked explicitly for the module to be registered (static plugins are not
# discovered dynamically at runtime)
target_link_libraries(appChameleon PRIVATE chameleon_libplugin)

set_target_properties(appChameleon PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appChameleon
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# Deploy the runtime config files next to the executable
add_custom_command(TARGET appChameleon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/configs"
        "$<TARGET_FILE_DIR:appChameleon>/configs"
    COMMENT "Deploying configs to $<TARGET_FILE_DIR:appChameleon>/configs"
)

install(TARGETS appChameleon
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
