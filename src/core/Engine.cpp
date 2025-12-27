#include "Engine.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <regex>

namespace aegis {
    namespace fs = std::filesystem;

    static float g_Scroll = 0;

    static const float WORLD_WIDTH = 5.632f;
    static const float WORLD_HEIGHT = 2.048f;

    void Engine::InitWindow() {
        if (!glfwInit()) return;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        m_Window = glfwCreateWindow(1920, 1080, "Aegis", NULL, NULL);
        if (!m_Window) return;

        glfwMakeContextCurrent(m_Window);
        
        glewExperimental = GL_TRUE;
        GLenum err = glewInit();
        if (err != GLEW_OK) {
            std::cerr << "Failed to initialize GLEW: " << glewGetErrorString(err) << std::endl;
            return;
        }

        glfwSetScrollCallback(m_Window, [](GLFWwindow*, double, double yoffset) { g_Scroll = (float)yoffset; });
    }

    void Engine::InitImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        static const ImWchar ranges[] = 
        {
            0x0020, 0x00FF,
            0x0400, 0x04FF,
            0,
        };
        
        io.FontDefault = io.Fonts->AddFontFromFileTTF(
            "./font/JetBrainsMono-Medium.ttf", 
            16.0f, 
            NULL, 
            ranges
        );
        
        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void Engine::LoadDefinitions(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) { std::cerr << "Fail: definition.csv" << std::endl; return; }
        
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string t;
            std::vector<std::string> v;
            while (std::getline(ss, t, ';')) v.push_back(t);
            
