#include "input_gamepad_xinput.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Xinput.h>
#include <algorithm>

#pragma comment(lib, "xinput.lib")

namespace input::xinput
{
    static State s_state{};
    static WORD  s_prevButtons = 0;

    // --- internal vibration state ---
    static bool  s_vibDirty = false;
    static Vibration s_vibCurrent{ 0,0 };

    // impulse timer
    static float s_impulseTimeLeft = 0.0f;
    static Vibration s_impulseVib{ 0,0 };

    // helper clamp
    static float Clamp01(float v)
    {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    static float NormalizeThumb(SHORT v, SHORT deadzone)
    {
        // v ∈ [-32768, 32767]
        const int iv = static_cast<int>(v);
        const int a = (iv >= 0) ? iv : -iv;
        const int dz = static_cast<int>(deadzone);

        if (a <= dz) return 0.0f;

        const float sign = (iv >= 0) ? 1.0f : -1.0f;
        float mag = static_cast<float>(a - dz) / static_cast<float>(32767 - dz);
        if (mag > 1.0f) mag = 1.0f;
        return sign * mag;
    }

    void Initialize()
    {
        s_state = {};
        s_prevButtons = 0;
    }

    void Update()
    {
        s_state = {};

        XINPUT_STATE st{};
        const DWORD r = XInputGetState(0, &st);
        if (r != ERROR_SUCCESS)
        {
            // Reset edges when disconnected to avoid false press on reconnect.
            s_prevButtons = 0;
            return;
        }

        s_state.connected = true;

        const SHORT LDZ = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
        const SHORT RDZ = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;

        s_state.lx = NormalizeThumb(st.Gamepad.sThumbLX, LDZ);
        s_state.ly = NormalizeThumb(st.Gamepad.sThumbLY, LDZ);
        s_state.rx = NormalizeThumb(st.Gamepad.sThumbRX, RDZ);
        s_state.ry = NormalizeThumb(st.Gamepad.sThumbRY, RDZ);

        const WORD now = st.Gamepad.wButtons;

        auto JP = [&](WORD mask) { return ((now & mask) && !(s_prevButtons & mask)); };
        auto HD = [&](WORD mask) { return (now & mask) != 0; };

        s_state.pressA = JP(XINPUT_GAMEPAD_A);
        s_state.pressB = JP(XINPUT_GAMEPAD_B);
        s_state.pressX = JP(XINPUT_GAMEPAD_X);
        s_state.pressY = JP(XINPUT_GAMEPAD_Y);
        s_state.pressR3 = JP(XINPUT_GAMEPAD_RIGHT_THUMB);

        s_state.holdA = HD(XINPUT_GAMEPAD_A);
        s_state.holdB = HD(XINPUT_GAMEPAD_B);
        s_state.holdX = HD(XINPUT_GAMEPAD_X);
        s_state.holdY = HD(XINPUT_GAMEPAD_Y);
        s_state.holdR3 = HD(XINPUT_GAMEPAD_RIGHT_THUMB);

        s_prevButtons = now;
    }

    const State& GetState() { return s_state; }
    bool Connected() { return s_state.connected; }

    bool Hold(Button b)
    {
        switch (b)
        {
        case Button::A:  return s_state.holdA;
        case Button::B:  return s_state.holdB;
        case Button::X:  return s_state.holdX;
        case Button::Y:  return s_state.holdY;
        case Button::R3: return s_state.holdR3;
        default:         return false;
        }
    }

    bool Press(Button b)
    {
        switch (b)
        {
        case Button::A:  return s_state.pressA;
        case Button::B:  return s_state.pressB;
        case Button::X:  return s_state.pressX;
        case Button::Y:  return s_state.pressY;
        case Button::R3: return s_state.pressR3;
        default:         return false;
        }
    }

    void RightStickToMouseDelta(double dtSec, float yawSpeed, float pitchSpeed, float& outDx, float& outDy)
    {
        outDx = 0.0f;
        outDy = 0.0f;
        if (!s_state.connected) return;

        outDx = s_state.rx * yawSpeed * static_cast<float>(dtSec);
        // Up on stick => look up (common feel)
        outDy = -s_state.ry * pitchSpeed * static_cast<float>(dtSec);
    }

    static WORD ToMotorU16(float v01)
    {
        v01 = Clamp01(v01);
        // XInput motor is 0..65535
        const float scaled = v01 * 65535.0f;
        const int iv = (int)(scaled + 0.5f);
        if (iv < 0) return 0;
        if (iv > 65535) return 65535;
        return (WORD)iv;
    }

    static void ApplyVibrationNow()
    {
        if (!s_state.connected) return;

        XINPUT_VIBRATION vib{};
        vib.wLeftMotorSpeed = ToMotorU16(s_vibCurrent.leftMotor);
        vib.wRightMotorSpeed = ToMotorU16(s_vibCurrent.rightMotor);

        XInputSetState(0, &vib);
        s_vibDirty = false;
    }

    void SetVibration(float leftMotor01, float rightMotor01)
    {
        s_vibCurrent.leftMotor = Clamp01(leftMotor01);
        s_vibCurrent.rightMotor = Clamp01(rightMotor01);

        // 直接覆盖 impulse（你也可以选择不覆盖，这里我给“显式设置优先”）
        s_impulseTimeLeft = 0.0f;
        s_impulseVib = { 0,0 };

        s_vibDirty = true;
        ApplyVibrationNow();
    }

    void StopVibration()
    {
        s_vibCurrent = { 0,0 };
        s_impulseTimeLeft = 0.0f;
        s_impulseVib = { 0,0 };

        s_vibDirty = true;
        ApplyVibrationNow();
    }

    void PlayImpulse(float leftMotor01, float rightMotor01, float durationSec)
    {
        if (durationSec <= 0.0f) return;

        s_impulseVib.leftMotor = Clamp01(leftMotor01);
        s_impulseVib.rightMotor = Clamp01(rightMotor01);
        s_impulseTimeLeft = durationSec;

        // 立即应用：impulse 期间，输出 = max(当前持续, impulse)
        Vibration out{};
        out.leftMotor = std::max(s_vibCurrent.leftMotor, s_impulseVib.leftMotor);
        out.rightMotor = std::max(s_vibCurrent.rightMotor, s_impulseVib.rightMotor);

        // 这里不改 s_vibCurrent，本质是“临时覆盖输出”
        XINPUT_VIBRATION vib{};
        vib.wLeftMotorSpeed = ToMotorU16(out.leftMotor);
        vib.wRightMotorSpeed = ToMotorU16(out.rightMotor);

        if (s_state.connected)
        {
            XInputSetState(0, &vib);
        }
    }

    void AddImpulse(float leftMotor01, float rightMotor01, float durationSec)
    {
        if (durationSec <= 0.0f) return;

        // 叠加强度取 max，时长取 max
        s_impulseVib.leftMotor = std::max(s_impulseVib.leftMotor, Clamp01(leftMotor01));
        s_impulseVib.rightMotor = std::max(s_impulseVib.rightMotor, Clamp01(rightMotor01));
        s_impulseTimeLeft = std::max(s_impulseTimeLeft, durationSec);

        // 立刻输出一下
        PlayImpulse(s_impulseVib.leftMotor, s_impulseVib.rightMotor, s_impulseTimeLeft);
    }

    void UpdateVibration(double dtSec)
    {
        if (!s_state.connected) return;

        if (s_impulseTimeLeft > 0.0f)
        {
            s_impulseTimeLeft -= (float)dtSec;
            if (s_impulseTimeLeft <= 0.0f)
            {
                // impulse 结束：回到持续震动（s_vibCurrent）
                s_impulseTimeLeft = 0.0f;
                s_impulseVib = { 0,0 };

                s_vibDirty = true;
                ApplyVibrationNow();
            }
            // impulse 进行中，不用每帧 SetState（除非你要做曲线/衰减）
        }
        else
        {
            // 没有 impulse 时，如果有 dirty 才下发（避免每帧调用 SetState）
            if (s_vibDirty) ApplyVibrationNow();
        }
    }
}
