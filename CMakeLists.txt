cmake_minimum_required(VERSION 3.16)
project(TouchstoneViewer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Concurrent)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/Backend.cpp
    src/S11Parser.cpp
    src/GraphRenderer.cpp
    src/GraphWidget.cpp
)

set(HEADERS
    src/Backend.h
    src/Measurement.h
    src/S11Parser.h
    src/GraphRenderer.h
    src/GraphWidget.h
    src/PerformanceUtils.h
)

qt_add_executable(TouchstoneViewer ${SOURCES} ${HEADERS})

set_target_properties(TouchstoneViewer PROPERTIES WIN32_EXECUTABLE TRUE)


qt_add_resources(TouchstoneViewer "qml_resources"
    PREFIX "/"
    FILES qml/Main.qml
)

target_link_libraries(TouchstoneViewer PRIVATE Qt6::Core Qt6::Quick Qt6::Concurrent)

target_include_directories(TouchstoneViewer PRIVATE src)
