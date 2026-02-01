#include "sky.h"
#include "model.h"
#include "shader3d_unlit.h"
#include "direct3d.h"
#include "sampler.h"

using namespace DirectX;

static MODEL* g_pModelSky{ nullptr };
static XMFLOAT3 g_position{};
static float g_scale = 500.0f; // 先给个默认

static ID3D11RasterizerState* g_pSkyRS = nullptr;
static ID3D11BlendState* g_pSkyBlendOpaque = nullptr;


void Sky_Initialize()
{
    g_pModelSky = ModelLoad("resources/skybox/sky.fbx", true);

    // 1) Sky 专用：不剔除（先确保一定能看到）
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;      // ★关键：先关剔除验证
    rd.DepthClipEnable = TRUE;
    Direct3D_GetDevice()->CreateRasterizerState(&rd, &g_pSkyRS);

    // 2) Sky 专用：关闭混合（即使 alpha=0 也能写进颜色）
    D3D11_BLEND_DESC bd{};
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    bd.RenderTarget[0].BlendEnable = FALSE; // ★关键
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Direct3D_GetDevice()->CreateBlendState(&bd, &g_pSkyBlendOpaque);
}

void Sky_Finalize()
{
    ModelRelease(g_pModelSky);

    if (g_pSkyRS) { g_pSkyRS->Release(); g_pSkyRS = nullptr; }
    if (g_pSkyBlendOpaque) { g_pSkyBlendOpaque->Release(); g_pSkyBlendOpaque = nullptr; }
}

void Sky_SetPosition(const DirectX::XMFLOAT3& position)
{
    g_position = position;
}


void Sky_SetScale(float s)
{
    g_scale = s;
}

void Sky_Draw()
{
    if (!g_pModelSky) return;

    auto* ctx = Direct3D_GetContext();

    // --- 保存旧状态 ---
    ID3D11RasterizerState* oldRS = nullptr;
    ctx->RSGetState(&oldRS);

    ID3D11BlendState* oldBS = nullptr;
    float oldFactor[4]{};
    UINT oldMask = 0xffffffff;
    ctx->OMGetBlendState(&oldBS, oldFactor, &oldMask);

    ID3D11DepthStencilState* oldDS = nullptr;
    UINT oldStencilRef = 0;
    ctx->OMGetDepthStencilState(&oldDS, &oldStencilRef);

    // --- 设置 Sky 状态 ---
    ctx->RSSetState(g_pSkyRS);                           // 关剔除
    ctx->OMSetBlendState(g_pSkyBlendOpaque, nullptr, 0xffffffff); // 关混合
    Direct3D_SetDepthEnable(false);                      // 关深度（因为 sky 先画）

    Sampler_SetFillterLinear();                          // ★保证 sampler 有绑定

    // --- 绘制 ---
    Shader3d_Unlit_Begin();

    XMMATRIX S = XMMatrixScaling(g_scale, g_scale, g_scale);
    XMMATRIX T = XMMatrixTranslationFromVector(XMLoadFloat3(&g_position));
    ModelDraw_Unlit(g_pModelSky, S * T);

    // --- 恢复旧状态 ---
    ctx->OMSetDepthStencilState(oldDS, oldStencilRef);
    ctx->OMSetBlendState(oldBS, oldFactor, oldMask);
    ctx->RSSetState(oldRS);

    if (oldDS) oldDS->Release();
    if (oldBS) oldBS->Release();
    if (oldRS) oldRS->Release();
}

