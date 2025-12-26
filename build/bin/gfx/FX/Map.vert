#version 410 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 FragPos;

uniform mat4 m; // Model
uniform mat4 v; // View
uniform mat4 p; // Projection

void main() {
    FragPos = vec3(m * vec4(aPos, 1.0));
    TexCoords = aTexCoords;
    gl_Position = p * v * vec4(FragPos, 1.0);
}