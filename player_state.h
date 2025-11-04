#pragma once
#include <string>
#include "player.h"

struct PlayerSMResult {
    PlayerState state;
    bool        animChanged = false;
    const wchar_t* animName = nullptr; // "Idle" / "Walk"
    float       rate = 1.0f;           // 可用于按速度调整步频（先留 1.0）
};

PlayerSMResult PlayerSM_Update(double dt, const PlayerInput& in);
