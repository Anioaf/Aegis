#version 410 core
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D textSampler;
uniform vec3 textColor;

void main() {
    float alpha = texture(textSampler, TexCoords).r;
    if (alpha < 0.1) discard;
    FragColor = vec4(textColor, alpha);
}