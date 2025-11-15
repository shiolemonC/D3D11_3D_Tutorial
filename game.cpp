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
#include "player_sm_condition.h"
#include "AnimatorRegistry.h"
#include "model.h"
#include "player_test.h"
#include "player_camera_test.h"
#include "mouse.h"
#include "billboard.h"
#include "texture.h"
using namespace DirectX;

static float g_x = 0.0f;
static float g_angle = 0.0f;
static float g_scale = 1.0f;
static float g_AccumulatedTime = 0.0f;
static XMFLOAT3 g_CubePosition{};
static XMFLOAT3 g_CubeVelocity{};

static MODEL* g_pModelTest = nullptr;
static MODEL* g_pModelTreeTest = nullptr;

static int g_TestTexid = -1;

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

    Billboard_Initialize();
    g_TestTexid = Texture_Load(L"resources/runningman001.png");

    g_pModelTest = ModelLoad("resources/fbx/larva.fbx", true);
    g_pModelTreeTest = ModelLoad("resources/fbx/larva.fbx", true);

    if (!g_pModelTreeTest)
    {
        OutputDebugStringA("[DRAW TREE] Failed to load.\n");
    }
    else
    {
        OutputDebugStringA("[DRAW TREE] Successed to load.\n");
    }

    // 注册动画（你已有的）
    AnimRegister();

    // 初始化玩家
    PlayerDesc pd{};
    pd.spawnPos = { 0,0,0 };
    pd.moveSpeed = 2.5f;
    pd.turnSharpness = 12.0f;
    pd.scale = 1.0f;
    Player_Initialize(pd);

    Cond_Init(/*defaultTriggerBufferSec*/ 0.15f);
    if (!PlayerSM_LoadConfigJSON(L"resources/fsm_player.json")) {
        OutputDebugStringA("[PlayerSM] Failed to load 'resources/fsm_player.json'. Falling back to built-in defaults.\n");
        PlayerSM_LoadConfigDefaults();   // 读不到就回退默认 Idle/Move
    }
    PlayerSM_Reset();                // 初始状态=Idle
    // 播放初始动画
    auto out0 = PlayerSM_Update(0.0);
    AnimatorRegistry_Play(out0.clip, nullptr);

    // 相机跟随
    PlayerCamera_Initialize({});
}

void Game_Finalize()
{
    ModelRelease(g_pModelTest);
    Billboard_Finalize();
    Camera_Finalize();
    //PlayerCameraTest_Finalize();
}

void Game_Update(double elapsed_time)
{

    g_AccumulatedTime += elapsed_time;
    Cube_Update(elapsed_time);

    // 采集鼠标
    Mouse_State ms{};
    Mouse_GetState(&ms);

    // 计算鼠标移动量（假设 Mouse_State 有 x/y，你根据自己结构改一下）
    static int s_prevMouseX = 0;
    static int s_prevMouseY = 0;
    float deltaX = static_cast<float>(ms.x - s_prevMouseX);
    float deltaY = static_cast<float>(ms.y - s_prevMouseY);
    s_prevMouseX = ms.x;
    s_prevMouseY = ms.y;

    // 简单的“刚按下”检测（边沿）
    static bool s_prevLB = false;
    bool justPressedLB = (ms.leftButton && !s_prevLB);
    s_prevLB = ms.leftButton;

    // 1) 摄像机：由鼠标控制绕玩家旋转
    PlayerCameraInput camIn{};
    camIn.deltaX = deltaX;
    camIn.deltaY = deltaY;
    // 如果 Mouse_State 有 wheel 字段，你可以填 camIn.wheelDelta
    PlayerCamera_Update(elapsed_time, camIn);

    // 2) 玩家输入（WASD）
    PlayerUpdateInput pin{};
    pin.moveZ += KeyLogger_IsPressed(KK_W) ? 1.0f : 0.0f;
    pin.moveZ -= KeyLogger_IsPressed(KK_S) ? 1.0f : 0.0f;
    pin.moveX -= KeyLogger_IsPressed(KK_A) ? 1.0f : 0.0f;
    pin.moveX += KeyLogger_IsPressed(KK_D) ? 1.0f : 0.0f;

    pin.attack = justPressedLB; // 攻击输入交给 Player_Update 里触发 FSM

    // 3) 从摄像机模块拿到「移动用坐标系」（按摄像机方向移动）
    PlayerCamera_GetMoveBasis(&pin.camForwardXZ, &pin.camRightXZ);

    // 4) 把所有和玩家相关的逻辑都交给 Player_Update
    Player_Update(elapsed_time, pin);

    // 5) 让底层 Camera 模块更新 view/proj（原来就有）
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
    World *= XMMatrixTranslation(0.0f, 0.5f, 2.0f);

    Sampler_SetFillterAnisotropic();

    Cube_Draw(World);

    XMMATRIX kirby = XMMatrixIdentity();

    //World = XMMatrixTranslation(3.0f, 20.0f, 0.0f);

    kirby *= XMMatrixScaling(0.1f, 0.1f, 0.1f);
    kirby *= XMMatrixRotationX(90.0f);
    kirby *= XMMatrixTranslation(4.0f, 0.5f, 2.0f);

    ModelDraw(g_pModelTreeTest, kirby);

    XMMATRIX tree = XMMatrixIdentity();

    //tree *= XMMatrixTranslation(3.0f, 0.0f, 5.0f);

    tree *= XMMatrixScaling(0.1f, 0.1f, 0.1f);
    tree *= XMMatrixRotationX(90.0f);
    tree *= XMMatrixRotationY(45.0f);
    tree *= XMMatrixTranslation(-5.0f, 0.0f, 0.0f);

    ModelDraw(g_pModelTreeTest, tree);


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

    Billboard_Draw(g_TestTexid, { -2.0f, 2.5f, 2.0f }, 1.5f, 2.0f, {0.0f, 0.0f});

#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効
    PlayerSM_DebugDraw();
    //Camera_DebugDraw();
#endif


}



