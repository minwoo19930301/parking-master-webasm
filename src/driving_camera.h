#pragma once
#include <algorithm>
#include <cmath>

namespace driving_camera {
struct Point3 { float x,y,z; };
struct Pose { Point3 position,target; float fovy; };
inline float RenderYawDegrees(float heading){return -heading*180.f/3.14159265358979323846f;}
inline Pose Chase(float x,float z,float heading,float ground,float speedKph,float aspect) {
    const float fx=std::cos(heading),fz=std::sin(heading);
    // Widen portrait framing by moving back; full vehicle stays above the wheel.
    const float portrait=std::clamp(1.f/std::max(.3f,aspect)-1.f,0.f,1.8f);
    const float distance=8.5f+portrait*3.f+std::clamp(speedKph/60.f,0.f,1.f)*1.5f;
    return {{x-fx*distance,ground+4.8f+portrait,z-fz*distance},
            {x+fx*2.f,ground+1.1f,z+fz*2.f},60.f};
}
inline float FollowBlend(float dt) { return 1.f-std::exp(-std::max(0.f,dt)*8.f); }
} // namespace driving_camera
