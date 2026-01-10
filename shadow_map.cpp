#include "shadow_map.h"
#include "direct3d.h"
#include "shader3d.h"   // 用它来临时把 view/proj 设成 light 的
#include <cmath>
#include <algorithm>

using namespace DirectX;

static ID3D11Device* gDev = nullptr;
static ID3D11DeviceContext* gCtx = nullptr;

static ID3D11Texture2D* gShadowTex = nullptr;
static ID3D11DepthStencilView* gShadowDSV = nullptr;
static ID3D11ShaderResourceView* gShadowSRV = nullptr;

static ID3D11SamplerState* gShadowCmpSamp = nullptr;
static ID3D11RasterizerState* gShadowRS = nullptr;

static D3D11_VIEWPORT          gShadowVP{};
static int                     gShadowSize = 2048;

struct ShadowCB
{
    XMFLOAT4X4 lightViewProj; // CPU侧已转置（配合你当前 mul(rowVec, M) 的写法）
    XMFLOAT4   params;        // x=bias, y=strength, z,w=unused
};
static ID3D11Buffer* gShadowCB = nullptr;
static ShadowCB      gShadowCBData{};

// 备份状态（简化版：假设你只用 1 个 viewport）
static ID3D11RenderTargetView* gPrevRTV = nullptr;
static ID3D11DepthStencilView* gPrevDSV = nullptr;
static ID3D11RasterizerState* gPrevRS = nullptr;
static D3D11_VIEWPORT          gPrevVP{};
static bool                    gHasPrevVP = false;

// Dummy Resource
static ID3D11Texture2D* gDummyColorTex = nullptr;
static ID3D11RenderTargetView* gDummyRTV = nullptr;

// 可选：完全关闭颜色写入（即使绑了 RTV 也不写）
static ID3D11BlendState* gNoColorWriteBS = nullptr;
static ID3D11BlendState* gPrevBS = nullptr;
static float                   gPrevBlendFactor[4] = {};
static UINT                    gPrevSampleMask = 0xffffffff;

static XMMATRIX BuildLightView(const XMFLOAT3& lightDirW, const XMFLOAT3& centerW, float radius)
{
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDirW));

    // 避免 up 与 dir 平行
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    float d = std::fabs(XMVectorGetX(XMVector3Dot(dir, up)));
    if (d > 0.98f) up = XMVectorSet(0, 0, 1, 0);

    XMVECTOR center = XMLoadFloat3(&centerW);
    float dist = radius * 2.0f;
    XMVECTOR eye = center - dir * dist;

    return XMMatrixLookAtLH(eye, center, up);
}

static XMMATRIX BuildLightProj(float radius)
{
    // 正交投影：覆盖一个半径为 radius 的区域
    float w = radius * 2.0f;
    float h = radius * 2.0f;
    float zn = 0.1f;
    float zf = radius * 6.0f; // 给足深度范围（地面+角色高度）
    return XMMatrixOrthographicLH(w, h, zn, zf);
}

bool ShadowMap_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx, int shadowSize)
{
    if (!dev || !ctx) return false;
    gDev = dev;
    gCtx = ctx;
    gShadowSize = std::max(256, shadowSize);

    // 1) Depth Texture（typeless）
    D3D11_TEXTURE2D_DESC td{};
    td.Width = (UINT)gShadowSize;
    td.Height = (UINT)gShadowSize;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_TYPELESS;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(gDev->CreateTexture2D(&td, nullptr, &gShadowTex))) return false;

    // 2) DSV
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
    dsvd.Format = DXGI_FORMAT_D32_FLOAT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    if (FAILED(gDev->CreateDepthStencilView(gShadowTex, &dsvd, &gShadowDSV))) return false;

    // 3) SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = DXGI_FORMAT_R32_FLOAT;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    if (FAILED(gDev->CreateShaderResourceView(gShadowTex, &srvd, &gShadowSRV))) return false;

    // 4) Comparison Sampler（PCF 2x2）
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;

    if (FAILED(gDev->CreateSamplerState(&sd, &gShadowCmpSamp))) return false;

    // 5) Rasterizer（Depth Bias）
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = FALSE;

    // 这些值需要你之后根据场景微调（先给一个“能跑”的默认）
    rd.DepthBias = 1000;
    rd.SlopeScaledDepthBias = 2.0f;
    rd.DepthBiasClamp = 0.0f;

    if (FAILED(gDev->CreateRasterizerState(&rd, &gShadowRS))) return false;

    // 6) Shadow Constant Buffer（PS b5）
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(ShadowCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(gDev->CreateBuffer(&cbd, nullptr, &gShadowCB))) return false;

    // 7) Viewport
    gShadowVP.TopLeftX = 0.0f;
    gShadowVP.TopLeftY = 0.0f;
    gShadowVP.Width = (float)gShadowSize;
    gShadowVP.Height = (float)gShadowSize;
    gShadowVP.MinDepth = 0.0f;
    gShadowVP.MaxDepth = 1.0f;

    // 默认参数
    gShadowCBData.params = XMFLOAT4(0.0025f, 1.0f, 0, 0);

    // Dummy
    // 1x1 dummy color target
    // Dummy color target (MUST match shadow map size)
    D3D11_TEXTURE2D_DESC cd{};
    cd.Width = (UINT)gShadowSize;
    cd.Height = (UINT)gShadowSize;
    cd.MipLevels = 1;
    cd.ArraySize = 1;
    cd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    cd.SampleDesc.Count = 1;
    cd.SampleDesc.Quality = 0;
    cd.Usage = D3D11_USAGE_DEFAULT;
    cd.BindFlags = D3D11_BIND_RENDER_TARGET;

    HRESULT hr = gDev->CreateTexture2D(&cd, nullptr, &gDummyColorTex);
    if (FAILED(hr)) return false;

    hr = gDev->CreateRenderTargetView(gDummyColorTex, nullptr, &gDummyRTV);
    if (FAILED(hr)) return false;

    // no color write blend state (RenderTargetWriteMask = 0)
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = 0;
    gDev->CreateBlendState(&bd, &gNoColorWriteBS);

    return true;
}

