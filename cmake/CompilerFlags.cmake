# CompilerFlags.cmake — project-wide compiler options

include(CheckCXXCompilerFlag)

function(apply_compiler_flags target)
    target_compile_features(${target} PUBLIC cxx_std_17)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)

    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /wd4100  # unreferenced formal parameter
            /wd4127  # conditional expression is constant
            /permissive-
        )
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
            -Wno-deprecated-declarations
        )
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target} PRIVATE -Wno-gnu-zero-variadic-macro-arguments)
        endif()
    endif()

    if(APPLE)
        # OpenGL API is deprecated on macOS 10.14+ but still functional through 14.x
        target_compile_definitions(${target} PRIVATE GL_SILENCE_DEPRECATION)
    endif()
endfunction()
