# FindGDAL.cmake — fallback finder using gdal-config
# Used when CMake's bundled FindGDAL doesn't locate the Homebrew install.

find_program(GDAL_CONFIG gdal-config
    HINTS
        /opt/homebrew/bin
        /usr/local/bin
        /usr/bin
)

if(GDAL_CONFIG)
    execute_process(
        COMMAND ${GDAL_CONFIG} --cflags
        OUTPUT_VARIABLE _gdal_cflags
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GDAL_CONFIG} --libs
        OUTPUT_VARIABLE _gdal_libs
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${GDAL_CONFIG} --prefix
        OUTPUT_VARIABLE GDAL_PREFIX
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Parse include dirs from cflags
    string(REGEX MATCHALL "-I([^ ]+)" _inc_flags "${_gdal_cflags}")
    set(GDAL_INCLUDE_DIRS)
    foreach(f ${_inc_flags})
        string(REGEX REPLACE "^-I" "" d "${f}")
        list(APPEND GDAL_INCLUDE_DIRS "${d}")
    endforeach()

    # Parse libs
    string(REGEX MATCHALL "-l([^ ]+)" _lib_names "${_gdal_libs}")
    string(REGEX MATCHALL "-L([^ ]+)" _lib_dirs  "${_gdal_libs}")

    set(_lib_search_dirs)
    foreach(d ${_lib_dirs})
        string(REGEX REPLACE "^-L" "" dir "${d}")
        list(APPEND _lib_search_dirs "${dir}")
    endforeach()

    find_library(GDAL_LIBRARY
        NAMES gdal gdal_i
        HINTS ${_lib_search_dirs} "${GDAL_PREFIX}/lib"
    )

    if(GDAL_LIBRARY AND GDAL_INCLUDE_DIRS)
        set(GDAL_FOUND TRUE)
        if(NOT TARGET GDAL::GDAL)
            add_library(GDAL::GDAL UNKNOWN IMPORTED)
            set_target_properties(GDAL::GDAL PROPERTIES
                IMPORTED_LOCATION "${GDAL_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${GDAL_INCLUDE_DIRS}"
            )
        endif()
        message(STATUS "Found GDAL via gdal-config: ${GDAL_LIBRARY}")
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GDAL
    REQUIRED_VARS GDAL_LIBRARY GDAL_INCLUDE_DIRS
)
