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
#include "input_gamepad_xinput.h"
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
#include "sprite_effect.h"
#include "collider_system.h"
#include "direct3d.h"

#include "anim_event.h"
#include "anim_event_player.h"
#include "hitbox_system.h"
#include "boss.h"
#include "BossAnimatorRegistry.h"

#include "hud_hp.h"
#include "shadow_map.h"

#include "vfx_config.h"
#include "particle_system.h"
#include "sprite.h"


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

// ===== Pause overlay resources =====
static int s_pauseOverlayTex = -1;   
static bool s_pauseOverlayLoaded = false;
static bool g_GamePaused = false;

bool Game_IsPaused() { return g_GamePaused; }

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
    VfxConfig_Initialize();
    ParticleSystem_Initialize();
    //g_TestTexid = Texture_Load(L"resources/runningman001.png");

    //g_AnimPatternId = SpriteAnim_RegisterPattern(
    //    g_TestTexid, 10, 5, 0.1, {140,200},{0,0},true);

    //g_AnimPlayId = SpriteAnim_CreatePlayer(g_AnimPatternId);

    input::xinput::Initialize();

    SpriteSheetDesc hit{};
    hit.path = L"resources/fx/hit_effect.png";
    hit.cols = 11;
    hit.rows = 1;
    hit.frameCount = 0;          // 0=cols*rows
    hit.secondsPerFrame = 0.04;
    hit.looped = false;

    SpriteSheetDesc parry{};
    parry.path = L"resources/fx/parry_success_effect.png";
    parry.cols = 1;
    parry.rows = 6;
    parry.secondsPerFrame = 0.05;
    parry.looped = false;

    SpriteEffect_Initialize(hit, parry);


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
    bd.spawnPos = DirectX::XMFLOAT3(0.0f, 0.0f, 20.0f); // 比如在玩家前方 10 米
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
    Sky_SetScale(10.0f);   // 想大就调大

    ShadowMap_Initialize(Direct3D_GetDevice(), Direct3D_GetContext(), 2048);
    ShadowMap_SetParams(0.0025f, 1.0f);

    s_pauseOverlayTex = Texture_Load(L"resources/ui/controller_setting.png");

    s_pauseOverlayLoaded = true;
}

void Game_Finalize()
{
    //ModelRelease(g_pModelTest);
    Sky_Finalize();
    Billboard_Finalize();
    Camera_Finalize();
    ParticleSystem_Finalize();
    VfxConfig_Finalize();
    //PlayerCameraTest_Finalize();
    ShadowMap_Finalize();
    input::xinput::StopVibration();
}

void Game_Update(double elapsed_time)
{
    // --- 1) 仍然允许读取输入（用于恢复）---
    input::xinput::Update();
    input::xinput::UpdateVibration(elapsed_time);

    // ESC / START（Options）切换暂停
    bool pausePressed =
        KeyLogger_IsTrigger(KK_ESCAPE) ||
        input::xinput::Press(input::xinput::Button::Start);

    if (pausePressed)
    {
        g_GamePaused = !g_GamePaused;

        // 可选：暂停瞬间立刻停震动，避免“暂停还在嗡嗡”
        if (g_GamePaused) input::xinput::StopVibration();
    }

    // --- 2) 暂停：冻结游戏逻辑（不推进时间）---
    if (g_GamePaused)
    {
        return;
    }

    HitboxSystem_Update(static_cast<float>(elapsed_time));

    g_AccumulatedTime += elapsed_time;
    Cube_Update(elapsed_time);

    //// 手柄输入
    //input::xinput::Update();
    //input::xinput::UpdateVibration(elapsed_time);
    const auto& gp = input::xinput::GetState();

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

    //右摇杆 -> 虚拟鼠标 delta（驱动镜头旋转）
    float stickDx = 0.0f, stickDy = 0.0f;
    input::xinput::RightStickToMouseDelta(elapsed_time, 480.0f, 400.0f, stickDx, stickDy);
    camIn.deltaX += stickDx;
    camIn.deltaY += stickDy;

    //R3(右摇杆按下) = 鼠标中键锁定
    camIn.lockTogglePressed = ms.middleButton || input::xinput::Press(input::xinput::Button::R3);

    PlayerCamera_Update(elapsed_time, camIn);

    // 2) 玩家输入（WASD）
    PlayerUpdateInput pin = BuildPlayerInput(ms);

    if (gp.connected)
    {
        pin.moveX += gp.lx;   // 左右
        pin.moveZ += gp.ly;   // 前后（W/S）

        // 映射按键：X=攻击, Y=格挡, B=翻滚
        pin.attack = pin.attack || input::xinput::Press(input::xinput::Button::X);
        pin.parry = pin.parry || input::xinput::Press(input::xinput::Button::Y);
        pin.roll = pin.roll || input::xinput::Press(input::xinput::Button::B);

        // （如果你有 pin.confirm / UI 交互，可用 A）
        // pin.confirm = pin.confirm || input::xinput::Press(input::xinput::Button::A);
    }

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

    //Sky_SetPosition(Player_GetPosition());//
    Sky_SetPosition(Camera_GetPosition());

    ParticleSystem_Update(elapsed_time);
    SpriteAnim_Update(elapsed_time);
    SpriteEffect_Update(elapsed_time);


    HudHP_Update(elapsed_time);
}

