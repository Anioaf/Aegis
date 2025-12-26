#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>

namespace aegis::render {
class Shader {
public:
    void SetVec3(const std::string& n, const glm::vec3& v) {
        glUniform3f(glGetUniformLocation(ID, n.c_str()), v.x, v.y, v.z);
    }
    uint32_t ID = 0;
    std::string vPath, fPath;
    Shader(const std::string& vp, const std::string& fp) : vPath(vp), fPath(fp) { Reload(); }
    void Reload() {
        auto rd = [](const std::string& p){std::ifstream f(p); std::stringstream s; s<<f.rdbuf(); return s.str();};
        std::string vC = rd(vPath), fC = rd(fPath);
        if(vC.empty()) return;
        auto cp = [](const char* s, GLenum t){
            uint32_t sh = glCreateShader(t); glShaderSource(sh, 1, &s, 0); glCompileShader(sh);
            int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
            if(!ok){char b[512]; glGetShaderInfoLog(sh, 512, 0, b); std::cerr<<b<<std::endl; return 0u;}
            return sh;
        };
        uint32_t v = cp(vC.c_str(), GL_VERTEX_SHADER), f = cp(fC.c_str(), GL_FRAGMENT_SHADER);
        uint32_t n = glCreateProgram(); glAttachShader(n, v); glAttachShader(n, f); glLinkProgram(n);
        if(ID) glDeleteProgram(ID); ID = n; glDeleteShader(v); glDeleteShader(f);
        std::cout << "Shader Loaded!" << std::endl;
    }
    void Use() { glUseProgram(ID); }
    void SetMat4(const std::string& n, const glm::mat4& m) { glUniformMatrix4fv(glGetUniformLocation(ID, n.c_str()), 1, 0, glm::value_ptr(m)); }
    void SetFloat(const std::string& n, float v) { glUniform1f(glGetUniformLocation(ID, n.c_str()), v); }
    void SetInt(const std::string& n, int v) { glUniform1i(glGetUniformLocation(ID, n.c_str()), v); }
};
}