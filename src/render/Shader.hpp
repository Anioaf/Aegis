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
        void SetVec3(const std::string& n, const glm::vec3& v) const {
            glUniform3f(glGetUniformLocation(ID, n.c_str()), v.x, v.y, v.z);
        }
        uint32_t ID = 0;
        std::string vPath, fPath;
        Shader(const std::string& vp, const std::string& fp) : vPath(vp), fPath(fp) { Reload(); }
        void Reload() {
            auto rd = [](const std::string& p) {
                std::ifstream f(p);
                if (!f.is_open()) return std::string("");
                std::stringstream s;
                s << f.rdbuf();
                return s.str();
            };

            std::string vC = rd(vPath);
            std::string fC = rd(fPath);
            std::string constCode = rd("gfx/FX/constants.fxh");

            auto processShader = [&](std::string& code) {
                if (constCode.empty()) return;
                size_t verPos = code.find("#version");
                if (verPos != std::string::npos) {
                    size_t lineEnd = code.find("\n", verPos);
                    code.insert(lineEnd + 1, "\n" + constCode + "\n");
                }
            };

            processShader(vC);
            processShader(fC);

            if(vC.empty() || fC.empty()) return;

            auto cp = [](const char* s, GLenum t) {
                uint32_t sh = glCreateShader(t);
                glShaderSource(sh, 1, &s, 0);
                glCompileShader(sh);
                int ok; glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
                if(!ok) {
                    char b[512]; glGetShaderInfoLog(sh, 512, 0, b);
                    std::cerr << "Shader Error: " << b << std::endl;
                    return 0u;
                }
                return sh;
            };

            uint32_t v = cp(vC.c_str(), GL_VERTEX_SHADER);
            uint32_t f = cp(fC.c_str(), GL_FRAGMENT_SHADER);
            uint32_t n = glCreateProgram();
            glAttachShader(n, v);
            glAttachShader(n, f);
            glLinkProgram(n);

            if(ID) glDeleteProgram(ID);
            ID = n;
            glDeleteShader(v);
            glDeleteShader(f);
            std::cout << "Shader Hot-Reloaded!" << std::endl;
        }
        void Use() const { glUseProgram(ID); }
        void SetMat4(const std::string& n, const glm::mat4& m) const { glUniformMatrix4fv(glGetUniformLocation(ID, n.c_str()), 1, 0, glm::value_ptr(m)); }
        void SetFloat(const std::string& n, float v) const { glUniform1f(glGetUniformLocation(ID, n.c_str()), v); }
        void SetInt(const std::string& n, int v) const { glUniform1i(glGetUniformLocation(ID, n.c_str()), v); }
    };
}