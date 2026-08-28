cmake_minimum_required(VERSION 3.23)

project(engine)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

find_package(Vulkan REQUIRED)
add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)

add_executable(engine
    src/main.cpp
    src/audio/engine.cpp
    src/input/action.cpp
)

target_include_directories(engine PRIVATE include)

target_link_libraries(engine PRIVATE SDL3::SDL3 Vulkan::Vulkan)
