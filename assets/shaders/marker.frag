#version 330 core

out vec4 FragColor;

uniform vec3 uColor;

void main() {
    // Render each point as a round dot instead of a square.
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) {
        discard;
    }
    FragColor = vec4(uColor, 1.0);
}
