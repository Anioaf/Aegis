#version 410 core
layout (location = 0) in vec4 vertex; 

uniform mat4 p; 
uniform mat4 v;

out vec2 TexCoords;

void main() {
    gl_Position = p * v * vec4(vertex.x, vertex.y, 0.0, 1.0); 
    TexCoords = vertex.zw; // vertex.z = u, vertex.w = v
}