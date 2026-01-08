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
#include "shader3d_unlit.h"
#include "key_logger.h"
#include "sampler.h"
#include "meshfield.h"
#include "sky.h"
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
#include "sprite_anim.h"
#include "collider_system.h"
#include "direct3d.h"

#include "anim_event.h"
#include "anim_event_player.h"
#include "hitbox_system.h"
#include "boss.h"
#include "BossAnimatorRegistry.h"

#include "hud_hp.h"


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
static int g_AnimPatternId = -1;
static int g_AnimPlayId = -1;

// 玩家输入构建辅助：把键盘 + 鼠标状态 → PlayerUpdateInput
static PlayerUpdateInput BuildPlayerInput(const Mouse_State& ms)
{
    PlayerUpdateInput pin{};

    // WASD 移动
    pin.moveZ += KeyLogger_IsPressed(KK_W) ? 1.0f : 0.0f;
    pin.moveZ -= KeyLogger_IsPressed(KK_S) ? 1.0f : 0.0f;
    pin.moveX -= KeyLogger_IsPressed(KK_A) ? 1.0f : 0.0f;
    pin.moveX += KeyLogger_IsPressed(KK_D) ? 1.0f : 0.0f;

    // 攻击：鼠标左键“刚按下”
    static bool s_prevLB = false;
    bool justPressedLB = (ms.leftButton && !s_prevLB);
    s_prevLB = ms.leftButton;
    pin.attack = justPressedLB;

    // 格挡/弹反：鼠标右键“刚按下”
    static bool s_prevRB = false;
    bool justPressedRB = (ms.rightButton && !s_prevRB);
    s_prevRB = ms.rightButton;
    pin.parry = justPressedRB;

    // 翻滚：左 Shift “刚按下”
    pin.roll = KeyLogger_IsTrigger(KK_LEFTSHIFT);

    return pin;
}

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

    Sky_Initialize();
    Billboard_Initialize();
    g_TestTexid = Texture_Load(L"resources/runningman001.png");

    g_AnimPatternId = SpriteAnim_RegisterPattern(
        g_TestTexid, 10, 5, 0.1, {140,200},{0,0},true);

    g_AnimPlayId = SpriteAnim_CreatePlayer(g_AnimPatternId);


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
    BossAnimRegister();
    AnimEventRegister();   // ★ 新增：注册动画事件

    // 初始化玩家
    PlayerDesc pd{};
    pd.spawnPos = { 0,0,0 };
    pd.moveSpeed = 2.5f;
    pd.turnSharpness = 12.0f;
    pd.scale = 1.0f;
    Player_Initialize(pd);

    // 初始化 Boss 本体
    BossDesc bd{};
    bd.spawnPos = DirectX::XMFLOAT3(0.0f, 0.0f, 10.0f); // 比如在玩家前方 10 米
    bd.scale = 3.0f;
    Boss_Initialize(bd);


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

    HudHP_Initialize();

}

void Game_Finalize()
{
    ModelRelease(g_pModelTest);
    Sky_Finalize();
    Billboard_Finalize();
    Camera_Finalize();
    //PlayerCameraTest_Finalize();
}

void Game_Update(double elapsed_time)
{

    HitboxSystem_Update(static_cast<float>(elapsed_time));

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

    // 格挡/弹反：鼠标右键“刚按下”
    static bool s_prevRB = false;
    bool justPressedRB = (ms.rightButton && !s_prevRB);
    s_prevRB = ms.rightButton;

    // 1) 摄像机：由鼠标控制绕玩家旋转
    PlayerCameraInput camIn{};
    camIn.deltaX = deltaX;
    camIn.deltaY = deltaY;
    // 如果 Mouse_State 有 wheel 字段，你可以填 camIn.wheelDelta
    camIn.lockTogglePressed = ms.middleButton;
    PlayerCamera_Update(elapsed_time, camIn);

    // 2) 玩家输入（WASD）
    PlayerUpdateInput pin = BuildPlayerInput(ms);

    // 3) 从摄像机模块拿到「移动用坐标系」（按摄像机方向移动）
    PlayerCamera_GetMoveBasis(&pin.camForwardXZ, &pin.camRightXZ);

    // 4) 把所有和玩家相关的逻辑都交给 Player_Update
    Player_Update(elapsed_time, pin);

    // Boss 更新
    BossUpdateContext bctx{};
    bctx.playerPos = Player_GetPosition();
    Boss_Update(elapsed_time, bctx);

    // 5) 让底层 Camera 模块更新 view/proj（原来就有）
    Camera_Update(elapsed_time);

    Sky_SetPosition(Player_GetPosition());
    SpriteAnim_Update(elapsed_time);

    HudHP_Update(elapsed_time);
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

    //Cube_Draw(World);

    XMMATRIX kirby = XMMatrixIdentity();

    //World = XMMatrixTranslation(3.0f, 20.0f, 0.0f);

    kirby *= XMMatrixScaling(0.1f, 0.1f, 0.1f);
    kirby *= XMMatrixRotationX(90.0f);
    kirby *= XMMatrixTranslation(4.0f, 0.5f, 2.0f);

    //ModelDraw(g_pModelTreeTest, kirby);

    XMMATRIX tree = XMMatrixIdentity();

    //tree *= XMMatrixTranslation(3.0f, 0.0f, 5.0f);

    tree *= XMMatrixScaling(0.1f, 0.1f, 0.1f);
    tree *= XMMatrixRotationX(90.0f);
    tree *= XMMatrixRotationY(45.0f);
    tree *= XMMatrixTranslation(-5.0f, 0.0f, 0.0f);

    //ModelDraw(g_pModelTreeTest, tree);

    //Direct3D_SetDepthEnable(false);
    //Sky_Draw();
    //Direct3D_SetDepthEnable(true);

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

    // 再画 Boss
    BossAnimatorRegistry_Draw();

    //Billboard_Draw(g_TestTexid, { -2.0f, 2.5f, 2.0f }, { 1.5f, 2.0f }, {140.0f * 3, 200.0f, 140.0f, 200.0f}, { 0.0f, 0.0f });
    //BillboardAnim_Draw(g_AnimPlayId, { -2.0f, 2.5f, 2.0f }, { 1.5f, 2.0f },  { 0.0f, 0.0f });

#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効
    PlayerSM_DebugDraw();
    //Camera_DebugDraw();
    GetCollisionWorld().DebugDraw3D();

#endif

    HudHP_Draw();

}



