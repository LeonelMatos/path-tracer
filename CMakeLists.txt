cmake_minimum_required(VERSION 3.10)
project(PathTracer)

# Corrige o aviso CMP0072 e força as libs GLVND modernas
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

set(CMAKE_CXX_STANDARD 17)

find_package(OpenGL REQUIRED)

find_package(GLEW)
if (NOT GLEW_FOUND)
    message(FATAL_ERROR "GLEW not found. Install with:\nsudo apt install libglew-dev")
endif()

find_package(glfw3)
if (NOT glfw3_FOUND)
    message(FATAL_ERROR "GLFW not found. Install with:\nsudo apt install libglfw3-dev")
endif()

find_package(glm REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "GLFW not found. Install with:\nsudo apt install libglm-dev")
endif()

find_package(assimp REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "ASSIMP not found. Install with:\nsudo apt install libassimp-dev")
endif()

# Copia os shaders para build
#file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})

# Cria symlink para ter edições em tempo real
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_BINARY_DIR}/shaders
)

find_package(PkgConfig REQUIRED)

add_executable(pathtracer 
    src/main.cpp
    src/mesh.cpp
    common/shader.cpp)

target_include_directories(pathtracer PRIVATE .)

target_link_libraries(pathtracer
    GL
    GLEW::GLEW
    glfw
    m
    glm::glm
    assimp
)