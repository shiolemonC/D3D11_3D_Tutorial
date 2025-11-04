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
#include "player_test.h"
#include "player_camera_test.h"
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

    Player_Test_Initialize(
        { 0.0f, 15.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f }
    );

    g_pModelTest = ModelLoad("resources/test.fbx");
}

void Game_Finalize()
{
    ModelRelease(g_pModelTest);

    Camera_Finalize();
    //PlayerCameraTest_Finalize();
}

void Game_Update(double elapsed_time)
{

    g_AccumulatedTime += elapsed_time;
    Player_Update(elapsed_time);
    Camera_Update(elapsed_time);
    //PlayerCameraTest_Update(elapsed_time);
    Cube_Update(elapsed_time);

    AnimatorRegistry_Update(elapsed_time);

    g_angle = g_AccumulatedTime * 3.0f;



}

void Game_Draw()
{
    Light_SetAmbient({0.7f, 0.7f, 0.7f});

    XMVECTOR v{ -1.0f, -1.0f, 1.0f, 0.0f };

    v = XMVector3Normalize(v);

    Light_SetDirectionWorld({1.0f, -0.6f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f});

    Light_SetSpecularWorld(Camera_GetPosition(), 2.0f, { 0.1f, 0.1f, 0.1f, 1.0f });
    MeshField_Draw();

    XMMATRIX World = XMMatrixRotationY(g_angle * 0.0f);

    World *= XMMatrixTranslationFromVector(XMLoadFloat3(&g_CubePosition)); 

    Sampler_SetFillterAnisotropic();

    //Cube_Draw(World);

    XMMATRIX kirby = XMMatrixIdentity();

    //World = XMMatrixTranslation(3.0f, 20.0f, 0.0f);

    kirby *= XMMatrixScaling(0.1f, 0.1f, 0.1f);

    kirby *= XMMatrixTranslation(0.0f, 2.0f, 0.0f);

    Light_SetAmbient({ 1.0f, 1.0f, 1.0f });
    Light_SetDirectionWorld({ 1.0f, -0.6f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });
    //Light_SetSpecularWorld(Camera_GetPosition(), 10.0f, {0.0f, 0.0f, 0.0f, 1.0f});
    //Light_SetDirectionWorld({ 1.0f, -0.6f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });

    //Light_SetPointCount(3);
    //XMVECTOR position = { 0.0f, 0.3f, -3.0f };
    //XMMATRIX rot = XMMatrixRotationY(g_angle);
    //position = XMVector3Transform(position, rot);
    //XMFLOAT3 pp;
    //XMStoreFloat3(&pp, position);

    //Light_SetPointWorld(0, pp, 5.0f, { 0.0f, 1.0f, 0.0f});
    //Light_SetPointWorld(1, { 3.0f, 0.0f, 0.0f }, 5.0f, { 1.0f, 0.0f, 0.0f});
    //Light_SetPointWorld(2, { 0.0f, 0.0f, -2.0f }, 5.0f, { 0.0f, 0.0f, 1.0f });

    Sampler_SetFillterLinear();

    //Cube_Draw(World);

    //Grid_Draw();

    XMMATRIX W = XMMatrixIdentity();
    // 如果尺度不合适可加缩放：
    //W = XMMatrixScaling(0.01f, 0.01f, 0.01f);

    ModelStatic_SetWorld(W);

    // 你已有的采样器（任选）
    Sampler_SetFillterAnisotropic();

    Player_Draw();

    AnimatorRegistry_SetWorld(W);
    AnimatorRegistry_Draw();

#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効
    Camera_DebugDraw();
#endif
}



