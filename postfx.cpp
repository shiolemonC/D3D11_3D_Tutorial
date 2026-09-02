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

static ID3D11Texture2D* gBloomTex[2]{};
static ID3D11RenderTargetView* gBloomRTV[2]{};
static ID3D11ShaderResourceView* gBloomSRV[2]{};
static UINT gBloomWidth = 1;
static UINT gBloomHeight = 1;
static constexpr float kBloomBlurScale = 0.6f;

static ID3D11SamplerState* gLinearClamp = nullptr;

static ID3D11VertexShader* gVS = nullptr;
static ID3D11PixelShader* gPS = nullptr;
static ID3D11PixelShader* gBloomExtractPS = nullptr;
static ID3D11PixelShader* gBloomBlurPS = nullptr;

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

struct BloomBlurCB
{
    XMFLOAT2 texelStep;
    XMFLOAT2 pad;
};
static ID3D11Buffer* gBloomBlurCB = nullptr;

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

static bool CreateColorTarget(UINT width, UINT height, DXGI_FORMAT format,
    ID3D11Texture2D** outTex,
    ID3D11RenderTargetView** outRTV,
    ID3D11ShaderResourceView** outSRV)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = format;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(gDev->CreateTexture2D(&td, nullptr, outTex))) return false;
    if (FAILED(gDev->CreateRenderTargetView(*outTex, nullptr, outRTV))) return false;
    if (FAILED(gDev->CreateShaderResourceView(*outTex, nullptr, outSRV))) return false;
    return true;
}

static bool CreateSceneRT()
{
    const UINT w = Direct3D_GetBackBufferWidth();
    const UINT h = Direct3D_GetBackBufferHeight();

    SAFE_RELEASE(gSceneSRV);
    SAFE_RELEASE(gSceneRTV);
    SAFE_RELEASE(gSceneTex);

    for (int i = 0; i < 2; ++i)
    {
        SAFE_RELEASE(gBloomSRV[i]);
        SAFE_RELEASE(gBloomRTV[i]);
        SAFE_RELEASE(gBloomTex[i]);
    }

    const DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    if (!CreateColorTarget(w, h, hdrFormat, &gSceneTex, &gSceneRTV, &gSceneSRV))
        return false;

    gBloomWidth = (w >= 4) ? (w / 4) : 1;
    gBloomHeight = (h >= 4) ? (h / 4) : 1;
    for (int i = 0; i < 2; ++i)
    {
        if (!CreateColorTarget(gBloomWidth, gBloomHeight, hdrFormat,
            &gBloomTex[i], &gBloomRTV[i], &gBloomSRV[i]))
            return false;
    }

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

    // Fullscreen composite + HDR bloom shaders.
    std::vector<unsigned char> vsb, psb, extractPsb, blurPsb;
    if (!LoadCSO("postfx_fullscreen_vs.cso", vsb) ||
        !LoadCSO("postfx_radialblur_ps.cso", psb) ||
        !LoadCSO("postfx_bloom_extract_ps.cso", extractPsb) ||
        !LoadCSO("postfx_bloom_blur_ps.cso", blurPsb))
    {
        MessageBox(nullptr, "Missing postfx .cso", "PostFx", MB_OK);
        return false;
    }

    if (FAILED(gDev->CreateVertexShader(vsb.data(), vsb.size(), nullptr, &gVS))) return false;
    if (FAILED(gDev->CreatePixelShader(psb.data(), psb.size(), nullptr, &gPS))) return false;
    if (FAILED(gDev->CreatePixelShader(extractPsb.data(), extractPsb.size(), nullptr, &gBloomExtractPS))) return false;
    if (FAILED(gDev->CreatePixelShader(blurPsb.data(), blurPsb.size(), nullptr, &gBloomBlurPS))) return false;

    // constant buffer
    D3D11_BUFFER_DESC cbd{};
    cbd.Usage = D3D11_USAGE_DEFAULT;
    cbd.ByteWidth = sizeof(PostFxCB);
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (FAILED(gDev->CreateBuffer(&cbd, nullptr, &gCB))) return false;

    cbd.ByteWidth = sizeof(BloomBlurCB);
    if (FAILED(gDev->CreateBuffer(&cbd, nullptr, &gBloomBlurCB))) return false;

    return true;
}

