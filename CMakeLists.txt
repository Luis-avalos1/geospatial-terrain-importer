cmake_minimum_required(VERSION 3.22)
project(terrain-importer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ── Options ───────────────────────────────────────────────────────────────────
# The GUI pulls in Qt + OpenGL; the core library and tests need neither, so CI
# (and anyone who just wants the algorithms) can build with -DBUILD_GUI=OFF.
option(BUILD_GUI     "Build the Qt/OpenGL desktop application" ON)
option(BUILD_TESTING "Build the C++ unit tests"               ON)

include(cmake/CompilerFlags.cmake)

# ── Dependencies needed by the core library ───────────────────────────────────

# GDAL: try CMake's built-in config first, then our gdal-config finder.
find_package(GDAL QUIET)
if(NOT GDAL_FOUND)
    find_package(GDAL REQUIRED MODULE)
endif()

# GLM is header-only and used by the renderer + the camera math tests.
find_package(glm QUIET)
if(glm_FOUND)
    message(STATUS "Found system glm")
    set(GLM_TARGET glm::glm)
else()
    message(STATUS "glm not found via CMake — assuming headers are on the system include path")
    set(GLM_TARGET "")
endif()

# ── terrain_core: the dependency-light core library (GDAL only, no Qt/GL) ──────

set(CORE_SOURCES
    src/core/GeoTiffReader.cpp
    src/core/SatelliteImageLoader.cpp
    src/core/TerrainMesh.cpp
    src/core/LodManager.cpp
    src/core/MeshSimplifier.cpp
    src/core/TextureAtlas.cpp
    src/core/CoordConverter.cpp
)

add_library(terrain_core STATIC ${CORE_SOURCES})
target_include_directories(terrain_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GDAL_INCLUDE_DIRS}
)
target_link_libraries(terrain_core PUBLIC GDAL::GDAL)
apply_compiler_flags(terrain_core)

# ── terrain-importer: the desktop GUI application (optional) ───────────────────

if(BUILD_GUI)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTORCC ON)
    set(CMAKE_AUTOUIC OFF)

    # Qt6 with Qt5 fallback
    find_package(Qt6 QUIET COMPONENTS Core Widgets OpenGL OpenGLWidgets Concurrent)
    if(Qt6_FOUND)
        set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Concurrent)
        message(STATUS "Using Qt6")
    else()
        find_package(Qt5 REQUIRED COMPONENTS Core Widgets OpenGL Concurrent)
        set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::OpenGL Qt5::Concurrent)
        message(STATUS "Using Qt5 (fallback)")
    endif()

    find_package(OpenGL REQUIRED)

    # Shaders + UI stylesheet (compiled into the binary as Qt resources)
    if(Qt6_FOUND)
        qt6_add_resources(SHADER_RCC src/shaders/shaders.qrc)
        qt6_add_resources(STYLE_RCC  src/styles/style.qrc)
    else()
        qt5_add_resources(SHADER_RCC src/shaders/shaders.qrc)
        qt5_add_resources(STYLE_RCC  src/styles/style.qrc)
    endif()

    set(RENDERER_SOURCES
        src/renderer/TerrainRenderer.cpp
        src/renderer/ShaderProgram.cpp
        src/renderer/Camera.cpp
        src/renderer/GpuMesh.cpp
    )

    set(GUI_SOURCES
        src/gui/MainWindow.cpp
        src/gui/FileBrowserPanel.cpp
        src/gui/ImportControlsWidget.cpp
        src/gui/SettingsPanel.cpp
        src/gui/ProgressDialog.cpp
    )

    add_executable(terrain-importer
        src/main.cpp
        ${RENDERER_SOURCES}
        ${GUI_SOURCES}
        ${SHADER_RCC}
        ${STYLE_RCC}
    )

    target_include_directories(terrain-importer PRIVATE src/)
    target_link_libraries(terrain-importer PRIVATE
        terrain_core
        ${QT_LIBS}
        OpenGL::GL
        ${GLM_TARGET}
    )
    apply_compiler_flags(terrain-importer)

    # macOS: produce a proper double-clickable .app bundle
    if(APPLE)
        set_target_properties(terrain-importer PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME "Terrain Importer"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.geospatial-tools.terrain-importer"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist.in"
            # Run in-place against Homebrew Qt during development; @rpath/Frameworks
            # is where macdeployqt would place them for distribution.
            INSTALL_RPATH "@executable_path/../Frameworks"
            BUILD_WITH_INSTALL_RPATH FALSE
        )
    endif()
else()
    message(STATUS "BUILD_GUI=OFF — skipping the Qt/OpenGL application")
endif()

# ── Tests ──────────────────────────────────────────────────────────────────────

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests/cpp)
endif()
