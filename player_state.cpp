#include "player_state.h"
#include <cmath>

static PlayerState s_state = PlayerState::Idle;

PlayerSMResult PlayerSM_Update(double /*dt*/, const PlayerInput& in)
{
    float mag2 = in.moveX * in.moveX + in.moveZ * in.moveZ;
    PlayerState next = (mag2 > 1e-6f) ? PlayerState::Move : PlayerState::Idle;

    PlayerSMResult r{};
    r.animChanged = (next != s_state);
    s_state = next;
    r.state = s_state;
    if (s_state == PlayerState::Idle) {
        r.animName = L"Idle";
        r.rate = 1.0f;
    }
    else {
        r.animName = L"Walk";
        r.rate = 1.0f; // 将来按速度映射步频可在此调整
    }
    return r;
}
