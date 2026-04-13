cmake_minimum_required(VERSION 3.16)
project(botnik-config VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(botnik-config main.cpp)

set_target_properties(botnik-config PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_link_libraries(botnik-config PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
