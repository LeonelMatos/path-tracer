#pragma once
#include <GL/glew.h>
#include <utility>

///\brief RAII wrapper around an OpenGL texture object of any target
///Be it GL_TEXTURE_2D, GL_TEXTURE_2D_ARRAY...
class GLTexture {
    GLuint id_ = 0;
    GLenum target_ = 0;
public:
    GLTexture() = default;
    ~GLTexture() { reset(); }

    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;

    GLTexture(GLTexture&& other) noexcept
        : id_(std::exchange(other.id_, 0)), target_(std::exchange(other.target_, 0)) {}
    GLTexture& operator=(GLTexture&& other) noexcept {
        if(this != &other) {
            reset();
            id_ = std::exchange(other.id_, 0);
            target_ = std::exchange(other.target_, 0);
        }
        return *this;
    }

    void create(GLenum target) {
        reset();
        target_ = target;
        glCreateTextures(target, 1, &id_);
    }
    
    void reset() {
        if(id_) { glDeleteTextures(1, &id_); id_ = 0; target_ = 0; }
    }

    GLuint id() const { return id_; }
    GLenum target() const { return target_; }
    explicit operator bool() const { return id_ != 0; }
};