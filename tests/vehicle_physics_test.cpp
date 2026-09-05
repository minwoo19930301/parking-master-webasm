#include "src/vehicle_physics.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    for (int hz : {20, 30, 60, 120}) {
        const float dt = 1.0f / hz;
        float speed = 0, distance = 0;
        while (speed * 3.6f < 20.0f && distance < 40.0f) {
            speed = driving_physics::UpdateSpeed(speed, dt, false, 1, 0);
            distance += speed * dt;
        }
        assert(speed * 3.6f >= 20.0f && distance < 5.0f);
        for (int i = 0; i < 10 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, false, 1, 0);
        assert(std::fabs(speed - 7.2f) < 0.001f);
        for (int i = 0; i < 10 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, false, 0, 0);
        assert(std::fabs(speed - 1.05f) < 0.001f);
        for (int i = 0; i < 2 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, false, 1, 1);
        assert(speed == 0.0f);  // Brake overrides throttle and never reverses.
        for (int i = 0; i < 10 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, true, 1, 0);
        assert(std::fabs(speed + 3.2f) < 0.001f);
        for (int i = 0; i < 2 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, true, 0, 1);
        assert(speed == 0.0f);
        for (int i = 0; i < 10 * hz; ++i) speed = driving_physics::UpdateSpeed(speed, dt, true, 0, 0);
        assert(std::fabs(speed + 0.85f) < 0.001f);
    }
    std::cout << "Vehicle physics passed at 20/30/60/120 Hz\n";
}
