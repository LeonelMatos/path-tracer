#ifndef SHADER_HPP
#define SHADER_HPP

#include <GL/glew.h>
#include <initializer_list>
#include <utility>

/*Uso
    LoadShaders({
        { GL_VERTEX_SHADER, "shaders/shader.vert" },
        { GL_FRAGMENT_SHADER, "shaders/shader.frag" },
    });
*/
GLuint LoadShaders(std::initializer_list<std::pair<GLenum, const char*>> shaders);

#endif
