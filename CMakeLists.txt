cmake_minimum_required(VERSION 3.20)
project(Magneticraft2Studio VERSION 0.3.0 LANGUAGES CXX)
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
    set(CMAKE_AUTOMOC ON)
    add_executable(magneticraft2-studio
        src/main.cpp
        src/io/ProjectIO.hpp src/io/ProjectIO.cpp
        src/ui/MainWindow.hpp src/ui/MainWindow.cpp
        src/ui/Viewport.hpp src/ui/Viewport.cpp
        src/ui/LayerCanvas.hpp src/ui/LayerCanvas.cpp
    )
    target_link_libraries(magneticraft2-studio PRIVATE studio_core Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
    install(TARGETS magneticraft2-studio RUNTIME DESTINATION bin)
    install(FILES packaging/magneticraft2-studio.desktop DESTINATION share/applications)
    install(FILES studio-icon.svg DESTINATION share/icons/hicolor/scalable/apps RENAME magneticraft2-studio.svg)
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
        add_executable(studio-io-tests tests/test_io.cpp src/io/ProjectIO.hpp src/io/ProjectIO.cpp)
        target_link_libraries(studio-io-tests PRIVATE studio_core Qt6::Core)
        target_compile_definitions(studio-io-tests PRIVATE STUDIO_TEST_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
        add_test(NAME studio-io-tests COMMAND studio-io-tests)
    endif()
endif()
