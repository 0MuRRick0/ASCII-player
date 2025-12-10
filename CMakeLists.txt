cmake_minimum_required(VERSION 3.18)
project(ascii_player LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW REQUIRED glfw3)
find_package(GLEW REQUIRED)

add_executable(ascii_player main.cpp)

target_include_directories(ascii_player PRIVATE
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(ascii_player
    ${OpenCV_LIBS}
    glfw
    GLEW::GLEW
    GL
    pthread
)

file(COPY ./shaders/compute_shader.glsl DESTINATION ${CMAKE_BINARY_DIR})