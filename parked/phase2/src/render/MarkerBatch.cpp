#include "render/MarkerBatch.h"

#include <glad/gl.h>

namespace globe {

MarkerBatch::MarkerBatch() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

MarkerBatch::~MarkerBatch() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
}

void MarkerBatch::setPositions(const std::vector<glm::vec3>& points) {
    count_ = static_cast<int>(points.size());
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(points.size() * sizeof(glm::vec3)),
                 points.data(), GL_DYNAMIC_DRAW);
}

void MarkerBatch::draw() const {
    if (count_ == 0) {
        return;
    }
    glBindVertexArray(vao_);
    glDrawArrays(GL_POINTS, 0, count_);
    glBindVertexArray(0);
}

} // namespace globe
