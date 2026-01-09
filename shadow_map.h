#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

bool ShadowMap_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int shadowSize = 2048);
void ShadowMap_Finalize();

// 生成 shadow map（只绑定 DSV、设置 viewport、清 depth、设置 light view/proj 到 Shader3d）
void ShadowMap_BeginDirectional(const DirectX::XMFLOAT3& lightDirW,
    const DirectX::XMFLOAT3& centerW,
    float radius);

// 结束 shadow pass（还原 RT/DSV/viewport/RS state）
void ShadowMap_End();

// 主 pass：给 MeshField 的 PS 绑定：t2=shadow SRV, s1=comparison sampler, b5=shadow cbuffer
void ShadowMap_BindForFieldPS();

// 可选：解绑，避免下一帧绑定 DSV 时产生 D3D debug warning
void ShadowMap_UnbindForFieldPS();

// 参数：bias 建议 0.001~0.005；strength 0~1
void ShadowMap_SetParams(float bias, float strength);
