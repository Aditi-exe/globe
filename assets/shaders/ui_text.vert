#version 330 core

layout(location = 0) in vec2 aPos; // pixel coordinates
layout(location = 1) in vec2 aUv;  // glyph-atlas texture coords

uniform mat4 uProj;

out vec2 vUv;

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vUv = aUv;
}
