/*==============================================================================

　 リザルト制御 [result.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/11
--------------------------------------------------------------------------------

==============================================================================*/
#include "result.h"
#include "fade.h"
#include "scene.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "direct3d.h"
#include "input_gamepad_xinput.h"

static int g_ResultBgTexId = -1;

void Result_Initialize()
{
    g_ResultBgTexId = Texture_Load(L"resources/result_bg.png");
    input::xinput::Initialize();
}

void Result_Finalize()
{
    // 和 Title 一样：目前不做 Texture release（避免别的场景还在用）
    //Texture_AllRelease();
}

void Result_Update(double elapsed_time)
{
    input::xinput::Update();

    if (KeyLogger_IsTrigger(KK_ENTER) || input::xinput::Press(input::xinput::Button::A))
    {
        Scene_Change(SCENE_TITLE);
    }

}

void Result_Draw()
{
    float sw = (float)Direct3D_GetBackBufferWidth();
    float sh = (float)Direct3D_GetBackBufferHeight();
    Sprite_Draw(g_ResultBgTexId, 0.0f, 0.0f, sw, sh, false, { 1,1,1,1 });
}
