# ------------------------------------------------------------------------------
# Application targets.
#
#   chameleon_lib  - static library with the application sources, shared by
#                    the executable and the unit tests
#   appChameleon   - the application executable (entry point: src/main.cpp)
# ------------------------------------------------------------------------------

include(GNUInstallDirs)

add_library(chameleon_lib STATIC)

target_sources(chameleon_lib PRIVATE
    include/app/app_state.h
    include/config/config_manager.h
    include/processing/binary.h
    include/processing/contour_match.h
    include/processing/filter_processor.h
    include/processing/geometry.h
    include/processing/histogram.h
    include/processing/hough.h
    include/processing/image_io.h
    include/processing/noise.h
    include/rendering/opengl_image.h
    include/ui/back_project_dialog.h
    include/ui/connected_components_dialog.h
    include/ui/contour_dialog.h
    include/ui/contour_match_dialog.h
    include/ui/convolution_dialog.h
    include/ui/edge_dialog.h
    include/ui/filter_panel.h
    include/ui/geometry_dialog.h
    include/ui/gradient_dialog.h
    include/ui/hough_dialog.h
    include/ui/noise_dialog.h
    include/ui/sharpen_dialog.h
    include/ui/threshold_dialog.h
    include/ui/histogram_compare_dialog.h
    include/ui/histogram_match_dialog.h
    include/ui/histogram_window.h
    include/ui/image_conversion.h
    include/ui/image_window.h
    include/ui/main_window.h
    include/ui/image_viewer_widget.h
    include/utils/file_utils.h
    src/app/app_state.cc
    src/config/config_manager.cc
    src/processing/binary.cc
    src/processing/contour_match.cc
    src/processing/filter_processor.cc
    src/processing/geometry.cc
    src/processing/histogram.cc
    src/processing/hough.cc
    src/processing/image_io.cc
    src/processing/noise.cc
    src/rendering/opengl_image.cc
    src/ui/back_project_dialog.cc
    src/ui/connected_components_dialog.cc
    src/ui/contour_dialog.cc
    src/ui/contour_match_dialog.cc
    src/ui/convolution_dialog.cc
    src/ui/edge_dialog.cc
    src/ui/filter_panel.cc
    src/ui/geometry_dialog.cc
    src/ui/gradient_dialog.cc
    src/ui/hough_dialog.cc
    src/ui/noise_dialog.cc
    src/ui/sharpen_dialog.cc
    src/ui/threshold_dialog.cc
    src/ui/histogram_compare_dialog.cc
    src/ui/histogram_match_dialog.cc
    src/ui/histogram_window.cc
    src/ui/image_conversion.cc
    src/ui/image_window.cc
    src/ui/main_window.cc
    src/ui/image_viewer_widget.cc
    src/utils/file_utils.cc
)

target_include_directories(chameleon_lib PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
)

# OpenCV 5 exports its modules without the OpenCV:: namespace
target_link_libraries(chameleon_lib PUBLIC
    Qt6::Widgets
    Qt6::Concurrent
    opencv_core
    opencv_imgproc
    opencv_imgcodecs
    spdlog::spdlog
    yaml-cpp::yaml-cpp
    glfw
    glad
    OpenGL::GL
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
