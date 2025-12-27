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
#include <cmath>

namespace aegis {

    namespace fs = std::filesystem;

    static float g_Scroll = 0;
    static const float WORLD_WIDTH = 5.632f;
    static const float WORLD_HEIGHT = 2.048f;

    static std::unordered_map<std::string, std::string> g_CountryPolicies;

    void Engine::InitWindow() {
        if (!glfwInit()) return;
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        m_Window = glfwCreateWindow(1920, 1080, "Aegis", NULL, NULL);

        if (!m_Window) return;

        glfwMakeContextCurrent(m_Window);
        glfwSetScrollCallback(m_Window, [](GLFWwindow*, double, double yoffset) { g_Scroll = (float)yoffset; });
        glewExperimental = GL_TRUE;
        glewInit();
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void Engine::InitImGui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();

        // ВАЖНО: Загружаем шрифт с диапазоном Кириллицы для ImGui
        if (fs::exists("./font/JetBrainsMono-Medium.ttf")) {
            const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();
            // Размер 20.0f для интерфейса
            io.Fonts->AddFontFromFileTTF("./font/JetBrainsMono-Medium.ttf", 20.0f, NULL, ranges);
        } else {
            // Если шрифта нет, ImGui загрузит стандартный, который НЕ умеет в русский
            std::cout << "[ImGui] Warning: Font not found, Cyrillic may not work!" << std::endl;
        }

        ImGui_ImplGlfw_InitForOpenGL(m_Window, true);
        ImGui_ImplOpenGL3_Init("#version 410");
    }

    void Engine::LoadDefinitions(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::vector<std::string> v;
            std::string t;
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

    void LoadCountryHistory(const std::string& folder) {
        if (!fs::exists(folder)) return;
        g_CountryPolicies.clear();

        std::regex partyRe("ruling_party\\s*=\\s*([a-zA-Z0-9_]+)");

        for (const auto& entry : fs::directory_iterator(folder)) {
            if (entry.path().extension() != ".txt") continue;

            std::string filename = entry.path().filename().string();
            if (filename.length() < 3) continue;
            std::string tag = filename.substr(0, 3);
            for (auto & c: tag) c = toupper(c);

            std::ifstream f(entry.path());
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            content = std::regex_replace(content, std::regex("#.*"), "");

            std::smatch m;
            if (std::regex_search(content, m, partyRe)) {
                g_CountryPolicies[tag] = m[1].str();
            }
        }
    }

    void Engine::LoadLocalizations(const std::string& rootFolder, const std::string& lang) {
        m_Localizations.clear();
        std::string path = rootFolder + "/" + lang;
        if (!fs::exists(path)) return;

        std::regex re("^\\s*([A-Za-z0-9_\\.\\-]+)\\s*:\\d*\\s*\"([^\"]*)\"");
        std::string langHeader = "l_" + lang + ":";

        for (const auto& entry : fs::directory_iterator(path)) {
            if (entry.path().extension() != ".yml") continue;

            std::ifstream file(entry.path());
            std::string line;
            bool correctLangSection = false;

            while (std::getline(file, line)) {
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF) line.erase(0, 3);

                if (line.find(langHeader) != std::string::npos) {
                    correctLangSection = true;
                    continue;
                }
                if (!correctLangSection || line.empty() || line[0] == '#') continue;

                std::smatch match;
                if (std::regex_search(line, match, re)) {
                    m_Localizations[match[1].str()] = match[2].str();
                }
            }
        }
    }

    void Engine::CalculateTextPositions() {
        m_CountryTexts.clear();
        
        struct TagSum {
            double sumX = 0;
            double sumY = 0;
            uint64_t count = 0;
            std::string tag;
        };

        std::unordered_map<std::string, TagSum> acc;

        for (int y = 0; y < m_ProvImgH; ++y) {
            for (int x = 0; x < m_ProvImgW; ++x) {
                size_t idx = (size_t(y) * m_ProvImgW + x) * 3;
                if (idx + 2 >= m_RawProvData.size()) continue;

                uint8_t r = m_RawProvData[idx];
                uint8_t g = m_RawProvData[idx+1];
                uint8_t b = m_RawProvData[idx+2];
                uint32_t color = PackRGB(r, g, b);

                if (m_ColorToProvID.count(color)) {
                    uint32_t provId = m_ColorToProvID[color];
                    if (m_ProvToOwner.count(provId)) {
                        std::string tag = m_ProvToOwner[provId];
                        acc[tag].tag = tag;
                        acc[tag].sumX += x;
                        acc[tag].sumY += y;
                        acc[tag].count++;
                    }
                }
            }
        }

        for (auto& [tag, data] : acc) {
            if (data.count < 100) continue; 

            float avgX = (float)(data.sumX / data.count);
            float avgY = (float)(data.sumY / data.count);

            float u = avgX / (float)m_ProvImgW;
            float v = avgY / (float)m_ProvImgH;

            float worldX = u * WORLD_WIDTH;
            float worldY = v * WORLD_HEIGHT;

            CountryTextData txt;
            txt.tag = tag;
            
            std::string finalName = tag; 
            std::string keyToCheck = tag;
            if (g_CountryPolicies.count(tag)) {
                keyToCheck = tag + "_" + g_CountryPolicies[tag];
            }

            if (m_Localizations.count(keyToCheck)) {
                finalName = m_Localizations[keyToCheck];
            } else if (m_Localizations.count(tag)) {
                finalName = m_Localizations[tag];
            }
            txt.displayName = finalName;

            txt.worldPos = glm::vec3(worldX, worldY, 0.0f); 
            
            float scaleFactor = std::sqrt((float)data.count);
            
            txt.size = 0.00001f * scaleFactor; 
            if (txt.size < 0.0008f) txt.size = 0.0008f; 
            if (txt.size > 0.0030f) txt.size = 0.0030f; 

            m_CountryTexts.push_back(txt);
        }
    }

