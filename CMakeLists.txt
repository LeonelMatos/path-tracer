cmake_minimum_required(VERSION 3.10)
project(PathTracer)

# Corrige o aviso CMP0072 e força as libs GLVND modernas
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

set(CMAKE_CXX_STANDARD 11)

find_package(OpenGL REQUIRED)
find_package(GLEW   REQUIRED)
find_package(glfw3  REQUIRED)

find_package(PkgConfig REQUIRED)

add_executable(pathtracer src/main.cpp)

target_link_libraries(pathtracer
    GL
    GLEW::GLEW
    glfw
    m
)