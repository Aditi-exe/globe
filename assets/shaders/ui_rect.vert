#version 330 core

layout(location = 0) in vec2 aPos; // pixel coordinates

uniform mat4 uProj; // orthographic: pixels -> clip space

void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
