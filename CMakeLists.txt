cmake_minimum_required(VERSION 3.23)


project(engine)
add_executable(engine
    src/main.cpp
)

find_package(SDL3 REQUIRED CONFIG REQUIRED COMPONENTS SDL3)
find_package(Vulkan REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

target_link_libraries(engine PRIVATE SDL3::SDL3 Vulkan::Vulkan)
