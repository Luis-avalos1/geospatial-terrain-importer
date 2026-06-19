#include "ShaderProgram.hpp"

#include <QFile>
#include <glm/gtc/type_ptr.hpp>

#include <stdexcept>
#include <string>
#include <vector>

namespace {
    std::string readQrc(const std::string &path) {
        QFile f(QString::fromStdString(path));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            throw std::runtime_error("ShaderProgram: cannot open resource '" + path + "'");
        const QByteArray data = f.readAll();
        return std::string(data.constData(), data.size());
    }
}

ShaderProgram::~ShaderProgram()
{
    if (m_program) {
        initializeOpenGLFunctions();
        glDeleteProgram(m_program);
    }
}

void ShaderProgram::load(const std::string &vertPath, const std::string &fragPath)
{
    initializeOpenGLFunctions();

    const std::string vertSrc = readQrc(vertPath);
    const std::string fragSrc = readQrc(fragPath);

    const GLuint vert = compileShader(*this, GL_VERTEX_SHADER,   vertSrc, vertPath);
    const GLuint frag = compileShader(*this, GL_FRAGMENT_SHADER, fragSrc, fragPath);

    const GLuint prog = glCreateProgram();
    glAttachShader(prog, vert);
    glAttachShader(prog, frag);
    glLinkProgram(prog);

    GLint ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        glGetProgramInfoLog(prog, len, nullptr, log.data());
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(prog);
        throw std::runtime_error("ShaderProgram link error:\n" + log);
    }

    glDetachShader(prog, vert);
    glDetachShader(prog, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    if (m_program) glDeleteProgram(m_program);
    m_program = prog;
}

void ShaderProgram::bind()    { glUseProgram(m_program); }
void ShaderProgram::release() { glUseProgram(0); }

void ShaderProgram::setUniform(const char *name, float v) {
    glUniform1f(glGetUniformLocation(m_program, name), v);
}
void ShaderProgram::setUniform(const char *name, int v) {
    glUniform1i(glGetUniformLocation(m_program, name), v);
}
void ShaderProgram::setUniform(const char *name, const glm::vec3 &v) {
    glUniform3fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(v));
}
void ShaderProgram::setUniform(const char *name, const glm::vec4 &v) {
    glUniform4fv(glGetUniformLocation(m_program, name), 1, glm::value_ptr(v));
}
void ShaderProgram::setUniform(const char *name, const glm::mat4 &m) {
    glUniformMatrix4fv(glGetUniformLocation(m_program, name), 1, GL_FALSE, glm::value_ptr(m));
}

// static
GLuint ShaderProgram::compileShader(QOpenGLFunctions_4_1_Core &gl,
                                    GLenum type,
                                    const std::string &src,
                                    const std::string &label)
{
    const GLuint shader = gl.glCreateShader(type);
    const char *ptr = src.c_str();
    gl.glShaderSource(shader, 1, &ptr, nullptr);
    gl.glCompileShader(shader);

    GLint ok = 0;
    gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<size_t>(len), '\0');
        gl.glGetShaderInfoLog(shader, len, nullptr, log.data());
        gl.glDeleteShader(shader);
        throw std::runtime_error("Shader compile error in '" + label + "':\n" + log);
    }
    return shader;
}
