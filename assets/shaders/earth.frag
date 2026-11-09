#version 330 core

in vec2 vUV;
in vec3 vNormal;

uniform sampler2D uTexture;

out vec4 FragColor;

void main() {
    vec3 n = normalize(vNormal);
    // Simple fixed directional "sun" so the globe reads as 3D in Phase 1.
    vec3 lightDir = normalize(vec3(1.0, 0.3, 0.6));
    float diff = max(dot(n, lightDir), 0.0);
    float ambient = 0.25;
    vec3 base = texture(uTexture, vUV).rgb;
    FragColor = vec4(base * (ambient + diff * 0.9), 1.0);
}
