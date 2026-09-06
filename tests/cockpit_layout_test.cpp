#include "src/cockpit_layout.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main() {
    for (const auto width : {320.0f,390.0f,768.0f,1024.0f,1440.0f,2560.0f}) {
        for (const auto height : {240.0f,390.0f,720.0f,900.0f,1440.0f}) {
            const auto wheel = cockpit_layout::FitWheel(width,height);
            assert(wheel.x - wheel.radius >= 0.0f);
            assert(wheel.x + wheel.radius <= width);
            assert(wheel.y - wheel.radius >= 0.0f);
            assert(wheel.y + wheel.radius <= height - 13.0f);
        }
    }
    assert(cockpit_layout::WheelAngle(0.0f) == 0.0f);
    assert(std::fabs(cockpit_layout::WheelAngle(0.66f) - 360.0f) < 0.01f);
    assert(std::fabs(cockpit_layout::WheelAngle(-0.66f) + 360.0f) < 0.01f);
    std::cout << "Complete steering rim fits all 30 viewports; 720-degree span passes\n";
}
