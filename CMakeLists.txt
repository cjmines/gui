# Minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cjmines_gui)

file(GLOB_RECURSE SOURCES "src/*.cpp")


# Add the main executable
add_executable(${PROJECT_NAME} ${SOURCES})

find_package(glad)
find_package(glfw3)
target_link_libraries(${PROJECT_NAME} glad::glad glfw)
