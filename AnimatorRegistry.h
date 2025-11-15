#pragma once
#include <string>
#include <vector>
#include <d3d11.h>
#include <DirectXMath.h>

// RootMotion 策略
enum class RootMotionType : uint8_t {
    None = 0,           // 不使用动画位移
    VelocityDriven = 1, // 由逻辑速度驱动，动画只摆动
    UseAnimDelta = 2,   // 使用动画的 Δ 位移/Δ 朝向
};

// 动画资源描述（注册时提供）
struct AnimClipDesc {
    std::wstring name;                 // 唯一名（播放用）
    std::wstring meshPath;             // .mesh
    std::wstring skelPath;             // .skel
    std::wstring animPath;             // .anim（可空）
    std::wstring matPath;              // .mat（可空）
    std::wstring baseColorOverride;    // 可空：强制底色贴图

    // 默认播放参数（可在 Play 时覆盖）
    bool          loop = true;
    float         playbackRate = 1.0f;

    // RootMotion 相关
    RootMotionType rmType = RootMotionType::None;
    float          velocity = 0.0f;    // VelocityDriven 使用

    // ★ 新增：为该动画显式指定“驱动根”名称（UTF-8）。为空则用 ModelSkinned 的自动解析/上次设置。
    std::string motionRootNameUTF8; 
};

// 初始化/结束
bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
void AnimatorRegistry_Finalize();

// 注册/查询
void AnimatorRegistry_Clear();
bool AnimatorRegistry_Register(const AnimClipDesc& clip);
bool AnimatorRegistry_Has(const std::wstring& name);
const AnimClipDesc* AnimatorRegistry_Get(const std::wstring& name);
bool AnimatorRegistry_LoadAll(); // 目前不做 IO，仅校验

// 播放控制（可传入临时覆盖参数）
bool AnimatorRegistry_Play(const std::wstring& name,
    bool* outChanged = nullptr,
    bool overrideLoop = false, bool loopValue = true,
    bool overrideRate = false, float rateValue = 1.0f);

// 世界矩阵/更新/绘制
void AnimatorRegistry_SetWorld(const DirectX::XMMATRIX& world);
void AnimatorRegistry_Update(double dtSec);
void AnimatorRegistry_Draw();

// 状态查询
std::wstring  AnimatorRegistry_CurrentName();
RootMotionType AnimatorRegistry_CurrentRootMotionType();
float          AnimatorRegistry_CurrentPlaybackRate();
bool           AnimatorRegistry_CurrentLoop();

// 供外部一次性注册用（若你有 animator_register.cpp）
void AnimRegister();

// RootMotionDelta（世界系）
struct RootMotionDelta {
    DirectX::XMFLOAT3 pos; // Δ位置（世界系；一般只用 XZ）
    float             yaw; // Δ朝向（弧度，世界系；当前未应用到相机/玩家朝向）
};

// 读取并消费本帧 RootMotion（返回 true 表示本帧存在 Δ）
bool AnimatorRegistry_ConsumeRootMotionDelta(RootMotionDelta* out);

// DEBUG（保留在用）
bool AnimatorRegistry_DebugGetCurrentClipName(const wchar_t** outName);
// 返回 motion-root 的局部 yaw：首帧 yaw0、当前帧 yawNow（弧度）
bool AnimatorRegistry_DebugGetRootYaw(float* yaw0, float* yawNow);
// 返回“当前剪辑总时长（秒）”（考虑 playbackRate）
bool AnimatorRegistry_DebugGetCurrentClipLengthSec(float* outSec);
