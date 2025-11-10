#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <d3d11.h>

// 运行时接口（简单版，内部保存全局状态；Draw() 无参数）
struct ModelSkinnedDesc {
    std::wstring meshPath;                 // 必填：.mesh（v1，HAS_SKIN）
    std::wstring skelPath;                 // 必填：.skel
    std::wstring animPath;                 // 可选：.anim（没有也能用 bind pose）
    std::wstring matPath;                  // 可选：.mat（如果不指定，默认 = mesh 同名 .mat）
    std::wstring baseColorTexOverride;     // 可选：强制覆盖一张漫反射贴图
};

// 初始化：传入 D3D 设备与上下文（与 Shader3d 相同）
bool ModelSkinned_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);

// 加载资源（mesh/skel/anim/mat/贴图；内部保存世界矩阵，默认单位阵）
bool ModelSkinned_Load(const ModelSkinnedDesc& d);

// 卸载 / 释放
void ModelSkinned_Finalize();

// 每帧推进动画时间（秒）；没有 .anim 则忽略
void ModelSkinned_Update(double dtSec);

// 设世界矩阵（如不调用，默认 I）
void ModelSkinned_SetWorldMatrix(const DirectX::XMMATRIX& world);

// 渲染（内部会：Shader3d_Begin(); 绑定蒙皮 VS；设置 VB/IB/布局；上传骨矩阵；绑定贴图；DrawIndexed）
void ModelSkinned_Draw();

// （可选）切换是否循环播放
void ModelSkinned_SetLoop(bool loop);

// （可选）设置播放速度（倍率）
void ModelSkinned_SetPlaybackRate(float rate);

// （可选）跳到某时间（秒）
void ModelSkinned_Seek(float timeSec);

bool ModelSkinned_LoadAnimOnly(const std::wstring& animPath);

int  ModelSkinned_GetRootJointIndex();

// 以“当前时间 t”为起点，采样区间 [t, t+dt] 的“根关节局部位移 ΔT（单位：模型局部空间，米）”
// 成功返回 true；若没有动画/根关节不存在返回 false
bool ModelSkinned_SampleRootDelta_Local(float dt, DirectX::XMFLOAT3* outDeltaT);

// VelocityDriven 时把根关节局部平移的 XZ 清零（保留 Y），启/停
void ModelSkinned_SetZeroRootTranslationXZ(bool enable);

// Debug: 取得当前已加载剪辑的 root 局部 yaw（弧度）
// yaw0  = 第0帧（首帧）root 局部朝向
// yawNow= 当前时间点（内部时间）的 root 局部朝向
bool ModelSkinned_DebugGetRootYaw_F0(float* yaw0);
bool ModelSkinned_DebugGetRootYaw_Current(float* yawNow);

void ModelSkinned_SetRootYawFix(float yawFixRad); // 弧度

// —— 根局部旋转的“入场对齐 + ΔYaw 抽取”接口 ——
// 设定对齐目标（通常 = 第一次播放的 Idle 首帧 yaw，单位：弧度）
void ModelSkinned_SetRootYawAlignTarget(float yawTargetRad);
// 重置 yaw 基准（本剪辑首帧 yaw，单位：弧度）
void ModelSkinned_ResetRootYawTrack(float yawStartRad);
// 采样区间 [t, t+dt] 的根局部 Δyaw（单位：弧度；成功返回 true）
// 注意：要在 ModelSkinned_Update(dt) 之前调用，和 SampleRootDelta_Local 使用同样的时间点
bool ModelSkinned_SampleRootYawDelta(float dt, float* outDeltaYaw);

// === meta getters ===
uint32_t ModelSkinned_GetFrameCount();
float    ModelSkinned_GetSampleRate();

