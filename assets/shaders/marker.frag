#version 330 core

out vec4 FragColor;

void main() {
    // Render each point as a soft round dot instead of a square.
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d = dot(c, c);
    if (d > 1.0) {
        discard;
    }
    FragColor = vec4(1.0, 0.85, 0.2, 1.0); // amber marker
}
