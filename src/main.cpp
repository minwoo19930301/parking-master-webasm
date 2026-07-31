#include "exam_rules.h"
#include "course_data.h"

#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

EM_JS(float, WebSteerInput, (), {
    const input = window.__examInput || {};
    return typeof input.steerValue === "number" ? input.steerValue : 0;
});

EM_JS(float, WebThrottleInput, (), {
    return window.__examInput?.throttle ? 1 : 0;
});

EM_JS(float, WebBrakeInput, (), {
    return window.__examInput?.brake ? 1 : 0;
});

EM_JS(int, WebConsumePressed, (const char* key), {
    const input = window.__examInput || {};
    const name = UTF8ToString(key);
    const pressed = input[name] ? 1 : 0;
    input[name] = false;
    return pressed;
});

EM_JS(void, WebUpdateExam,
      (const char* phaseTitle,
       const char* instruction,
       const char* status,
       const char* eventText,
       int score,
       int step,
       int stepTotal,
       float elapsed,
       float speedKph,
       const char* gear,
       int phaseCode,
       int seatbelt,
       int ignition,
       int parkingBrake,
       int headlights,
       int wiper,
       int leftSignal,
       int rightSignal,
       int hazard,
       int trafficLight,
       int emergency,
       int finished,
       int passed),
      {
          if (!window.__examUpdate) return;
          window.__examUpdate({
              phaseTitle: UTF8ToString(phaseTitle),
              instruction: UTF8ToString(instruction),
              status: UTF8ToString(status),
              eventText: UTF8ToString(eventText),
              score,
              step,
              stepTotal,
              elapsed,
              speedKph,
              gear: UTF8ToString(gear),
              phaseCode,
              seatbelt: Boolean(seatbelt),
              ignition: Boolean(ignition),
              parkingBrake: Boolean(parkingBrake),
              headlights,
              wiper: Boolean(wiper),
              leftSignal: Boolean(leftSignal),
              rightSignal: Boolean(rightSignal),
              hazard: Boolean(hazard),
              trafficLight,
              emergency: Boolean(emergency),
              finished: Boolean(finished),
              passed: Boolean(passed),
          });
      });

EM_JS(void, WebSpeak, (const char* message), {
    window.__examSpeak?.(UTF8ToString(message));
});
#else
inline float WebSteerInput() { return 0.0f; }
inline float WebThrottleInput() { return 0.0f; }
inline float WebBrakeInput() { return 0.0f; }
inline int WebConsumePressed(const char*) { return 0; }
inline void WebUpdateExam(const char*, const char*, const char*, const char*, int, int, int,
                          float, float, const char*, int, int, int, int, int, int, int, int,
                          int, int, int, int, int) {}
inline void WebSpeak(const char*) {}
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kWorldHalfWidth = 78.0f;
constexpr float kWorldHalfHeight = 72.0f;
constexpr float kCarLength = 4.45f;
constexpr float kCarWidth = 1.82f;
constexpr float kPadHalfX = 46.0f;
constexpr float kPadHalfY = 34.0f;
constexpr float kHillUpStartX = -37.0f;
constexpr float kHillTopStartX = -29.0f;
constexpr float kHillTopEndX = -25.0f;
constexpr float kHillDownEndX = -12.5f;

constexpr Color kSkyTop = {105, 157, 191, 255};
constexpr Color kSkyHorizon = {218, 230, 232, 255};
constexpr Color kAsphalt = {67, 72, 72, 255};
constexpr Color kAsphaltLight = {78, 83, 82, 255};
constexpr Color kGrass = {92, 119, 80, 255};
constexpr Color kConcrete = {179, 184, 180, 255};
constexpr Color kLaneWhite = {238, 238, 226, 255};
constexpr Color kSafetyYellow = {239, 190, 42, 255};
constexpr Color kCourseBlue = {32, 105, 157, 255};
constexpr Color kExamRed = {207, 47, 41, 255};
constexpr Color kExamGreen = {37, 153, 86, 255};
constexpr Color kVehicleBlue = {42, 88, 128, 255};

enum class TransmissionGear {
    Park,
    Drive,
    Reverse,
};

enum class ExamPhase {
    Briefing = 0,
    Precheck = 1,
    Running = 2,
    Finished = 3,
    Disqualified = 4,
    FreeDrive = 5,
};

enum class ObstacleType {
    Building,
    Barrier,
    Cone,
};

enum class CollisionKind {
    None,
    CourseBoundary,
    SolidObstacle,
};

struct OrientedRect {
    Vector2 center{};
    Vector2 half{};
    float angle = 0.0f;
};

struct RoadSurface {
    OrientedRect footprint{};
    bool centerLine = true;
    bool edgeLines = true;
};

struct Obstacle {
    OrientedRect footprint{};
    float height = 1.0f;
    Color color{};
    ObstacleType type = ObstacleType::Barrier;
};

struct CarState {
    Vector2 position{};
    float heading = 0.0f;
    float speed = 0.0f;
    float steering = 0.0f;
    bool contactLatch = false;
};

struct InputFrame {
    float steer = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;
    bool startPressed = false;
    bool freeDrivePressed = false;
    bool retryPressed = false;
    bool gearDrivePressed = false;
    bool gearReversePressed = false;
    bool gearParkPressed = false;
    bool seatbeltPressed = false;
    bool ignitionPressed = false;
    bool headlightPressed = false;
    bool wiperPressed = false;
    bool leftSignalPressed = false;
    bool rightSignalPressed = false;
    bool hazardPressed = false;
    bool parkingBrakePressed = false;
};

Vector2 VAdd(Vector2 a, Vector2 b) {
    return {a.x + b.x, a.y + b.y};
}

Vector2 VSub(Vector2 a, Vector2 b) {
    return {a.x - b.x, a.y - b.y};
}

Vector2 VScale(Vector2 value, float scale) {
    return {value.x * scale, value.y * scale};
}

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float LerpFloat(float a, float b, float amount) {
    return a + (b - a) * Clamp01(amount);
}

Vector3 LerpVector3(Vector3 a, Vector3 b, float amount) {
    return {
        LerpFloat(a.x, b.x, amount),
        LerpFloat(a.y, b.y, amount),
        LerpFloat(a.z, b.z, amount),
    };
}

float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

Vector2 RotateVector(Vector2 value, float angle) {
    const float cs = std::cos(angle);
    const float sn = std::sin(angle);
    return {
        value.x * cs - value.y * sn,
        value.x * sn + value.y * cs,
    };
}

Vector2 ForwardFromAngle(float angle) {
    return {std::cos(angle), std::sin(angle)};
}

std::array<Vector2, 4> GetCorners(const OrientedRect& rect) {
    const Vector2 forward = ForwardFromAngle(rect.angle);
    const Vector2 side = {-forward.y, forward.x};
    return {
        VAdd(VAdd(rect.center, VScale(forward, rect.half.x)), VScale(side, rect.half.y)),
        VAdd(VSub(rect.center, VScale(forward, rect.half.x)), VScale(side, rect.half.y)),
        VSub(VSub(rect.center, VScale(forward, rect.half.x)), VScale(side, rect.half.y)),
        VSub(VAdd(rect.center, VScale(forward, rect.half.x)), VScale(side, rect.half.y)),
    };
}

bool PointInsideRect(Vector2 point, const OrientedRect& rect, float margin = 0.0f) {
    const Vector2 local = RotateVector(VSub(point, rect.center), -rect.angle);
    return std::fabs(local.x) <= rect.half.x + margin &&
           std::fabs(local.y) <= rect.half.y + margin;
}

void ProjectOntoAxis(const std::array<Vector2, 4>& corners, Vector2 axis,
                     float* outMin, float* outMax) {
    axis = Vector2Normalize(axis);
    float minValue = Vector2DotProduct(corners[0], axis);
    float maxValue = minValue;
    for (size_t i = 1; i < corners.size(); ++i) {
        const float projection = Vector2DotProduct(corners[i], axis);
        minValue = std::min(minValue, projection);
        maxValue = std::max(maxValue, projection);
    }
    *outMin = minValue;
    *outMax = maxValue;
}

bool Intersects(const OrientedRect& a, const OrientedRect& b) {
    const auto aCorners = GetCorners(a);
    const auto bCorners = GetCorners(b);
    const std::array<Vector2, 4> edges = {
        VSub(aCorners[1], aCorners[0]),
        VSub(aCorners[3], aCorners[0]),
        VSub(bCorners[1], bCorners[0]),
        VSub(bCorners[3], bCorners[0]),
    };

    for (const Vector2 edge : edges) {
        const Vector2 axis = {-edge.y, edge.x};
        float aMin = 0.0f;
        float aMax = 0.0f;
        float bMin = 0.0f;
        float bMax = 0.0f;
        ProjectOntoAxis(aCorners, axis, &aMin, &aMax);
        ProjectOntoAxis(bCorners, axis, &bMin, &bMax);
        if (aMax < bMin || bMax < aMin) return false;
    }
    return true;
}

Color Shade(Color color, float factor) {
    return {
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.r * factor), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.g * factor), 0, 255)),
        static_cast<unsigned char>(std::clamp(static_cast<int>(color.b * factor), 0, 255)),
        color.a,
    };
}

