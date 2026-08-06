#pragma once
#include <GL/glew.h>
#include <common/shader.hpp>
#include <initializer_list>
#include <utility>

///\brief RAII wrapper around an OpenGL shader program.
///load() only replaces the held program on a successfull compile+link,
///a failed reload keeps whatever program was there, instead of putting the renderer
///without a valid program.
///This works as a safeguard to call reloadShaders().
class GLProgram {
    GLuint id_ = 0;
public:
    GLProgram() = default;
    ~GLProgram() { reset(); }

    GLProgram(const GLProgram&) = delete;
    GLProgram& operator=(const GLProgram&) = delete;

    GLProgram(GLProgram&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
    GLProgram& operator=(GLProgram&& other) noexcept {
        if(this != &other) { reset(); id_ = std::exchange(other.id_, 0); }
        return *this;
    }

    bool load(std::initializer_list<std::pair<GLenum, const char*>> shaders) {
        GLuint new_id = LoadShaders(shaders);
        if(new_id == 0) return false;
        reset();
        id_ = new_id;
        return true;
    }

    void reset() {
        if(id_) { glDeleteProgram(id_); id_ = 0; }
    }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }
    void use() const { glUseProgram(id_); }
};