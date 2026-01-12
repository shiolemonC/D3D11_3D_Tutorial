#include "sky.h"
#include "model.h"
#include "shader3d_unlit.h"
using namespace DirectX;

static MODEL* g_pModelSky{ nullptr };
static XMFLOAT3 g_position{};
static float g_scale = 500.0f; // 先给个默认


void Sky_Initialize()
{
    g_pModelSky = ModelLoad("resources/sky.fbx", true);
}

void Sky_Finalize()
{
    ModelRelease(g_pModelSky);
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
    Shader3d_Unlit_Begin();

    XMMATRIX S = XMMatrixScaling(g_scale, g_scale, g_scale);
    XMMATRIX T = XMMatrixTranslationFromVector(XMLoadFloat3(&g_position));

    // row-vector 乘法习惯下：v * (S*T) = 先缩放再平移
    ModelDraw_Unlit(g_pModelSky, S * T);
}