void DrawOrientedCube(Vector2 center, float centerY, Vector3 size, float angle, Color color) {
    rlPushMatrix();
    rlTranslatef(center.x, centerY, center.y);
    rlRotatef(angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawCube({0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, color);
    rlPopMatrix();
}

void DrawOrientedCubeWires(Vector2 center, float centerY, Vector3 size, float angle,
                           Color color) {
    rlPushMatrix();
    rlTranslatef(center.x, centerY, center.y);
    rlRotatef(angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawCubeWires({0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, color);
    rlPopMatrix();
}

class DobongExamSimulator {
  public:
    DobongExamSimulator() {
#if defined(PLATFORM_WEB)
        SetConfigFlags(FLAG_VSYNC_HINT);
#else
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#endif
        SetTraceLogLevel(LOG_WARNING);
        InitWindow(1280, 720, "Dobong Driving Skills Test Simulator");
        SetTargetFPS(60);
        SetExitKey(KEY_NULL);

        camera_.position = {-43.0f, 1.35f, 30.0f};
        camera_.target = {-30.0f, 0.8f, 30.0f};
        camera_.up = {0.0f, 1.0f, 0.0f};
        camera_.fovy = 75.0f;
        camera_.projection = CAMERA_PERSPECTIVE;

        BuildCourse();
        InitRenderTargets();
        InitGround();
        BeginFreeDrive();
    }

    ~DobongExamSimulator() {
        UnloadRenderTargets();
        if (groundReady_) {
            UnloadModel(groundModel_);
            UnloadTexture(groundTexture_);
        }
        CloseWindow();
    }

    void Tick() {
        SyncCanvasSize();
        const double now = GetTime();
        const float clockDt =
            lastWallClock_ < 0.0
                ? 0.0f
                : std::max(0.0f, static_cast<float>(now - lastWallClock_));
        lastWallClock_ = now;
        const float physicsDt = std::min(clockDt, 1.0f / 20.0f);
        sceneTime_ += physicsDt;
        eventTimer_ = std::max(0.0f, eventTimer_ - clockDt);
        collisionFlash_ = std::max(0.0f, collisionFlash_ - physicsDt);

        const InputFrame input = GatherInput();
        Update(physicsDt, clockDt, input);
        UpdateCamera(physicsDt);
        PushWebState();
        Draw();
    }

  private:
    void AddRoad(Vector2 center, Vector2 half, float angle = 0.0f,
                 bool centerLine = true, bool edgeLines = true) {
        roads_.push_back({{center, half, angle}, centerLine, edgeLines});
    }

    void AddBuilding(Vector2 center, Vector2 half, float height, Color color) {
        obstacles_.push_back({{center, half, 0.0f}, height, color, ObstacleType::Building});
    }

    void BuildCourse() {
        roads_.clear();
        obstacles_.clear();
        route_.clear();

        // The first Dobong pack follows the public 1/2-class course dimensions and
        // the recognizable compact east-side loop seen in the center's site layout.
        for (const auto& road : driving_test_data::dobong_v1::kRoads) {
            AddRoad({road.centerX, road.centerY},
                    {road.halfLength, road.halfWidth},
                    road.angleRadians, road.centerLine, road.edgeLines);
        }

        parkingZone_ = {{-12.0f, -20.0f}, {3.0f, 1.62f}, -kPi * 0.5f};
        // T-shape perpendicular parking: entry corridor and slot
        parkingEntryZone_ = {{-12.0f, -14.0f}, {4.0f, 2.8f}, -kPi * 0.5f};
        parkingConfirmLine_ = {{-12.0f, -22.4f}, {2.9f, 0.12f}, 0.0f};
        finishZone_ = {{27.0f, 30.0f}, {3.3f, 2.6f}, 0.0f};

        for (const auto& point : driving_test_data::dobong_v1::kRoute) {
            route_.push_back({point.x, point.y});
        }

        // --- Dobong/Nowon Procedural Apartment Complex (copyright-safe, no real branding) ---
        // Multiple slab-type apartment blocks (15–25 floors, ~2.8m/floor)
        // Positioned beyond course boundaries to provide Dobong atmosphere
        AddBuilding({-3.0f, 52.0f}, {22.0f, 5.5f}, 50.4f, {218, 222, 217, 255});  // 18F
        AddBuilding({-3.0f, 64.0f}, {20.0f, 5.0f}, 56.0f, {222, 225, 219, 255});  // 20F
        AddBuilding({25.0f, 58.0f}, {18.0f, 5.0f}, 67.2f, {215, 219, 213, 255});  // 24F
        AddBuilding({50.0f, 55.0f}, {16.0f, 5.5f}, 42.0f, {220, 223, 216, 255});  // 15F
        AddBuilding({-35.0f, 55.0f}, {19.0f, 5.0f}, 58.8f, {214, 218, 212, 255}); // 21F
        AddBuilding({-50.0f, 52.0f}, {14.0f, 5.0f}, 44.8f, {221, 224, 218, 255}); // 16F
        // Behind the west barrier
        AddBuilding({-65.0f, 20.0f}, {6.0f, 18.0f}, 53.2f, {216, 220, 214, 255}); // 19F
        AddBuilding({-65.0f, -10.0f}, {6.0f, 16.0f}, 70.0f, {213, 217, 211, 255}); // 25F
        // South side
        AddBuilding({-30.0f, -52.0f}, {18.0f, 5.5f}, 47.6f, {219, 222, 216, 255}); // 17F
        AddBuilding({10.0f, -52.0f}, {20.0f, 5.5f}, 61.6f, {216, 220, 214, 255});  // 22F
        AddBuilding({45.0f, -50.0f}, {15.0f, 5.0f}, 56.0f, {218, 221, 215, 255});  // 20F

        // East-side buildings (closer, original)
        AddBuilding({51.5f, 27.0f}, {4.2f, 7.0f}, 21.0f, {215, 219, 211, 255});
        AddBuilding({51.5f, 3.0f}, {4.2f, 7.0f}, 24.0f, {220, 223, 215, 255});
        AddBuilding({51.5f, -22.0f}, {4.2f, 7.0f}, 19.0f, {211, 216, 208, 255});

        // West barrier
        obstacles_.push_back({{{-55.0f, 0.0f}, {1.0f, 43.0f}, 0.0f}, 5.0f,
                              {116, 128, 127, 255}, ObstacleType::Barrier});

        const std::array<Vector2, 20> conePositions = {{
            {-48.0f, 25.4f}, {-42.0f, 25.4f}, {-36.0f, 25.4f}, {-30.0f, 25.4f},
            {-18.0f, 25.4f}, {-10.0f, 25.4f}, {7.3f, 25.0f}, {16.7f, 21.0f},
            {16.7f, 14.0f}, {16.7f, -4.0f}, {7.0f, -15.5f}, {0.0f, -15.5f},
            {-20.5f, -15.5f}, {-29.5f, -18.0f}, {-20.0f, -35.5f}, {-8.0f, -35.5f},
            {8.0f, -35.5f}, {24.0f, -35.5f}, {35.5f, -28.0f}, {35.5f, -12.0f},
        }};
        for (const Vector2 position : conePositions) {
            obstacles_.push_back({{position, {0.27f, 0.27f}, 0.0f}, 0.72f,
                                  {225, 104, 31, 255}, ObstacleType::Cone});
        }
    }

    void InitRenderTargets() {
        mirrorRear_ = LoadRenderTexture(384, 112);
        mirrorLeft_ = LoadRenderTexture(300, 150);
        mirrorRight_ = LoadRenderTexture(300, 150);
        mirrorsReady_ = mirrorRear_.id != 0 && mirrorLeft_.id != 0 && mirrorRight_.id != 0;
        if (mirrorsReady_) {
            SetTextureFilter(mirrorRear_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(mirrorLeft_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(mirrorRight_.texture, TEXTURE_FILTER_BILINEAR);
        }
    }

    void UnloadRenderTargets() {
        if (!mirrorsReady_) return;
        UnloadRenderTexture(mirrorRear_);
        UnloadRenderTexture(mirrorLeft_);
        UnloadRenderTexture(mirrorRight_);
        mirrorsReady_ = false;
    }

    void InitGround() {
        constexpr int kSize = 256;
        Image image = GenImageColor(kSize, kSize, kGrass);
        Color* pixels = static_cast<Color*>(image.data);
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                unsigned int hash = static_cast<unsigned int>(x) * 374761393u +
                                    static_cast<unsigned int>(y) * 668265263u;
                hash = (hash ^ (hash >> 13)) * 1274126177u;
                const int jitter = static_cast<int>(hash % 15u) - 7;
                Color& pixel = pixels[y * kSize + x];
                pixel.r = static_cast<unsigned char>(
                    std::clamp(static_cast<int>(kGrass.r) + jitter, 0, 255));
                pixel.g = static_cast<unsigned char>(
                    std::clamp(static_cast<int>(kGrass.g) + jitter, 0, 255));
                pixel.b = static_cast<unsigned char>(
                    std::clamp(static_cast<int>(kGrass.b) + jitter / 2, 0, 255));
            }
        }

        groundTexture_ = LoadTextureFromImage(image);
        UnloadImage(image);
        SetTextureFilter(groundTexture_, TEXTURE_FILTER_BILINEAR);
        SetTextureWrap(groundTexture_, TEXTURE_WRAP_REPEAT);

        Mesh mesh = GenMeshPlane(kWorldHalfWidth * 2.0f, kWorldHalfHeight * 2.0f, 1, 1);
        for (int i = 0; i < mesh.vertexCount * 2; ++i) mesh.texcoords[i] *= 12.0f;
        groundModel_ = LoadModelFromMesh(mesh);
        groundModel_.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = groundTexture_;
        groundReady_ = true;
    }

    void ResetExam() {
        phase_ = ExamPhase::Briefing;
        car_.position = {-44.0f, 30.0f};
        car_.heading = 0.0f;
        car_.speed = 0.0f;
        car_.steering = 0.0f;
        car_.contactLatch = false;
        gear_ = TransmissionGear::Park;
        score_ = dobong_exam::kInitialScore;
        courseStep_ = 0;
        precheckStep_ = 0;
        precheckTimer_ = 0.0f;
        examTimer_ = 0.0f;
        stepTimer_ = 0.0f;
        hillStopTimer_ = 0.0f;
        hillAttemptTimer_ = 0.0f;
        hillAttemptStarted_ = false;
        hillRollbackOrigin_ = -100.0f;
        hillRollbackPenalty_ = false;
        hillComplete_ = false;
        emergencyTriggered_ = false;
        emergencyActive_ = false;
        emergencyStopped_ = false;
        emergencyPenalty_ = false;
        emergencyTimer_ = 0.0f;
        stoppedEmergencyTimer_ = 0.0f;
        trafficLight_ = 0;
        trafficTimer_ = 0.0f;
        trafficArmed_ = false;
        trafficStopped_ = false;
        parkingTimer_ = 0.0f;
        parkingComplete_ = false;
        parkingOvertimePenalty_ = false;
        parkingEnteredReverse_ = false;
        accelerationStarted_ = false;
        accelerationMaxKph_ = 0.0f;
        speedPenaltyLatch_ = false;
        ignitionPenaltyLatch_ = false;
        nextOvertimePenaltyAt_ = dobong_exam::FirstOvertimePenaltyAt();
        finishSignalPenalty_ = false;
        resultPassed_ = false;
        seatbelt_ = false;
        ignition_ = false;
        parkingBrake_ = true;
        headlights_ = 0;
        wiper_ = false;
        leftSignal_ = false;
        rightSignal_ = false;
        hazard_ = false;
        eventText_ = "도봉 코스가 준비되었습니다.";
        eventTimer_ = 3.0f;
        lastSpoken_.clear();
    }

    // Free driving: no scoring, no exam gating. Car is ready to move at once.
    void BeginFreeDrive() {
        ResetExam();
        phase_ = ExamPhase::FreeDrive;
        car_.position = {-22.0f, 6.0f};
        car_.heading = 0.0f;
        seatbelt_ = true;
        ignition_ = true;
        parkingBrake_ = false;
        gear_ = TransmissionGear::Drive;
        eventText_ = "자유 주행 · 채점 없음 · 가속 페달을 밟아 출발하세요.";
        eventTimer_ = 5.0f;
        Speak("자유 주행을 시작합니다. 가속 페달을 밟아 출발하세요.");
    }

    void UpdateFreeDrive(float dt, const InputFrame& input) {
        if (input.startPressed) {
            ResetExam();
            BeginPrecheck();
            return;
        }
        if (input.retryPressed) {
            BeginFreeDrive();
            return;
        }
        const CarState previous = car_;
        UpdateVehicle(dt, input);
        if (DetectCollision() != CollisionKind::None) {
            car_ = previous;
            car_.speed = 0.0f;
            collisionFlash_ = 0.3f;
        }
    }

    void BeginPrecheck() {
        phase_ = ExamPhase::Precheck;
        precheckStep_ = 0;
        precheckTimer_ = 0.0f;
        eventText_ = "전자채점 기본조작 시험을 시작합니다.";
        eventTimer_ = 4.0f;
        SpeakCurrentInstruction();
    }

    void BeginDriving() {
        phase_ = ExamPhase::Running;
        courseStep_ = 0;
        examTimer_ = 0.0f;
        stepTimer_ = 0.0f;
        parkingBrake_ = true;
        eventText_ = "기본조작 확인 완료 · 100점";
        eventTimer_ = 4.0f;
        SpeakCurrentInstruction();
    }

    void FinishExam() {
        car_.speed = 0.0f;
        phase_ = ExamPhase::Finished;
        resultPassed_ = dobong_exam::IsPassingScore(score_);
        eventText_ = resultPassed_ ? "합격 기준을 충족했습니다." : "80점 미만으로 불합격입니다.";
        eventTimer_ = 999.0f;
        Speak(resultPassed_ ? "시험이 종료되었습니다. 합격입니다."
                            : "시험이 종료되었습니다. 불합격입니다.");
    }

    void Disqualify(const std::string& reason) {
        if (phase_ == ExamPhase::Disqualified) return;
        car_.speed = 0.0f;
        phase_ = ExamPhase::Disqualified;
        resultPassed_ = false;
        eventText_ = "실격 · " + reason;
        eventTimer_ = 999.0f;
        Speak("실격입니다. " + reason);
    }

    void ApplyPenalty(dobong_exam::Penalty penalty, const std::string& reason) {
        const int points = dobong_exam::Points(penalty);
        score_ = dobong_exam::ApplyPenalty(score_, penalty);
        char message[196];
        std::snprintf(message, sizeof(message), "-%d점 · %s", points, reason.c_str());
        eventText_ = message;
        eventTimer_ = 5.0f;
        Speak(eventText_);
    }

    void AdvanceStep(int nextStep) {
        courseStep_ = nextStep;
        stepTimer_ = 0.0f;
        SpeakCurrentInstruction();
    }

    void Speak(const std::string& message) {
        if (message.empty() || message == lastSpoken_) return;
        lastSpoken_ = message;
        WebSpeak(message.c_str());
    }

    void SpeakCurrentInstruction() {
        Speak(InstructionText());
    }

    void SyncCanvasSize() {
#if defined(PLATFORM_WEB)
        double cssWidth = 0.0;
        double cssHeight = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight) ==
            EMSCRIPTEN_RESULT_SUCCESS) {
            const int width = std::max(1, static_cast<int>(std::round(cssWidth)));
            const int height = std::max(1, static_cast<int>(std::round(cssHeight)));
            if (width != GetScreenWidth() || height != GetScreenHeight()) {
                emscripten_set_canvas_element_size("#canvas", width, height);
                SetWindowSize(width, height);
            }
        }
#endif
    }

    InputFrame GatherInput() const {
        InputFrame input{};
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.steer -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.steer += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.throttle = 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_SPACE)) {
            input.brake = 1.0f;
        }

        input.startPressed = IsKeyPressed(KEY_ENTER);
        input.freeDrivePressed = IsKeyPressed(KEY_F);
        input.retryPressed = IsKeyPressed(KEY_R);
        input.gearDrivePressed = IsKeyPressed(KEY_ONE);
        input.gearReversePressed = IsKeyPressed(KEY_TWO);
        input.seatbeltPressed = IsKeyPressed(KEY_K);
        input.ignitionPressed = IsKeyPressed(KEY_I);
        input.headlightPressed = IsKeyPressed(KEY_L);
        input.wiperPressed = IsKeyPressed(KEY_V);
        input.leftSignalPressed = IsKeyPressed(KEY_Z);
        input.rightSignalPressed = IsKeyPressed(KEY_X);
        input.hazardPressed = IsKeyPressed(KEY_C);
        input.parkingBrakePressed = IsKeyPressed(KEY_B);

#if defined(PLATFORM_WEB)
        input.steer += WebSteerInput();
        input.throttle = std::max(input.throttle, WebThrottleInput());
        input.brake = std::max(input.brake, WebBrakeInput());
        input.startPressed = input.startPressed || WebConsumePressed("startPressed");
        input.freeDrivePressed =
            input.freeDrivePressed || WebConsumePressed("freeDrivePressed");
        input.retryPressed = input.retryPressed || WebConsumePressed("retryPressed");
        input.gearDrivePressed =
            input.gearDrivePressed || WebConsumePressed("gearDrivePressed");
        input.gearReversePressed =
            input.gearReversePressed || WebConsumePressed("gearReversePressed");
        input.gearParkPressed =
            input.gearParkPressed || WebConsumePressed("gearParkPressed");
        input.seatbeltPressed =
            input.seatbeltPressed || WebConsumePressed("seatbeltPressed");
        input.ignitionPressed =
            input.ignitionPressed || WebConsumePressed("ignitionPressed");
        input.headlightPressed =
            input.headlightPressed || WebConsumePressed("headlightPressed");
        input.wiperPressed = input.wiperPressed || WebConsumePressed("wiperPressed");
        input.leftSignalPressed =
            input.leftSignalPressed || WebConsumePressed("leftSignalPressed");
        input.rightSignalPressed =
            input.rightSignalPressed || WebConsumePressed("rightSignalPressed");
        input.hazardPressed = input.hazardPressed || WebConsumePressed("hazardPressed");
        input.parkingBrakePressed =
            input.parkingBrakePressed || WebConsumePressed("parkingBrakePressed");
#endif
        input.steer = std::clamp(input.steer, -1.0f, 1.0f);
        return input;
    }

    void HandleToggleInputs(const InputFrame& input) {
        if (input.seatbeltPressed) seatbelt_ = !seatbelt_;
        if (input.ignitionPressed) ignition_ = !ignition_;
        if (input.headlightPressed) headlights_ = (headlights_ + 1) % 3;
        if (input.wiperPressed) wiper_ = !wiper_;

        if (input.leftSignalPressed) {
            const bool turnOn = !leftSignal_;
            leftSignal_ = turnOn;
            rightSignal_ = false;
            if (turnOn) hazard_ = false;
        }
        if (input.rightSignalPressed) {
            const bool turnOn = !rightSignal_;
            rightSignal_ = turnOn;
            leftSignal_ = false;
            if (turnOn) hazard_ = false;
        }
        if (input.hazardPressed) {
            hazard_ = !hazard_;
            if (hazard_) {
                leftSignal_ = false;
                rightSignal_ = false;
            }
        }
        if (input.parkingBrakePressed && std::fabs(car_.speed) < 0.3f) {
            parkingBrake_ = !parkingBrake_;
        }
    }

    void Update(float physicsDt, float clockDt, const InputFrame& input) {
        if (input.freeDrivePressed) {
            BeginFreeDrive();
            return;
        }

        if (input.retryPressed &&
            (phase_ == ExamPhase::Finished || phase_ == ExamPhase::Disqualified)) {
            ResetExam();
            return;
        }

        if (phase_ == ExamPhase::Briefing) {
            if (input.startPressed) BeginPrecheck();
            return;
        }

        HandleToggleInputs(input);

        if (phase_ == ExamPhase::FreeDrive) {
            UpdateFreeDrive(physicsDt, input);
            return;
        }

        if (phase_ == ExamPhase::Precheck) {
            UpdatePrecheck(clockDt, input);
            return;
        }

        if (phase_ == ExamPhase::Finished || phase_ == ExamPhase::Disqualified) {
            car_.speed = LerpFloat(car_.speed, 0.0f, physicsDt * 6.0f);
            return;
        }

        if (!seatbelt_) {
            Disqualify("시험 중 좌석안전띠를 해제했습니다.");
            return;
        }

        if (!ignition_) {
            if (!ignitionPenaltyLatch_) {
                ApplyPenalty(dobong_exam::Penalty::EngineState,
                             "시험 중 시동 상태를 유지하지 못했습니다.");
            }
            ignitionPenaltyLatch_ = true;
        } else {
            ignitionPenaltyLatch_ = false;
        }

        examTimer_ += clockDt;
        stepTimer_ += clockDt;
        while (examTimer_ >= nextOvertimePenaltyAt_) {
            ApplyPenalty(dobong_exam::Penalty::TimeOrSpeed,
                         "전체 지정시간을 5초 초과했습니다.");
            nextOvertimePenaltyAt_ +=
                dobong_exam::kOvertimePenaltyIntervalSeconds;
        }
        const CarState previous = car_;
        UpdateVehicle(physicsDt, input);

        const CollisionKind collision = DetectCollision();
        if (collision == CollisionKind::SolidObstacle) {
            car_ = previous;
            car_.speed = 0.0f;
            collisionFlash_ = 0.42f;
            Disqualify("안전사고 또는 연석 접촉이 발생했습니다.");
            return;
        }
        if (collision == CollisionKind::CourseBoundary) {
            car_ = previous;
            car_.speed = 0.0f;
            if (!previous.contactLatch) {
                ApplyPenalty(dobong_exam::Penalty::RoadBoundary,
                             "차로·길가장자리 구역선을 이탈하거나 표지물을 접촉했습니다.");
                collisionFlash_ = 0.42f;
            }
            car_.contactLatch = true;
        } else {
            car_.contactLatch = false;
        }

        const float speedKph = DisplaySpeedKph();
        const bool inAcceleration = courseStep_ == 7 && accelerationStarted_;
        if (!inAcceleration && dobong_exam::IsNormalSpeedViolation(speedKph)) {
            if (!speedPenaltyLatch_) {
                ApplyPenalty(dobong_exam::Penalty::TimeOrSpeed,
                             "가속구간 밖에서 시속 20km를 초과했습니다.");
            }
            speedPenaltyLatch_ = true;
        } else if (speedKph < 18.5f) {
            speedPenaltyLatch_ = false;
        }

        UpdateCourseLogic(clockDt, input);
    }

    void UpdatePrecheck(float clockDt, const InputFrame& input) {
        precheckTimer_ += clockDt;
        const bool wrongBasicControl =
            (precheckStep_ != 1 && input.ignitionPressed) ||
            (precheckStep_ != 2 && input.headlightPressed) ||
            (precheckStep_ != 3 && input.wiperPressed) ||
            (precheckStep_ != 4 &&
             (input.leftSignalPressed || input.rightSignalPressed)) ||
            input.gearDrivePressed || input.gearReversePressed;
        if (wrongBasicControl) {
            ApplyPenalty(dobong_exam::Penalty::BasicControl,
                         "시험관 지시와 다른 기본조작을 했습니다.");
        }

        bool completed = false;
        switch (precheckStep_) {
            case 0:
                completed = input.seatbeltPressed && seatbelt_;
                break;
            case 1:
                completed = input.ignitionPressed && ignition_;
                break;
            case 2:
                completed = input.headlightPressed && headlights_ == 1;
                break;
            case 3:
                completed = input.wiperPressed && wiper_;
                break;
            case 4:
                completed = input.leftSignalPressed && leftSignal_;
                break;
            default:
                BeginDriving();
                return;
        }

        if (completed) {
            ++precheckStep_;
            precheckTimer_ = 0.0f;
            eventText_ = "기본조작 확인";
            eventTimer_ = 1.2f;
            if (precheckStep_ >= 5) {
                BeginDriving();
            } else {
                SpeakCurrentInstruction();
            }
        } else if (dobong_exam::IsBasicControlOvertime(precheckTimer_)) {
            ApplyPenalty(dobong_exam::Penalty::BasicControl,
                         "기본조작 지시를 5초 이내에 이행하지 못했습니다.");
            ++precheckStep_;
            precheckTimer_ = 0.0f;
            if (precheckStep_ >= 5) {
                BeginDriving();
            } else {
                SpeakCurrentInstruction();
            }
        }
    }

    void UpdateVehicle(float dt, const InputFrame& input) {
        const bool shiftAllowed =
            std::fabs(car_.speed) < 0.5f &&
            (input.brake > 0.0f || phase_ == ExamPhase::FreeDrive);
        if (input.gearDrivePressed && shiftAllowed) {
            gear_ = TransmissionGear::Drive;
            if (phase_ == ExamPhase::FreeDrive) parkingBrake_ = false;
        }
        if (input.gearReversePressed && shiftAllowed) {
            gear_ = TransmissionGear::Reverse;
            if (phase_ == ExamPhase::FreeDrive) parkingBrake_ = false;
        }
        if (input.gearParkPressed && shiftAllowed) {
            gear_ = TransmissionGear::Park;
        }

        const float targetSteering = input.steer * 0.66f;
        car_.steering = LerpFloat(car_.steering, targetSteering, dt * 7.0f);

        if (!ignition_ || parkingBrake_ || gear_ == TransmissionGear::Park) {
            car_.speed = LerpFloat(car_.speed, 0.0f, dt * 9.0f);
            return;
        }

        float desiredSpeed = car_.speed;
        const float direction = gear_ == TransmissionGear::Drive ? 1.0f : -1.0f;
        const float creep = gear_ == TransmissionGear::Drive ? 1.05f : -0.85f;

        if (input.brake > 0.0f) {
            const float brakeDelta = 8.8f * dt;
            if (desiredSpeed > brakeDelta) {
                desiredSpeed -= brakeDelta;
            } else if (desiredSpeed < -brakeDelta) {
                desiredSpeed += brakeDelta;
            } else {
                desiredSpeed = 0.0f;
            }
        } else {
            desiredSpeed = LerpFloat(desiredSpeed, creep, dt * 1.25f);
            if (input.throttle > 0.0f) {
                desiredSpeed += direction * 4.1f * dt;
            }
        }

        if (car_.position.y > 25.5f && car_.position.y < 34.5f &&
            car_.position.x > kHillUpStartX &&
            car_.position.x < kHillTopStartX && input.brake <= 0.0f) {
            desiredSpeed -= 1.55f * dt;
        } else if (car_.position.y > 25.5f && car_.position.y < 34.5f &&
                   car_.position.x > kHillTopEndX &&
                   car_.position.x < kHillDownEndX && input.brake <= 0.0f) {
            desiredSpeed += 0.78f * dt;
        }

        car_.speed = std::clamp(desiredSpeed, -3.2f, 7.2f);
        const float turnRate = std::tan(car_.steering) * car_.speed / 2.72f;
        car_.heading = NormalizeAngle(car_.heading + turnRate * dt);
        car_.position =
            VAdd(car_.position, VScale(ForwardFromAngle(car_.heading), car_.speed * dt));
    }

    void UpdateCourseLogic(float dt, const InputFrame& input) {
        switch (courseStep_) {
            case 0:
                UpdateStartCourse();
                break;
            case 1:
                UpdateHillCourse(dt, input);
                break;
            case 2:
                UpdateEmergencyCourse(dt);
                break;
            case 3:
                UpdateFirstTurn();
                break;
            case 4:
                UpdateSignalIntersection(dt);
                break;
            case 5:
                UpdateParkingCourse(dt);
                break;
            case 6:
                UpdateConnectorTurns();
                break;
            case 7:
                UpdateAccelerationCourse();
                break;
            case 8:
                UpdateFinishCourse(input);
                break;
            default:
                break;
        }
    }

    void UpdateStartCourse() {
        if (dobong_exam::IsStartOvertime(stepTimer_)) {
            Disqualify("출발지시 후 30초 이내에 출발하지 못했습니다.");
            return;
        }

        if (car_.position.x > -39.0f) {
            if (!leftSignal_) {
                ApplyPenalty(dobong_exam::Penalty::TurnSignal,
                             "출발 시 좌측 방향지시등을 켜지 않았습니다.");
            }
            leftSignal_ = false;
            AdvanceStep(1);
        }
    }

    void UpdateHillCourse(float dt, const InputFrame& input) {
        if (!hillAttemptStarted_ && car_.position.x >= kHillUpStartX) {
            hillAttemptStarted_ = true;
            hillAttemptTimer_ = 0.0f;
        }
        if (hillAttemptStarted_) {
            hillAttemptTimer_ += dt;
            if (dobong_exam::IsHillCourseOvertime(hillAttemptTimer_)) {
                Disqualify("경사로 진입 후 30초 이내에 통과하지 못했습니다.");
                return;
            }
        }

        const bool inStopZone = car_.position.x >= -28.5f && car_.position.x <= -25.5f &&
                                std::fabs(car_.position.y - 30.0f) < 3.2f;
        if (inStopZone && std::fabs(car_.speed) < 0.22f && input.brake > 0.0f) {
            hillStopTimer_ += dt;
            if (hillRollbackOrigin_ < -90.0f) hillRollbackOrigin_ = car_.position.x;
            if (hillStopTimer_ >= 3.0f && !hillComplete_) {
                hillComplete_ = true;
                eventText_ = "경사로 정지 3초 확인";
                eventTimer_ = 3.0f;
                Speak("경사로 정지 확인. 뒤로 밀리지 않게 출발하세요.");
            }
        }

        if (hillRollbackOrigin_ > -90.0f) {
            const float rollback = std::max(0.0f, hillRollbackOrigin_ - car_.position.x);
            if (!hillRollbackPenalty_ &&
                dobong_exam::IsHillRollbackPenalty(rollback)) {
                ApplyPenalty(dobong_exam::Penalty::HillStop,
                             "경사로 출발 시 50센티미터 이상 밀렸습니다.");
                hillRollbackPenalty_ = true;
            }
            if (dobong_exam::IsHillRollbackFailure(rollback)) {
                Disqualify("경사로에서 1미터 이상 후방으로 밀렸습니다.");
                return;
            }
        }

        if (car_.position.x > kHillDownEndX) {
            if (!hillComplete_) {
                Disqualify("경사로 정지구간을 이행하지 않았습니다.");
                return;
            }
            AdvanceStep(2);
        }
    }

    void UpdateEmergencyCourse(float dt) {
        if (!emergencyTriggered_ && car_.position.x > -12.0f) {
            emergencyTriggered_ = true;
            emergencyActive_ = true;
            emergencyTimer_ = 0.0f;
            eventText_ = "돌발! 즉시 정지";
            eventTimer_ = 6.0f;
            Speak("돌발. 돌발. 즉시 정지하세요.");
        }

        if (!emergencyActive_) return;
        emergencyTimer_ += dt;

        if (!emergencyStopped_ && std::fabs(car_.speed) < 0.22f) {
            emergencyStopped_ = true;
            stoppedEmergencyTimer_ = 0.0f;
            if (dobong_exam::IsEmergencyStopLate(emergencyTimer_)) {
                ApplyPenalty(dobong_exam::Penalty::EmergencyStop,
                             "돌발등 점등 후 2초 이내 정지하지 못했습니다.");
                emergencyPenalty_ = true;
            }
            Speak("정지 확인. 3초 이내에 비상점멸등을 켜세요.");
        }

        if (emergencyStopped_) {
            stoppedEmergencyTimer_ += dt;
            if (!hazard_ && !emergencyPenalty_ &&
                dobong_exam::IsHazardLate(stoppedEmergencyTimer_)) {
                ApplyPenalty(dobong_exam::Penalty::EmergencyStop,
                             "정지 후 3초 이내 비상점멸등을 켜지 않았습니다.");
                emergencyPenalty_ = true;
            }

            if (hazard_) {
                eventText_ = "돌발 대응 확인 · 비상등을 끄고 출발";
                eventTimer_ = 4.0f;
            }

            if (hazard_ == false && stoppedEmergencyTimer_ > 0.35f &&
                (std::fabs(car_.speed) > 0.35f || car_.position.x > -6.0f)) {
                emergencyActive_ = false;
                AdvanceStep(3);
            }
        }
    }

    void UpdateFirstTurn() {
        const bool turnedSouth = car_.position.y < 26.0f &&
                                 std::sin(car_.heading) < -0.55f &&
                                 car_.position.x > 7.0f &&
                                 car_.position.x < 17.0f;
        if (turnedSouth) {
            if (!rightSignal_) {
                ApplyPenalty(dobong_exam::Penalty::TurnSignal,
                             "우회전 시 방향지시등을 켜지 않았습니다.");
            }
            rightSignal_ = false;
            AdvanceStep(4);
        }
    }

    void UpdateSignalIntersection(float dt) {
        const bool inSignalLane =
            car_.position.x >= 8.0f && car_.position.x <= 16.0f;
        if (!trafficArmed_ && inSignalLane && car_.position.y < 11.5f) {
            trafficArmed_ = true;
            trafficLight_ = 1;
            trafficTimer_ = 0.0f;
            Speak("신호교차로입니다. 적색 신호에 정지하세요.");
        }

        if (trafficArmed_ && trafficLight_ == 1) {
            trafficTimer_ += dt;
            const Vector2 frontBumper =
                VAdd(car_.position,
                     VScale(ForwardFromAngle(car_.heading), kCarLength * 0.5f));
            if (std::fabs(car_.speed) < 0.22f && frontBumper.y >= 7.4f &&
                frontBumper.y <= 11.5f) {
                trafficStopped_ = true;
            }
            if (inSignalLane && frontBumper.y < 7.4f) {
                Disqualify("신호교차로에서 적색 신호를 위반했습니다.");
                return;
            }
            if (trafficTimer_ >= 4.5f) {
                trafficLight_ = 2;
                eventText_ = "녹색 신호 · 직진";
                eventTimer_ = 3.0f;
                Speak("녹색 신호입니다. 직진하세요.");
            }
        }

        if (trafficLight_ == 2 && inSignalLane && car_.position.y < -0.5f) {
            if (!trafficStopped_) {
                ApplyPenalty(dobong_exam::Penalty::SignalIntersection,
                             "신호교차로에서 정지 확인을 이행하지 않았습니다.");
            }
            trafficLight_ = 0;
            AdvanceStep(5);
        }
    }

    void UpdateParkingCourse(float dt) {
        parkingTimer_ += dt;
        if (!parkingOvertimePenalty_ &&
            dobong_exam::IsParkingOvertime(parkingTimer_)) {
            ApplyPenalty(dobong_exam::Penalty::PerpendicularParking,
                         "직각주차 지정시간 120초를 초과했습니다.");
            parkingOvertimePenalty_ = true;
        }

        // Track whether car entered the parking slot in reverse
        if (!parkingEnteredReverse_ && PointInsideRect(car_.position, parkingEntryZone_)) {
            parkingEnteredReverse_ = (gear_ == TransmissionGear::Reverse);
        }

        if (!parkingComplete_ && IsFullyInside(parkingZone_) &&
            TouchesParkingConfirmationLine() &&
            std::fabs(car_.speed) < 0.22f && parkingBrake_) {
            // Penalize if not entered in reverse
            if (!parkingEnteredReverse_) {
                ApplyPenalty(dobong_exam::Penalty::PerpendicularParking,
                             "직각주차를 후진으로 진입하지 않았습니다.");
            }
            parkingComplete_ = true;
            eventText_ = "직각주차 확인선 접촉 · 주차브레이크 확인";
            eventTimer_ = 5.0f;
            Speak("직각주차 확인되었습니다. 주차브레이크를 해제하고 전진으로 나오세요.");
        }

        if (parkingComplete_ && car_.position.y > -13.5f &&
            car_.position.x < -18.0f) {
            AdvanceStep(6);
        }
    }

    void UpdateConnectorTurns() {
        const bool turnedSouth = car_.position.x < -21.0f && car_.position.y < -16.0f &&
                                 std::sin(car_.heading) < -0.52f;
        if (turnedSouth) {
            if (!leftSignal_) {
                ApplyPenalty(dobong_exam::Penalty::TurnSignal,
                             "진로변경 시 좌측 방향지시등을 켜지 않았습니다.");
            }
            leftSignal_ = false;
            AdvanceStep(7);
        }
    }

    void UpdateAccelerationCourse() {
        const bool enteredStraight = car_.position.y < -27.0f &&
                                     car_.position.x > -22.0f &&
                                     std::cos(car_.heading) > 0.55f;
        if (!accelerationStarted_ && enteredStraight) {
            if (!leftSignal_) {
                ApplyPenalty(dobong_exam::Penalty::TurnSignal,
                             "가속구간 진입 좌회전 시 방향지시등을 켜지 않았습니다.");
            }
            leftSignal_ = false;
            accelerationStarted_ = true;
            accelerationMaxKph_ = DisplaySpeedKph();
            eventText_ = "40m 가속구간 · 시속 20km 이상";
            eventTimer_ = 5.0f;
            Speak("가속구간입니다. 시속 20킬로미터 이상으로 주행하세요.");
        }

        if (accelerationStarted_) {
            accelerationMaxKph_ = std::max(accelerationMaxKph_, DisplaySpeedKph());
            if (car_.position.x > 22.0f) {
                if (accelerationMaxKph_ < 20.0f) {
                    ApplyPenalty(dobong_exam::Penalty::Acceleration,
                                 "가속구간에서 시속 20km에 도달하지 못했습니다.");
                }
                AdvanceStep(8);
            }
        }
    }

    void UpdateFinishCourse(const InputFrame& input) {
        if (PointInsideRect(car_.position, finishZone_) && !rightSignal_ &&
            !finishSignalPenalty_) {
            ApplyPenalty(dobong_exam::Penalty::TurnSignal,
                         "종료지점 진입 시 우측 방향지시등을 켜지 않았습니다.");
            finishSignalPenalty_ = true;
        }

        if (PointInsideRect(car_.position, finishZone_) &&
            std::fabs(car_.speed) < 0.2f && input.brake > 0.0f) {
            FinishExam();
        }
    }

    OrientedRect CarRect() const {
        return {car_.position, {kCarLength * 0.5f, kCarWidth * 0.5f}, car_.heading};
    }

    bool IsCarOnCourse() const {
        const auto corners = GetCorners(CarRect());
        for (const Vector2 corner : corners) {
            bool onSurface = false;
            for (const RoadSurface& road : roads_) {
                if (PointInsideRect(corner, road.footprint, 0.06f)) {
                    onSurface = true;
                    break;
                }
            }
            if (!onSurface) return false;
        }
        return true;
    }

    bool TouchesRoadEdgeLine() const {
        const RoadSurface* activeRoad = nullptr;
        int containingRoads = 0;
        for (const RoadSurface& road : roads_) {
            if (PointInsideRect(car_.position, road.footprint, 0.08f)) {
                activeRoad = &road;
                ++containingRoads;
            }
        }
        if (containingRoads != 1 || activeRoad == nullptr ||
            !activeRoad->edgeLines) {
            return false;
        }

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        constexpr float kWheelLongitudinal = kCarLength * 0.32f;
        constexpr float kWheelLateral = kCarWidth * 0.38f;
        const std::array<Vector2, 4> wheels = {{
            VAdd(VAdd(car_.position, VScale(forward, kWheelLongitudinal)),
                 VScale(side, kWheelLateral)),
            VSub(VAdd(car_.position, VScale(forward, kWheelLongitudinal)),
                 VScale(side, kWheelLateral)),
            VAdd(VSub(car_.position, VScale(forward, kWheelLongitudinal)),
                 VScale(side, kWheelLateral)),
            VSub(VSub(car_.position, VScale(forward, kWheelLongitudinal)),
                 VScale(side, kWheelLateral)),
        }};

        const OrientedRect& footprint = activeRoad->footprint;
        const float edgeOffset = footprint.half.y - 0.17f;
        for (const Vector2 wheel : wheels) {
            const Vector2 local =
                RotateVector(VSub(wheel, footprint.center), -footprint.angle);
            if (std::fabs(local.x) <= footprint.half.x &&
                std::fabs(std::fabs(local.y) - edgeOffset) <= 0.11f) {
                return true;
            }
        }
        return false;
    }

    CollisionKind DetectCollision() const {
        const OrientedRect carRect = CarRect();
        if (phase_ == ExamPhase::FreeDrive) {
            for (const Obstacle& obstacle : obstacles_) {
                if (obstacle.type == ObstacleType::Cone) continue;
                if (Intersects(carRect, obstacle.footprint)) {
                    return CollisionKind::SolidObstacle;
                }
            }
            if (std::fabs(car_.position.x) > kPadHalfX ||
                std::fabs(car_.position.y) > kPadHalfY) {
                return CollisionKind::SolidObstacle;
            }
            return CollisionKind::None;
        }
        bool touchedCone = false;
        for (const Obstacle& obstacle : obstacles_) {
            if (!Intersects(carRect, obstacle.footprint)) continue;
            if (obstacle.type == ObstacleType::Cone) {
                touchedCone = true;
            } else {
                return CollisionKind::SolidObstacle;
            }
        }
        if (touchedCone || TouchesRoadEdgeLine() || !IsCarOnCourse()) {
            return CollisionKind::CourseBoundary;
        }
        return CollisionKind::None;
    }

    bool IsFullyInside(const OrientedRect& zone) const {
        for (const Vector2 corner : GetCorners(CarRect())) {
            if (!PointInsideRect(corner, zone, -0.08f)) return false;
        }
        return true;
    }

    bool TouchesParkingConfirmationLine() const {
        return Intersects(CarRect(), parkingConfirmLine_);
    }

    float GroundHeightAt(Vector2 position) const {
        if (std::fabs(position.y - 30.0f) > 4.2f) return 0.0f;
        if (position.x >= kHillUpStartX && position.x < kHillTopStartX) {
            return (position.x - kHillUpStartX) /
                   (kHillTopStartX - kHillUpStartX);
        }
        if (position.x >= kHillTopStartX && position.x <= kHillTopEndX) {
            return 1.0f;
        }
        if (position.x > kHillTopEndX && position.x <= kHillDownEndX) {
            return 1.0f - (position.x - kHillTopEndX) /
                              (kHillDownEndX - kHillTopEndX);
        }
        return 0.0f;
    }

    float DisplaySpeedKph() const {
        return std::fabs(car_.speed) * 3.6f;
    }

    const char* GearLabel() const {
        if (gear_ == TransmissionGear::Park) return "P";
        return gear_ == TransmissionGear::Drive ? "D" : "R";
    }

    std::string PhaseTitle() const {
        switch (phase_) {
            case ExamPhase::Briefing:
                return "도봉 2종 보통 자동 기능시험";
            case ExamPhase::FreeDrive:
                return "자유 주행";
            case ExamPhase::Precheck:
                return "출발 전 기본조작";
            case ExamPhase::Running:
                switch (courseStep_) {
                    case 0: return "출발";
                    case 1: return "경사로 정지·출발";
                    case 2: return "돌발상황 급정지";
                    case 3: return "우회전";
                    case 4: return "신호교차로";
                    case 5: return "직각주차";
                    case 6: return "연결구간";
                    case 7: return "가속구간";
                    case 8: return "종료";
                    default: return "장내기능시험";
                }
            case ExamPhase::Finished:
                return resultPassed_ ? "시험 종료 · 합격" : "시험 종료 · 불합격";
            case ExamPhase::Disqualified:
                return "시험 종료 · 실격";
        }
        return "장내기능시험";
    }

    std::string InstructionText() const {
        if (phase_ == ExamPhase::FreeDrive) {
            return "핸들을 돌리고 가속·브레이크로 자유롭게 주행하세요. R로 후진할 수 있습니다.";
        }
        if (phase_ == ExamPhase::Briefing) {
            return "도봉운전면허시험장 재구성 코스에서 실전 순서로 연습합니다.";
        }
        if (phase_ == ExamPhase::Precheck) {
            switch (precheckStep_) {
                case 0: return "좌석안전띠를 착용하세요.";
                case 1: return "브레이크를 밟고 시동을 켜세요.";
                case 2: return "전조등을 하향으로 켜세요.";
                case 3: return "앞유리 와이퍼를 작동하세요.";
                case 4: return "좌측 방향지시등을 켜세요.";
                default: return "기본조작 확인을 마쳤습니다.";
            }
        }
        if (phase_ == ExamPhase::Finished || phase_ == ExamPhase::Disqualified) {
            return "다시 응시를 눌러 처음부터 연습할 수 있습니다.";
        }

        switch (courseStep_) {
            case 0:
                return "브레이크를 밟아 D로 전환하고 주차브레이크를 해제한 뒤 좌측 방향지시등을 켜고 출발하세요.";
            case 1:
                return hillComplete_
                           ? "뒤로 밀리지 않게 가속하여 경사로를 통과하세요."
                           : "경사로 정지검지구역 안에서 3초 이상 완전히 정지하세요.";
            case 2:
                if (!emergencyTriggered_) return "속도를 유지하며 돌발구간으로 진입하세요.";
                if (!emergencyStopped_) return "돌발! 2초 이내에 완전히 정지하세요.";
                if (!hazard_) return "정지 후 3초 이내에 비상점멸등을 켜세요.";
                return "비상점멸등을 끈 뒤 다시 출발하세요.";
            case 3:
                return "교차로 앞에서 우측 방향지시등을 켜고 우회전하세요.";
            case 4:
                return trafficLight_ == 1 ? "적색 신호입니다. 정지선 앞에 정지하세요."
                                         : "녹색 신호에 직진 통과하세요.";
            case 5:
                return parkingComplete_
                           ? "주차브레이크를 해제하고 D로 전환해 차고를 빠져나오세요."
                           : "우측 통로로 진입한 뒤 R로 전환해 직각주차 확인선까지 후진하세요.";
            case 6:
                return "좌측 방향지시등을 켜고 왼쪽 연결로로 진행하세요.";
            case 7:
                return accelerationStarted_
                           ? "가속구간에서 시속 20km 이상에 도달한 뒤 부드럽게 감속하세요."
                           : "좌측 방향지시등을 켜고 가속 직선구간에 진입하세요.";
            case 8:
                return "왼쪽으로 돌아 종료선에 접근해 우측 방향지시등을 켜고 정지하세요.";
            default:
                return "음성 안내와 차로 표시를 따라 진행하세요.";
        }
    }

    std::string StatusText() const {
        if (phase_ == ExamPhase::FreeDrive) {
            return "자유 주행 모드 · 감점·실격 없음 · 다시 시작으로 위치 초기화";
        }
        if (phase_ == ExamPhase::Briefing) {
            return "100점 시작 · 80점 이상 합격 · 공개 법정 규격 기반";
        }
        if (phase_ == ExamPhase::Precheck) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer),
                          "기본조작 %d/5 · 직접 조작 · 남은 시간 %.1f초",
                          std::min(precheckStep_ + 1, 5),
                          std::max(0.0f, dobong_exam::kBasicControlLimitSeconds -
                                             precheckTimer_));
            return buffer;
        }
        if (phase_ == ExamPhase::Disqualified) return "즉시 실격 처리되었습니다.";
        if (phase_ == ExamPhase::Finished) {
            return resultPassed_ ? "최종점수 80점 이상" : "합격 기준 80점 미달";
        }

        if (courseStep_ == 1) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "경사로 정지 %.1f / 3.0초",
                          std::min(hillStopTimer_, 3.0f));
            return buffer;
        }
        if (courseStep_ == 2 && emergencyTriggered_) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "돌발 대응 %.1f초", emergencyTimer_);
            return buffer;
        }
        if (courseStep_ == 5) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "직각주차 %.0f / 120초", parkingTimer_);
            return buffer;
        }
        if (courseStep_ == 7 && accelerationStarted_) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "가속구간 최고 %.0f km/h",
                          accelerationMaxKph_);
            return buffer;
        }
        return "차로 경계 접촉과 시속 20km 초과에 주의하세요.";
    }

    void PushWebState() const {
        const std::string phaseTitle = PhaseTitle();
        const std::string instruction = InstructionText();
        const std::string status = StatusText();
        const std::string event = eventTimer_ > 0.0f ? eventText_ : "";
        const bool freeDrive = phase_ == ExamPhase::FreeDrive;
        const int displayStep =
            freeDrive ? 0
                      : (phase_ == ExamPhase::Precheck ? std::min(precheckStep_ + 1, 5)
                                                       : std::min(courseStep_ + 1, 9));
        const int displayTotal = freeDrive ? 0 : (phase_ == ExamPhase::Precheck ? 5 : 9);

        WebUpdateExam(
            phaseTitle.c_str(), instruction.c_str(), status.c_str(), event.c_str(),
            score_, displayStep, displayTotal, examTimer_, DisplaySpeedKph(), GearLabel(),
            static_cast<int>(phase_), seatbelt_ ? 1 : 0, ignition_ ? 1 : 0,
            parkingBrake_ ? 1 : 0, headlights_, wiper_ ? 1 : 0,
            leftSignal_ ? 1 : 0, rightSignal_ ? 1 : 0, hazard_ ? 1 : 0,
            trafficLight_, emergencyActive_ ? 1 : 0,
            (phase_ == ExamPhase::Finished || phase_ == ExamPhase::Disqualified) ? 1 : 0,
            resultPassed_ ? 1 : 0);
    }

    void UpdateCamera(float dt) {
        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        const float groundY = GroundHeightAt(car_.position);
        const float speedRatio = Clamp01(DisplaySpeedKph() / 26.0f);
        const float vibration =
            std::sin(sceneTime_ * (6.0f + speedRatio * 8.0f)) * 0.009f * speedRatio;
        const float glance = car_.steering * 1.05f;

        // Driver seat offset: ~0.50m left of vehicle center (left-hand drive Korea)
        // Provides bonnet-left sensation matching real driver perspective
        constexpr float kDriverLateralOffset = -0.62f;
        constexpr float kDriverLongitudinalOffset = 0.42f;

        const Vector3 desiredPosition = {
            car_.position.x + forward.x * kDriverLongitudinalOffset + side.x * kDriverLateralOffset,
            groundY + 1.28f + vibration,
            car_.position.y + forward.y * kDriverLongitudinalOffset + side.y * kDriverLateralOffset,
        };
        const Vector3 desiredTarget = {
            car_.position.x + forward.x * 15.0f + side.x * (glance + kDriverLateralOffset),
            groundY + 0.82f,
            car_.position.y + forward.y * 15.0f + side.y * (glance + kDriverLateralOffset),
        };
        const float blend = 1.0f - std::exp(-dt * 8.0f);
        camera_.position = LerpVector3(camera_.position, desiredPosition, blend);
        camera_.target = LerpVector3(camera_.target, desiredTarget, blend);
        camera_.fovy = 74.0f;
    }

    void DrawGroundBox(Vector2 center, Vector2 size, float angle, float thickness,
                       Color color, float elevation = 0.0f) const {
        DrawOrientedCube(center, elevation + thickness * 0.5f,
                         {size.x, thickness, size.y}, angle, color);
    }

    void DrawLineBox(Vector2 start, Vector2 end, float width, Color color,
                     float elevation = 0.03f) const {
        const Vector2 delta = VSub(end, start);
        const float length = Vector2Length(delta);
        const Vector2 center = VScale(VAdd(start, end), 0.5f);
        DrawGroundBox(center, {length, width}, std::atan2(delta.y, delta.x), 0.018f,
                      color, elevation);
    }

    void DrawOutline(const OrientedRect& rect, Color color, float width = 0.12f) const {
        const auto corners = GetCorners(rect);
        DrawLineBox(corners[0], corners[1], width, color);
        DrawLineBox(corners[1], corners[2], width, color);
        DrawLineBox(corners[2], corners[3], width, color);
        DrawLineBox(corners[3], corners[0], width, color);
    }

    void DrawBackdrop() const {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();
        const int horizon = static_cast<int>(height * 0.66f);
        DrawRectangleGradientV(0, 0, width, horizon, kSkyTop, kSkyHorizon);
        DrawRectangleGradientV(0, horizon, width, height - horizon,
                               Fade(kSkyHorizon, 0.9f), Fade(kGrass, 0.1f));

        const float headingShift = car_.heading / (2.0f * kPi) * width;

        // Suraksan/Dobongsan-style rocky ridgeline (far layer, jagged peaks)
        {
            const float baseY = height * 0.44f;
            const Color ridge{95, 112, 118, 255};
            for (int i = -3; i < 10; ++i) {
                const float x = i * width * 0.16f - std::fmod(headingShift * 0.1f, width * 0.32f);
                const float peak = 72.0f + ((i * 7 + 13) % 5) * 22.0f;
                DrawTriangle({x - width * 0.02f, baseY},
                             {x + width * 0.08f, baseY - peak},
                             {x + width * 0.18f, baseY}, Fade(ridge, 0.58f));
                // Secondary sub-peak for rocky appearance
                const float subPeak = peak * 0.6f + ((i * 3 + 7) % 4) * 8.0f;
                DrawTriangle({x + width * 0.04f, baseY},
                             {x + width * 0.12f, baseY - subPeak},
                             {x + width * 0.22f, baseY}, Fade(ridge, 0.42f));
            }
        }

        // Nearer mountain layers (Dobong ridgeline character)
        for (int layer = 0; layer < 2; ++layer) {
            const float baseY = height * (0.48f + layer * 0.07f);
            const Color mountain =
                layer == 0 ? Color{119, 139, 143, 255} : Color{92, 118, 110, 255};
            for (int i = -2; i < 8; ++i) {
                const float x = i * width * 0.2f - std::fmod(headingShift * (0.15f + layer * 0.08f),
                                                            width * 0.4f);
                const float peak = 55.0f + ((i + layer * 3) % 3) * 28.0f;
                DrawTriangle({x, baseY}, {x + width * 0.13f, baseY - peak},
                             {x + width * 0.28f, baseY}, Fade(mountain, 0.72f));
            }
        }
    }

    void DrawRoad(const RoadSurface& road) const {
        const OrientedRect& rect = road.footprint;
        DrawGroundBox(rect.center, {rect.half.x * 2.0f, rect.half.y * 2.0f},
                      rect.angle, 0.035f, kAsphalt);

        const Vector2 forward = ForwardFromAngle(rect.angle);
        const Vector2 side = {-forward.y, forward.x};
        if (road.edgeLines) {
            for (const float sideSign : {-1.0f, 1.0f}) {
                const Vector2 lineCenter =
                    VAdd(rect.center, VScale(side, sideSign * (rect.half.y - 0.17f)));
                DrawGroundBox(lineCenter, {rect.half.x * 2.0f, 0.12f}, rect.angle,
                              0.018f, kLaneWhite, 0.04f);
            }
        }

        if (road.centerLine) {
            const int dashCount = std::max(1, static_cast<int>(rect.half.x / 2.4f));
            for (int i = 0; i < dashCount; ++i) {
                const float t = dashCount == 1 ? 0.0f
                                               : -rect.half.x + 2.0f +
                                                     i * ((rect.half.x * 2.0f - 4.0f) /
                                                          (dashCount - 1));
                DrawGroundBox(VAdd(rect.center, VScale(forward, t)), {1.8f, 0.11f},
                              rect.angle, 0.018f, Fade(kLaneWhite, 0.9f), 0.041f);
            }
        }
    }

    void DrawRampQuad(float x0, float x1, float h0, float h1, float z0, float z1,
                      Color color) const {
        rlBegin(RL_QUADS);
        rlColor4ub(color.r, color.g, color.b, color.a);
        rlVertex3f(x0, h0 + 0.055f, z0);
        rlVertex3f(x1, h1 + 0.055f, z0);
        rlVertex3f(x1, h1 + 0.055f, z1);
        rlVertex3f(x0, h0 + 0.055f, z1);
        rlEnd();
    }

    void DrawHillSurface() const {
        DrawRampQuad(kHillUpStartX, kHillTopStartX, 0.0f, 1.0f, 26.05f, 33.95f,
                     kAsphaltLight);
        DrawRampQuad(kHillTopStartX, kHillTopEndX, 1.0f, 1.0f, 26.05f, 33.95f,
                     kAsphaltLight);
        DrawRampQuad(kHillTopEndX, kHillDownEndX, 1.0f, 0.0f, 26.05f, 33.95f,
                     kAsphaltLight);

        for (const float z : {26.18f, 33.82f}) {
            DrawRampQuad(kHillUpStartX, kHillTopStartX, 0.03f, 1.03f,
                         z - 0.06f, z + 0.06f, kLaneWhite);
            DrawRampQuad(kHillTopStartX, kHillTopEndX, 1.03f, 1.03f,
                         z - 0.06f, z + 0.06f, kLaneWhite);
            DrawRampQuad(kHillTopEndX, kHillDownEndX, 1.03f, 0.03f,
                         z - 0.06f, z + 0.06f, kLaneWhite);
        }

        const OrientedRect stopZone{{-27.0f, 30.0f}, {2.0f, 3.72f}, 0.0f};
        DrawOutline(stopZone, kSafetyYellow, 0.16f);
    }

    void DrawArrow(Vector2 center, float angle, Color color) const {
        const Vector2 forward = ForwardFromAngle(angle);
        const Vector2 side = {-forward.y, forward.x};
        const Vector2 tail = VSub(center, VScale(forward, 1.4f));
        DrawLineBox(tail, center, 0.22f, color, 0.055f);
        DrawLineBox(center, VSub(VAdd(center, VScale(side, 0.72f)), VScale(forward, 0.7f)),
                    0.22f, color, 0.055f);
        DrawLineBox(center, VSub(VSub(center, VScale(side, 0.72f)), VScale(forward, 0.7f)),
                    0.22f, color, 0.055f);
    }

    Vector2 GuidanceTarget() const {
        switch (courseStep_) {
            case 0: return {-38.0f, 30.0f};
            case 1: return {-25.0f, 30.0f};
            case 2: return {-6.0f, 30.0f};
            case 3: return {12.0f, 25.0f};
            case 4: return {12.0f, 7.5f};
            case 5: return parkingComplete_ ? Vector2{-21.0f, -11.0f}
                                            : parkingZone_.center;
            case 6: return {-25.0f, -22.0f};
            case 7: return {20.0f, -31.0f};
            case 8: return finishZone_.center;
            default: return car_.position;
        }
    }

    void DrawCourseMarkings() const {
        DrawGroundBox({-45.5f, 30.0f}, {0.28f, 7.2f}, 0.0f, 0.02f,
                      kLaneWhite, 0.055f);
        DrawGroundBox({27.0f, 30.0f}, {0.28f, 7.2f}, 0.0f, 0.02f,
                      kLaneWhite, 0.055f);
        DrawGroundBox({12.0f, 7.4f}, {7.0f, 0.28f}, 0.0f, 0.02f,
                      kLaneWhite, 0.055f);

        // Crosswalk at the signalized intersection.
        for (int i = -3; i <= 3; ++i) {
            DrawGroundBox({12.0f + i * 0.76f, 5.2f}, {0.42f, 2.0f}, 0.0f,
                          0.018f, Fade(kLaneWhite, 0.9f), 0.055f);
        }

        // T-shape perpendicular parking: entry corridor outline
        DrawOutline(parkingEntryZone_, Fade(kCourseBlue, 0.6f), 0.10f);
        // Parking slot outline (clear boundary)
        DrawOutline(parkingZone_, kSafetyYellow, 0.15f);
        // T-shape entry guide arrows
        DrawArrow({-12.0f, -16.0f}, -kPi * 0.5f, Fade(kCourseBlue, 0.45f));

        // Confirmation line (yellow, prominent) — same geometry used by scoring.
        DrawGroundBox(parkingConfirmLine_.center,
                      {parkingConfirmLine_.half.x * 2.0f,
                       parkingConfirmLine_.half.y * 2.0f},
                      parkingConfirmLine_.angle, 0.02f, kSafetyYellow, 0.06f);
        // Confirmation text indicator on ground
        DrawGroundBox({-12.0f, -22.7f}, {2.5f, 0.4f}, 0.0f, 0.015f,
                      Fade(kSafetyYellow, 0.5f), 0.058f);

        if (phase_ == ExamPhase::FreeDrive) return;
        const Vector2 target = GuidanceTarget();
        const Vector2 toTarget = VSub(target, car_.position);
        if (Vector2Length(toTarget) > 2.5f) {
            const float angle = std::atan2(toTarget.y, toTarget.x);
            DrawArrow(target, angle, Fade(kCourseBlue, 0.62f));
        }
    }

    void DrawTrafficSignal() const {
        const Vector3 poleBase{7.2f, 2.5f, 8.3f};
        DrawCylinder(poleBase, 0.12f, 0.12f, 5.0f, 10, {80, 87, 84, 255});
        DrawCube({9.4f, 4.65f, 8.3f}, 4.4f, 0.14f, 0.14f, {80, 87, 84, 255});
        DrawCube({11.2f, 4.2f, 8.3f}, 0.72f, 1.65f, 0.58f, {42, 47, 45, 255});
        const Color red = trafficLight_ == 1 ? Color{255, 55, 45, 255}
                                            : Color{83, 28, 25, 255};
        const Color green = trafficLight_ == 2 ? Color{54, 230, 116, 255}
                                              : Color{25, 73, 42, 255};
        DrawSphere({11.2f, 4.66f, 7.98f}, 0.19f, red);
        DrawSphere({11.2f, 3.78f, 7.98f}, 0.19f, green);
    }

    void DrawTree(Vector2 position, float scale) const {
        DrawCylinder({position.x, 1.25f * scale, position.y}, 0.18f * scale,
                     0.25f * scale, 2.5f * scale, 8, {95, 70, 48, 255});
        DrawSphere({position.x, 3.25f * scale, position.y}, 1.25f * scale,
                   {74, 116, 69, 255});
        DrawSphere({position.x + 0.65f * scale, 3.0f * scale, position.y},
                   0.9f * scale, {86, 129, 76, 255});
    }

    void DrawEnvironment() const {
        for (const Obstacle& obstacle : obstacles_) {
            if (obstacle.type == ObstacleType::Building) {
                DrawOrientedCube(obstacle.footprint.center, obstacle.height * 0.5f,
                                 {obstacle.footprint.half.x * 2.0f, obstacle.height,
                                  obstacle.footprint.half.y * 2.0f},
                                 obstacle.footprint.angle, obstacle.color);
                const Color roof =
                    obstacle.footprint.center.y > 35.0f ? Color{67, 126, 99, 255}
                                                       : Color{66, 123, 94, 255};
                DrawOrientedCube(obstacle.footprint.center, obstacle.height + 0.15f,
                                 {obstacle.footprint.half.x * 2.08f, 0.3f,
                                  obstacle.footprint.half.y * 2.08f},
                                 obstacle.footprint.angle, roof);

                // Procedural windows/balconies for slab-type apartments (판상형).
                // Sparse facade rows preserve the Nowon skyline without thousands of
                // WebGL1 draw calls (the world is also rendered into three mirrors).
                if (obstacle.height > 10.0f) {
                    const int floors = static_cast<int>(obstacle.height / 2.8f);
                    const int floorStep = floors >= 20 ? 4 : 3;
                    const float halfX = obstacle.footprint.half.x;
                    const float halfY = obstacle.footprint.half.y;
                    const int windowCount = std::clamp(
                        static_cast<int>(halfX / 3.2f), 3, 5);
                    const float facadeSpan = std::max(0.1f, halfX * 2.0f - 3.0f);
                    const Color windowColor{92, 124, 137, 255};
                    const Color balconyColor{185, 190, 182, 255};

                    rlPushMatrix();
                    rlTranslatef(obstacle.footprint.center.x, 0.0f,
                                 obstacle.footprint.center.y);
                    rlRotatef(obstacle.footprint.angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
                    int facadeRow = 0;
                    for (int floor = 2; floor <= floors; floor += floorStep, ++facadeRow) {
                        const float floorY = floor * 2.8f - 1.0f;
                        for (int w = 0; w < windowCount; ++w) {
                            const float wx = windowCount == 1
                                                 ? 0.0f
                                                 : -halfX + 1.5f +
                                                       w * facadeSpan / (windowCount - 1);
                            for (const float side : {-1.0f, 1.0f}) {
                                DrawCube({wx, floorY, side * (halfY + 0.02f)},
                                         1.1f, 1.45f, 0.06f, windowColor);
                            }
                        }
                        if (facadeRow % 2 == 1) {
                            for (const float side : {-1.0f, 1.0f}) {
                                DrawCube({0.0f, floorY - 0.9f,
                                          side * (halfY + 0.13f)},
                                         halfX * 1.85f, 0.08f, 0.24f,
                                         balconyColor);
                            }
                        }
                    }
                    rlPopMatrix();
                }
            } else if (obstacle.type == ObstacleType::Barrier) {
                DrawOrientedCube(obstacle.footprint.center, obstacle.height * 0.5f,
                                 {obstacle.footprint.half.x * 2.0f, obstacle.height,
                                  obstacle.footprint.half.y * 2.0f},
                                 obstacle.footprint.angle, obstacle.color);
                for (float z = -40.0f; z <= 40.0f; z += 4.0f) {
                    DrawCube({-53.95f, 2.5f, z}, 0.12f, 5.0f, 0.18f,
                             Shade(obstacle.color, 0.72f));
                }
            } else {
                DrawCylinder({obstacle.footprint.center.x, 0.36f,
                              obstacle.footprint.center.y},
                             0.24f, 0.08f, 0.72f, 10, obstacle.color);
                DrawCylinder({obstacle.footprint.center.x, 0.35f,
                              obstacle.footprint.center.y},
                             0.18f, 0.13f, 0.16f, 10, kLaneWhite);
            }
        }

        const std::array<Vector2, 18> trees = {{
            {-47.0f, 42.0f}, {-40.0f, 42.5f}, {-33.0f, 42.0f}, {-28.0f, 42.0f},
            {-51.0f, 18.0f}, {-50.5f, 8.0f}, {-50.5f, -7.0f}, {-50.0f, -22.0f},
            {-42.0f, -42.0f}, {-31.0f, -42.0f}, {-16.0f, -42.0f}, {0.0f, -42.0f},
            {17.0f, -42.0f}, {31.0f, -42.0f}, {48.0f, -40.0f}, {48.0f, -4.0f},
            {47.0f, 14.0f}, {45.0f, 40.0f},
        }};
        for (size_t i = 0; i < trees.size(); ++i) {
            DrawTree(trees[i], 0.85f + static_cast<float>(i % 3) * 0.12f);
        }

        // Course control booth and observation canopy (관제동).
        DrawCube({19.0f, 1.3f, -2.0f}, 4.5f, 2.6f, 3.6f, {225, 229, 221, 255});
        DrawCube({19.0f, 2.75f, -2.0f}, 5.0f, 0.3f, 4.1f, {54, 121, 93, 255});
        DrawCube({16.7f, 1.45f, -2.0f}, 0.08f, 1.0f, 2.5f, {78, 119, 135, 255});
        // Control booth signage strip
        DrawCube({19.0f, 2.4f, -3.82f}, 3.6f, 0.4f, 0.05f, {42, 88, 128, 255});

        DrawTrafficSignal();
    }

    void DrawPlayerCar() const {
        const float base = GroundHeightAt(car_.position);
        const Color body = collisionFlash_ > 0.0f ? Color{184, 49, 44, 255}
                                                  : kVehicleBlue;
        DrawOrientedCube(car_.position, base + 0.52f,
                         {kCarLength, 0.96f, kCarWidth}, car_.heading, body);
        DrawOrientedCube(
            VAdd(car_.position, VScale(ForwardFromAngle(car_.heading), 0.12f)),
            base + 1.12f, {2.2f, 0.55f, 1.48f}, car_.heading,
            {62, 91, 105, 255});

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        for (const float fore : {-1.35f, 1.35f}) {
            for (const float lateral : {-0.83f, 0.83f}) {
                const Vector2 wheel =
                    VAdd(VAdd(car_.position, VScale(forward, fore)), VScale(side, lateral));
                DrawOrientedCube(wheel, base + 0.34f, {0.58f, 0.58f, 0.22f},
                                 car_.heading, {29, 31, 31, 255});
            }
        }
    }

    void DrawWorld() const {
        if (groundReady_) {
            DrawModel(groundModel_, {0.0f, -0.025f, 0.0f}, 1.0f, WHITE);
        } else {
            DrawPlane({0.0f, -0.02f, 0.0f},
                      {kWorldHalfWidth * 2.0f, kWorldHalfHeight * 2.0f}, kGrass);
        }

        if (phase_ == ExamPhase::FreeDrive) {
            DrawGroundBox({0.0f, 0.0f}, {kPadHalfX * 2.0f, kPadHalfY * 2.0f}, 0.0f,
                          0.03f, kAsphalt);
            for (float x = -kPadHalfX + 10.0f; x < kPadHalfX; x += 10.0f) {
                DrawGroundBox({x, 0.0f}, {0.16f, kPadHalfY * 2.0f - 2.0f}, 0.0f, 0.016f,
                              Fade(kLaneWhite, 0.32f), 0.038f);
            }
            for (float y = -kPadHalfY + 10.0f; y < kPadHalfY; y += 10.0f) {
                DrawGroundBox({0.0f, y}, {kPadHalfX * 2.0f - 2.0f, 0.16f}, 0.0f, 0.016f,
                              Fade(kLaneWhite, 0.32f), 0.038f);
            }
            DrawGroundBox({0.0f, -kPadHalfY}, {kPadHalfX * 2.0f, 0.4f}, 0.0f, 0.02f,
                          kSafetyYellow, 0.04f);
            DrawGroundBox({0.0f, kPadHalfY}, {kPadHalfX * 2.0f, 0.4f}, 0.0f, 0.02f,
                          kSafetyYellow, 0.04f);
            DrawGroundBox({-kPadHalfX, 0.0f}, {0.4f, kPadHalfY * 2.0f}, 0.0f, 0.02f,
                          kSafetyYellow, 0.04f);
            DrawGroundBox({kPadHalfX, 0.0f}, {0.4f, kPadHalfY * 2.0f}, 0.0f, 0.02f,
                          kSafetyYellow, 0.04f);
        }
        for (const RoadSurface& road : roads_) DrawRoad(road);
        DrawHillSurface();
        DrawCourseMarkings();
        DrawEnvironment();
    }

    void RenderMirrorView(RenderTexture2D& target, const Camera3D& mirrorCamera) {
        BeginTextureMode(target);
        ClearBackground(kSkyHorizon);
        BeginMode3D(mirrorCamera);
        DrawWorld();
        DrawPlayerCar();
        EndMode3D();
        EndTextureMode();
    }

    void UpdateMirrorTextures() {
        if (!mirrorsReady_) return;
        if ((mirrorFrame_++ % 2) != 0) return;

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        const float base = GroundHeightAt(car_.position);

        Camera3D rear = camera_;
        rear.position = {car_.position.x - forward.x * 0.45f, base + 1.45f,
                         car_.position.y - forward.y * 0.45f};
        rear.target = {rear.position.x - forward.x * 18.0f, base + 1.05f,
                       rear.position.z - forward.y * 18.0f};
        rear.fovy = 43.0f;

        Camera3D left = camera_;
        left.position = {
            car_.position.x + forward.x * 0.2f - side.x * 0.96f,
            base + 1.18f,
            car_.position.y + forward.y * 0.2f - side.y * 0.96f,
        };
        left.target = {
            left.position.x - forward.x * 17.0f - side.x * 4.8f,
            base + 0.92f,
            left.position.z - forward.y * 17.0f - side.y * 4.8f,
        };
        left.fovy = 47.0f;

        Camera3D right = camera_;
        right.position = {
            car_.position.x + forward.x * 0.2f + side.x * 0.96f,
            base + 1.18f,
            car_.position.y + forward.y * 0.2f + side.y * 0.96f,
        };
        right.target = {
            right.position.x - forward.x * 17.0f + side.x * 4.8f,
            base + 0.92f,
            right.position.z - forward.y * 17.0f + side.y * 4.8f,
        };
        right.fovy = 47.0f;

        RenderMirrorView(mirrorRear_, rear);
        RenderMirrorView(mirrorLeft_, left);
        RenderMirrorView(mirrorRight_, right);
    }

    void DrawMirror(Rectangle rect, const RenderTexture2D& texture) const {
        DrawRectangleRounded({rect.x - 7.0f, rect.y - 7.0f, rect.width + 14.0f,
                              rect.height + 14.0f},
                             0.22f, 10, {26, 30, 30, 255});
        const Rectangle source{0.0f, 0.0f,
                               -static_cast<float>(texture.texture.width),
                               -static_cast<float>(texture.texture.height)};
        DrawTexturePro(texture.texture, source, rect, {0.0f, 0.0f}, 0.0f, WHITE);
        DrawRectangleRoundedLinesEx(rect, 0.18f, 10, 2.0f,
                                    Fade({220, 229, 230, 255}, 0.45f));
    }

    void DrawCockpit() const {
        const float width = static_cast<float>(GetScreenWidth());
        const float height = static_cast<float>(GetScreenHeight());
        // Left-hand drive: the interior mirror sits right of the driver's eye line.
        const Rectangle rear{width * 0.50f, height * 0.10f, width * 0.27f,
                             height * 0.072f};
        const Rectangle left{width * 0.02f, height * 0.40f, width * 0.175f,
                             height * 0.125f};
        const Rectangle right{width * 0.805f, height * 0.40f, width * 0.175f,
                              height * 0.125f};

        DrawTriangle({0.0f, 0.0f}, {width * 0.055f, 0.0f},
                     {0.0f, height * 0.64f}, {39, 44, 44, 255});
        DrawTriangle({width, 0.0f}, {width * 0.945f, 0.0f},
                     {width, height * 0.64f}, {39, 44, 44, 255});

        if (mirrorsReady_) {
            DrawMirror(rear, mirrorRear_);
            DrawMirror(left, mirrorLeft_);
            DrawMirror(right, mirrorRight_);
        }

        DrawRectangleGradientV(0, static_cast<int>(height * 0.76f),
                               static_cast<int>(width), static_cast<int>(height * 0.24f),
                               Fade({41, 47, 47, 255}, 0.15f), {24, 28, 29, 255});

        // Dashboard shroud (neutral, no arrow-like wedge).
        DrawTriangle({width * 0.30f, height}, {width * 0.5f, height * 0.77f},
                     {width * 0.70f, height}, {44, 50, 56, 255});
        DrawTriangle({width * 0.36f, height}, {width * 0.5f, height * 0.80f},
                     {width * 0.64f, height}, Fade({86, 96, 106, 255}, 0.45f));

        // Cockpit steering wheel: full rim, three spokes, airbag hub.
        const Vector2 wheelCenter{width * 0.30f, height * 1.02f};
        const float wheelOuter = std::min(width, height) * 0.36f;
        const float rotation = car_.steering * 150.0f;
        DrawRing(wheelCenter, wheelOuter * 0.80f, wheelOuter, 0.0f, 360.0f, 96,
                 {20, 25, 30, 255});
        DrawRing(wheelCenter, wheelOuter * 0.80f, wheelOuter * 0.85f, 0.0f, 360.0f, 96,
                 Fade({214, 226, 232, 255}, 0.16f));
        for (const float angleDegrees : {180.0f, 0.0f, 92.0f}) {
            const float angle = (angleDegrees + rotation) * DEG2RAD;
            const Vector2 direction{std::cos(angle), std::sin(angle)};
            DrawLineEx(VAdd(wheelCenter, VScale(direction, wheelOuter * 0.26f)),
                       VAdd(wheelCenter, VScale(direction, wheelOuter * 0.84f)),
                       std::max(9.0f, wheelOuter * 0.11f), {35, 42, 50, 255});
        }
        DrawCircleV(wheelCenter, wheelOuter * 0.30f, {33, 40, 48, 255});
        DrawCircleV(wheelCenter, wheelOuter * 0.30f - 3.0f, {19, 24, 30, 255});

        const Rectangle cluster{width * 0.185f, height * 0.70f, width * 0.23f,
                                height * 0.075f};
        DrawRectangleRounded(cluster, 0.18f, 10, Fade({10, 14, 15, 255}, 0.92f));
        char speed[16];
        std::snprintf(speed, sizeof(speed), "%02d",
                      static_cast<int>(std::round(DisplaySpeedKph())));
        DrawText(speed, static_cast<int>(cluster.x) + 18,
                 static_cast<int>(cluster.y) + 8, 28, {229, 239, 232, 255});
        DrawText("KM/H", static_cast<int>(cluster.x) + 58,
                 static_cast<int>(cluster.y) + 18, 12, {142, 158, 154, 255});
        DrawText(GearLabel(), static_cast<int>(cluster.x + cluster.width) - 36,
                 static_cast<int>(cluster.y) + 8, 27,
                 gear_ == TransmissionGear::Drive ? kExamGreen : kSafetyYellow);
    }

    Vector2 ToMap(Vector2 point, Rectangle area) const {
        return {
            area.x + (point.x + kWorldHalfWidth) / (kWorldHalfWidth * 2.0f) * area.width,
            area.y + (point.y + kWorldHalfHeight) / (kWorldHalfHeight * 2.0f) * area.height,
        };
    }

    void DrawMiniRect(Rectangle area, const OrientedRect& rect, Color color) const {
        const Vector2 center = ToMap(rect.center, area);
        const float sx = area.width / (kWorldHalfWidth * 2.0f);
        const float sy = area.height / (kWorldHalfHeight * 2.0f);
        const Rectangle projected{
            center.x,
            center.y,
            rect.half.x * 2.0f * sx,
            rect.half.y * 2.0f * sy,
        };
        DrawRectanglePro(projected, {projected.width * 0.5f, projected.height * 0.5f},
                         rect.angle * RAD2DEG, color);
    }

    void DrawMiniMap() const {
        const float width = static_cast<float>(GetScreenWidth());
        const float height = static_cast<float>(GetScreenHeight());
        const float mapWidth = std::clamp(width * 0.17f, 150.0f, 230.0f);
        const float mapHeight = mapWidth * 0.72f;
        const Rectangle panel{width - mapWidth - 18.0f, height * 0.17f,
                              mapWidth, mapHeight};
        DrawRectangleRounded(panel, 0.08f, 8, Fade({19, 27, 27, 255}, 0.78f));
        DrawRectangleRoundedLinesEx(panel, 0.08f, 8, 1.0f,
                                    Fade(WHITE, 0.22f));

        const Rectangle area{panel.x + 8.0f, panel.y + 19.0f,
                             panel.width - 16.0f, panel.height - 27.0f};
        for (const RoadSurface& road : roads_) {
            DrawMiniRect(area, road.footprint, Fade({165, 175, 172, 255}, 0.76f));
        }

        for (size_t i = 1; i < route_.size(); ++i) {
            DrawLineEx(ToMap(route_[i - 1], area), ToMap(route_[i], area), 1.5f,
                       Fade(kCourseBlue, 0.78f));
        }
        DrawMiniRect(area, CarRect(), kSafetyYellow);
        DrawText("DOBONG", static_cast<int>(panel.x) + 9,
                 static_cast<int>(panel.y) + 5, 10, Fade(WHITE, 0.72f));
    }

    void DrawCenterAlert() const {
        if (!emergencyActive_) return;
        const float width = static_cast<float>(GetScreenWidth());
        const float pulse = 0.55f + 0.45f * std::sin(sceneTime_ * 12.0f);
        const Rectangle banner{width * 0.5f - 150.0f, 28.0f, 300.0f, 54.0f};
        DrawRectangleRounded(banner, 0.16f, 10, Fade(kExamRed, 0.78f + pulse * 0.16f));
        const char* label = emergencyStopped_ ? "HAZARD LIGHTS" : "EMERGENCY STOP";
        const int textWidth = MeasureText(label, 22);
        DrawText(label, static_cast<int>(banner.x + banner.width * 0.5f -
                                         textWidth * 0.5f),
                 static_cast<int>(banner.y) + 16, 22, WHITE);
    }

    void Draw() {
        UpdateMirrorTextures();
        BeginDrawing();
        ClearBackground(kSkyHorizon);
        DrawBackdrop();

        BeginMode3D(camera_);
        DrawWorld();
        EndMode3D();

        DrawCockpit();
        DrawMiniMap();
        DrawCenterAlert();

        if (collisionFlash_ > 0.0f) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                          Fade(kExamRed, collisionFlash_ * 0.28f));
        }
        EndDrawing();
    }

    Camera3D camera_{};
    CarState car_{};
    TransmissionGear gear_ = TransmissionGear::Park;
    ExamPhase phase_ = ExamPhase::Briefing;

    std::vector<RoadSurface> roads_{};
    std::vector<Obstacle> obstacles_{};
    std::vector<Vector2> route_{};
    OrientedRect parkingZone_{};
    OrientedRect parkingEntryZone_{};
    OrientedRect parkingConfirmLine_{};
    OrientedRect finishZone_{};
    bool parkingEnteredReverse_ = false;
    RenderTexture2D mirrorRear_{};
    RenderTexture2D mirrorLeft_{};
    RenderTexture2D mirrorRight_{};
    bool mirrorsReady_ = false;
    int mirrorFrame_ = 0;

    Texture2D groundTexture_{};
    Model groundModel_{};
    bool groundReady_ = false;

    int score_ = dobong_exam::kInitialScore;
    int courseStep_ = 0;
    int precheckStep_ = 0;
    float precheckTimer_ = 0.0f;
    float examTimer_ = 0.0f;
    float stepTimer_ = 0.0f;
    float hillStopTimer_ = 0.0f;
    float hillAttemptTimer_ = 0.0f;
    bool hillAttemptStarted_ = false;
    float hillRollbackOrigin_ = -100.0f;
    bool hillRollbackPenalty_ = false;
    bool hillComplete_ = false;
    bool emergencyTriggered_ = false;
    bool emergencyActive_ = false;
    bool emergencyStopped_ = false;
    bool emergencyPenalty_ = false;
    float emergencyTimer_ = 0.0f;
    float stoppedEmergencyTimer_ = 0.0f;
    int trafficLight_ = 0;
    float trafficTimer_ = 0.0f;
    bool trafficArmed_ = false;
    bool trafficStopped_ = false;
    float parkingTimer_ = 0.0f;
    bool parkingComplete_ = false;
    bool parkingOvertimePenalty_ = false;
    bool accelerationStarted_ = false;
    float accelerationMaxKph_ = 0.0f;
    bool speedPenaltyLatch_ = false;
    bool ignitionPenaltyLatch_ = false;
    float nextOvertimePenaltyAt_ = dobong_exam::FirstOvertimePenaltyAt();
    bool finishSignalPenalty_ = false;
    bool resultPassed_ = false;

    bool seatbelt_ = false;
    bool ignition_ = false;
    bool parkingBrake_ = true;
    int headlights_ = 0;
    bool wiper_ = false;
    bool leftSignal_ = false;
    bool rightSignal_ = false;
    bool hazard_ = false;

    std::string eventText_{};
    std::string lastSpoken_{};
    float eventTimer_ = 0.0f;
    float sceneTime_ = 0.0f;
    float collisionFlash_ = 0.0f;
    double lastWallClock_ = -1.0;
};

DobongExamSimulator* gSimulator = nullptr;

void TickFrame() {
    if (gSimulator != nullptr) gSimulator->Tick();
}

}  // namespace

int main() {
    DobongExamSimulator simulator;
    gSimulator = &simulator;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(TickFrame, 0, 1);
#else
    while (!WindowShouldClose()) TickFrame();
#endif

    gSimulator = nullptr;
    return 0;
}