void PostFx_Finalize()
{
    SAFE_RELEASE(gBloomBlurCB);
    SAFE_RELEASE(gCB);
    SAFE_RELEASE(gBloomBlurPS);
    SAFE_RELEASE(gBloomExtractPS);
    SAFE_RELEASE(gPS);
    SAFE_RELEASE(gVS);
    SAFE_RELEASE(gLinearClamp);

    SAFE_RELEASE(gSceneSRV);
    SAFE_RELEASE(gSceneRTV);
    SAFE_RELEASE(gSceneTex);

    for (int i = 0; i < 2; ++i)
    {
        SAFE_RELEASE(gBloomSRV[i]);
        SAFE_RELEASE(gBloomRTV[i]);
        SAFE_RELEASE(gBloomTex[i]);
    }

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
    // Final composite constants.
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

    // Shared fullscreen pipeline state.
    Direct3D_SetDepthEnable(false);
    gCtx->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    gCtx->IASetInputLayout(nullptr);
    gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gCtx->VSSetShader(gVS, nullptr, 0);
    gCtx->PSSetSamplers(0, 1, &gLinearClamp);

    D3D11_VIEWPORT bloomVP{};
    bloomVP.Width = static_cast<float>(gBloomWidth);
    bloomVP.Height = static_cast<float>(gBloomHeight);
    bloomVP.MinDepth = 0.0f;
    bloomVP.MaxDepth = 1.0f;
    gCtx->RSSetViewports(1, &bloomVP);

    ID3D11ShaderResourceView* nullSRV[2]{};

    // 1) Extract HDR highlights while downsampling to quarter resolution.
    gCtx->OMSetRenderTargets(1, &gBloomRTV[0], nullptr);
    gCtx->PSSetShader(gBloomExtractPS, nullptr, 0);
    gCtx->PSSetShaderResources(0, 1, &gSceneSRV);
    gCtx->Draw(3, 0);
    gCtx->PSSetShaderResources(0, 1, nullSRV);

    // 2) Horizontal Gaussian blur.
    BloomBlurCB blurCB{};
    blurCB.texelStep = XMFLOAT2(kBloomBlurScale / gBloomWidth, 0.0f);
    gCtx->UpdateSubresource(gBloomBlurCB, 0, nullptr, &blurCB, 0, 0);

    gCtx->OMSetRenderTargets(1, &gBloomRTV[1], nullptr);
    gCtx->PSSetShader(gBloomBlurPS, nullptr, 0);
    gCtx->PSSetShaderResources(0, 1, &gBloomSRV[0]);
    gCtx->PSSetConstantBuffers(0, 1, &gBloomBlurCB);
    gCtx->Draw(3, 0);
    gCtx->PSSetShaderResources(0, 1, nullSRV);

    // 3) Vertical Gaussian blur back into bloom buffer 0.
    blurCB.texelStep = XMFLOAT2(0.0f, kBloomBlurScale / gBloomHeight);
    gCtx->UpdateSubresource(gBloomBlurCB, 0, nullptr, &blurCB, 0, 0);

    gCtx->OMSetRenderTargets(1, &gBloomRTV[0], nullptr);
    gCtx->PSSetShaderResources(0, 1, &gBloomSRV[1]);
    gCtx->Draw(3, 0);
    gCtx->PSSetShaderResources(0, 1, nullSRV);

    // 4) FXAA/radial blur + bloom composite to the back buffer.
    ID3D11RenderTargetView* bb = Direct3D_GetBackBufferRTV();
    gCtx->OMSetRenderTargets(1, &bb, nullptr);
    const D3D11_VIEWPORT& vp = Direct3D_GetViewport();
    gCtx->RSSetViewports(1, &vp);

    gCtx->PSSetShader(gPS, nullptr, 0);
    ID3D11ShaderResourceView* finalSRV[2]{ gSceneSRV, gBloomSRV[0] };
    gCtx->PSSetShaderResources(0, 2, finalSRV);
    gCtx->PSSetConstantBuffers(0, 1, &gCB);
    gCtx->Draw(3, 0);

    // Unbind both SRVs before the next frame uses them as render targets.
    gCtx->PSSetShaderResources(0, 2, nullSRV);
}
