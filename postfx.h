#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

bool PostFx_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx);
void PostFx_Finalize();

void PostFx_Update(double dt);

// 把“主场景”画到 SceneColorRT（内部会清 SceneColor + 清 Depth）
void PostFx_BeginScene(const float clearColor[4]);

// 把 SceneColorRT 做后处理输出到 BackBuffer（没开效果时就是copy）
void PostFx_ApplyToBackBuffer();

void PostFx_StartRadialBlurWorld(const DirectX::XMFLOAT3& centerWorld,
    float durationSec,
    float strength,
    float radius,
    int sampleCount);

void PostFx_StopRadialBlur();
