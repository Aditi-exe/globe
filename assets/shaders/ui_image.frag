#version 330 core

in vec2 vUv;
out vec4 FragColor;

uniform sampler2D uTex;
uniform vec4 uTint; // multiplied over the sampled texel (rgb tint + alpha)

void main() {
    FragColor = texture(uTex, vUv) * uTint;
}
