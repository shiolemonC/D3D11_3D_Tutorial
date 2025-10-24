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