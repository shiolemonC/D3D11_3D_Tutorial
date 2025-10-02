#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <string>
#include "asset_format.h"   // 与 AssetCooker 保持一致

// ------------ 设备初始化/结束 ------------
bool ModelStatic_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
void ModelStatic_Finalize();

// ------------ 多模型（可选：保留原能力） ------------
struct ModelStaticDesc {
    std::wstring meshPath;       // L".../xxx.mesh"
    std::wstring matPath;        // 可空：L".../xxx.mat"
};
bool ModelStatic_Load(const ModelStaticDesc& desc, int* outHandle);
void ModelStatic_Unload(int handle);
void ModelStatic_Draw(int handle, const DirectX::XMMATRIX& world);
bool ModelStatic_OverrideBaseColorTex(int handle, const wchar_t* texturePath);

// ------------ “默认模型”一键封装（你要的精简用法） ------------
void ModelStatic_SetDefaultPaths(const wchar_t* meshPath,
    const wchar_t* matPath = nullptr,
    const wchar_t* overrideDiffuse = nullptr);
// 加载默认模型（内部保存句柄）
bool ModelStatic_LoadDefault();

// 设置默认模型的世界矩阵（每帧或需要时调用）
void ModelStatic_SetWorld(const DirectX::XMMATRIX& world);

// 绘制默认模型（不带参数）
void ModelStatic_Draw();

// 卸载默认模型（可选）
void ModelStatic_UnloadDefault();