void Game_Draw()
{
    //========================
    // 0) Shadow Pass：先生成 shadow map
    //========================
    {
        // 方向与 MeshField 那边保持一致
        DirectX::XMFLOAT3 lightDir = { 1.0f, -0.6f, 0.0f };

        // MeshField 大致范围：x=[0..50], z=[0..25]，中心大概 (25,0,12.5)
        DirectX::XMFLOAT3 center = { 25.0f, 0.0f, 12.5f };
        float radius = 40.0f; // 覆盖整个场地（先保守点）

        ShadowMap_BeginDirectional(lightDir, center, radius);

//#if defined(_DEBUG)
//        ID3D11PixelShader* ps = nullptr;
//        Direct3D_GetContext()->PSGetShader(&ps, nullptr, nullptr);
//        OutputDebugStringA(ps ? "[ShadowPass] PS != nullptr BEFORE draw\n"
//            : "[ShadowPass] PS == nullptr BEFORE draw\n");
//        if (ps) ps->Release();
//#endif

        // Shadow pass：把 PS 关掉（只写深度）
        Shader3d_SetShadowPass(true);
        AnimatorRegistry_Draw();
                   
//#if defined(_DEBUG)
//        Direct3D_GetContext()->PSGetShader(&ps, nullptr, nullptr);
//        OutputDebugStringA(ps ? "[ShadowPass] PS != nullptr AFTER draw\n"
//            : "[ShadowPass] PS == nullptr AFTER draw\n");
//        if (ps) ps->Release();
//#endif

        BossAnimatorRegistry_Draw();
        Shader3d_SetShadowPass(false);
        ShadowMap_End();

        // 还原给主相机的 view/proj（避免主 pass 玩家/Boss 的 view/proj 仍然是 light 的）
        Shader3d_SetViewMatrix(DirectX::XMLoadFloat4x4(&Camera_GetMatrix()));
        Shader3d_SetProjectionMatrix(DirectX::XMLoadFloat4x4(&Camera_GetPerspectiveMatrix()));

        // ★在这里插入 Sky 的 View/Proj + Sky_Draw
        Shader3d_Unlit_SetViewMatrix(DirectX::XMLoadFloat4x4(&Camera_GetMatrix()));
        Shader3d_Unlit_SetProjectionMatrix(DirectX::XMLoadFloat4x4(&Camera_GetPerspectiveMatrix()));
        Sky_Draw();
    }

    //========================
    // 1) Main Pass：画地面（绑定 shadow map 给 MeshField PS）
    //========================
    ShadowMap_BindForFieldPS();

    // Base lighting
// 方向光：建议先归一化（至少别让强度随向量长度变）
    //DirectX::XMFLOAT4 dir = { 1.0f, -0.6f, 0.0f, 0.0f };

    //// 1) 环境光：从 1.0 降到 0.06~0.20 区间（越低对比越强）
    //Light_SetAmbient({ 0.3f, 0.3f, 0.3f });

    //// 2) 方向光颜色：别用黑；先用接近白或略偏暖
    //Light_SetDirectionWorld(dir, { 1.0f, 0.98f, 0.95f, 1.0f });

    //// 3) 高光：你现在传 2.0 会让高光很“糊一片”，更容易显平
    ////    先试 32~128（越大越锐利）
    //Light_SetSpecularWorld(Camera_GetPosition(), 5.0f, { 0.20f, 0.20f, 0.20f, 1.0f });
// 先把环境光压低一点，让点光/方向光更“立体”
    Light_SetAmbient({ 0.25f, 0.25f, 0.25f });

    Light_SetDirectionWorld({ -0.857f, -0.514f, 0.0f, 0.0f },
        { 0.25f, 0.85f, 1.00f, 1.0f }); // 偏冷：B 更高

    // 高光强一点更容易看出 normal 细节
    Light_SetSpecularWorld(Camera_GetPosition(), 2.0f, { 0.20f, 0.20f, 0.20f, 1.0f });

    // ---- 关键：点光源数量改成 3（最多 4）----
    Light_SetPointCount(0);


    MeshField_Draw();

    // （可选）解绑，避免下一帧 BeginDirectional 绑定 DSV 前还挂着 SRV
    ShadowMap_UnbindForFieldPS();

    //========================
    // 2) 后续你的原逻辑：画玩家/Boss 等
    //========================
    Light_SetAmbient({ 0.5f, 0.5f, 0.5f });
    Light_SetDirectionWorld({ 0.857f, -0.514f, 0.0f, 0.0f },
        { 0.55f, 0.65f, 1.00f, 1.0f }); // 偏冷：B 更高

    // ---- 关键：点光源数量改成 3（最多 4）----
    Light_SetPointCount(2);

    XMFLOAT3 boss = Boss_GetPosition();
    XMFLOAT3 player = Player_GetPosition();

    // Fill light：偏冷，从左前补
    Light_SetPointWorld(0,
        { boss.x - 2.2f, boss.y + 3.2f, boss.z + 2.0f },
        7.0f,
        { 0.2f, 0.1f, 0.1f });

    // Fill light：偏冷，从左前补
    Light_SetPointWorld(1,
        { player.x - 1.2f, player.y + 1.6f, player.z + 1.0f },
        7.0f,
        { 0.3f, 0.2f, 0.2f });

    Sampler_SetFillterLinear();
    Sampler_SetFillterAnisotropic();

    AnimatorRegistry_Draw();
    BossAnimatorRegistry_Draw();

#if defined(DEBUG) || defined(_DEBUG)
    //PlayerSM_DebugDraw();
    //GetCollisionWorld().DebugDraw3D();
#endif
    ParticleSystem_DrawWorld();
    SpriteEffect_Draw();   // ★特效在这里画
    HudHP_Draw();

    if (Game_IsPaused())
    {
        const float sw = (float)Direct3D_GetBackBufferWidth();
        const float sh = (float)Direct3D_GetBackBufferHeight();

        // 1) 黑幕
        //Sprite_DrawRect(0.0f, 0.0f, sw, sh, { 0.0f, 0.0f, 0.0f, 0.55f });

        // 2) 在黑幕上画暂停图片（居中）
        if (s_pauseOverlayTex != -1)
        {
            // 用“显式尺寸”的 Sprite_Draw 重载，避免 Texture_Width/Height=0 导致画不出来
            const float w = sw * 0.60f;
            const float h = sh * 0.20f;
            const float x = (sw - w) * 0.5f;
            const float y = (sh - h) * 0.35f;

            Sprite_Draw(s_pauseOverlayTex, 0.0f, 0.0f, sw, sh, false, { 1,1,1,1 });

        }

    }
}




