#ifndef SHADER_H
#define SHADER_H

#include "glm/ext/vector_float2.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/ext/vector_float4.hpp"
#include "glm/ext/matrix_float4x4.hpp"
#include "glm/gtc/type_ptr.hpp"
#include <glad/glad.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

class Shader {
  public:
    unsigned int programId;

    Shader(const char* vertexPath, const char* fragmentPath) {
        std::string vertexSourceCode = readFile(vertexPath);
        std::string fragmentSourceCode = readFile(fragmentPath);

        const char* vertexShaderCode = vertexSourceCode.c_str();
        const char* fragmentShaderCode = fragmentSourceCode.c_str();

        unsigned int vertex, fragment;
        int success;
        char infoLog[1024];

        vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vertexShaderCode, NULL);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(vertex, 1024, NULL, infoLog);
            std::string message = std::string("Vertex shader compilation failed:\n") + infoLog;
            glDeleteShader(vertex);
            throw std::runtime_error(message);
        };

        fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fragmentShaderCode, NULL);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(fragment, 1024, NULL, infoLog);
            std::string message = std::string("Fragment shader compilation failed:\n") + infoLog;
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            throw std::runtime_error(message);
        };

        this->programId = glCreateProgram();
        glAttachShader(programId, vertex);
        glAttachShader(programId, fragment);
        glLinkProgram(programId);

        glGetProgramiv(programId, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(programId, 1024, NULL, infoLog);
            std::string message = std::string("Shader program linking failed:\n") + infoLog;
            glDeleteShader(vertex);
            glDeleteShader(fragment);
            glDeleteProgram(programId);
            throw std::runtime_error(message);
        }

        glDeleteShader(vertex);
        glDeleteShader(fragment);
    }

    ~Shader() {
        if (programId != 0) {
            glDeleteProgram(programId);
        }
    }

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() { glUseProgram(this->programId); }

    void setBool(const std::string& name, bool value) const {
        glUniform1i(glGetUniformLocation(programId, name.c_str()), (int)value);
    }
    void setInt(const std::string& name, int value) const {
        glUniform1i(glGetUniformLocation(programId, name.c_str()), value);
    }
    void setFloat(const std::string& name, float value) const {
        glUniform1f(glGetUniformLocation(programId, name.c_str()), value);
    }
    void setVec2(const std::string& name, const glm::vec2& value) const {
        glUniform2fv(glGetUniformLocation(programId, name.c_str()), 1, glm::value_ptr(value));
    }
    void setVec3(const std::string& name, const glm::vec3& value) const {
        glUniform3fv(glGetUniformLocation(programId, name.c_str()), 1, glm::value_ptr(value));
    }
    void setVec4(const std::string& name, const glm::vec4& value) const {
        glUniform4fv(glGetUniformLocation(programId, name.c_str()), 1, glm::value_ptr(value));
    }
    void setMat4(const std::string& name, const glm::mat4& value) const {
        glUniformMatrix4fv(glGetUniformLocation(programId, name.c_str()), 1, GL_FALSE,
                           glm::value_ptr(value));
    }

  private:
    static std::string readFile(const char* path) {
        std::ifstream file(path);
        if (!file) {
            throw std::runtime_error(std::string("Could not open shader file: ") + path);
        }

        std::stringstream stream;
        stream << file.rdbuf();
        return stream.str();
    }
};

#endif
