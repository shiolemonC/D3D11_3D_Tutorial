#include "postfx.h"
#include "direct3d.h"
#include "camera.h"
#include <fstream>
#include <Windows.h>

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) do{ if(p){ (p)->Release(); (p)=nullptr; } }while(0)
#endif
#include <vector>

using namespace DirectX;

static ID3D11Device* gDev = nullptr;
static ID3D11DeviceContext* gCtx = nullptr;

static ID3D11Texture2D* gSceneTex = nullptr;
static ID3D11RenderTargetView* gSceneRTV = nullptr;
static ID3D11ShaderResourceView* gSceneSRV = nullptr;

static ID3D11SamplerState* gLinearClamp = nullptr;

static ID3D11VertexShader* gVS = nullptr;
static ID3D11PixelShader* gPS = nullptr;

struct PostFxCB
{
    XMFLOAT2 invTexSize; // 8, FXAA texel size
    float    strength;   // 12
    float    radius;     // 16
    XMFLOAT2 centerUV;   // 24
    float    enable;     // 28
    float    sampleCount;// 32
    XMFLOAT4 _pad;       // 48 (对齐)
};
static ID3D11Buffer* gCB = nullptr;
static PostFxCB gCBData{};

static bool gRadialActive = false;
static float gRadialTimer = 0.0f;
static float gRadialDuration = 0.0f;
static float gRadialStrength = 0.0f;
static float gRadialRadius = 0.08f;
static int   gRadialSamples = 12;
static XMFLOAT3 gRadialCenterWorld{ 0,0,0 };

static bool LoadCSO(const char* path, std::vector<unsigned char>& out)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    ifs.seekg(0, std::ios::end);
    std::streamsize size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out.resize((size_t)size);
    ifs.read((char*)out.data(), size);
    return true;
}

static bool CreateSceneRT()
{
    const UINT w = Direct3D_GetBackBufferWidth();
    const UINT h = Direct3D_GetBackBufferHeight();

    SAFE_RELEASE(gSceneSRV);
    SAFE_RELEASE(gSceneRTV);
    SAFE_RELEASE(gSceneTex);

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(gDev->CreateTexture2D(&td, nullptr, &gSceneTex))) return false;
    if (FAILED(gDev->CreateRenderTargetView(gSceneTex, nullptr, &gSceneRTV))) return false;
    if (FAILED(gDev->CreateShaderResourceView(gSceneTex, nullptr, &gSceneSRV))) return false;

    gCBData.invTexSize = XMFLOAT2(1.0f / w, 1.0f / h);
    return true;
}

bool PostFx_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    if (!dev || !ctx) return false;
    gDev = dev;
    gCtx = ctx;

    if (!CreateSceneRT())
    {
        MessageBox(nullptr, "CreateSceneRT failed", "PostFx", MB_OK);
        return false;
    }

    // sampler
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MinLOD = 0;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    if (FAILED(gDev->CreateSamplerState(&sd, &gLinearClamp))) return false;

    // shaders: 你需要新增并编译 postfx_fullscreen_vs.cso / postfx_radialblur_ps.cso
    std::vector<unsigned char> vsb, psb;
    if (!LoadCSO("postfx_fullscreen_vs.cso", vsb) ||
        !LoadCSO("postfx_radialblur_ps.cso", psb))
    {
        MessageBox(nullptr, "Missing postfx .cso", "PostFx", MB_OK);
        return false;
    }

    if (FAILED(gDev->CreateVertexShader(vsb.data(), vsb.size(), nullptr, &gVS))) return false;
    if (FAILED(gDev->CreatePixelShader(psb.data(), psb.size(), nullptr, &gPS))) return false;

    // constant buffer
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PostFxCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(gDev->CreateBuffer(&cbd, nullptr, &gCB))) return false;

    return true;
}

