# ------------------------------------------------------------------------------
# Global compile options.
# ------------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /utf-8      - source files are UTF-8
    # /permissive-- strict standard conformance
    # /Zc:__cplusplus - correct __cplusplus value (checked by some headers)
    add_compile_options(/W4 /utf-8 /permissive- /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra)
endif()
