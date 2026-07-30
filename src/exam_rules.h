#pragma once

#include <algorithm>

namespace dobong_exam {

constexpr int kInitialScore = 100;
constexpr int kPassScore = 80;
constexpr float kNormalSpeedLimitKph = 20.0f;
constexpr float kHillRollbackLimitMeters = 0.5f;
constexpr float kEmergencyStopLimitSeconds = 2.0f;
constexpr float kHazardLimitSeconds = 3.0f;
constexpr float kParkingLimitSeconds = 120.0f;
constexpr float kStartLimitSeconds = 30.0f;
constexpr float kBasicControlLimitSeconds = 5.0f;
constexpr float kHillCourseLimitSeconds = 30.0f;
constexpr float kDesignatedTimeSeconds = 520.0f;
constexpr float kOvertimePenaltyIntervalSeconds = 5.0f;

enum class Penalty {
    BasicControl = 5,
    RoadBoundary = 15,
    EmergencyStop = 10,
    HillStop = 10,
    TurnSignal = 5,
    Acceleration = 10,
    SignalIntersection = 5,
    PerpendicularParking = 10,
    EngineState = 5,
    TimeOrSpeed = 3,
};

constexpr int Points(Penalty penalty) {
    return static_cast<int>(penalty);
}

constexpr int ApplyPenalty(int score, Penalty penalty) {
    return std::max(0, score - Points(penalty));
}

constexpr bool IsPassingScore(int score) {
    return score >= kPassScore;
}

constexpr bool IsNormalSpeedViolation(float speedKph) {
    return speedKph > kNormalSpeedLimitKph;
}

constexpr bool IsHillRollbackFailure(float rollbackMeters) {
    return rollbackMeters >= 1.0f;
}

constexpr bool IsHillRollbackPenalty(float rollbackMeters) {
    return rollbackMeters >= kHillRollbackLimitMeters;
}

constexpr bool IsEmergencyStopLate(float elapsedSeconds) {
    return elapsedSeconds > kEmergencyStopLimitSeconds;
}

constexpr bool IsHazardLate(float stoppedElapsedSeconds) {
    return stoppedElapsedSeconds > kHazardLimitSeconds;
}

constexpr bool IsParkingOvertime(float elapsedSeconds) {
    return elapsedSeconds > kParkingLimitSeconds;
}

constexpr bool IsStartOvertime(float elapsedSeconds) {
    return elapsedSeconds > kStartLimitSeconds;
}

constexpr bool IsBasicControlOvertime(float elapsedSeconds) {
    return elapsedSeconds > kBasicControlLimitSeconds;
}

constexpr bool IsHillCourseOvertime(float elapsedSeconds) {
    return elapsedSeconds > kHillCourseLimitSeconds;
}

constexpr float FirstOvertimePenaltyAt() {
    return kDesignatedTimeSeconds + kOvertimePenaltyIntervalSeconds;
}

}  // namespace dobong_exam
