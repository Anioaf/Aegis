#include "Camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace aegis::render {
void Camera::Update(float dt, GLFWwindow* w, float s) {
    float spd = 2.0f * (position.z / m_MaxZ);
    if(glfwGetKey(w, GLFW_KEY_UP)) position.y += spd*dt;
    if(glfwGetKey(w, GLFW_KEY_DOWN)) position.y -= spd*dt;
    if(glfwGetKey(w, GLFW_KEY_LEFT)) position.x -= spd*dt;
    if(glfwGetKey(w, GLFW_KEY_RIGHT)) position.x += spd*dt;
    position.z -= s * 0.1f * position.z;
    position.z = std::clamp(position.z, m_MinZ, m_MaxZ);
    position.y = std::clamp(position.y, 0.0f, mapHeight);
    if(position.x > mapWidth) position.x -= mapWidth; else if(position.x < 0) position.x += mapWidth;
}
glm::mat4 Camera::GetView() { return glm::lookAt(position, {position.x, position.y, 0}, {0,1,0}); }
glm::mat4 Camera::GetProj(float a) { return glm::perspective(glm::radians(45.f), a, 0.01f, 100.f); }
}