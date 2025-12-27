#include "TextRenderer.hpp"
#include <iostream>

#include <ft2build.h>
#include FT_FREETYPE_H

namespace aegis::render {

    uint32_t TextRenderer::GetCharCodepoint(const char*& p) {
        uint32_t codepoint = 0;
        unsigned char c = *p;

        if (c < 0x80) {
            codepoint = c;
            p++;
        } else if ((c & 0xE0) == 0xC0) {
            codepoint = (c & 0x1F) << 6;
            c = *++p;
            codepoint |= (c & 0x3F);
            p++;
        } else if ((c & 0xF0) == 0xE0) {
            codepoint = (c & 0x0F) << 12;
            c = *++p;
            codepoint |= (c & 0x3F) << 6;
            c = *++p;
            codepoint |= (c & 0x3F);
            p++;
        } else {
            p++;
        }
        return codepoint;
    }


    void TextRenderer::LoadGlyphs(const std::string& fontPath, uint32_t fontSize) {
        FT_Library ft;
        if (FT_Init_FreeType(&ft)) {
            std::cerr << "Could not init FreeType Library" << std::endl;
            return;
        }

        FT_Face face;
        if (FT_New_Face(ft, fontPath.c_str(), 0, &face)) {
            std::cerr << "Failed to load font: " << fontPath << std::endl;
            FT_Done_FreeType(ft);
            return;
        }
        
        FT_Set_Pixel_Sizes(face, 0, fontSize);

        glPixelStorei(GL_UNPACK_ALIGNMENT, 1); 

        for (uint32_t c = 32; c <= 0x04FF; c++) {
            if (c > 126 && c < 0x0400) continue; 
            
            if (FT_Load_Char(face, c, FT_LOAD_RENDER)) continue;
            
            uint32_t texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RED,
                (int)face->glyph->bitmap.width,
                (int)face->glyph->bitmap.rows,
                0,
                GL_RED,
                GL_UNSIGNED_BYTE,
                face->glyph->bitmap.buffer
            );
            
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            m_Characters.emplace(c, Character{
                texture,
                glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
                glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
                (uint32_t)face->glyph->advance.x
            });
        }

        FT_Done_Face(face);
        FT_Done_FreeType(ft);
    }

    TextRenderer::TextRenderer(const std::string& fontPath, uint32_t fontSize) {
        LoadGlyphs(fontPath, fontSize); 

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    TextRenderer::~TextRenderer() {
        for (auto const& [key, val] : m_Characters) {
            glDeleteTextures(1, &val.TextureID);
        }
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void TextRenderer::RenderText(
        const Shader& shader, 
        const std::string& text, 
        glm::vec3 worldPos, 
        float scale, 
        glm::vec3 color
    ) {
        shader.Use();
        shader.SetVec3("textColor", color); 
        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(VAO);

        float x = worldPos.x;
        float y = worldPos.y; 
        
        const char* p = text.c_str();

        while (*p) {
            uint32_t c = GetCharCodepoint(p);

            if (m_Characters.find(c) == m_Characters.end()) continue;

            Character ch = m_Characters.at(c);

            float xpos = x + ch.Bearing.x * scale;
            float ypos = y - (ch.Size.y - ch.Bearing.y) * scale;
            float w = ch.Size.x * scale;
            float h = ch.Size.y * scale;

            float vertices[6][4] = {
                { xpos,     ypos + h, 0.0f, 0.0f },            
                { xpos,     ypos,     0.0f, 1.0f },
                { xpos + w, ypos,     1.0f, 1.0f },

                { xpos,     ypos + h, 0.0f, 0.0f },
                { xpos + w, ypos,     1.0f, 1.0f },
                { xpos + w, ypos + h, 1.0f, 0.0f }           
            };
            
            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 6 * 4, vertices); 
            glDrawArrays(GL_TRIANGLES, 0, 6);
            
            x += (ch.Advance >> 6) * scale;
        }
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}