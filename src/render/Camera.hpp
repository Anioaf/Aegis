#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace aegis::render {

    class Camera {
    public:
        glm::vec3 position = { 2.816f, 1.024f, 1.0f };
        float mapWidth = 5.632f, mapHeight = 2.048f;
        void Update(float dt, GLFWwindow* w, float s);
        glm::mat4 GetView();
        glm::mat4 GetProj(float a);
    private:
        float m_MinZ = 0.1f, m_MaxZ = 2.5f;
    };
}