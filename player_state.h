// player_state.h
#pragma once
#include <string>
#include <vector>
#include <utility>

struct PlayerSMOutput {
    const char* state;          // 最终状态名（UTF-8）
    const wchar_t* clip;           // 动画名（直接喂 AnimatorRegistry_Play / CrossFade）
    bool            changed;        // 本帧是否发生状态切换
    float           blendSeconds;   // 预留：融合时间（秒）
    const char* blendCurve;     // 预留：曲线名
    bool            useRootMotion;  // 本状态是否消费动画Δ
    bool            locomotionActive; // ★ 是否允许基于输入的行走位移
};

bool PlayerSM_LoadConfigJSON(const wchar_t* jsonPath); // 读取 JSON（占位：详见 .cpp 里的说明）
void PlayerSM_LoadConfigDefaults();                     // 临时内置 Idle/Move 配置（可立即跑）
void PlayerSM_Reset();                                  // 切到初始状态（默认 Idle）并清零计时

// 将输入/黑板同步给 FSM（FSM 内部会再写入到条件子模块）
void PlayerSM_SetMoveInput(float x, float z);           // -1..1，内部会计算 move.mag
void PlayerSM_SetBool(const char* name, bool v);        // e.g., grounded
void PlayerSM_SetFloat(const char* name, float v);      // e.g., stamina
void PlayerSM_FireTrigger(const char* name);            // e.g., Attack

// 每帧更新，做转移决策并返回执行结果
PlayerSMOutput PlayerSM_Update(double dt);

// 获取当前状态名（调试/HUD）
const char* PlayerSM_GetCurrentStateName();

// Debug HUD（与 Camera_DebugDraw 类似）
void PlayerSM_DebugDraw();

void PlayerSM_OverrideCurrentStateLength(float seconds);