#include "render/EarthSphere.h"

#include <cmath>
#include <vector>

#include <glad/gl.h>

namespace globe {

namespace {
constexpr float kPi = 3.14159265358979323846f;
} // namespace

EarthSphere::EarthSphere(int stacks, int slices) {
    std::vector<float> verts;              // interleaved: pos(3) normal(3) uv(2)
    verts.reserve(static_cast<size_t>((stacks + 1) * (slices + 1) * 8));

    for (int i = 0; i <= stacks; ++i) {
        float v = static_cast<float>(i) / static_cast<float>(stacks);
        float phi = v * kPi; // 0 at north pole .. pi at south pole
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (int j = 0; j <= slices; ++j) {
            float u = static_cast<float>(j) / static_cast<float>(slices);
            float theta = u * 2.0f * kPi;
            float x = sinPhi * std::cos(theta);
            float y = cosPhi;
            float z = sinPhi * std::sin(theta);

            // North pole (i=0, v=0) samples the texture's top row (+90 lat);
            // u is mirrored so east/west read correctly from outside the sphere.
            verts.insert(verts.end(), {x, y, z, x, y, z, 1.0f - u, v});
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>(stacks * slices * 6));
    int stride = slices + 1;
    for (int i = 0; i < stacks; ++i) {
        for (int j = 0; j < slices; ++j) {
            unsigned int a = static_cast<unsigned int>(i * stride + j);
            unsigned int b = static_cast<unsigned int>((i + 1) * stride + j);
            indices.insert(indices.end(), {a, b, a + 1, a + 1, b, b + 1});
        }
    }
    indexCount_ = static_cast<int>(indices.size());

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    const GLsizei stepBytes = 8 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stepBytes, reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stepBytes, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stepBytes, reinterpret_cast<void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

EarthSphere::~EarthSphere() {
    glDeleteVertexArrays(1, &vao_);
    glDeleteBuffers(1, &vbo_);
    glDeleteBuffers(1, &ebo_);
}

void EarthSphere::draw() const {
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace globe
