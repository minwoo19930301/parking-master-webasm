#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

EM_JS(float, WebSteerInput, (), {
    const input = window.__parkingInput || {};
    return (input.left ? -1 : 0) + (input.right ? 1 : 0);
});

EM_JS(float, WebThrottleInput, (), {
    const input = window.__parkingInput || {};
    return input.throttle ? 1 : 0;
});

EM_JS(float, WebBrakeInput, (), {
    const input = window.__parkingInput || {};
    return input.brake ? 1 : 0;
});

EM_JS(int, WebConsumeGearDrivePressed, (), {
    const input = window.__parkingInput || {};
    const pressed = input.gearDrivePressed ? 1 : 0;
    input.gearDrivePressed = false;
    return pressed;
});

EM_JS(int, WebConsumeGearReversePressed, (), {
    const input = window.__parkingInput || {};
    const pressed = input.gearReversePressed ? 1 : 0;
    input.gearReversePressed = false;
    return pressed;
});

EM_JS(int, WebConsumeRetryPressed, (), {
    const input = window.__parkingInput || {};
    const pressed = input.retryPressed ? 1 : 0;
    input.retryPressed = false;
    return pressed;
});

EM_JS(void, WebUpdateOverlay,
      (const char* stageTitle,
       const char* stageHint,
       const char* slotLabel,
       int stageNumber,
       int stageTotal,
       float timeSec,
       int collisions,
       const char* gearLabel,
       const char* guideLabel,
       float speedKph,
       float parkProgress,
       int stageClearing,
       int gameWon),
      {
          const ui = window.__parkingUi;
          if (!ui) return;

          ui.stageTitle.textContent = UTF8ToString(stageTitle);
          ui.stageHint.textContent = UTF8ToString(stageHint);
          ui.slotLabel.textContent = UTF8ToString(slotLabel);
          ui.stageIndex.textContent = `${stageNumber}/${stageTotal}`;
          ui.time.textContent = `${timeSec.toFixed(1)}s`;
          ui.hits.textContent = `${collisions}`;
          ui.gear.textContent = UTF8ToString(gearLabel);
          ui.guide.textContent = UTF8ToString(guideLabel);
          ui.speed.textContent = `${Math.round(speedKph)}`;
          ui.progress.style.transform = `scaleX(${Math.max(0, Math.min(1, parkProgress))})`;
          if (ui.progressValue) {
              ui.progressValue.textContent = `${Math.round(Math.max(0, Math.min(1, parkProgress)) * 100)}%`;
          }
          if (ui.driveButton && ui.reverseButton) {
              const inDrive = UTF8ToString(gearLabel) === "D";
              ui.driveButton.classList.toggle("selected", inDrive);
              ui.reverseButton.classList.toggle("selected", !inDrive);
          }

          let statusText = "Stop fully inside the glowing slot and hold Brake until Parking Lock reaches 100%.";
          if (stageClearing) statusText = "Locked in. Loading the next parking test.";
          if (gameWon) statusText = "All bays cleared. Tap Retry to run again.";
          ui.status.textContent = statusText;
      });
#else
inline float WebSteerInput() { return 0.0f; }
inline float WebThrottleInput() { return 0.0f; }
inline float WebBrakeInput() { return 0.0f; }
inline int WebConsumeGearDrivePressed() { return 0; }
inline int WebConsumeGearReversePressed() { return 0; }
inline int WebConsumeRetryPressed() { return 0; }
inline void WebUpdateOverlay(const char*, const char*, const char*, int, int, float, int, const char*, const char*, float, float, int, int) {}
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kWorldHalfWidth = 24.0f;
constexpr float kWorldHalfHeight = 19.0f;
constexpr float kCarLength = 4.2f;
constexpr float kCarWidth = 2.0f;

enum class ObstacleType {
    ParkedCar,
    Curb,
    Cone,
};

enum class TransmissionGear {
    Drive,
    Reverse,
};

struct OrientedRect {
    Vector2 center{};
    Vector2 half{};
    float angle = 0.0f;
};

struct Obstacle {
    OrientedRect footprint{};
    float height = 1.0f;
    Color color{};
    ObstacleType type = ObstacleType::ParkedCar;
};

struct ParkingZone {
    OrientedRect footprint{};
    std::string label;
};

struct Stage {
    Vector2 spawn{};
    float spawnAngle = 0.0f;
    ParkingZone target{};
    std::string title;
    std::string hint;
};

struct CarState {
    Vector2 position{};
    float heading = 0.0f;
    float speed = 0.0f;
    float steering = 0.0f;
    bool collisionLatch = false;
};

struct Buttons {
    Rectangle left{};
    Rectangle right{};
    Rectangle throttle{};
    Rectangle brake{};
    Rectangle gearDrive{};
    Rectangle gearReverse{};
    Rectangle retry{};
};

struct ButtonLatch {
    bool gearDrive = false;
    bool gearReverse = false;
    bool retry = false;
};

