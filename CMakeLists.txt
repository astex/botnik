cmake_minimum_required(VERSION 3.16)
project(botnik VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick WaylandCompositor Network)

qt_add_executable(botnik
    src/main.cpp
    src/compositor.h src/compositor.cpp
    src/chatmodel.h src/chatmodel.cpp
    src/ollamaclient.h src/ollamaclient.cpp
    src/toolhost.h src/toolhost.cpp
    src/applauncher.h src/applauncher.cpp
    src/stdinreader.h src/stdinreader.cpp
    src/headlesscompositor.h src/headlesscompositor.cpp
    src/windowtools.h src/windowtools.cpp
)

target_link_libraries(botnik PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::WaylandCompositor
    Qt6::Network
)

qt_add_resources(botnik "qml"
    PREFIX /
    FILES
        qml/Main.qml
)

add_subdirectory(apps/clock)
add_subdirectory(apps/config)
add_subdirectory(apps/volume)
