#include "src/exam_rules.h"
#include "src/course_data.h"

#include <cassert>
#include <cmath>

using namespace dobong_exam;

int main() {
    static_assert(kInitialScore == 100);
    static_assert(kPassScore == 80);
    static_assert(Points(Penalty::RoadBoundary) == 15);
    static_assert(ApplyPenalty(100, Penalty::RoadBoundary) == 85);
    static_assert(ApplyPenalty(3, Penalty::TurnSignal) == 0);
    static_assert(IsPassingScore(80));
    static_assert(!IsPassingScore(79));
    static_assert(!IsNormalSpeedViolation(20.0f));
    static_assert(IsNormalSpeedViolation(20.01f));
    static_assert(!IsHillRollbackPenalty(0.49f));
    static_assert(IsHillRollbackPenalty(0.5f));
    static_assert(IsHillRollbackFailure(1.0f));
    static_assert(!IsEmergencyStopLate(2.0f));
    static_assert(IsEmergencyStopLate(2.01f));
    static_assert(!IsHazardLate(3.0f));
    static_assert(IsHazardLate(3.01f));
    static_assert(IsParkingOvertime(120.01f));
    static_assert(IsStartOvertime(30.01f));
    static_assert(!IsBasicControlOvertime(5.0f));
    static_assert(IsBasicControlOvertime(5.01f));
    static_assert(!IsHillCourseOvertime(30.0f));
    static_assert(IsHillCourseOvertime(30.01f));
    static_assert(FirstOvertimePenaltyAt() == 525.0f);

    assert(ApplyPenalty(100, Penalty::EmergencyStop) == 90);
    static_assert(driving_test_data::kOfficialCenters.size() == 27);
    static_assert(driving_test_data::dobong_v1::kRoads.size() == 11);
    static_assert(driving_test_data::dobong_v1::kRoute.size() == 16);
    int availableCenters = 0;
    for (const auto& center : driving_test_data::kOfficialCenters) {
        if (center.available) ++availableCenters;
    }
    assert(availableCenters == 1);

    float routeLength = 0.0f;
    const auto& route = driving_test_data::dobong_v1::kRoute;
    for (std::size_t index = 1; index < route.size(); ++index) {
        const float dx = route[index].x - route[index - 1].x;
        const float dy = route[index].y - route[index - 1].y;
        routeLength += std::sqrt(dx * dx + dy * dy);
    }
    assert(routeLength >= 300.0f);
    return 0;
}
