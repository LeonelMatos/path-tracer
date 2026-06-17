cmake_minimum_required(VERSION 3.10)
project(PathTracer)

# Corrige o aviso CMP0072 e força as libs GLVND modernas
cmake_policy(SET CMP0072 NEW)
set(OpenGL_GL_PREFERENCE GLVND)

set(CMAKE_CXX_STANDARD 17)

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
find_package(glm REQUIRED)
if (NOT glm_FOUND)
    message(FATAL_ERROR "GLFW not found. Install with:\nsudo apt install libglm-dev")
endif()

# Check ASSIMP
find_package(assimp REQUIRED)
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

find_package(PkgConfig REQUIRED)

add_executable(pathtracer 
    src/main.cpp
    src/mesh.cpp
    src/bvh.cpp
    src/texture.cpp
    common/shader.cpp
    include/imgui/imgui.cpp
    include/imgui/imgui_draw.cpp
    include/imgui/imgui_tables.cpp
    include/imgui/imgui_widgets.cpp
    include/imgui/imgui_impl_glfw.cpp
    include/imgui/imgui_impl_opengl3.cpp)

target_include_directories(pathtracer PRIVATE . include imgui)

target_link_libraries(pathtracer
    GL
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