    void Engine::GenerateStateMap(const std::string& provMapPath) {
        int w, h, c;
        stbi_set_flip_vertically_on_load(true);
        unsigned char* d = stbi_load(provMapPath.c_str(), &w, &h, &c, 3);
        if (!d) return;
        m_ProvImgW = w; m_ProvImgH = h;
        m_RawProvData.assign(d, d + (size_t(w) * h * 3)); 

        std::vector<unsigned char> out(size_t(w) * h * 3, 0);
        for (int i = 0; i < w * h; ++i) {
            uint32_t color = PackRGB(d[i * 3], d[i * 3 + 1], d[i * 3 + 2]);
            if (m_ColorToProvID.count(color)) {
                uint32_t id = m_ColorToProvID[color];
                if (m_ProvToOwner.count(id)) {
                    glm::vec3 col = m_CountryColors[m_ProvToOwner[id]];
                    out[i * 3] = (unsigned char)(col.r * 255);
                    out[i * 3 + 1] = (unsigned char)(col.g * 255);
                    out[i * 3 + 2] = (unsigned char)(col.b * 255);
                }
            }
        }

        glGenTextures(1, &m_TexStateMap);
        glBindTexture(GL_TEXTURE_2D, m_TexStateMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, out.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        glGenTextures(1, &m_TexProv);
        glBindTexture(GL_TEXTURE_2D, m_TexProv);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, d);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        stbi_image_free(d);
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

        float u = worldX / WORLD_WIDTH;
        float v = worldY / WORLD_HEIGHT;
        u = u - floor(u); 

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
        m_TextShader = std::make_unique<render::Shader>("gfx/FX/Text.vert", "gfx/FX/Text.frag");
        
        if (!fs::exists("./font/JetBrainsMono-Medium.ttf")) {
            std::cerr << "FONT NOT FOUND!" << std::endl;
        }
        m_TextRenderer = std::make_unique<render::TextRenderer>("./font/JetBrainsMono-Medium.ttf", 64);

        auto loadTex = [](const char* p, GLenum f) {
            int w, h, c;
            stbi_set_flip_vertically_on_load(true);
            unsigned char* d = stbi_load(p, &w, &h, &c, 0);
            uint32_t tid;
            glGenTextures(1, &tid);
            glBindTexture(GL_TEXTURE_2D, tid);
            glTexImage2D(GL_TEXTURE_2D, 0, f, w, h, 0, f, GL_UNSIGNED_BYTE, d);
            glGenerateMipmap(GL_TEXTURE_2D);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            if (d) stbi_image_free(d);
            return tid;
        };

        m_TexNorm = loadTex("map/normal.bmp", GL_RGB);
        m_TexHeight = loadTex("map/heightmap.bmp", GL_RGB);

        LoadDefinitions("map/definition.csv");
        LoadCountryColors("common/countries/colors.txt");
        LoadStates("history/state");
        LoadCountryHistory("history/countries");
        LoadLocalizations("localisation", "russian"); 
        
        GenerateStateMap("map/provinces.bmp");
        CalculateTextPositions();

        float verts[] = { 
            0, 0, 0,            0, 0, 
            WORLD_WIDTH, 0, 0,  1, 0, 
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
            ImGui::Text("Province ID: %u", m_SelectedProvID);
            ImGui::Text("Tag: %s", m_SelectedOwner.c_str());
            
            std::string finalName = m_SelectedOwner; 
            std::string keyToCheck = m_SelectedOwner;

            if (g_CountryPolicies.count(m_SelectedOwner)) {
                std::string ideology = g_CountryPolicies[m_SelectedOwner];
                ImGui::Text("Party: %s", ideology.c_str()); 
                keyToCheck = m_SelectedOwner + "_" + ideology; 
            } else {
                ImGui::Text("Party: Unknown");
            }

            if (m_Localizations.count(keyToCheck)) {
                finalName = m_Localizations[keyToCheck];
            } 
            else if (m_Localizations.count(m_SelectedOwner)) {
                finalName = m_Localizations[m_SelectedOwner];
            }
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Name: %s", finalName.c_str());

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
            glm::mat4 proj = m_Camera.GetProj((float)fbW / (float)fbH);
            glm::mat4 view = m_Camera.GetView();
            m_Shader->SetMat4("p", proj);
            m_Shader->SetMat4("v", view);
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

            glDisable(GL_DEPTH_TEST);
            
            m_TextShader->Use();
            m_TextShader->SetMat4("projection", proj);
            m_TextShader->SetMat4("view", view);
            
            for (float offset : {-WORLD_WIDTH, 0.0f, WORLD_WIDTH}) {
                m_TextShader->SetMat4("model", glm::translate(glm::mat4(1.0f), {offset, 0.0f, 0.0f}));
                for (const auto& txt : m_CountryTexts) {
                    float estW = txt.displayName.length() * txt.size * 30.0f; 
                    glm::vec3 pos = txt.worldPos;
                    pos.x -= estW * 0.5f; 
                    
                    m_TextRenderer->RenderText(*m_TextShader, txt.displayName, pos, txt.size, glm::vec3(1.0f));
                }
            }
            glEnable(GL_DEPTH_TEST);

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