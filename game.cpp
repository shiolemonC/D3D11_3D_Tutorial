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
#include "player.h"
#include "player_state.h"
#include "player_camera.h"
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

    // 注册动画（你已有的）
    AnimRegister();

    // 初始化玩家
    PlayerDesc pd{};
    pd.spawnPos = { 0,0,0 };
    pd.moveSpeed = 2.5f;
    pd.turnSharpness = 12.0f;
    pd.scale = 1.0f;
    Player_Initialize(pd);

    // ★ 开局强制播默认动画（避免第一帧没动画）
    AnimatorRegistry_Play(L"Idle", nullptr);
    // 相机跟随
    PlayerCamera_Initialize({});
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
    Cube_Update(elapsed_time);

    // 1) 采集输入 → PlayerInput
    PlayerInput pin{};
    pin.moveZ += KeyLogger_IsPressed(KK_W) ? 1.0f : 0.0f;
    pin.moveZ -= KeyLogger_IsPressed(KK_S) ? 1.0f : 0.0f;
    pin.moveX -= KeyLogger_IsPressed(KK_A) ? 1.0f : 0.0f;
    pin.moveX += KeyLogger_IsPressed(KK_D) ? 1.0f : 0.0f;

    // 2) 状态机（决定 Idle / Walk，并输出动画名）
    auto sm = PlayerSM_Update(elapsed_time, pin);

    // 3) 动画切换（仅在状态改变时）
    if (sm.animChanged && sm.animName) {
        bool changed = false;
        AnimatorRegistry_Play(sm.animName, &changed /*outChanged*/);
    }

    // 4) Player 推进（写世界矩阵进 AnimatorRegistry）
    Player_Update(elapsed_time, pin, sm.state);

    // 5) 骨骼更新（若动画是 UseAnimDelta，会在内部累积根Δ并作用到模型）
    AnimatorRegistry_Update(elapsed_time);

    // 6) 消费根Δ并同步回 Player（把 Player 当“真值源”）
    RootMotionDelta rm{};
    if (AnimatorRegistry_ConsumeRootMotionDelta(&rm)) {
        // 一般只保留XZ，避免动画上下起伏带来穿地感
        rm.pos.y = 0.0f;
        Player_ApplyRootMotionDelta(rm);
    }

    // 7) 相机跟随
    PlayerCamera_Update(elapsed_time);

    // 8) （仍保留）Camera_Update：它会根据当前 Pose 生成 view/proj 并下发
    Camera_Update(elapsed_time);

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

    //Player_Draw();

    //AnimatorRegistry_SetWorld(W);
    AnimatorRegistry_Draw();

#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効
    Camera_DebugDraw();
#endif
}



