# Minimum required version of CMake
cmake_minimum_required(VERSION 3.10)

add_compile_definitions(_USE_MATH_DEFINES)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(cjmines_gui)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

  add_custom_target(copy_resources ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${PROJECT_SOURCE_DIR}/assets
        ${PROJECT_BINARY_DIR}/assets
        COMMENT "Copying resources into binary directory")

add_dependencies(${PROJECT_NAME} copy_resources)

find_package(glad)
find_package(glfw3)
find_package(spdlog)
find_package(freetype)
find_package(OpenAL)
find_package(SndFile)
find_package(glm)
find_package(stb)
find_package(nlohmann_json)
target_link_libraries(${PROJECT_NAME} glad::glad glfw spdlog::spdlog Freetype::Freetype OpenAL::OpenAL SndFile::sndfile glm::glm stb::stb nlohmann_json::nlohmann_json)
