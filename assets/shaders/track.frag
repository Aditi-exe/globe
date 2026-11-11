#version 330 core

in float vAlpha;

out vec4 FragColor;

uniform vec4 uColor;

void main() {
    FragColor = vec4(uColor.rgb, uColor.a * vAlpha);
}
