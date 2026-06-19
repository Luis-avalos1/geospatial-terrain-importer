#pragma once

#include <QOpenGLFunctions_4_1_Core>
#include <glm/glm.hpp>
#include <string>

class ShaderProgram : protected QOpenGLFunctions_4_1_Core {
public:
    ShaderProgram() = default;
    ~ShaderProgram();

    // No copy; moveable but not needed yet
    ShaderProgram(const ShaderProgram &) = delete;
    ShaderProgram &operator=(const ShaderProgram &) = delete;

    // Load GLSL source from Qt resource paths and link the program.
    // Throws std::runtime_error on compile/link failure.
    void load(const std::string &vertResourcePath,
              const std::string &fragResourcePath);

    // Not const: these call non-const QOpenGLFunctions_4_1_Core members.
    void bind();
    void release();

    void setUniform(const char *name, float v);
    void setUniform(const char *name, int v);
    void setUniform(const char *name, const glm::vec3 &v);
    void setUniform(const char *name, const glm::vec4 &v);
    void setUniform(const char *name, const glm::mat4 &v);

    bool isValid() const { return m_program != 0; }

private:
    GLuint m_program = 0;

    static GLuint compileShader(QOpenGLFunctions_4_1_Core &gl,
                                GLenum type,
                                const std::string &src,
                                const std::string &label);
};
