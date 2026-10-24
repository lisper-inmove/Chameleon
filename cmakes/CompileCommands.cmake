# ------------------------------------------------------------------------------
# Generates ${CMAKE_BINARY_DIR}/compile_commands.json for clangd/clang-tidy.
#
# The Visual Studio generator does not export compile commands, so the
# commands are composed here from the targets' interface properties:
# every include directory of chameleon_lib (own + linked Qt/OpenCV/spdlog/
# yaml-cpp/GTest targets) plus the compile definitions of the test target.
# ------------------------------------------------------------------------------

function(_chameleon_append_interface_includes out_var target)
    get_target_property(_incs ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(_incs)
        list(APPEND ${out_var} ${_incs})
        set(${out_var} ${${out_var}} PARENT_SCOPE)
    endif()
endfunction()

function(_chameleon_append_interface_defines out_var target)
    get_target_property(_defs ${target} INTERFACE_COMPILE_DEFINITIONS)
    if(_defs)
        list(APPEND ${out_var} ${_defs})
        set(${out_var} ${${out_var}} PARENT_SCOPE)
    endif()
endfunction()

if(TARGET chameleon_lib)
    set(_includes)
    # Own include dirs (include/ + generated header dir)
    get_target_property(_own chameleon_lib INCLUDE_DIRECTORIES)
    if(_own)
        list(APPEND _includes ${_own})
    endif()

    _chameleon_append_interface_includes(_includes Qt6::Widgets)
    _chameleon_append_interface_includes(_includes Qt6::Gui)
    _chameleon_append_interface_includes(_includes Qt6::Core)
    _chameleon_append_interface_includes(_includes Qt6::Concurrent)
    _chameleon_append_interface_includes(_includes Qt6::Test)
    _chameleon_append_interface_includes(_includes opencv_core)
    _chameleon_append_interface_includes(_includes spdlog::spdlog)
    _chameleon_append_interface_includes(_includes yaml-cpp::yaml-cpp)
    _chameleon_append_interface_includes(_includes GTest::gtest)
    _chameleon_append_interface_includes(_includes glad)
    # GLFW's exported include dirs contain unresolved BUILD_INTERFACE
    # generator expressions; add the source-tree header dir directly
    list(APPEND _includes "${CHAMELEON_GLFW_SOURCE_DIR}/include")
    _chameleon_append_interface_includes(_includes OpenGL::GL)

    list(REMOVE_DUPLICATES _includes)

    # Interface compile definitions from linked targets: Qt's library
    # macros (QT_WIDGETS_LIB, ...) gate half of the public headers
    # (e.g. QTest::mousePress), so they must reach clangd too
    set(_lib_defs)
    _chameleon_append_interface_defines(_lib_defs Qt6::Widgets)
    _chameleon_append_interface_defines(_lib_defs Qt6::Gui)
    _chameleon_append_interface_defines(_lib_defs Qt6::Core)
    _chameleon_append_interface_defines(_lib_defs Qt6::Concurrent)
    _chameleon_append_interface_defines(_lib_defs Qt6::Test)
    _chameleon_append_interface_defines(_lib_defs opencv_core)
    _chameleon_append_interface_defines(_lib_defs spdlog::spdlog)
    _chameleon_append_interface_defines(_lib_defs yaml-cpp::yaml-cpp)
    _chameleon_append_interface_defines(_lib_defs GTest::gtest)
    get_target_property(_own_defs chameleon_lib INTERFACE_COMPILE_DEFINITIONS)
    if(_own_defs)
        list(APPEND _lib_defs ${_own_defs})
    endif()
    # Drop raw generator expressions (unexpanded outside generation):
    # they would be written verbatim and are invalid macro names
    list(FILTER _lib_defs EXCLUDE REGEX "\\$<")
    # Explicitly add the definitions whose genexes evaluate to active
    # here, so clangd sees the same macro set as the real MSVC build
    list(APPEND _lib_defs YAML_CPP_STATIC_DEFINE)
    list(REMOVE_DUPLICATES _lib_defs)

    get_target_property(_test_defs chameleon_tests COMPILE_DEFINITIONS)

    # No quotes around paths: every path in this project is space-free,
    # which keeps the generated JSON trivially valid (no escaping needed)
    # Flags mirror cmakes/CompilerOptions.cmake + the MSVC defaults
    set(_common_flags "/std:c++17 /EHsc /permissive- /Zc:__cplusplus")
    foreach(_inc IN LISTS _includes)
        string(APPEND _common_flags " /I${_inc}")
    endforeach()
    foreach(_def IN LISTS _lib_defs)
        string(APPEND _common_flags " /D${_def}")
    endforeach()

    function(_chameleon_write_entry file defs)
        set(_flags "${_common_flags}")
        foreach(_def IN LISTS ${defs})
            string(APPEND _flags " /D${_def}")
        endforeach()
        # Definitions may carry double quotes (string literal macro values).
        # The JSON file needs \\\" (3 backslashes + quote): JSON decoding
        # yields \" and clangd's tokenizer keeps the quote inside the value
        string(REPLACE [=["]=] [=[\\\"]=] _flags "${_flags}")
        string(APPEND _json "  {\n    \"directory\": \"${CMAKE_BINARY_DIR}\",\n    \"file\": \"${file}\",\n    \"command\": \"clang-cl ${_flags} /c ${file}\"\n  },\n")
        set(_json "${_json}" PARENT_SCOPE)
    endfunction()

    set(_json "[\n")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/main.cpp" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/config/config_manager.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/utils/file_utils.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/app/app_state.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/binary.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/filter_processor.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/geometry.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/histogram.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/hough.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/image_io.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/processing/noise.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/rendering/opengl_image.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/image_viewer_widget.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/back_project_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/connected_components_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/contour_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/convolution_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/edge_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/filter_panel.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/geometry_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/gradient_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/hough_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/noise_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/sharpen_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/threshold_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/histogram_compare_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/histogram_match_dialog.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/histogram_window.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/image_conversion.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/image_window.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/src/ui/main_window.cc" "")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/main.cpp" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/config_manager_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/file_utils_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/app_state_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/binary_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/geometry_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/filter_processor_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/histogram_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/hough_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/noise_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/opengl_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/image_viewer_test.cc" "_test_defs")
    _chameleon_write_entry("${CMAKE_SOURCE_DIR}/tests/main_window_test.cc" "_test_defs")
    # Drop the trailing comma (strict JSON forbids it)
    string(REGEX REPLACE ",\n$" "\n" _json "${_json}")
    string(APPEND _json "]\n")

    file(WRITE "${CMAKE_BINARY_DIR}/compile_commands.json" "${_json}")
    # Also drop a copy at the source root: clangd's default discovery
    # walks up from each source file, so navigation works even without
    # the .clangd CompilationDatabase directive (git-ignored)
    file(WRITE "${CMAKE_SOURCE_DIR}/compile_commands.json" "${_json}")
    message(STATUS "Generated compile_commands.json for clangd (${CMAKE_BINARY_DIR} + source root)")
endif()