void PostFx_Finalize()
{
    SAFE_RELEASE(gCB);
    SAFE_RELEASE(gPS);
    SAFE_RELEASE(gVS);
    SAFE_RELEASE(gLinearClamp);

    SAFE_RELEASE(gSceneSRV);
    SAFE_RELEASE(gSceneRTV);
    SAFE_RELEASE(gSceneTex);

    gDev = nullptr;
    gCtx = nullptr;
}

void PostFx_StartRadialBlurWorld(const XMFLOAT3& centerWorld,
    float durationSec,
    float strength,
    float radius,
    int sampleCount)
{
    gRadialActive = true;
    gRadialTimer = 0.0f;
    gRadialDuration = (durationSec <= 0.0f) ? 0.001f : durationSec;
    gRadialStrength = strength;
    gRadialRadius = radius;
    gRadialSamples = sampleCount;
    gRadialCenterWorld = centerWorld;
}

void PostFx_StopRadialBlur()
{
    gRadialActive = false;
    gRadialTimer = 0.0f;
}

void PostFx_Update(double dt)
{
    if (!gRadialActive) return;

    gRadialTimer += (float)dt;
    if (gRadialTimer >= gRadialDuration)
    {
        PostFx_StopRadialBlur();
        return;
    }
}

void PostFx_BeginScene(const float clearColor[4])
{
    Direct3D_SetDepthEnable(true); // ★ 加这一句：保证后续 3D 正常深度测试

    ID3D11DepthStencilView* dsv = Direct3D_GetDepthStencilView();
    gCtx->OMSetRenderTargets(1, &gSceneRTV, dsv);

    const D3D11_VIEWPORT& vp = Direct3D_GetViewport();
    gCtx->RSSetViewports(1, &vp);

    gCtx->ClearRenderTargetView(gSceneRTV, clearColor);
    gCtx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void PostFx_ApplyToBackBuffer()
{
    // 输出到 backbuffer
    ID3D11RenderTargetView* bb = Direct3D_GetBackBufferRTV();
    gCtx->OMSetRenderTargets(1, &bb, nullptr); // 后处理不需要 depth
    const D3D11_VIEWPORT& vp = Direct3D_GetViewport();
    gCtx->RSSetViewports(1, &vp);

    // 常量：默认 copy
    gCBData.enable = 0.0f;
    gCBData.strength = 0.0f;
    gCBData.radius = gRadialRadius;
    gCBData.sampleCount = (float)gRadialSamples;
    gCBData.centerUV = XMFLOAT2(0.5f, 0.5f);

    if (gRadialActive)
    {
        XMFLOAT2 uv{};
        if (Camera_ProjectWorldToScreenUV(gRadialCenterWorld, uv))
        {
            // 这里我默认做一个线性淡出，更像“冲击力消散”
            float t = gRadialTimer / gRadialDuration;
            float k = (t < 0.0f) ? 1.0f : (t > 1.0f ? 0.0f : (1.0f - t));

            gCBData.enable = 1.0f;
            gCBData.centerUV = uv;
            gCBData.strength = gRadialStrength * k;
        }
    }

    gCtx->UpdateSubresource(gCB, 0, nullptr, &gCBData, 0, 0);

    // pipeline
    Direct3D_SetDepthEnable(false);
    gCtx->OMSetBlendState(nullptr, nullptr, 0xffffffff);

    gCtx->IASetInputLayout(nullptr);
    gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    gCtx->VSSetShader(gVS, nullptr, 0);
    gCtx->PSSetShader(gPS, nullptr, 0);

    gCtx->PSSetShaderResources(0, 1, &gSceneSRV);
    gCtx->PSSetSamplers(0, 1, &gLinearClamp);
    gCtx->PSSetConstantBuffers(0, 1, &gCB);

    gCtx->Draw(3, 0);

    // 解绑 SRV，避免下一帧把它当 RTV 绑定时报 warning
    ID3D11ShaderResourceView* nullSRV = nullptr;
    gCtx->PSSetShaderResources(0, 1, &nullSRV);
}
