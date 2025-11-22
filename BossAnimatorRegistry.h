// BossAnimatorRegistry.h
#pragma once
#include <string>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

#include "AnimatorRegistry.h"     // 复用 RootMotionType / RootMotionDelta / AnimClipDesc
#include "BossModelSkinned.h"

// 直接复用 AnimClipDesc / RootMotionType / RootMotionDelta
// 如有需要，之后可以再为 Boss 定制自己的 Desc

bool BossAnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
void BossAnimatorRegistry_Finalize();

void BossAnimatorRegistry_Clear();
bool BossAnimatorRegistry_Register(const AnimClipDesc& clip);
bool BossAnimatorRegistry_LoadAll();

bool BossAnimatorRegistry_Play(const std::wstring& name,
    bool* outChanged = nullptr,
    bool overrideLoop = false, bool loopValue = true,
    bool overrideRate = false, float rateValue = 1.0f);

void BossAnimatorRegistry_SetWorld(const DirectX::XMMATRIX& world);
void BossAnimatorRegistry_Update(double dtSec);
void BossAnimatorRegistry_Draw();

std::wstring   BossAnimatorRegistry_CurrentName();
RootMotionType BossAnimatorRegistry_CurrentRootMotionType();
float          BossAnimatorRegistry_CurrentPlaybackRate();
bool           BossAnimatorRegistry_CurrentLoop();

bool BossAnimatorRegistry_ConsumeRootMotionDelta(RootMotionDelta* out);

bool BossAnimatorRegistry_DebugGetCurrentClipName(const wchar_t** outName);
bool BossAnimatorRegistry_DebugGetRootYaw(float* yaw0, float* yawNow);
bool BossAnimatorRegistry_DebugGetCurrentClipLengthSec(float* outSec);
bool BossAnimatorRegistry_DebugGetCurrentNormalizedTime(float* outNorm);

void BossAnimatorRegistry_CrossFade(const std::wstring& name,
    float durationSec,
    const char* curveNameUTF8);
