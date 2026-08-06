#pragma once
#include <GL/glew.h>
#include <utility>

///\brief RAII wrapper around an OpenGL buffer object (ssbo/ubo/vbo...)
///Deletes the buffer automatically on destruction and frees any previous buffer before creating
///a new one.
///\note This structure replaces recreateBuffer(), same thing.
class GLBuffer {
    GLuint id_ = 0;
public:
    GLBuffer() = default;
    ~GLBuffer() { reset(); }

    GLBuffer(const GLBuffer&) = delete;
    GLBuffer& operator=(const GLBuffer&) = delete;

    GLBuffer(GLBuffer&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
    GLBuffer& operator=(GLBuffer&& other) noexcept {
        if(this != &other) { reset(); id_ = std::exchange(other.id_, 0); }
        return *this;
    }

    ///\brief Uploads/Reuploads data, freeing any previous held buffer first
    void upload(GLsizeiptr size_bytes, const void* data, GLenum usage) {
        reset();
        glCreateBuffers(1, &id_);
        glNamedBufferData(id_, size_bytes, data, usage);
    }

    void bindBase(GLenum target, GLuint binding) const {
        glBindBufferBase(target, binding, id_);
    }

    void reset() {
        if(id_) { glDeleteBuffers(1, &id_); id_ = 0; }
    }

    GLuint id() const { return id_; }
    explicit operator bool() const { return id_ != 0; }
};