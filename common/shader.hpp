/**
 * @file shader.hpp
 * @author Leonel Matos
 * @brief OpenGL shader compilation header
 * @date 2026-04-02
 * @copyright Copyright (c) 2026
 */
#ifndef SHADER_HPP
#define SHADER_HPP

#include <GL/glew.h>
#include <initializer_list>
#include <utility>

/**
 *\brief Compile shaders\n
 Compiles and links on runtime any type of shader (VERTEX, FRAGMENT, COMPUTE)
 *\return GLuint ID

 * Example:
    LoadShaders({
        { GL_VERTEX_SHADER, "shaders/shader.vert" },
        { GL_FRAGMENT_SHADER, "shaders/shader.frag" },
    });
 \param char* shader list
*/
GLuint LoadShaders(std::initializer_list<std::pair<GLenum, const char*>> shaders);

#endif
