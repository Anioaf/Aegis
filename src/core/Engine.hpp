#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "render/Shader.hpp"
#include "render/Camera.hpp"
#include "render/TextRenderer.hpp"

namespace aegis {

struct ProvinceDef {
	uint32_t id;
	std::string type;
	bool isCoastal;
};

class Engine {
    public:
        void Run();
    private:
        void InitWindow();
        void InitGraphics();
        void InitImGui();
        void MainLoop();
        void UpdateUI();

        void LoadDefinitions(const std::string& path);
        void LoadCountryColors(const std::string& path);
        void LoadStates(const std::string& folder);
        void LoadLocalizations(const std::string& folder, const std::string& lang = "russian"); 
        void CalculateTextPositions();
        
        void GenerateStateMap(const std::string& provMapPath);
        void UpdateMousePicking();

        uint32_t PackRGB(uint8_t r, uint8_t g, uint8_t b) {
            return (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
        }

        GLFWwindow* m_Window;
        std::unique_ptr<render::Shader> m_Shader;
        std::unique_ptr<render::Shader> m_TextShader;
        std::unique_ptr<render::TextRenderer> m_TextRenderer;

        render::Camera m_Camera;
        uint32_t m_VAO, m_VBO, m_EBO;
        uint32_t m_TexNorm, m_TexHeight, m_TexProv, m_TexStateMap;

        std::vector<unsigned char> m_RawProvData;
        int m_ProvImgW = 0, m_ProvImgH = 0;

        std::unordered_map<uint32_t, uint32_t> m_ColorToProvID;
        std::unordered_map<uint32_t, std::string> m_ProvToOwner;
        std::unordered_map<std::string, glm::vec3> m_CountryColors;
        std::unordered_map<std::string, std::string> m_Localizations; 

        struct CountryTextData {
            std::string tag;
            glm::vec3 worldPos; 
            std::string displayName;
            float size = 0.05f;
        };
        std::vector<CountryTextData> m_CountryTexts;

        uint32_t m_SelectedProvID = 0;
        std::string m_SelectedOwner = "None";
        glm::vec3 m_SelectedRGB = {0,0,0};

        float m_Last = 0, m_GameTime = 0, m_TimeScale = 1.0f;
        bool m_Paused = false;
    };
}