            if (v.size() >= 4) {
                try {
                    uint32_t id = std::stoi(v[0]);
                    uint32_t r = std::stoi(v[1]), g = std::stoi(v[2]), b = std::stoi(v[3]);
                    m_ColorToProvID[PackRGB(r, g, b)] = id;
                } catch (...) { continue; }
            }
        }
    }

    void Engine::LoadCountryColors(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::regex re("([A-Z0-9]{3})\\s*=\\s*\\{[^}]*color\\s*=\\s*rgb\\s*\\{\\s*(\\d+)\\s+(\\d+)\\s+(\\d+)\\s*\\}");
        auto it = std::sregex_iterator(content.begin(), content.end(), re);
        for (; it != std::sregex_iterator(); ++it) {
            m_CountryColors[(*it)[1].str()] = glm::vec3(std::stoi((*it)[2].str()) / 255.f, 
            std::stoi((*it)[3].str()) / 255.f, 
            std::stoi((*it)[4].str()) / 255.f);
        }
    }

    void Engine::LoadLocalizations(const std::string& folder, const std::string& lang) {
        std::string lang_tag = "_l_" + lang + ".yml";

        if (!fs::exists(folder)) {
            std::cerr << "Localisation folder not found: " << folder << std::endl;
            return;
        }

        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.is_regular_file() && entry.path().extension() == ".yml") {
                if (entry.path().string().find(lang_tag) == std::string::npos) {
                    continue; 
                }

                std::ifstream f(entry.path());
                if (!f.is_open()) continue;
                
                std::string content;
                content.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

                size_t start = content.find("l_" + lang + ":");
                if (start != std::string::npos) {
                    start = content.find('\n', start);
                } else {
                    continue;
                }

                std::regex re("^\\s*([A-Z0-9_]+):\\s*\"(.*?)\"", std::regex::multiline);
                
                auto it = std::sregex_iterator(content.begin() + (start != std::string::npos ? start : 0), content.end(), re);
                auto end = std::sregex_iterator();

                for (; it != end; ++it) {
                    m_Localizations[(*it)[1].str()] = (*it)[2].str();
                }
                std::cout << "Loaded localisation: " << entry.path().filename().string() << std::endl;
            }
        }
    }

    void Engine::LoadStates(const std::string& folder) {
        if (!fs::exists(folder)) return;
        for (auto& p : fs::directory_iterator(folder)) {
            if (p.path().extension() != ".txt") continue;
            std::ifstream f(p.path());
            std::string c((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            c = std::regex_replace(c, std::regex("#.*"), ""); 

            std::smatch m;
            std::string owner = "";
            if (std::regex_search(c, m, std::regex("owner\\s*=\\s*([A-Z0-9]+)"))) {
                owner = m[1].str();
            }

            std::regex provRe("provinces\\s*=\\s*\\{([^\\}]+)\\}");
            if (std::regex_search(c, m, provRe) && !owner.empty()) {
                std::stringstream ss(m[1].str());
                uint32_t id;
                while (ss >> id) m_ProvToOwner[id] = owner;
            }
        }
    }

    void Engine::CalculateTextPositions() {
        m_CountryTexts.clear();
        
        std::unordered_map<std::string, bool> uniqueOwners;
        for(const auto& pair : m_ProvToOwner) {
            uniqueOwners[pair.second] = true;
        }
        
        float offset = -0.5f;
        for(const auto& ownerPair : uniqueOwners) {
            const std::string& ownerTag = ownerPair.first;
            
            std::string displayName = ownerTag;
            if (m_Localizations.count(ownerTag)) {
                displayName = m_Localizations[ownerTag];
            } else if (m_Localizations.count(ownerTag + "_DEF")) {
                displayName = m_Localizations[ownerTag + "_DEF"];
            }

            m_CountryTexts.push_back({
                ownerTag, 
                {WORLD_WIDTH * 0.5f + offset, WORLD_HEIGHT * 0.5f, 0.0f}, 
                displayName, 
                0.05f 
            });
            offset += 0.2f;
        }
    }

    void Engine::GenerateStateMap(const std::string& provMapPath)
    {
        int w, h, c;
        unsigned char* prov = stbi_load(provMapPath.c_str(), &w, &h, &c, 3);
        if (!prov) return;

        int hw, hh, hc;
        unsigned char* height = stbi_load("map/heightmap.bmp", &hw, &hh, &hc, 1);
        if (!height || hw != w || hh != h) {
            std::cerr << "Heightmap size mismatch!" << std::endl;
            return;
        }

        m_ProvImgW = w;
        m_ProvImgH = h;
        m_RawProvData.assign(prov, prov + (size_t)w * h * 3);

        std::vector<unsigned char> out((size_t)w * h * 3, 0);

        for (int i = 0; i < w * h; ++i)
        {
            float h01 = height[i] / 255.0f;

            if (h01 < 0.36f)
                continue;

            uint32_t color = PackRGB(
                prov[i * 3],
                prov[i * 3 + 1],
                prov[i * 3 + 2]
            );

            auto it = m_ColorToProvID.find(color);
            if (it == m_ColorToProvID.end())
                continue;

            uint32_t provID = it->second;
            auto own = m_ProvToOwner.find(provID);
            if (own == m_ProvToOwner.end())
                continue;

            glm::vec3 col = m_CountryColors[own->second];

            out[i * 3 + 0] = (unsigned char)(col.r * 255);
            out[i * 3 + 1] = (unsigned char)(col.g * 255);
            out[i * 3 + 2] = (unsigned char)(col.b * 255);
        }

        glGenTextures(1, &m_TexStateMap);
        glBindTexture(GL_TEXTURE_2D, m_TexStateMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, out.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenTextures(1, &m_TexProv);
        glBindTexture(GL_TEXTURE_2D, m_TexProv);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, prov);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        stbi_image_free(prov);
        stbi_image_free(height);
    }

    void Engine::UpdateMousePicking() {
        if (ImGui::GetIO().WantCaptureMouse) return;
        if (glfwGetMouseButton(m_Window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) return;

        double mx, my;
        glfwGetCursorPos(m_Window, &mx, &my);

        int winW, winH;
        glfwGetWindowSize(m_Window, &winW, &winH); 

        float nx = (2.0f * (float)mx) / (float)winW - 1.0f;
        float ny = 1.0f - (2.0f * (float)my) / (float)winH;

        float aspect = (float)winW / (float)winH;
        glm::mat4 proj = m_Camera.GetProj(aspect);
        glm::mat4 view = m_Camera.GetView();
        glm::mat4 invVP = glm::inverse(proj * view);

        glm::vec4 nearPtr = invVP * glm::vec4(nx, ny, -1.0f, 1.0f);
        nearPtr /= nearPtr.w;
        glm::vec4 farPtr = invVP * glm::vec4(nx, ny, 1.0f, 1.0f);
        farPtr /= farPtr.w;

        float t = nearPtr.z / (nearPtr.z - farPtr.z);
        float worldX = nearPtr.x + t * (farPtr.x - nearPtr.x);
        float worldY = nearPtr.y + t * (farPtr.y - nearPtr.y);

        float u = worldX / 5.632f;
        u = u - floor(u); 
        
        float v = worldY / 2.048f;

        if (u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f) {
            int px = int(u * (float)(m_ProvImgW - 1));
            int py = int(v * (float)(m_ProvImgH - 1));
            
            size_t idx = (size_t(py) * m_ProvImgW + px) * 3;
            if (idx + 2 < m_RawProvData.size()) {
                uint8_t r = m_RawProvData[idx];
                uint8_t g = m_RawProvData[idx + 1];
                uint8_t b = m_RawProvData[idx + 2];
                
                m_SelectedRGB = glm::vec3(r/255.f, g/255.f, b/255.f);
                
                uint32_t color = PackRGB(r, g, b);
                if (m_ColorToProvID.count(color)) {
                    m_SelectedProvID = m_ColorToProvID[color];
                    m_SelectedOwner = m_ProvToOwner.count(m_SelectedProvID) ? m_ProvToOwner[m_SelectedProvID] : "None";
                }
            }
        }
    }

    void Engine::InitGraphics() {
        m_Shader = std::make_unique<render::Shader>("gfx/FX/Map.vert", "gfx/FX/Map.frag");
        
        auto loadTex = [](const char* p, GLenum f) {
            int w, h, c;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* d = stbi_load(p, &w, &h, &c, 3);

            uint32_t tid;
            glGenTextures(1, &tid);
            glBindTexture(GL_TEXTURE_2D, tid);

            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGB,
                w, h,
                0,
                GL_RGB,
                GL_UNSIGNED_BYTE,
                d
            );

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            stbi_image_free(d);
            return tid;
        };

        m_TexNorm = loadTex("map/normal.bmp", GL_RGB);
        m_TexHeight = loadTex("map/heightmap.bmp", GL_RGB);
        
        LoadDefinitions("map/definition.csv");
        LoadCountryColors("common/countries/colors.txt");
        LoadLocalizations("localisation/russian", "russian");
        LoadStates("history/state");
        GenerateStateMap("map/provinces.bmp");

        CalculateTextPositions();

        m_TextShader = std::make_unique<render::Shader>("gfx/FX/Text.vert", "gfx/FX/Text.frag");
        m_TextRenderer = std::make_unique<render::TextRenderer>("./font/JetBrainsMono-Medium.ttf", 24); 

        float verts[] = { 
            0, 0, 0,			0, 0, 
            WORLD_WIDTH, 0, 0,	1, 0, 
            WORLD_WIDTH, WORLD_HEIGHT, 0, 1, 1, 
            0, WORLD_HEIGHT, 0, 0, 1 
        };
        uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

        glGenVertexArrays(1, &m_VAO);
        glGenBuffers(1, &m_VBO);
        glGenBuffers(1, &m_EBO);

        glBindVertexArray(m_VAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    void Engine::UpdateUI() {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Global Map Info");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Separator();
        if (m_SelectedProvID != 0) {
            std::string country_tag = m_SelectedOwner;
            std::string displayName = country_tag;
            
            if (m_Localizations.count(country_tag)) {
                displayName = m_Localizations[country_tag];
            } else {
                std::string def_tag = country_tag + "_DEF";
                if (m_Localizations.count(def_tag)) {
                    displayName = m_Localizations[def_tag];
                }
            }
            ImGui::Text("Country Name: %s", displayName.c_str());
            
            ImGui::Text("Province ID: %u", m_SelectedProvID);
            ImGui::Text("Owner Tag: %s", m_SelectedOwner.c_str());
            if (m_CountryColors.count(m_SelectedOwner)) {
                glm::vec3 c = m_CountryColors[m_SelectedOwner];
                ImGui::ColorButton("##OwnerColor", {c.r, c.g, c.b, 1.0f}, 0, ImVec2(40, 40));
            }
        } else {
            ImGui::Text("No province selected.");
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    }

    void Engine::MainLoop() {
        while (!glfwWindowShouldClose(m_Window)) {
            float time = (float)glfwGetTime();
            float dt = time - m_Last;
            m_Last = time;

            glfwPollEvents();
            m_Camera.Update(dt, m_Window, g_Scroll);
            g_Scroll = 0;
            UpdateMousePicking();

            static bool f10_held = false;
            if (glfwGetKey(m_Window, GLFW_KEY_F10) == GLFW_PRESS) {
                if (!f10_held) { m_Shader->Reload(); m_TextShader->Reload(); f10_held = true; }
            } else { f10_held = false; }

            glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            int fbW, fbH;
            glfwGetFramebufferSize(m_Window, &fbW, &fbH);
            glViewport(0, 0, fbW, fbH);
            
            m_Shader->Use();
            m_Shader->SetMat4("p", m_Camera.GetProj((float)fbW / (float)fbH));
            m_Shader->SetMat4("v", m_Camera.GetView());
            m_Shader->SetFloat("time", time);
            m_Shader->SetVec3("u_SelectedColor", m_SelectedRGB); 

            glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, m_TexNorm); m_Shader->SetInt("normMap", 0);
            glActiveTexture(GL_TEXTURE1); glBindTexture(GL_TEXTURE_2D, m_TexHeight); m_Shader->SetInt("heightMap", 1);
            glActiveTexture(GL_TEXTURE2); glBindTexture(GL_TEXTURE_2D, m_TexProv); m_Shader->SetInt("provMap", 2);
            glActiveTexture(GL_TEXTURE3); glBindTexture(GL_TEXTURE_2D, m_TexStateMap); m_Shader->SetInt("stateMap", 3);

            glBindVertexArray(m_VAO);
            for (float offset : {-WORLD_WIDTH, 0.0f, WORLD_WIDTH}) {
                m_Shader->SetMat4("m", glm::translate(glm::mat4(1.0f), {offset, 0.0f, 0.0f}));
                glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
            }

            m_TextShader->SetMat4("p", m_Camera.GetProj((float)fbW / (float)fbH));
            m_TextShader->SetMat4("v", m_Camera.GetView());
            m_TextShader->SetInt("textSampler", 0); 
            
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            for(const auto& textData : m_CountryTexts) {
                m_TextRenderer->RenderText(
                    *m_TextShader, 
                    textData.displayName, 
                    textData.worldPos, 
                    textData.size, 
                    {1.0f, 1.0f, 1.0f}
                );
            }
            glDisable(GL_BLEND);
            
            UpdateUI();
            glfwSwapBuffers(m_Window);
        }
    }

    void Engine::Run() {
        InitWindow();
        InitImGui();
        InitGraphics();
        MainLoop();

        glDeleteVertexArrays(1, &m_VAO);
        glDeleteBuffers(1, &m_VBO);
        glDeleteBuffers(1, &m_EBO);
        glDeleteTextures(1, &m_TexNorm);
        glDeleteTextures(1, &m_TexHeight);
        glDeleteTextures(1, &m_TexProv);
        glDeleteTextures(1, &m_TexStateMap);

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_Window);
        glfwTerminate();
    }
}