#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>

// 你要的 RootMotion 策略枚举（先占位，后面状态机里再用）
// - None: 不用动画位移
// - VelocityDriven: 逻辑速度驱动，动画只做摆臂
// - UseAnimDelta: 完全使用动画的Δ位移/Δ朝向
enum class RootMotionType : uint8_t {
    None = 0,
    VelocityDriven = 1,
    UseAnimDelta = 2,
};

// 每条动画的“注册信息”（只读资源配置）
struct AnimClipDesc {
    std::wstring name;          // 唯一名（播放时用它）
    std::wstring meshPath;      // .mesh
    std::wstring skelPath;      // .skel
    std::wstring animPath;      // .anim（可空：静态POSE）
    std::wstring matPath;       // .mat（可空）
    std::wstring baseColorOverride; // 可空：强制漫反射贴图

    // 默认播放策略（可被播放时覆盖）
    bool          loop = true;
    float         playbackRate = 1.0f;
    RootMotionType rmType = RootMotionType::None;
    float         velocity = 0.0f;
};

// 对外 API
// ------------------------------------------------------------------
bool AnimatorRegistry_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
void AnimatorRegistry_Finalize();

// 登录/读取
void AnimatorRegistry_Clear();                                 // 清空注册表（可选）
bool AnimatorRegistry_Register(const AnimClipDesc& clip);       // 新增一条
bool AnimatorRegistry_Has(const std::wstring& name);
const AnimClipDesc* AnimatorRegistry_Get(const std::wstring& name);

// 统一加载（可选，当前实现里不做 IO，只做校验）
bool AnimatorRegistry_LoadAll();

// 播放控制（把选中的动作应用到 ModelSkinned —— 不改 ModelSkinned 内部）
bool AnimatorRegistry_Play(const std::wstring& name,           // 动画名
    bool* outChanged = nullptr,         // 返回是否真的切换了
    bool overrideLoop = false,
    bool loopValue = true,
    bool overrideRate = false,
    float rateValue = 1.0f);

// 世界矩阵与逐帧驱动（全部透传给 ModelSkinned）
void AnimatorRegistry_SetWorld(const DirectX::XMMATRIX& world);
void AnimatorRegistry_Update(double dtSec);
void AnimatorRegistry_Draw();

// 查询
std::wstring AnimatorRegistry_CurrentName();
RootMotionType AnimatorRegistry_CurrentRootMotionType();
float AnimatorRegistry_CurrentPlaybackRate();
bool AnimatorRegistry_CurrentLoop();

// 方便：把“登录脚本”里的函数声明一下（你可以在 animator_register.cpp 实现它）
void AnimRegister();   // 在游戏初始化时调用一次

// ---- 根运动Δ读取（从动画侧同步位移到 Player 的“真值源”） ----
struct RootMotionDelta {
    DirectX::XMFLOAT3 pos; // 世界系Δ位置（一般只用XZ，Y可按需丢弃）
    float yaw;             // 世界系Δ朝向（先给 0，将来需要可扩展）
};

// 读取并“消费”本帧动画位移（返回 true 表示本帧有Δ；同时清空内部累积）
bool AnimatorRegistry_ConsumeRootMotionDelta(RootMotionDelta* out);