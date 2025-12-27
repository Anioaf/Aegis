#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include "Shader.hpp"

namespace aegis::render {

struct Character {
	uint32_t TextureID; 
	glm::ivec2 Size;    
	glm::ivec2 Bearing; 
	uint32_t Advance;   
};

class TextRenderer {
    public:
        TextRenderer(const std::string& fontPath, uint32_t fontSize);
        ~TextRenderer();
        
        void RenderText(
            const Shader& shader, 
            const std::string& text, 
            glm::vec3 worldPos, 
            float scale, 
            glm::vec3 color
        );
        
    private:
        std::map<uint32_t, Character> m_Characters; 
        uint32_t VAO = 0, VBO = 0;
        
        void LoadGlyphs(const std::string& fontPath, uint32_t fontSize); 
        uint32_t GetCharCodepoint(const char*& p);
    };
}