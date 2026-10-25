cmake_minimum_required(VERSION 3.20)
project(Magneticraft2Studio VERSION 0.5.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(STUDIO_HEADLESS_ONLY "Build native core and tests without Qt 6" OFF)
option(STUDIO_BUILD_TESTS "Build unit tests" ON)

add_library(studio_core STATIC
    src/core/Project.hpp src/core/Project.cpp
    src/core/Geometry.hpp src/core/Geometry.cpp
    src/core/Modeling.hpp src/core/Modeling.cpp
)
target_include_directories(studio_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(studio_core PUBLIC cxx_std_17)
if(MSVC)
    target_compile_options(studio_core PRIVATE /W4)
else()
    target_compile_options(studio_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(NOT STUDIO_HEADLESS_ONLY)
    find_package(Qt6 6.4 REQUIRED COMPONENTS Core Widgets Gui OpenGL OpenGLWidgets)
    find_package(ZLIB REQUIRED)
    set(CMAKE_AUTOMOC ON)
    add_executable(magneticraft2-studio
        src/main.cpp
        src/io/ProjectIO.hpp src/io/ProjectIO.cpp
        src/io/BlockModelImport.hpp src/io/BlockModelImport.cpp
        src/io/TextureAssets.hpp src/io/TextureAssets.cpp
        src/ui/MainWindow.hpp src/ui/MainWindow.cpp
        src/ui/StartPage.hpp src/ui/StartPage.cpp
        src/ui/Viewport.hpp src/ui/Viewport.cpp
        src/ui/LayerCanvas.hpp src/ui/LayerCanvas.cpp
        src/ui/UvEditor.hpp src/ui/UvEditor.cpp
    )
    target_link_libraries(magneticraft2-studio PRIVATE studio_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets ZLIB::ZLIB)
    install(TARGETS magneticraft2-studio RUNTIME DESTINATION bin)
    install(FILES packaging/magneticraft2-studio.desktop DESTINATION share/applications)
    install(FILES studio-icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME magneticraft2-studio.svg)
    install(DIRECTORY samples/ DESTINATION share/magneticraft2-studio/samples)
endif()

if(STUDIO_BUILD_TESTS)
    enable_testing()
    add_executable(studio-core-tests tests/test_project.cpp)
    target_link_libraries(studio-core-tests PRIVATE studio_core)
    add_test(NAME studio-core-tests COMMAND studio-core-tests)
    add_executable(studio-geometry-tests tests/test_geometry.cpp)
    target_link_libraries(studio-geometry-tests PRIVATE studio_core)
    add_test(NAME studio-geometry-tests COMMAND studio-geometry-tests)
    add_executable(studio-modeling-tests tests/test_modeling.cpp)
    target_link_libraries(studio-modeling-tests PRIVATE studio_core)
    add_test(NAME studio-modeling-tests COMMAND studio-modeling-tests)
    if(NOT STUDIO_HEADLESS_ONLY)
        find_package(Qt6 6.4 REQUIRED COMPONENTS Test)
        add_executable(studio-gizmos-uv-tests tests/test_gizmos_uv.cpp
            src/ui/Viewport.hpp src/ui/Viewport.cpp
            src/ui/UvEditor.hpp src/ui/UvEditor.cpp)
        target_link_libraries(studio-gizmos-uv-tests PRIVATE studio_core
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
        add_executable(studio-model-ui-tests tests/test_model_ui.cpp
            src/ui/Viewport.hpp src/ui/Viewport.cpp)
        target_link_libraries(studio-model-ui-tests PRIVATE studio_core
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test)
        add_executable(studio-io-tests tests/test_io.cpp src/io/ProjectIO.hpp src/io/ProjectIO.cpp)
        target_link_libraries(studio-io-tests PRIVATE studio_core Qt6::Core Qt6::Gui)
        target_compile_definitions(studio-io-tests PRIVATE STUDIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME studio-io-tests COMMAND studio-io-tests)
        add_executable(studio-import-tests tests/test_import.cpp src/io/BlockModelImport.hpp src/io/BlockModelImport.cpp)
        target_link_libraries(studio-import-tests PRIVATE studio_core Qt6::Core Qt6::Gui)
        target_compile_definitions(studio-import-tests PRIVATE STUDIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME studio-import-tests COMMAND studio-import-tests)
        add_executable(studio-texture-assets-tests tests/test_texture_assets.cpp
            src/io/TextureAssets.cpp src/io/TextureAssets.hpp
            src/io/BlockModelImport.cpp src/io/BlockModelImport.hpp)
        target_link_libraries(studio-texture-assets-tests PRIVATE studio_core Qt6::Core Qt6::Gui ZLIB::ZLIB)
        target_compile_definitions(studio-texture-assets-tests PRIVATE STUDIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME studio-texture-assets-tests COMMAND studio-texture-assets-tests)
        add_executable(studio-editor-ui-tests tests/test_editor_ui.cpp
            src/ui/MainWindow.hpp src/ui/MainWindow.cpp
            src/ui/StartPage.hpp src/ui/StartPage.cpp
            src/ui/Viewport.hpp src/ui/Viewport.cpp
            src/ui/LayerCanvas.hpp src/ui/LayerCanvas.cpp
            src/ui/UvEditor.hpp src/ui/UvEditor.cpp
            src/io/ProjectIO.hpp src/io/ProjectIO.cpp
            src/io/BlockModelImport.hpp src/io/BlockModelImport.cpp
            src/io/TextureAssets.hpp src/io/TextureAssets.cpp)
        target_link_libraries(studio-editor-ui-tests PRIVATE studio_core
            Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets Qt6::Test ZLIB::ZLIB)
    endif()
endif()
