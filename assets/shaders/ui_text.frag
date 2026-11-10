#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uAtlas; // single-channel (R8) coverage
uniform vec4 uColor;

void main() {
    float coverage = texture(uAtlas, vUv).r;
    FragColor = vec4(uColor.rgb, uColor.a * coverage);
}
