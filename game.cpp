/*==============================================================================

　 ゲーム本体[game.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/06/27
--------------------------------------------------------------------------------

==============================================================================*/
#include "game.h"
#include "cube.h"
#include "light.h"
#include "grid.h"
#include "camera.h"
#include <DirectXMath.h>
#include "shader3d.h"
#include "key_logger.h"
#include "sampler.h"
#include "meshfield.h"
#include "ModelStatic.h"
#include "ModelSkinned.h"
#include "AnimatorRegistry.h"
#include "model.h"
using namespace DirectX;

static float g_x = 0.0f;
static float g_angle = 0.0f;
static float g_scale = 1.0f;
static float g_AccumulatedTime = 0.0f;
static XMFLOAT3 g_CubePosition{};
static XMFLOAT3 g_CubeVelocity{};

static MODEL* g_pModelTest = nullptr;

void Game_Initialize()
{
    Camera_Initialize(
        {4.2f, 2.4f, -5.7f},
        {-0.5f, -0.3f, 0.7f},
        {0.8f, 0.0f, 0.5f});

    g_pModelTest = ModelLoad("resources/test.fbx");
}

void Game_Finalize()
{
    ModelRelease(g_pModelTest);

    Camera_Finalize();
}

void Game_Update(double elapsed_time)
{

    g_AccumulatedTime += elapsed_time;
    Camera_Update(elapsed_time);
    Cube_Update(elapsed_time);

    //ModelSkinned_Update(elapsed_time);

    //AnimatorRegistry_Update(elapsed_time);


    g_angle = g_AccumulatedTime * 3.0f;

    if (KeyLogger_IsTrigger(KK_F))
    {
        g_CubePosition = Camera_GetPosition();
        XMStoreFloat3(&g_CubeVelocity, XMLoadFloat3(&Camera_GetFront()) * 200.0f * elapsed_time);
    }

    XMVECTOR cube_position = XMLoadFloat3(&g_CubePosition);

    cube_position += XMLoadFloat3(&g_CubeVelocity) * elapsed_time;

    XMStoreFloat3(&g_CubePosition, cube_position);


}

void Game_Draw()
{
    Light_SetAmbient({0.7f, 0.7f, 0.7f});

    XMVECTOR v{ -1.0f, -1.0f, 1.0f, 0.0f };

    v = XMVector3Normalize(v);

    Light_SetDirectionWorld({1.0f, -0.6f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f});

    MeshField_Draw();

    ModelDraw(g_pModelTest, XMMatrixIdentity());

    XMMATRIX World = XMMatrixRotationY(g_angle * 0.0f);

    World *= XMMatrixTranslationFromVector(XMLoadFloat3(&g_CubePosition)); 

    Sampler_SetFillterAnisotropic();

    //Cube_Draw(World);

    World = XMMatrixTranslation(3.0f, 0.0f, 0.0f);

    Sampler_SetFillterLinear();

    //Cube_Draw(World);

    //Grid_Draw();

    XMMATRIX W = XMMatrixIdentity();
    // 如果尺度不合适可加缩放：
    //W = XMMatrixScaling(0.01f, 0.01f, 0.01f);

    ModelStatic_SetWorld(W);

    // 你已有的采样器（任选）
    Sampler_SetFillterAnisotropic();

    // 一行绘制
    //ModelStatic_Draw();

    //ModelSkinned_SetWorldMatrix(W);

    //// 你的 view/proj/光照 仍旧通过 Shader3d_* 设置
    //// ...
    //ModelSkinned_Draw();

    //W = XMMatrixScaling(0.01f, 0.01f, 0.01f);
    //AnimatorRegistry_SetWorld(W);
    //AnimatorRegistry_Draw();



	int baseCount = 5;
    float spacing = 1.0f;
    float baseY = 0.5f;
    XMMATRIX S = XMMatrixIdentity();
    
    XMMATRIX mtxTrans = XMMatrixTranslation(2.0f, 0.0f, 2.0f);
    for (int level = 0; level < baseCount; level++)
    {
    	const int side = baseCount - level;                 // 这一层一边几个方块
    	const float y = baseY + level * spacing;           // 层高（每层抬高 spacing）
    	const float start = (side - 1) * -0.5f * spacing;   // 让这一层以原点居中
    
    	for (int i = 0; i < side; i++)
    	{
    		for (int j = 0; j < side; j++)
    		{
    			float x = start + i * spacing;
    			float z = start + j * spacing;
    
    			XMMATRIX T = XMMatrixTranslation(x, y, z);
    			XMMATRIX R = XMMatrixRotationY(g_angle);
    			XMMATRIX World = S * T * R * mtxTrans; // 如需自转可插 XMMatrixRotationY(θ)
    
                //Cube_Draw(World);
    		}
    	}
    }

#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効
    Camera_DebugDraw();
#endif
}



