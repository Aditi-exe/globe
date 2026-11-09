#pragma once

namespace globe {

// A UV sphere (radius 1, centered at origin) with positions, normals, and
// texture coordinates. Owns its GL buffers.
class EarthSphere {
public:
    explicit EarthSphere(int stacks = 64, int slices = 128);
    ~EarthSphere();

    EarthSphere(const EarthSphere&) = delete;
    EarthSphere& operator=(const EarthSphere&) = delete;

    void draw() const;

private:
    unsigned int vao_ = 0;
    unsigned int vbo_ = 0;
    unsigned int ebo_ = 0;
    int indexCount_ = 0;
};

} // namespace globe
