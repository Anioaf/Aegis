#version 410 core
out vec4 FragColor;
in vec2 TexCoords;
in vec3 FragPos;

uniform sampler2D normMap, heightMap, provMap, stateMap;
uniform vec3 u_SelectedColor; // ЦВЕТ ДЛЯ ПОДСВЕТКИ
uniform float time;

const float SEA_LEVEL = 0.36;

void main() {
    float h = texture(heightMap, TexCoords).r;
    vec3 n = normalize(texture(normMap, TexCoords).rgb * 2.0 - 1.0);
    vec3 provColor = texture(provMap, TexCoords).rgb;
    vec3 stateColor = texture(stateMap, TexCoords).rgb;

    float landMask = step(SEA_LEVEL, h);
    float light = max(dot(n, normalize(vec3(-0.5, 0.5, 0.8))), 0.0) * 0.7 + 0.35;

    vec3 finalColor;
    if (landMask > 0.5) {
        vec3 base = (length(stateColor) > 0.01) ? stateColor : vec3(0.3, 0.35, 0.2);
        finalColor = base * light;

        // ПОДСВЕТКА ВЫБРАННОЙ ПРОВИНЦИИ (делаем чуть ярче)
        if (distance(provColor, u_SelectedColor) < 0.001) {
            finalColor += vec3(0.15, 0.15, 0.0);
        }

        finalColor *= (1.0 - length(n.xy) * 0.3);
    } else {
        finalColor = vec3(0.05, 0.1, 0.2) * light;
    }


    FragColor = vec4(finalColor, 1.0);
}