void ShadowMap_Finalize()
{
    // Shadow resources
    SAFE_RELEASE(gShadowCB);
    SAFE_RELEASE(gShadowRS);
    SAFE_RELEASE(gShadowCmpSamp);
    SAFE_RELEASE(gShadowSRV);
    SAFE_RELEASE(gShadowDSV);
    SAFE_RELEASE(gShadowTex);

    // Dummy + BlendState（只在这里释放一次）
    SAFE_RELEASE(gNoColorWriteBS);
    SAFE_RELEASE(gDummyRTV);
    SAFE_RELEASE(gDummyColorTex);

    gDev = nullptr;
    gCtx = nullptr;
}

void ShadowMap_SetParams(float bias, float strength)
{
    gShadowCBData.params.x = bias;
    gShadowCBData.params.y = strength;
}

void ShadowMap_UnbindForFieldPS()
{
    if (!gCtx) return;
    ID3D11ShaderResourceView* nullSRV = nullptr;
    gCtx->PSSetShaderResources(2, 1, &nullSRV);
}

void ShadowMap_BeginDirectional(const XMFLOAT3& lightDirW, const XMFLOAT3& centerW, float radius)
{
    if (!gCtx) return;

    // 重要：作为 DSV 使用之前，把 SRV 从 PS slot2 解绑（避免 debug layer 警告/冲突）
    ShadowMap_UnbindForFieldPS();

    // 备份当前 RT/DSV
    gCtx->OMGetRenderTargets(1, &gPrevRTV, &gPrevDSV);

    // 备份 viewport（简化：只取第 1 个）
    UINT n = 1;
    gHasPrevVP = false;
    gCtx->RSGetViewports(&n, &gPrevVP);   // 注意：这个函数是 void
    gHasPrevVP = (n == 1);

    // 备份 RS state
    gCtx->RSGetState(&gPrevRS);

    // 设置 shadow viewport + RS
    gCtx->RSSetViewports(1, &gShadowVP);
    gCtx->RSSetState(gShadowRS);

    // 保存并设置“禁止颜色写入”
    gCtx->OMGetBlendState(&gPrevBS, gPrevBlendFactor, &gPrevSampleMask);
    gCtx->OMSetBlendState(gNoColorWriteBS, nullptr, 0xffffffff);

    // 绑定 dummy RTV + shadow DSV（这样 PS 即使不小心被绑定也不会 warning）
    gCtx->OMSetRenderTargets(1, &gDummyRTV, gShadowDSV);

    // 你仍然可以关 PS（对性能更好）
    gCtx->PSSetShader(nullptr, nullptr, 0);

    gCtx->ClearDepthStencilView(gShadowDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    // 计算 Light View/Proj，并写到 Shader3d（让你现有 skinned VS 直接输出到 shadow clip）
    XMMATRIX V = BuildLightView(lightDirW, centerW, radius);
    XMMATRIX P = BuildLightProj(radius);
    Shader3d_SetViewMatrix(V);
    Shader3d_SetProjectionMatrix(P);

    // 写 PS b5 用的 viewProj（转置）
    XMMATRIX VP = XMMatrixMultiply(V, P);
    XMFLOAT4X4 vpT{};
    XMStoreFloat4x4(&vpT, XMMatrixTranspose(VP));
    gShadowCBData.lightViewProj = vpT;

    gCtx->UpdateSubresource(gShadowCB, 0, nullptr, &gShadowCBData, 0, 0);
}

void ShadowMap_End()
{
    if (!gCtx) return;

    // 还原 RT/DSV
    if (gPrevRTV || gPrevDSV)
        gCtx->OMSetRenderTargets(1, &gPrevRTV, gPrevDSV);

    // 还原 viewport
    if (gHasPrevVP)
        gCtx->RSSetViewports(1, &gPrevVP);

    // 还原 RS state
    if (gPrevRS)
        gCtx->RSSetState(gPrevRS);

    // 还原 BlendState（注意：这是“备份的 gPrevBS”，每帧要 release）
    gCtx->OMSetBlendState(gPrevBS, gPrevBlendFactor, gPrevSampleMask);
    SAFE_RELEASE(gPrevBS);

    // 释放“备份的 prev”（这些是 Get* 得到的引用，每帧要 release）
    SAFE_RELEASE(gPrevRTV);
    SAFE_RELEASE(gPrevDSV);
    SAFE_RELEASE(gPrevRS);

    gHasPrevVP = false;
}

void ShadowMap_BindForFieldPS()
{
    if (!gCtx) return;

    // PS: t2
    gCtx->PSSetShaderResources(2, 1, &gShadowSRV);
    // PS: s1（比较采样）
    gCtx->PSSetSamplers(1, 1, &gShadowCmpSamp);
    // PS: b5
    gCtx->PSSetConstantBuffers(5, 1, &gShadowCB);
}