struct InputFrame {
    float steer = 0.0f;
    float throttle = 0.0f;
    float brake = 0.0f;
    bool gearDrivePressed = false;
    bool gearReversePressed = false;
    bool retryPressed = false;
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

float LerpFloat(float a, float b, float t) {
    return a + (b - a) * Clamp01(t);
}

Vector3 LerpVector3(Vector3 a, Vector3 b, float t) {
    return {
        LerpFloat(a.x, b.x, t),
        LerpFloat(a.y, b.y, t),
        LerpFloat(a.z, b.z, t),
    };
}

float NormalizeAngle(float angle) {
    while (angle > kPi) angle -= 2.0f * kPi;
    while (angle < -kPi) angle += 2.0f * kPi;
    return angle;
}

float AngleDiff(float a, float b) {
    return NormalizeAngle(a - b);
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
    const Vector2 right = ForwardFromAngle(rect.angle);
    const Vector2 up = {-right.y, right.x};

    return {
        VAdd(VAdd(rect.center, VScale(right, rect.half.x)), VScale(up, rect.half.y)),
        VAdd(VSub(rect.center, VScale(right, rect.half.x)), VScale(up, rect.half.y)),
        VSub(VSub(rect.center, VScale(right, rect.half.x)), VScale(up, rect.half.y)),
        VSub(VAdd(rect.center, VScale(right, rect.half.x)), VScale(up, rect.half.y)),
    };
}

void ProjectOntoAxis(const std::array<Vector2, 4>& corners, Vector2 axis, float* outMin, float* outMax) {
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

bool OverlapOnAxis(const std::array<Vector2, 4>& aCorners,
                   const std::array<Vector2, 4>& bCorners,
                   Vector2 axis) {
    axis = Vector2Normalize(axis);
    float aMin = 0.0f;
    float aMax = 0.0f;
    float bMin = 0.0f;
    float bMax = 0.0f;

    ProjectOntoAxis(aCorners, axis, &aMin, &aMax);
    ProjectOntoAxis(bCorners, axis, &bMin, &bMax);

    return !(aMax < bMin || bMax < aMin);
}

bool Intersects(const OrientedRect& a, const OrientedRect& b) {
    const auto aCorners = GetCorners(a);
    const auto bCorners = GetCorners(b);

    const std::array<Vector2, 4> axes = {
        VSub(aCorners[1], aCorners[0]),
        VSub(aCorners[3], aCorners[0]),
        VSub(bCorners[1], bCorners[0]),
        VSub(bCorners[3], bCorners[0]),
    };

    for (const Vector2 edge : axes) {
        const Vector2 axis = {-edge.y, edge.x};
        if (!OverlapOnAxis(aCorners, bCorners, axis)) {
            return false;
        }
    }

    return true;
}

Vector3 WorldPoint(Vector2 point, float y = 0.0f) {
    return {point.x, y, point.y};
}

void DrawOrientedCube(Vector2 center, float centerY, Vector3 size, float angle, Color color) {
    rlPushMatrix();
    rlTranslatef(center.x, centerY, center.y);
    rlRotatef(angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawCube({0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, color);
    rlPopMatrix();
}

void DrawOrientedCubeWires(Vector2 center, float centerY, Vector3 size, float angle, Color color) {
    rlPushMatrix();
    rlTranslatef(center.x, centerY, center.y);
    rlRotatef(angle * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawCubeWires({0.0f, 0.0f, 0.0f}, size.x, size.y, size.z, color);
    rlPopMatrix();
}

Rectangle MakeCenteredRect(float centerX, float centerY, float width, float height) {
    return {centerX - width * 0.5f, centerY - height * 0.5f, width, height};
}

class ParkingMasterGame {
  public:
    ParkingMasterGame() {
#if defined(PLATFORM_WEB)
        SetConfigFlags(FLAG_VSYNC_HINT);
#else
        SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
#endif
        InitWindow(1280, 720, "Parking Master WebASM");
        SetTargetFPS(60);
        SetExitKey(KEY_NULL);

        camera_.position = {0.0f, 4.0f, 8.0f};
        camera_.target = {0.0f, 1.0f, 0.0f};
        camera_.up = {0.0f, 1.0f, 0.0f};
        camera_.fovy = 60.0f;
        camera_.projection = CAMERA_PERSPECTIVE;

        BuildCourse();
        RestartRun();
        InitCockpitRenderTextures();
    }

    ~ParkingMasterGame() {
        UnloadCockpitRenderTextures();
        CloseWindow();
    }

    void Tick() {
        SyncCanvasSize();

        const float dt = std::min(GetFrameTime(), 1.0f / 20.0f);
        elapsedSceneTime_ += dt;
        collisionFlash_ = std::max(0.0f, collisionFlash_ - dt);

        const InputFrame input = GatherInput();
        Update(dt, input);
        UpdateCamera(dt);
        PushWebOverlayState();
        Draw(input);
    }

  private:
    void BuildCourse() {
        obstacles_.clear();
        paintedSlots_.clear();
        stages_.clear();

        const Color curbColor = {186, 193, 204, 255};
        const Color parkedBlue = {67, 108, 255, 255};
        const Color parkedCoral = {255, 121, 92, 255};
        const Color parkedSteel = {96, 121, 146, 255};
        const Color coneColor = {255, 164, 71, 255};

        AddCurb({0.0f, -18.4f}, {24.0f, 0.6f}, 0.0f, curbColor);
        AddCurb({0.0f, 18.4f}, {24.0f, 0.6f}, 0.0f, curbColor);
        AddCurb({-23.4f, 0.0f}, {0.6f, 18.4f}, 0.0f, curbColor);
        AddCurb({23.4f, 0.0f}, {0.6f, 18.4f}, 0.0f, curbColor);
        AddCurb({0.0f, -12.8f}, {7.4f, 0.4f}, 0.0f, curbColor);

        const std::array<float, 6> slotRows = {-12.0f, -7.0f, -2.0f, 3.0f, 8.0f, 13.0f};
        for (float row : slotRows) {
            paintedSlots_.push_back({{-15.6f, row}, {3.2f, 1.8f}, 0.0f});
            paintedSlots_.push_back({{15.6f, row}, {3.2f, 1.8f}, 0.0f});
        }
        paintedSlots_.push_back({{0.0f, -15.3f}, {3.4f, 1.7f}, 0.0f});

        AddParkedCar({-15.6f, -12.0f}, 0.0f, parkedSteel);
        AddParkedCar({-15.6f, 3.0f}, 0.0f, parkedBlue);
        AddParkedCar({-15.6f, 13.0f}, 0.0f, parkedCoral);
        AddParkedCar({15.6f, -7.0f}, kPi, parkedBlue);
        AddParkedCar({15.6f, 3.0f}, kPi, parkedSteel);
        AddParkedCar({15.6f, 8.0f}, kPi, parkedCoral);
        AddParkedCar({-6.7f, -15.3f}, 0.0f, parkedBlue);
        AddParkedCar({6.7f, -15.3f}, kPi, parkedSteel);

        AddCone({-4.2f, 15.8f}, coneColor);
        AddCone({4.2f, 15.8f}, coneColor);
        AddCone({-2.0f, 11.4f}, coneColor);
        AddCone({2.0f, 11.4f}, coneColor);

        stages_.push_back({
            {4.6f, 15.1f},
            -0.18f,
            {{{15.6f, 13.0f}, {3.2f, 1.8f}, 0.0f}, "E6"},
            "Stage 1 / Open Bay",
            "Keep the green bay in view, stop fully inside it, then hold Brake until Parking Lock reaches 100%."
        });

        stages_.push_back({
            {-4.0f, 6.5f},
            -2.55f,
            {{{-15.6f, -2.0f}, {3.2f, 1.8f}, 0.0f}, "W3"},
            "Stage 2 / Tight Fit",
            "Aim left into the west bay, straighten the wheel, then hold Brake to lock the stage."
        });

        stages_.push_back({
            {7.4f, -11.7f},
            kPi,
            {{{0.0f, -15.3f}, {3.4f, 1.7f}, 0.0f}, "P1"},
            "Stage 3 / Parallel Spot",
            "Pass the slot, shift to R, back in between the sedans, and stop inside the box."
        });
    }

    void RestartRun() {
        totalCollisions_ = 0;
        runTimer_ = 0.0f;
        currentStageIndex_ = 0;
        stageClearTimer_ = 0.0f;
        parkHoldTimer_ = 0.0f;
        gameWon_ = false;
        gear_ = TransmissionGear::Drive;
        ResetCurrentStage();
    }

    void ResetCurrentStage() {
        const Stage& stage = stages_[currentStageIndex_];
        car_.position = stage.spawn;
        car_.heading = stage.spawnAngle;
        car_.speed = 0.0f;
        car_.steering = 0.0f;
        car_.collisionLatch = false;
        parkHoldTimer_ = 0.0f;
        gear_ = TransmissionGear::Drive;
    }

    void AddParkedCar(Vector2 center, float angle, Color color) {
        obstacles_.push_back({{center, {2.15f, 1.05f}, angle}, 1.5f, color, ObstacleType::ParkedCar});
    }

    void AddCurb(Vector2 center, Vector2 half, float angle, Color color) {
        obstacles_.push_back({{center, half, angle}, 0.35f, color, ObstacleType::Curb});
    }

    void AddCone(Vector2 center, Color color) {
        obstacles_.push_back({{center, {0.35f, 0.35f}, 0.0f}, 0.8f, color, ObstacleType::Cone});
    }

    void InitCockpitRenderTextures() {
        mirrorRear_ = LoadRenderTexture(256, 80);
        mirrorLeft_ = LoadRenderTexture(112, 112);
        mirrorRight_ = LoadRenderTexture(112, 112);
        mirrorsReady_ = mirrorRear_.id != 0 && mirrorLeft_.id != 0 && mirrorRight_.id != 0;

        if (mirrorsReady_) {
            SetTextureFilter(mirrorRear_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(mirrorLeft_.texture, TEXTURE_FILTER_BILINEAR);
            SetTextureFilter(mirrorRight_.texture, TEXTURE_FILTER_BILINEAR);
        }
    }

    void UnloadCockpitRenderTextures() {
        if (!mirrorsReady_) return;
        UnloadRenderTexture(mirrorRear_);
        UnloadRenderTexture(mirrorLeft_);
        UnloadRenderTexture(mirrorRight_);
        mirrorsReady_ = false;
    }

    void SyncCanvasSize() {
#if defined(PLATFORM_WEB)
        double cssWidth = 0.0;
        double cssHeight = 0.0;
        if (emscripten_get_element_css_size("#canvas", &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS) {
            const int width = std::max(1, static_cast<int>(std::round(cssWidth)));
            const int height = std::max(1, static_cast<int>(std::round(cssHeight)));
            if (width != GetScreenWidth() || height != GetScreenHeight()) {
                emscripten_set_canvas_element_size("#canvas", width, height);
                SetWindowSize(width, height);
            }
        }
#endif
    }

    Buttons LayoutButtons() const {
        const float width = static_cast<float>(GetScreenWidth());
        const float height = static_cast<float>(GetScreenHeight());
        const bool portrait = height > width;

        const float pad = portrait ? 14.0f : 18.0f;
        const float button = portrait ? std::min(width * 0.23f, 128.0f) : std::min(height * 0.19f, 114.0f);
        const float medium = portrait ? std::min(width * 0.18f, 108.0f) : std::min(height * 0.14f, 92.0f);

        Buttons buttons{};

        if (portrait) {
            buttons.left = MakeCenteredRect(pad + button * 0.5f, height - pad - button * 0.5f, button, button);
            buttons.right = MakeCenteredRect(pad + button * 1.62f, height - pad - button * 0.5f, button, button);
            buttons.brake = MakeCenteredRect(width - pad - button * 1.62f, height - pad - button * 0.5f, button, button);
            buttons.throttle = MakeCenteredRect(width - pad - button * 0.5f, height - pad - button * 0.5f, button, button);
            buttons.gearDrive = MakeCenteredRect(width * 0.5f - medium * 0.58f, height - pad - button * 0.66f, medium, medium * 0.78f);
            buttons.gearReverse = MakeCenteredRect(width * 0.5f + medium * 0.58f, height - pad - button * 0.66f, medium, medium * 0.78f);
            buttons.retry = MakeCenteredRect(width - pad - medium * 0.5f, pad + medium * 0.5f, medium, medium * 0.78f);
        } else {
            buttons.left = MakeCenteredRect(pad + button * 0.5f, height - pad - button * 0.5f, button, button);
            buttons.right = MakeCenteredRect(pad + button * 1.62f, height - pad - button * 0.5f, button, button);
            buttons.brake = MakeCenteredRect(width - pad - button * 1.62f, height - pad - button * 0.5f, button, button);
            buttons.throttle = MakeCenteredRect(width - pad - button * 0.5f, height - pad - button * 0.5f, button, button);
            buttons.gearDrive = MakeCenteredRect(width * 0.5f - medium * 0.58f, height - pad - button * 0.66f, medium, medium * 0.78f);
            buttons.gearReverse = MakeCenteredRect(width * 0.5f + medium * 0.58f, height - pad - button * 0.66f, medium, medium * 0.78f);
            buttons.retry = MakeCenteredRect(width - pad - medium * 0.5f, pad + medium * 0.5f, medium, medium * 0.78f);
        }

        return buttons;
    }

    bool PointerDownInRect(const Rectangle& bounds) const {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), bounds)) {
            return true;
        }

        const int touchCount = GetTouchPointCount();
        for (int i = 0; i < touchCount; ++i) {
            if (CheckCollisionPointRec(GetTouchPosition(i), bounds)) {
                return true;
            }
        }

        return false;
    }

    InputFrame GatherInput() {
        buttons_ = LayoutButtons();

        InputFrame input{};
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT)) input.steer -= 1.0f;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) input.steer += 1.0f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP)) input.throttle = 1.0f;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_SPACE)) input.brake = 1.0f;
        input.gearDrivePressed = IsKeyPressed(KEY_E);
        input.gearReversePressed = IsKeyPressed(KEY_Q);

