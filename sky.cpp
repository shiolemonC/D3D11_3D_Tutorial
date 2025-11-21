#include "sky.h"
#include "model.h"
#include "shader3d_unlit.h"
using namespace DirectX;

static MODEL* g_pModelSky{ nullptr };
static XMFLOAT3 g_position{};


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

void Sky_Draw()
{
    Shader3d_Unlit_Begin();

    ModelDraw_Unlit(g_pModelSky, XMMatrixTranslationFromVector(XMLoadFloat3(&g_position)));
}
