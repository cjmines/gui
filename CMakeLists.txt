# Minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

project(cjmines_gui)

file(GLOB_RECURSE SOURCES "src/*.cpp")


# Add the main executable
add_executable(${PROJECT_NAME} ${SOURCES})


find_package(glad)
find_package(glfw3)
target_link_libraries(${PROJECT_NAME} glad::glad glfw)