#if defined(PLATFORM_WEB)
        input.steer += WebSteerInput();
        input.throttle = std::max(input.throttle, WebThrottleInput());
        input.brake = std::max(input.brake, WebBrakeInput());
        input.gearDrivePressed = input.gearDrivePressed || WebConsumeGearDrivePressed();
        input.gearReversePressed = input.gearReversePressed || WebConsumeGearReversePressed();
        input.retryPressed = IsKeyPressed(KEY_R) || WebConsumeRetryPressed();
#else
        if (PointerDownInRect(buttons_.left)) input.steer -= 1.0f;
        if (PointerDownInRect(buttons_.right)) input.steer += 1.0f;
        if (PointerDownInRect(buttons_.throttle)) input.throttle = 1.0f;
        if (PointerDownInRect(buttons_.brake)) input.brake = 1.0f;

        const bool driveHeld = PointerDownInRect(buttons_.gearDrive);
        const bool reverseHeld = PointerDownInRect(buttons_.gearReverse);
        const bool retryHeld = PointerDownInRect(buttons_.retry);

        input.gearDrivePressed = input.gearDrivePressed || (driveHeld && !buttonLatch_.gearDrive);
        input.gearReversePressed = input.gearReversePressed || (reverseHeld && !buttonLatch_.gearReverse);
        input.retryPressed = IsKeyPressed(KEY_R) || (retryHeld && !buttonLatch_.retry);

        buttonLatch_.gearDrive = driveHeld;
        buttonLatch_.gearReverse = reverseHeld;
        buttonLatch_.retry = retryHeld;
