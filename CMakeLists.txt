cmake_minimum_required(VERSION 3.16)
project(TouchstoneViewer)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Quick Concurrent)

qt_standard_project_setup()

set(SOURCES
    main.cpp
    Backend.cpp
    S11Parser.cpp
    GraphRenderer.cpp
    GraphWidget.cpp
)

set(HEADERS
    Backend.h
    Measurement.h
    S11Parser.h
    GraphRenderer.h
    GraphWidget.h
    PerformanceUtils.h
)

qt_add_executable(TouchstoneViewer ${SOURCES} ${HEADERS})

qt_add_resources(TouchstoneViewer "qml_resources"
    PREFIX "/"
    FILES Main.qml
)

target_link_libraries(TouchstoneViewer PRIVATE Qt6::Core Qt6::Quick Qt6::Concurrent)
