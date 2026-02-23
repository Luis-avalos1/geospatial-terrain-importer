# C++ unit tests.
#
# Everything links against terrain_core (GDAL only). Camera.cpp is compiled in
# directly because it is the one renderer file with no Qt/OpenGL dependency
# (pure glm math), so it is testable headlessly.

set(TEST_SOURCES
    main.cpp
    test_terrain_mesh.cpp
    test_mesh_simplifier.cpp
    test_texture_atlas.cpp
    test_lod_manager.cpp
    test_coord_converter.cpp
    test_geotiff_reader.cpp
    test_camera.cpp
    ${CMAKE_SOURCE_DIR}/src/renderer/Camera.cpp
)

add_executable(terrain_tests ${TEST_SOURCES})

target_include_directories(terrain_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/renderer
)

target_link_libraries(terrain_tests PRIVATE terrain_core ${GLM_TARGET})
apply_compiler_flags(terrain_tests)

# A single binary runs every case; register it once with CTest. The binary also
# accepts substring filters (e.g. `terrain_tests Camera`) for ad-hoc runs.
add_test(NAME unit COMMAND terrain_tests)

# Per-suite CTest entries for friendlier reporting / parallelism.
foreach(suite mesh simplifier atlas lod coord geotiff camera)
    add_test(NAME ${suite} COMMAND terrain_tests "[${suite}]")
endforeach()
