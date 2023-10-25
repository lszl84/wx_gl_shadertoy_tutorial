#pragma once

#include <GL/glew.h>

#include <optional>
#include <sstream>
#include <string>

struct ShaderProgram {

    void Build()
    {
        lastBuildLog = {};

        unsigned int vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource.c_str());
        unsigned int fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource.c_str());

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram.value(), vertexShader);
        glAttachShader(shaderProgram.value(), fragmentShader);

        glLinkProgram(shaderProgram.value());

        // check linking errors
        int success;

        glGetProgramiv(shaderProgram.value(), GL_LINK_STATUS, &success);

        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram.value(), 512, nullptr, infoLog);
            lastBuildLog << "Shader program linking failed: " << infoLog;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    unsigned int CompileShader(unsigned int shaderType,
        const char* shaderSource)
    {
        unsigned int shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &shaderSource, nullptr);
        glCompileShader(shader);

        int success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            lastBuildLog << "Shader compilation failed: " << infoLog;
        }

        return shader;
    }

    std::optional<unsigned int> shaderProgram = std::nullopt;

    std::string vertexShaderSource;
    std::string fragmentShaderSource;

    std::stringstream lastBuildLog;
};
