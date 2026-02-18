#pragma once

#include <cstdint>

// Xbox gamepad input (XInput)
// - Call Initialize() once (e.g., in Game_Initialize / Title_Initialize)
// - Call Update() once per frame
// - Then query GetState() / Press() / Hold()

namespace input::xinput
{
    enum class Button : std::uint8_t
    {
        A,
        B,
        X,
        Y,
        R3, // Right stick press
    };

    struct State
    {
        bool  connected = false;

        // Thumb sticks: [-1, +1] with deadzone applied
        float lx = 0.0f;
        float ly = 0.0f;
        float rx = 0.0f;
        float ry = 0.0f;

        // Held state
        bool holdA = false;
        bool holdB = false;
        bool holdX = false;
        bool holdY = false;
        bool holdR3 = false;

        // Just pressed (edge)
        bool pressA = false;
        bool pressB = false;
        bool pressX = false;
        bool pressY = false;
        bool pressR3 = false;
    };

    struct Vibration
    {
        // [0, 1]
        float leftMotor = 0.0f; // 通常是低频“大震”（更“沉”）
        float rightMotor = 0.0f; // 通常是高频“小震”（更“麻”）
    };

    void Initialize();
    void Update();
    const State& GetState();

    bool Connected();
    bool Hold(Button b);
    bool Press(Button b);

    // Convert right stick continuous input to per-frame mouse-like delta.
    // You should ADD these deltas onto your real mouse delta.
    void RightStickToMouseDelta(double dtSec, float yawSpeed, float pitchSpeed, float& outDx, float& outDy);

    // 立刻设置震动（持续到 Stop 或被其他设置覆盖）
    void SetVibration(float leftMotor01, float rightMotor01);

    // 立刻停止震动
    void StopVibration();

    // 持续 durationSec，自动停止（或被后续更强震动覆盖）
    void PlayImpulse(float leftMotor01, float rightMotor01, float durationSec);

    void UpdateVibration(double dtSec);

    // 可选取 max 叠加
    void AddImpulse(float leftMotor01, float rightMotor01, float durationSec);
}
