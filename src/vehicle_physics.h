#pragma once

#include <algorithm>
#include <cmath>

namespace driving_physics {

// Longitudinal motion on level ground. Hill forces are applied by the course.
inline float UpdateSpeed(float speed, float dt, bool reverse, float throttle, float brake) {
    if (!(dt > 0.0f)) return speed;
    if (brake > 0.0f) {
        const float delta = 8.8f * std::clamp(brake, 0.0f, 1.0f) * dt;
        speed = std::copysign(std::max(0.0f, std::fabs(speed) - delta), speed);
    } else if (throttle > 0.0f) {
        speed += (reverse ? -1.0f : 1.0f) * 4.1f * std::clamp(throttle, 0.0f, 1.0f) * dt;
    } else {
        const float creep = reverse ? -0.85f : 1.05f;
        speed = creep + (speed - creep) * std::exp(-1.25f * dt);
    }
    return std::clamp(speed, -3.2f, 7.2f);
}

}  // namespace driving_physics
