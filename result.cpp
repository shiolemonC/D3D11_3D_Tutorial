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

static int g_ResultBgTexId = -1;

void Result_Initialize()
{
    // 你把图片换成自己的资源名即可
    g_ResultBgTexId = Texture_Load(L"resources/result_bg.png");
}

void Result_Finalize()
{
    // 和 Title 一样：目前不做 Texture release（避免别的场景还在用）
    //Texture_AllRelease();
}

void Result_Update(double elapsed_time)
{
    // 回到 Title（或你想改成 SCENE_GAME 重新开始也行）
    if (KeyLogger_IsTrigger(KK_ENTER))
    {
        Fade_Start(1.0, true); // true: fade-out（沿用你的 Title 写法）
    }

    if (Fade_GetState() == FADE_STATE_FINISHED_OUT)
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
