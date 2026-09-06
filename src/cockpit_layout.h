#pragma once
#include <algorithm>

namespace cockpit_layout {
struct Wheel { float x; float y; float radius; };
inline Wheel FitWheel(float width, float height) {
    const float radius = std::min({width * 0.24f, height * 0.18f, 190.0f});
    const float margin = std::max(14.0f, height * 0.025f);
    return {std::max(radius + margin, width * 0.29f), height - radius - margin, radius};
}
inline float WheelAngle(float frontWheelRadians) {
    return std::clamp(frontWheelRadians / 0.66f, -1.0f, 1.0f) * 360.0f;
}
}
