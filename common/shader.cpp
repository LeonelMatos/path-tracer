/**
 * @file shader.cpp
 * @author Leonel Matos
 * @brief OpenGL shader compilation on runtime
 * @date 2026-04-02
 * @copyright Copyright (c) 2026
 */
#include <stdio.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <initializer_list>
#include <utility>
#include <GL/glew.h>
#include <common/shader.hpp>

using namespace std;

static bool readShaderFile(const char* path, string& out_code) {
    ifstream stream(path, ios::in);
    if(!stream.is_open()) {
        printf("Impossible to open '%s'\n", path);
        return false;
    }
    stringstream sstr;
    sstr << stream.rdbuf();
    out_code = sstr.str();
    //TODO stream clear? + close
    return true;
}

static string preProcess(const string& code, const string& base_dir) {
    string result, line;
    istringstream stream(code);

    while (getline(stream, line)) {
        if (line.find("#include") == 0) {
            size_t start = line.find('"') + 1;
            size_t end = line.rfind('"');
            string include_path = base_dir + "/" + line.substr(start, end-start);

            string include_code;
            if (!readShaderFile(include_path.c_str(), include_code)) return "";
            result += preProcess(include_code, base_dir) + "\n";
        }
        else {
            result += line + "\n";
        }
    }
    return result;
}

static GLuint compileShader(GLenum type, const char* path, const string& code) {
    string base_dir = string(path);
    base_dir = base_dir.substr(0, base_dir.find('/'));

    string processed = preProcess(code, base_dir);

    GLuint id = glCreateShader(type);
    
    printf(" Compiling '%s'", path);
    const char* src = processed.c_str();
    glShaderSource(id, 1, &src, NULL);
    glCompileShader(id);

    GLint result = GL_FALSE;
    int log_len;
    glGetShaderiv(id, GL_COMPILE_STATUS, &result);
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &log_len);
    if(log_len > 0) {
        vector<char> msg(log_len + 1);
        glGetShaderInfoLog(id, log_len, NULL, &msg[0]);
        printf("%s\n", &msg[0]);
    }
    return (result == GL_TRUE) ? id : 0;
}

static GLuint linkProgram(const vector<GLuint>& shader_IDs) {
    printf(" -> Linking program\n");
    GLuint program_id = glCreateProgram();

    for(GLuint id : shader_IDs)
        glAttachShader(program_id, id);
    
    glLinkProgram(program_id);

    GLint result = GL_FALSE;
    int log_len;
    glGetProgramiv(program_id, GL_LINK_STATUS, &result);
    glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_len);

    if(log_len > 0) {
        vector<char> msg(log_len + 1);
        glGetProgramInfoLog(program_id, log_len, NULL, &msg[0]);
        printf("%s\n", &msg[0]);
    }
    
    for (GLuint id : shader_IDs) {
        glDetachShader(program_id, id);
        glDeleteShader(id);
    }

    return (result == GL_TRUE) ? program_id : 0;
}

GLuint LoadShaders(std::initializer_list<std::pair<GLenum, const char*>> shaders) {
    vector<GLuint> compiled;
    
    //TODO Garantee that the standart is C++17
    for (auto& [type, path] : shaders) {
        string code;
        if(!readShaderFile(path, code)) return 0;

        GLuint id = compileShader(type, path, code);
        if(id == 0) return 0;

        compiled.push_back(id);
    }
    return linkProgram(compiled);
}