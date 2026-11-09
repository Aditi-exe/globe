#pragma once

#include <glm/glm.hpp>

namespace globe {

// Orbit (trackball-style) camera looking at the origin, where the Earth sits
// with radius 1. Drag to orbit, scroll to zoom.
class Camera {
public:
    void orbit(float dYaw, float dPitch); // radians
    void zoom(float factor);              // nudges the target distance (clamped)
    void update(float dt);                // eases distance toward its target

    [[nodiscard]] glm::vec3 position() const;
    [[nodiscard]] glm::mat4 view() const;
    [[nodiscard]] glm::mat4 projection(float aspect) const;

private:
    float yaw_ = 0.0f;            // rotation around world Y
    float pitch_ = 0.0f;         // elevation, clamped away from the poles
    float distance_ = 3.0f;      // current distance from origin (Earth radius == 1)
    float targetDistance_ = 3.0f; // eased toward by update()
};

} // namespace globe
