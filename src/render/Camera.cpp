#include "render/Camera.h"

#include <algorithm>
#include <cmath>

#include <glm/gtc/matrix_transform.hpp>

namespace globe {

namespace {
constexpr float kMinDistance = 1.2f; // just above the surface
constexpr float kMaxDistance = 20.0f;
constexpr float kPitchLimit = 1.55f; // ~89 degrees
} // namespace

void Camera::orbit(float dYaw, float dPitch) {
    yaw_ += dYaw;
    pitch_ = std::clamp(pitch_ + dPitch, -kPitchLimit, kPitchLimit);
}

void Camera::zoom(float factor) {
    targetDistance_ = std::clamp(targetDistance_ * factor, kMinDistance, kMaxDistance);
}

void Camera::update(float dt) {
    // Frame-rate-independent exponential easing toward the target distance.
    float t = 1.0f - std::exp(-dt * 12.0f);
    distance_ += (targetDistance_ - distance_) * t;
}

glm::vec3 Camera::position() const {
    return {
        distance_ * std::cos(pitch_) * std::sin(yaw_),
        distance_ * std::sin(pitch_),
        distance_ * std::cos(pitch_) * std::cos(yaw_),
    };
}

glm::mat4 Camera::view() const {
    return glm::lookAt(position(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

glm::mat4 Camera::projection(float aspect) const {
    return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
}

} // namespace globe