#endif
        input.steer = std::clamp(input.steer, -1.0f, 1.0f);
        return input;
    }

    OrientedRect CarRect() const {
        return {car_.position, {kCarLength * 0.5f, kCarWidth * 0.5f}, car_.heading};
    }

    bool CheckCourseCollision() const {
        const OrientedRect carRect = CarRect();
        for (const Obstacle& obstacle : obstacles_) {
            if (Intersects(carRect, obstacle.footprint)) {
                return true;
            }
        }
        return false;
    }

    bool IsFullyInsideZone(const ParkingZone& zone) const {
        const auto corners = GetCorners(CarRect());
        for (const Vector2& corner : corners) {
            const Vector2 local = RotateVector(VSub(corner, zone.footprint.center), -zone.footprint.angle);
            if (std::fabs(local.x) > zone.footprint.half.x - 0.1f ||
                std::fabs(local.y) > zone.footprint.half.y - 0.1f) {
                return false;
            }
        }
        return true;
    }

    bool IsParkedCorrectly() const {
        const Stage& stage = stages_[currentStageIndex_];
        const float facingErrorA = std::fabs(AngleDiff(car_.heading, stage.target.footprint.angle));
        const float facingErrorB = std::fabs(AngleDiff(car_.heading, stage.target.footprint.angle + kPi));
        const float facingError = std::min(facingErrorA, facingErrorB);

        return std::fabs(car_.speed) < 0.8f &&
               facingError < 0.24f &&
               IsFullyInsideZone(stage.target);
    }

    int FinalStars() const {
        if (totalCollisions_ == 0 && runTimer_ < 75.0f) return 3;
        if (totalCollisions_ <= 2 && runTimer_ < 120.0f) return 2;
        return 1;
    }

    float DisplaySpeedKph() const {
        return std::fabs(car_.speed) * 8.8f;
    }

    float ParkingProgress() const {
        return Clamp01(parkHoldTimer_ / 0.9f);
    }

    const char* GearLabel() const {
        return gear_ == TransmissionGear::Drive ? "D" : "R";
    }

    std::string DirectionPromptToTarget() const {
        const ParkingZone& target = stages_[currentStageIndex_].target;
        const Vector2 toTarget = VSub(target.footprint.center, car_.position);
        const float distance = Vector2Length(toTarget);
        if (distance < 0.01f) {
            return "Target centered. Straighten the wheel and brake.";
        }

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        const Vector2 direction = VScale(toTarget, 1.0f / distance);
        const float forwardDot = Vector2DotProduct(direction, forward);
        const float sideDot = Vector2DotProduct(direction, side);

        std::string turnText = "keep the bay centered";
        if (sideDot > 0.28f) turnText = "turn right toward the glowing bay";
        if (sideDot < -0.28f) turnText = "turn left toward the glowing bay";

        if (forwardDot < -0.2f) {
            return "The bay is behind you. Reposition, then " + turnText + ".";
        }
        return "Guide: " + turnText + ", stop fully inside, then hold Brake to 100%.";
    }

    std::string GuidanceText() const {
        if (gameWon_) return "All stages clear. Tap Retry to start another run.";
        if (stageClearTimer_ > 0.0f) return "Parking Lock complete. Loading the next stage.";

        if (currentStageIndex_ == 2 && gear_ != TransmissionGear::Reverse) {
            return "Stage 3 needs reverse parking. Shift to R, then back into the glowing box.";
        }

        if (ParkingProgress() > 0.0f) {
            return "Stay still and keep Brake held until Parking Lock reaches 100%.";
        }

        return DirectionPromptToTarget();
    }

    void Update(float dt, const InputFrame& input) {
        if (input.gearDrivePressed && std::fabs(car_.speed) < 1.8f) {
            gear_ = TransmissionGear::Drive;
        }
        if (input.gearReversePressed && std::fabs(car_.speed) < 1.8f) {
            gear_ = TransmissionGear::Reverse;
        }

        if (input.retryPressed) {
            if (gameWon_) {
                RestartRun();
            } else {
                ResetCurrentStage();
            }
        }

        if (gameWon_) {
            car_.speed = LerpFloat(car_.speed, 0.0f, dt * 4.0f);
            return;
        }

        if (stageClearTimer_ > 0.0f) {
            stageClearTimer_ -= dt;
            car_.speed = LerpFloat(car_.speed, 0.0f, dt * 5.0f);
            if (stageClearTimer_ <= 0.0f) {
                if (currentStageIndex_ + 1 >= static_cast<int>(stages_.size())) {
                    gameWon_ = true;
                } else {
                    ++currentStageIndex_;
                    ResetCurrentStage();
                }
            }
            return;
        }

        runTimer_ += dt;

        const CarState previous = car_;
        const float targetSteering = input.steer * 0.72f;
        car_.steering = LerpFloat(car_.steering, targetSteering, dt * 8.0f);

        float desiredSpeed = car_.speed;
        const float gearDirection = gear_ == TransmissionGear::Drive ? 1.0f : -1.0f;
        if (input.throttle > 0.0f) {
            const float accelStrength = gear_ == TransmissionGear::Drive ? 14.0f : 11.0f;
            desiredSpeed += gearDirection * accelStrength * dt;
        }
        if (input.brake > 0.0f) {
            const float brakeStrength = 24.0f * dt;
            if (desiredSpeed > brakeStrength) {
                desiredSpeed -= brakeStrength;
            } else if (desiredSpeed < -brakeStrength) {
                desiredSpeed += brakeStrength;
            } else {
                desiredSpeed = 0.0f;
            }
        } else if (input.throttle == 0.0f) {
            desiredSpeed = LerpFloat(desiredSpeed, 0.0f, dt * 1.65f);
        }

        car_.speed = std::clamp(desiredSpeed, -5.8f, 10.5f);

        const float turnRate = std::tan(car_.steering) * car_.speed / 2.85f;
        car_.heading = NormalizeAngle(car_.heading + turnRate * dt);
        car_.position = VAdd(car_.position, VScale(ForwardFromAngle(car_.heading), car_.speed * dt));

        const bool collided = CheckCourseCollision();
        if (collided) {
            car_.position = previous.position;
            car_.heading = previous.heading;
            car_.speed = -previous.speed * 0.18f;
            car_.steering = previous.steering * 0.5f;
            if (!car_.collisionLatch) {
                ++totalCollisions_;
                collisionFlash_ = 0.34f;
            }
            car_.collisionLatch = true;
            parkHoldTimer_ = 0.0f;
        } else {
            car_.collisionLatch = false;
            parkHoldTimer_ = IsParkedCorrectly() ? parkHoldTimer_ + dt : 0.0f;
            if (parkHoldTimer_ >= 0.9f) {
                parkHoldTimer_ = 0.9f;
                stageClearTimer_ = 1.35f;
            }
        }
    }

    void UpdateCamera(float dt) {
        const Vector2 forward2 = ForwardFromAngle(car_.heading);
        const Vector2 side2 = {-forward2.y, forward2.x};
        const float speedRatio = Clamp01(std::fabs(car_.speed) / 11.5f);
        const float headBob = std::sin(elapsedSceneTime_ * (4.0f + speedRatio * 7.0f)) * 0.012f * speedRatio;
        const float lookSide = car_.steering * 1.6f;

        const Vector3 desiredPosition = {
            car_.position.x + forward2.x * 0.42f + side2.x * 0.28f,
            1.28f + headBob,
            car_.position.y + forward2.y * 0.42f + side2.y * 0.28f,
        };
        const Vector3 desiredTarget = {
            car_.position.x + forward2.x * 12.5f + side2.x * lookSide,
            0.72f + headBob * 0.2f,
            car_.position.y + forward2.y * 12.5f + side2.y * lookSide,
        };
        camera_.fovy = 78.0f;

        const float blend = 1.0f - std::exp(-dt * 7.0f);
        camera_.position = LerpVector3(camera_.position, desiredPosition, blend);
        camera_.target = LerpVector3(camera_.target, desiredTarget, blend);
    }

    void PushWebOverlayState() const {
        const Stage& stage = stages_[currentStageIndex_];
        const std::string guide = GuidanceText();
        WebUpdateOverlay(stage.title.c_str(),
                         stage.hint.c_str(),
                         stage.target.label.c_str(),
                         currentStageIndex_ + 1,
                         static_cast<int>(stages_.size()),
                         runTimer_,
                         totalCollisions_,
                         GearLabel(),
                         guide.c_str(),
                         DisplaySpeedKph(),
                         ParkingProgress(),
                         stageClearTimer_ > 0.0f ? 1 : 0,
                         gameWon_ ? 1 : 0);
    }

    void DrawGroundBox(Vector2 center, Vector2 size, float angle, float thickness, Color color) const {
        DrawOrientedCube(center, thickness * 0.5f, {size.x, thickness, size.y}, angle, color);
    }

    void DrawLineBox(Vector2 start, Vector2 end, float width, Color color) const {
        const Vector2 delta = VSub(end, start);
        const float length = Vector2Length(delta);
        const Vector2 center = VScale(VAdd(start, end), 0.5f);
        const float angle = std::atan2(delta.y, delta.x);
        DrawGroundBox(center, {length, width}, angle, 0.03f, color);
    }

    void DrawParkingOutline(const OrientedRect& rect, Color color) const {
        const auto corners = GetCorners(rect);
        DrawLineBox(corners[0], corners[1], 0.12f, color);
        DrawLineBox(corners[1], corners[2], 0.12f, color);
        DrawLineBox(corners[2], corners[3], 0.12f, color);
        DrawLineBox(corners[3], corners[0], 0.12f, color);
    }

    void DrawArrowMarker(Vector2 center, float angle, Color color) const {
        DrawGroundBox(center, {0.34f, 1.6f}, angle + kPi * 0.5f, 0.02f, color);
        DrawGroundBox(VAdd(center, RotateVector({0.0f, 0.72f}, angle)), {0.28f, 0.9f}, angle + 0.68f, 0.02f, color);
        DrawGroundBox(VAdd(center, RotateVector({0.0f, 0.72f}, angle)), {0.28f, 0.9f}, angle - 0.68f, 0.02f, color);
    }

    void DrawBackdrop() const {
        const int width = GetScreenWidth();
        const int height = GetScreenHeight();

        DrawRectangleGradientV(0, 0, width, height, {64, 72, 92, 255}, {7, 10, 15, 255});
        DrawRectangleGradientV(0, height / 2, width, height / 2, {18, 23, 31, 0}, {4, 6, 10, 220});
        DrawCircleGradient(static_cast<int>(width * 0.72f), static_cast<int>(height * 0.18f), height * 0.12f, {255, 218, 164, 44}, BLANK);
        DrawCircleGradient(static_cast<int>(width * 0.22f), static_cast<int>(height * 0.24f), height * 0.22f, {88, 117, 168, 34}, BLANK);

        const int skylineBase = static_cast<int>(height * 0.56f);
        for (int i = 0; i < 10; ++i) {
            const int buildingWidth = 80 + (i % 3) * 28;
            const int buildingHeight = 80 + (i % 5) * 34;
            const int x = i * (width / 9) - 14;
            DrawRectangle(x, skylineBase - buildingHeight, buildingWidth, buildingHeight, Fade({17, 22, 33, 255}, 0.88f));
        }

        DrawRectangle(0, skylineBase, width, height - skylineBase, Fade({8, 11, 18, 255}, 0.34f));
    }

    void DrawEnvironmentDetails() const {
        for (int i = 0; i < 7; ++i) {
            const float z = -15.0f + i * 5.0f;
            DrawCylinder({-10.7f, 2.4f, z}, 0.12f, 0.12f, 4.8f, 8, {57, 67, 88, 255});
            DrawCylinder({10.7f, 2.4f, z}, 0.12f, 0.12f, 4.8f, 8, {57, 67, 88, 255});
            DrawSphere({-10.7f, 5.1f, z}, 0.3f, Fade({255, 229, 162, 255}, 0.82f));
            DrawSphere({10.7f, 5.1f, z}, 0.3f, Fade({255, 229, 162, 255}, 0.82f));
        }

        for (int i = 0; i < 6; ++i) {
            const float x = i < 3 ? -19.5f : 19.5f;
            const float z = -14.0f + (i % 3) * 13.0f;
            const float height = 5.5f + (i % 3) * 2.0f;
            const Color tower = i % 2 == 0 ? Color{45, 54, 76, 255} : Color{30, 38, 57, 255};
            DrawCube({x, height * 0.5f, z}, 4.8f, height, 5.0f, tower);
            DrawCubeWires({x, height * 0.5f, z}, 4.8f, height, 5.0f, Fade({154, 173, 207, 255}, 0.12f));
        }

        const std::array<Vector2, 4> planters = {{{-8.7f, -16.0f}, {8.7f, -16.0f}, {-8.7f, 16.0f}, {8.7f, 16.0f}}};
        for (const Vector2& planter : planters) {
            DrawOrientedCube(planter, 0.45f, {1.8f, 0.9f, 1.8f}, 0.0f, {108, 89, 72, 255});
            DrawSphere(WorldPoint(planter, 1.3f), 0.82f, {79, 154, 112, 255});
        }
    }

    void DrawCourse() const {
        DrawPlane({0.0f, 0.0f, 0.0f}, {50.0f, 40.0f}, {44, 48, 57, 255});
        DrawPlane({0.0f, -0.02f, 0.0f}, {56.0f, 46.0f}, {21, 24, 30, 255});

        const Color laneColor = {56, 60, 70, 255};
        DrawGroundBox({0.0f, 0.0f}, {11.0f, 31.5f}, 0.0f, 0.01f, laneColor);
        DrawGroundBox({0.0f, -15.3f}, {14.0f, 3.7f}, 0.0f, 0.01f, laneColor);
        DrawGroundBox({0.0f, 0.0f}, {11.9f, 32.4f}, 0.0f, 0.005f, Fade({255, 215, 149, 255}, 0.04f));

        for (float y = -13.8f; y <= 14.0f; y += 3.8f) {
            DrawGroundBox({0.0f, y}, {0.36f, 1.8f}, 0.0f, 0.02f, {223, 212, 182, 255});
        }
        DrawGroundBox({-5.5f, 0.0f}, {0.15f, 31.4f}, 0.0f, 0.02f, Fade({255, 255, 255, 255}, 0.48f));
        DrawGroundBox({5.5f, 0.0f}, {0.15f, 31.4f}, 0.0f, 0.02f, Fade({255, 255, 255, 255}, 0.48f));
        DrawArrowMarker({0.0f, 10.6f}, 0.0f, {227, 211, 168, 255});
        DrawArrowMarker({0.0f, -5.0f}, kPi, {227, 211, 168, 255});

        for (const OrientedRect& slot : paintedSlots_) {
            DrawParkingOutline(slot, {233, 239, 248, 255});
        }

        const ParkingZone& target = stages_[currentStageIndex_].target;
        DrawGroundBox(target.footprint.center, {target.footprint.half.x * 2.0f, target.footprint.half.y * 2.0f}, target.footprint.angle, 0.03f, Fade({91, 242, 197, 255}, 0.22f));
        DrawParkingOutline(target.footprint, {91, 242, 197, 255});

        const float pulse = 0.85f + std::sin(elapsedSceneTime_ * 3.2f) * 0.12f;
        DrawSphere(WorldPoint(target.footprint.center, 1.9f + std::sin(elapsedSceneTime_ * 2.5f) * 0.18f), 0.3f * pulse, {91, 242, 197, 220});
        DrawCylinder(WorldPoint(target.footprint.center, 0.8f), 0.32f, 0.32f, 1.8f, 16, Fade({91, 242, 197, 255}, 0.14f));

        for (const Obstacle& obstacle : obstacles_) {
            switch (obstacle.type) {
                case ObstacleType::ParkedCar:
                    DrawOrientedCube(
                        obstacle.footprint.center,
                        obstacle.height * 0.5f,
                        {obstacle.footprint.half.x * 2.0f, obstacle.height, obstacle.footprint.half.y * 2.0f},
                        obstacle.footprint.angle,
                        obstacle.color
                    );
                    DrawOrientedCubeWires(
                        obstacle.footprint.center,
                        obstacle.height * 0.5f,
                        {obstacle.footprint.half.x * 2.0f, obstacle.height, obstacle.footprint.half.y * 2.0f},
                        obstacle.footprint.angle,
                        {16, 20, 30, 200}
                    );
                    break;
                case ObstacleType::Curb:
                    DrawOrientedCube(
                        obstacle.footprint.center,
                        obstacle.height * 0.5f,
                        {obstacle.footprint.half.x * 2.0f, obstacle.height, obstacle.footprint.half.y * 2.0f},
                        obstacle.footprint.angle,
                        obstacle.color
                    );
                    break;
                case ObstacleType::Cone:
                    DrawCylinder(WorldPoint(obstacle.footprint.center, obstacle.height * 0.5f), 0.28f, 0.08f, obstacle.height, 10, obstacle.color);
                    DrawCylinderWires(WorldPoint(obstacle.footprint.center, obstacle.height * 0.5f), 0.28f, 0.08f, obstacle.height, 10, {255, 245, 220, 180});
                    break;
            }
        }

        DrawEnvironmentDetails();
    }

    void DrawPlayerCar() const {
        const Color body = collisionFlash_ > 0.0f ? Color{255, 107, 107, 255} : Color{255, 215, 85, 255};
        DrawGroundBox(car_.position, {4.9f, 2.45f}, car_.heading, 0.02f, Fade({6, 9, 15, 255}, 0.35f));

        DrawOrientedCube(car_.position, 0.68f, {kCarLength, 1.1f, kCarWidth}, car_.heading, body);

        DrawOrientedCube(
            VAdd(car_.position, VScale(ForwardFromAngle(car_.heading), 0.12f)),
            1.25f,
            {2.1f, 0.6f, 1.55f},
            car_.heading,
            {37, 53, 88, 255}
        );
        DrawOrientedCube(car_.position, 1.04f, {3.2f, 0.05f, 0.08f}, car_.heading, {240, 108, 66, 255});
        DrawOrientedCube(VAdd(car_.position, RotateVector({0.0f, 0.64f}, car_.heading)), 0.82f, {0.12f, 0.08f, 0.38f}, car_.heading, {255, 246, 204, 255});
        DrawOrientedCube(VAdd(car_.position, RotateVector({0.0f, -0.64f}, car_.heading)), 0.82f, {0.12f, 0.08f, 0.38f}, car_.heading, {255, 246, 204, 255});

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};
        const std::array<Vector2, 4> wheelCenters = {
            VAdd(VAdd(car_.position, VScale(forward, 1.25f)), VScale(side, 0.9f)),
            VAdd(VAdd(car_.position, VScale(forward, 1.25f)), VScale(side, -0.9f)),
            VAdd(VAdd(car_.position, VScale(forward, -1.25f)), VScale(side, 0.9f)),
            VAdd(VAdd(car_.position, VScale(forward, -1.25f)), VScale(side, -0.9f)),
        };

        for (const Vector2& wheel : wheelCenters) {
            DrawOrientedCube(wheel, 0.28f, {0.6f, 0.45f, 0.22f}, car_.heading, {18, 20, 24, 255});
        }

        const auto drawLight = [&](Vector2 local, float height, Vector3 size, Color color) {
            DrawOrientedCube(VAdd(car_.position, RotateVector(local, car_.heading)), height, size, car_.heading, color);
        };
        drawLight({1.95f, 0.62f}, 0.78f, {0.14f, 0.16f, 0.3f}, {255, 244, 196, 255});
        drawLight({1.95f, -0.62f}, 0.78f, {0.14f, 0.16f, 0.3f}, {255, 244, 196, 255});
        drawLight({-1.95f, 0.64f}, 0.78f, {0.14f, 0.16f, 0.28f}, {255, 99, 89, 255});
        drawLight({-1.95f, -0.64f}, 0.78f, {0.14f, 0.16f, 0.28f}, {255, 99, 89, 255});
    }

    Vector2 ToMiniMap(Vector2 worldPoint, Rectangle area) const {
        const float x = (worldPoint.x + kWorldHalfWidth) / (kWorldHalfWidth * 2.0f);
        const float y = (worldPoint.y + kWorldHalfHeight) / (kWorldHalfHeight * 2.0f);
        return {
            area.x + x * area.width,
            area.y + y * area.height,
        };
    }

    void DrawMiniRect(Rectangle area, const OrientedRect& rect, Color color) const {
        const Vector2 center = ToMiniMap(rect.center, area);
        const float scaleX = area.width / (kWorldHalfWidth * 2.0f);
        const float scaleY = area.height / (kWorldHalfHeight * 2.0f);
        Rectangle projected{
            center.x,
            center.y,
            rect.half.x * 2.0f * scaleX,
            rect.half.y * 2.0f * scaleY,
        };
        DrawRectanglePro(projected, {projected.width * 0.5f, projected.height * 0.5f}, rect.angle * RAD2DEG, color);
    }

    void DrawMiniMap() const {
        const bool portrait = GetScreenHeight() > GetScreenWidth();
        const float size = std::min(GetScreenWidth(), GetScreenHeight()) * (portrait ? 0.19f : 0.22f);
        Rectangle area{
            static_cast<float>(GetScreenWidth()) - size - 20.0f,
            portrait ? 162.0f : 116.0f,
            size,
            size,
        };

        DrawRectangleRounded(area, 0.08f, 10, Fade({10, 14, 28, 255}, 0.82f));
        DrawRectangleRoundedLinesEx(area, 0.08f, 10, 2.0f, Fade({255, 255, 255, 255}, 0.12f));

        for (const OrientedRect& slot : paintedSlots_) {
            DrawMiniRect(area, slot, Fade({210, 220, 233, 255}, 0.16f));
        }

        for (const Obstacle& obstacle : obstacles_) {
            const Color color = obstacle.type == ObstacleType::Cone ? Color{255, 164, 71, 255} : obstacle.color;
            DrawMiniRect(area, obstacle.footprint, color);
        }

        DrawMiniRect(area, stages_[currentStageIndex_].target.footprint, Fade({86, 240, 164, 255}, 0.55f));
        DrawMiniRect(area, CarRect(), {255, 215, 85, 255});

        DrawText("RADAR", static_cast<int>(area.x) + 14, static_cast<int>(area.y) + 12, 18, {237, 242, 252, 255});
    }

    void DrawPanel(Rectangle rect, Color fill, Color stroke) const {
        DrawRectangleRounded(rect, 0.2f, 12, fill);
        DrawRectangleRoundedLinesEx(rect, 0.2f, 12, 2.0f, stroke);
    }

    void DrawHud() const {
        const bool portrait = GetScreenHeight() > GetScreenWidth();
        const Rectangle hudPanel{
            20.0f,
            18.0f,
            portrait ? std::min(320.0f, GetScreenWidth() * 0.7f) : std::min(460.0f, GetScreenWidth() * 0.56f),
            portrait ? 124.0f : 134.0f
        };
        DrawPanel(hudPanel, Fade({10, 14, 28, 255}, 0.78f), Fade({255, 255, 255, 255}, 0.1f));

        const Stage& stage = stages_[currentStageIndex_];

        DrawText(stage.title.c_str(), static_cast<int>(hudPanel.x) + 18, static_cast<int>(hudPanel.y) + 16, 28, {247, 249, 255, 255});
        DrawText(stage.hint.c_str(), static_cast<int>(hudPanel.x) + 18, static_cast<int>(hudPanel.y) + 50, portrait ? 16 : 18, {196, 205, 220, 255});

        char statsLine[128];
        std::snprintf(statsLine, sizeof(statsLine), "Slot %s  |  Time %.1fs  |  Hits %d  |  1st Person",
                      stage.target.label.c_str(),
                      runTimer_,
                      totalCollisions_);
        DrawText(statsLine, static_cast<int>(hudPanel.x) + 18, static_cast<int>(hudPanel.y) + (portrait ? 86 : 92), portrait ? 16 : 18, {86, 240, 164, 255});

        if (portrait) {
            DrawText("Landscape feels better for this run.", 20, GetScreenHeight() - 170, 18, Fade(WHITE, 0.8f));
        }

        if (parkHoldTimer_ > 0.0f && stageClearTimer_ <= 0.0f) {
            const float barWidth = hudPanel.width - 36.0f;
            const float barY = portrait ? hudPanel.y + 108.0f : hudPanel.y + 118.0f;
            DrawRectangleRounded({hudPanel.x + 18.0f, barY, barWidth, 8.0f}, 0.9f, 10, Fade(WHITE, 0.1f));
            DrawRectangleRounded({hudPanel.x + 18.0f, barY, barWidth * (parkHoldTimer_ / 0.9f), 8.0f}, 0.9f, 10, {86, 240, 164, 255});
        }
    }

    void DrawButton(const Rectangle& rect, const char* label, bool active, Color color) const {
        const Color fill = active ? color : Fade(color, 0.38f);
        const Color stroke = active ? Fade(WHITE, 0.85f) : Fade(WHITE, 0.28f);
        DrawRectangleRounded({rect.x + 4.0f, rect.y + 6.0f, rect.width, rect.height}, 0.2f, 12, Fade(BLACK, 0.16f));
        DrawPanel(rect, fill, stroke);

        const int fontSize = static_cast<int>(std::min(rect.width, rect.height) * 0.26f);
        const int textWidth = MeasureText(label, fontSize);
        DrawText(label,
                 static_cast<int>(rect.x + rect.width * 0.5f - textWidth * 0.5f),
                 static_cast<int>(rect.y + rect.height * 0.5f - fontSize * 0.45f),
                 fontSize,
                 WHITE);
    }

    void DrawControls(const InputFrame& input) const {
        DrawButton(buttons_.left, "LEFT", input.steer < -0.2f, {47, 102, 212, 255});
        DrawButton(buttons_.right, "RIGHT", input.steer > 0.2f, {47, 102, 212, 255});
        DrawButton(buttons_.throttle, "GAS", input.throttle > 0.0f, {33, 183, 109, 255});
        DrawButton(buttons_.brake, "BRK", input.brake > 0.0f, {241, 145, 58, 255});
        DrawButton(buttons_.gearDrive, "D", gear_ == TransmissionGear::Drive, {70, 188, 156, 255});
        DrawButton(buttons_.gearReverse, "R", gear_ == TransmissionGear::Reverse, {212, 120, 84, 255});
        DrawButton(buttons_.retry, "RETRY", false, {229, 86, 113, 255});
    }

    void RenderMirrorView(RenderTexture2D& target, const Camera3D& mirrorCamera) {
        BeginTextureMode(target);
        ClearBackground({58, 64, 78, 255});
        BeginMode3D(mirrorCamera);
        DrawCourse();
        EndMode3D();
        EndTextureMode();
    }

    void UpdateMirrorTextures() {
        if (!mirrorsReady_) return;

        const Vector2 forward = ForwardFromAngle(car_.heading);
        const Vector2 side = {-forward.y, forward.x};

        Camera3D rear = camera_;
        rear.position = {
            car_.position.x - forward.x * 0.6f,
            1.35f,
            car_.position.y - forward.y * 0.6f,
        };
        rear.target = {
            rear.position.x - forward.x * 14.0f,
            1.0f,
            rear.position.z - forward.y * 14.0f,
        };
        rear.fovy = 48.0f;

        Camera3D left = camera_;
        left.position = {
            car_.position.x + forward.x * 0.15f - side.x * 0.88f,
            1.18f,
            car_.position.y + forward.y * 0.15f - side.y * 0.88f,
        };
        left.target = {
            left.position.x - side.x * 12.0f + forward.x * 2.4f,
            1.02f,
            left.position.z - side.y * 12.0f + forward.y * 2.4f,
        };
        left.fovy = 58.0f;

        Camera3D right = camera_;
        right.position = {
            car_.position.x + forward.x * 0.15f + side.x * 0.88f,
            1.18f,
            car_.position.y + forward.y * 0.15f + side.y * 0.88f,
        };
        right.target = {
            right.position.x + side.x * 12.0f + forward.x * 2.4f,
            1.02f,
            right.position.z + side.y * 12.0f + forward.y * 2.4f,
        };
        right.fovy = 58.0f;

        RenderMirrorView(mirrorRear_, rear);
        RenderMirrorView(mirrorLeft_, left);
        RenderMirrorView(mirrorRight_, right);
    }

    void DrawMirrorSurface(Rectangle rect, const RenderTexture2D& texture, float tiltDegrees) const {
        DrawRectangleRounded(rect, 0.24f, 10, Fade({8, 10, 14, 255}, 0.96f));
        DrawRectangleRoundedLinesEx(rect, 0.24f, 10, 2.0f, Fade({244, 231, 208, 255}, 0.12f));

        Rectangle inner{
            rect.x + 6.0f,
            rect.y + 6.0f,
            rect.width - 12.0f,
            rect.height - 12.0f,
        };
        Rectangle source{
            0.0f,
            0.0f,
            static_cast<float>(texture.texture.width),
            -static_cast<float>(texture.texture.height),
        };
        DrawTexturePro(texture.texture, source, inner, {0.0f, 0.0f}, tiltDegrees, Fade(WHITE, 0.9f));
        DrawRectangleRounded(inner, 0.18f, 8, Fade({124, 152, 178, 255}, 0.08f));
    }

    void DrawCockpitOverlay() const {
        const float width = static_cast<float>(GetScreenWidth());
        const float height = static_cast<float>(GetScreenHeight());
        const float dashTop = height * 0.82f;
        const float hoodTop = height * 0.79f;
        const float hoodWidth = width * 0.36f;
        const float hoodLift = 26.0f + Clamp01(std::fabs(car_.speed) / 11.5f) * 12.0f;
        const float speedRatio = Clamp01(DisplaySpeedKph() / 80.0f);
        const Rectangle rearMirror{width * 0.34f, height * 0.19f, width * 0.32f, height * 0.06f};
        const Rectangle leftMirror{width * 0.018f, height * 0.44f, width * 0.16f, height * 0.09f};
        const Rectangle rightMirror{width * 0.822f, height * 0.44f, width * 0.16f, height * 0.09f};
        const Rectangle cluster{width * 0.3f, height * 0.73f, width * 0.4f, height * 0.08f};

        DrawRectangleGradientV(0, static_cast<int>(height * 0.74f), static_cast<int>(width), static_cast<int>(height * 0.26f), Fade({5, 8, 12, 255}, 0), Fade({5, 8, 12, 255}, 0.98f));
        DrawTriangle({0.0f, 0.0f}, {width * 0.09f, 0.0f}, {0.0f, height * 0.48f}, Fade({4, 6, 10, 255}, 0.92f));
        DrawTriangle({width, 0.0f}, {width * 0.91f, 0.0f}, {width, height * 0.48f}, Fade({4, 6, 10, 255}, 0.92f));
        DrawRectangleGradientV(static_cast<int>(width * 0.2f), static_cast<int>(dashTop), static_cast<int>(width * 0.6f), static_cast<int>(height - dashTop), Fade({14, 17, 23, 255}, 0.06f), Fade({9, 11, 15, 255}, 0.98f));
        DrawLineEx({width * 0.2f, dashTop}, {width * 0.8f, dashTop}, 2.0f, Fade({255, 255, 255, 255}, 0.08f));

        DrawTriangle({width * 0.5f - hoodWidth * 0.5f, height}, {width * 0.5f, hoodTop + hoodLift}, {width * 0.5f + hoodWidth * 0.5f, height}, Fade({176, 145, 92, 255}, 0.92f));
        DrawTriangle({width * 0.5f - hoodWidth * 0.37f, height}, {width * 0.5f, hoodTop + hoodLift + 12.0f}, {width * 0.5f + hoodWidth * 0.37f, height}, Fade({216, 186, 116, 255}, 0.22f));

        if (mirrorsReady_) {
            DrawMirrorSurface(rearMirror, mirrorRear_, 0.0f);
            DrawMirrorSurface(leftMirror, mirrorLeft_, -2.0f);
            DrawMirrorSurface(rightMirror, mirrorRight_, 2.0f);
        }

        DrawPanel(cluster, Fade({8, 10, 14, 255}, 0.92f), Fade({244, 231, 208, 255}, 0.08f));
        char speedText[24];
        std::snprintf(speedText, sizeof(speedText), "%02d", static_cast<int>(std::round(DisplaySpeedKph())));
        DrawText(speedText, static_cast<int>(cluster.x) + 22, static_cast<int>(cluster.y) + 14, 34, {243, 239, 231, 255});
        DrawText("KM/H", static_cast<int>(cluster.x) + 26, static_cast<int>(cluster.y) + 50, 14, {157, 166, 181, 255});
        DrawText(GearLabel(), static_cast<int>(cluster.x + cluster.width) - 50, static_cast<int>(cluster.y) + 16, 30, gear_ == TransmissionGear::Drive ? Color{118, 215, 186, 255} : Color{240, 188, 120, 255});
        DrawText("GEAR", static_cast<int>(cluster.x + cluster.width) - 68, static_cast<int>(cluster.y) + 50, 14, {157, 166, 181, 255});
        DrawRectangleRounded({cluster.x + 96.0f, cluster.y + cluster.height - 18.0f, cluster.width - 120.0f, 6.0f}, 0.9f, 10, Fade(WHITE, 0.08f));
        DrawRectangleRounded({cluster.x + 96.0f, cluster.y + cluster.height - 18.0f, (cluster.width - 120.0f) * ParkingProgress(), 6.0f}, 0.9f, 10, {118, 215, 186, 255});

        const Vector2 wheelCenter{width * 0.5f, height * 0.85f};
        const float wheelOuter = std::min(width, height) * 0.16f;
        const float wheelInner = wheelOuter * 0.72f;
        const float wheelRotation = -car_.steering * 52.0f;
        DrawRing(wheelCenter, wheelInner, wheelOuter, 198.0f, 342.0f, 52, Fade({22, 24, 30, 255}, 0.98f));
        DrawRing(wheelCenter, wheelInner + 6.0f, wheelOuter - 10.0f, 198.0f, 342.0f, 52, Fade({189, 154, 96, 255}, 0.78f));

        const auto drawSpoke = [&](float angleDeg, float start, float end, float thickness) {
            const float angle = (angleDeg + wheelRotation) * DEG2RAD;
            const Vector2 dir{std::cos(angle), std::sin(angle)};
            DrawLineEx(
                VAdd(wheelCenter, VScale(dir, start)),
                VAdd(wheelCenter, VScale(dir, end)),
                thickness,
                Fade({28, 30, 37, 255}, 0.96f)
            );
        };
        drawSpoke(270.0f, 18.0f, wheelInner + 18.0f, 18.0f);
        drawSpoke(215.0f, 18.0f, wheelInner - 2.0f, 14.0f);
        drawSpoke(325.0f, 18.0f, wheelInner - 2.0f, 14.0f);
        DrawCircleV(wheelCenter, 28.0f, Fade({20, 22, 28, 255}, 0.98f));
        DrawCircleLines(static_cast<int>(wheelCenter.x), static_cast<int>(wheelCenter.y), 28.0f, Fade({255, 255, 255, 255}, 0.1f));
        DrawRectangleRounded({width * 0.28f, height * 0.87f, width * 0.44f, height * 0.024f}, 0.7f, 8, Fade({11, 14, 18, 255}, 0.96f));
        DrawRectangleRounded({width * 0.28f, height * 0.87f, width * 0.44f * speedRatio, height * 0.024f}, 0.7f, 8, Fade({240, 188, 120, 255}, 0.72f));
    }

    void DrawCenterBanner() const {
        if (stageClearTimer_ > 0.0f) {
            const Rectangle panel{
                GetScreenWidth() * 0.5f - 200.0f,
                24.0f,
                400.0f,
                72.0f,
            };
            DrawPanel(panel, Fade({11, 46, 29, 255}, 0.88f), Fade({86, 240, 164, 255}, 0.35f));
            DrawText("Parked. Loading next challenge...", static_cast<int>(panel.x) + 28, static_cast<int>(panel.y) + 24, 24, WHITE);
        }

        if (gameWon_) {
            const Rectangle overlay{
                GetScreenWidth() * 0.5f - std::min(260.0f, GetScreenWidth() * 0.4f),
                GetScreenHeight() * 0.5f - 120.0f,
                std::min(520.0f, GetScreenWidth() * 0.8f),
                240.0f,
            };
            DrawPanel(overlay, Fade({8, 13, 26, 255}, 0.92f), Fade({255, 255, 255, 255}, 0.12f));
            DrawText("All bays cleared", static_cast<int>(overlay.x) + 28, static_cast<int>(overlay.y) + 28, 34, WHITE);

            char summary[128];
            std::snprintf(summary, sizeof(summary), "Final time %.1fs  |  collisions %d  |  stars %d/3", runTimer_, totalCollisions_, FinalStars());
            DrawText(summary, static_cast<int>(overlay.x) + 28, static_cast<int>(overlay.y) + 88, 22, {86, 240, 164, 255});
            DrawText("Tap RETRY or press R to restart the full run.", static_cast<int>(overlay.x) + 28, static_cast<int>(overlay.y) + 146, 20, {196, 205, 220, 255});
        }
    }

    void Draw(const InputFrame& input) {
#if defined(PLATFORM_WEB)
        (void)input;
#endif
        UpdateMirrorTextures();
        BeginDrawing();
        ClearBackground({18, 24, 42, 255});
        DrawBackdrop();

        BeginMode3D(camera_);
        DrawCourse();
        EndMode3D();

        DrawCockpitOverlay();
        DrawCenterBanner();

#if !defined(PLATFORM_WEB)
        DrawHud();
        DrawControls(input);
#endif

        if (collisionFlash_ > 0.0f) {
            DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade({255, 88, 88, 255}, collisionFlash_ * 0.22f));
        }
        EndDrawing();
    }

    Camera3D camera_{};
    CarState car_{};
    TransmissionGear gear_ = TransmissionGear::Drive;
    Buttons buttons_{};
    ButtonLatch buttonLatch_{};
    RenderTexture2D mirrorRear_{};
    RenderTexture2D mirrorLeft_{};
    RenderTexture2D mirrorRight_{};
    std::vector<Obstacle> obstacles_{};
    std::vector<OrientedRect> paintedSlots_{};
    std::vector<Stage> stages_{};
    int currentStageIndex_ = 0;
    int totalCollisions_ = 0;
    float runTimer_ = 0.0f;
    float parkHoldTimer_ = 0.0f;
    float stageClearTimer_ = 0.0f;
    float collisionFlash_ = 0.0f;
    float elapsedSceneTime_ = 0.0f;
    bool mirrorsReady_ = false;
    bool gameWon_ = false;
};

ParkingMasterGame* gGame = nullptr;

void TickFrame() {
    if (gGame != nullptr) {
        gGame->Tick();
    }
}

}  // namespace

int main() {
    ParkingMasterGame game;
    gGame = &game;

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(TickFrame, 0, 1);
#else
    while (!WindowShouldClose()) {
        TickFrame();
    }
#endif

    gGame = nullptr;
    return 0;
}
