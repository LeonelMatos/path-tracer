cmake_minimum_required(VERSION 3.16)
project(PathTracer)

#Defaults to an optimized build if nothing was specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
endif()

# Corrige o aviso CMP0072 e força as libs GLVND modernas
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Check OpenGL
find_package(OpenGL REQUIRED)

# Check GLEW
find_package(GLEW)
if (NOT GLEW_FOUND)
    message(FATAL_ERROR "GLEW not found. Install with:\nsudo apt install libglew-dev")
endif()

# Check GLFW3
find_package(glfw3)
if (NOT glfw3_FOUND)
    message(FATAL_ERROR "GLFW not found. Install with:\nsudo apt install libglfw3-dev")
endif()

# Check GLM
find_package(glm)
if (NOT glm_FOUND)
    message(FATAL_ERROR "GLFW not found. Install with:\nsudo apt install libglm-dev")
endif()

# Check ASSIMP
find_package(assimp)
if (NOT glm_FOUND)
    message(FATAL_ERROR "ASSIMP not found. Install with:\nsudo apt install libassimp-dev")
endif()

# OIDN -> configured path with -DOIDN_ROOT or fallback to /opt/oidn for hard install
if(NOT DEFINED OIDN_ROOT)
    set(OIDN_ROOT "/opt/oidn")
endif()
set(OpenImageDenoise_DIR ${OIDN_ROOT}/lib/cmake/OpenImageDenoise-2.5.0)
find_package(OpenImageDenoise REQUIRED)

# Creates symlink shaders to update in real time
execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_SOURCE_DIR}/shaders
    ${CMAKE_BINARY_DIR}/shaders
)

find_package(PkgConfig)

add_library(imgui STATIC
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_impl_glfw.cpp
    include/imgui/imgui_impl_opengl3.cpp)
target_include_directories(imgui SYSTEM PUBLIC include)
target_link_libraries(imgui PUBLIC glfw)

add_executable(pathtracer 
    src/main.cpp
    src/mesh.cpp
    src/bvh.cpp
    src/texture.cpp
    common/shader.cpp)

target_include_directories(pathtracer PRIVATE . src)
target_include_directories(pathtracer SYSTEM PRIVATE include)

target_compile_options(pathtracer PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wshadow>)

target_link_libraries(pathtracer
    imgui
    OpenGL::GL
    GLEW::GLEW
    glfw
    m
    glm::glm
    assimp
    OpenImageDenoise
)

# Set Rpath to find .so in lib/
set_target_properties(pathtracer PROPERTIES
    BUILD_RPATH "$ORIGIN/lib"
    INSTALL_RPATH "$ORIGIN/lib"
)

# Copies OIDN's .so to build/lib as a symlink automatically after each build
add_custom_command(TARGET pathtracer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:pathtracer>/lib
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${OIDN_ROOT}/lib/libOpenImageDenoise.so.2.5.0
        ${OIDN_ROOT}/lib/libOpenImageDenoise_core.so.2.5.0
        ${OIDN_ROOT}/lib/libOpenImageDenoise_device_cpu.so.2.5.0
        ${OIDN_ROOT}/lib/libOpenImageDenoise_device_cuda.so.2.5.0
        ${OIDN_ROOT}/lib/libtbb.so.12.18
        $<TARGET_FILE_DIR:pathtracer>/lib
    #Symlinks
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    libOpenImageDenoise.so.2.5.0
    $<TARGET_FILE_DIR:pathtracer>/lib/libOpenImageDenoise.so.2
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    libOpenImageDenoise.so.2.5.0
    $<TARGET_FILE_DIR:pathtracer>/lib/libOpenImageDenoise.so
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    libtbb.so.12.18
    $<TARGET_FILE_DIR:pathtracer>/lib/libtbb.so.12
    COMMAND ${CMAKE_COMMAND} -E create_symlink
    libtbb.so.12.18
    $<TARGET_FILE_DIR:pathtracer>/lib/libtbb.so
    COMMENT "Copying OIDN libs to build/lib/"
)