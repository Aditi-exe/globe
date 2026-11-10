#include "ui/UiContext.h"

#include "render/Shader.h"
#include "ui/TextRenderer.h"

#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>

namespace globe {

UiContext::UiContext(const std::string& assetsDir) {
    rectShader_ = std::make_unique<Shader>(assetsDir + "/shaders/ui_rect.vert",
                                           assetsDir + "/shaders/ui_rect.frag");
    text_ = std::make_unique<TextRenderer>(assetsDir);

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

UiContext::~UiContext() {
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (vao_) glDeleteVertexArrays(1, &vao_);
}

void UiContext::begin(int width, int height) {
    // Pixel space: (0,0) top-left, y increases downward.
    screenH_ = height;
    proj_ = glm::ortho(0.0f, static_cast<float>(width),
                       static_cast<float>(height), 0.0f);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void UiContext::end() {
    glDisable(GL_SCISSOR_TEST); // in case a clip was left open
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void UiContext::pushClip(float x, float y, float w, float h) {
    // glScissor origin is bottom-left; our pixel space is top-left, so flip y.
    const int gy = screenH_ - static_cast<int>(y + h);
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<int>(x), gy, static_cast<int>(w), static_cast<int>(h));
}

void UiContext::popClip() {
    glDisable(GL_SCISSOR_TEST);
}

void UiContext::drawRect(float x, float y, float w, float h, const glm::vec4& color) {
    const float v[12] = {
        x,     y,
        x + w, y,
        x + w, y + h,
        x,     y,
        x + w, y + h,
        x,     y + h,
    };
    rectShader_->use();
    rectShader_->setMat4("uProj", proj_);
    rectShader_->setVec4("uColor", color);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void UiContext::drawText(float x, float y, const std::string& text, const glm::vec4& color) {
    text_->draw(proj_, x, y, text, color);
}

float UiContext::textWidth(const std::string& text) const { return text_->measure(text); }
float UiContext::lineHeight() const { return text_->lineHeight(); }
bool UiContext::textReady() const { return text_->ready(); }

} // namespace globe
