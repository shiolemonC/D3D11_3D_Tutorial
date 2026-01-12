/*==============================================================================

　 ゲームオーバー制御 [over.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/11
--------------------------------------------------------------------------------

==============================================================================*/
#include "over.h"
#include "fade.h"
#include "scene.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "direct3d.h"

static int g_OverBgTexId = -1;

void Over_Initialize()
{
    // 你把图片换成自己的资源名即可
    g_OverBgTexId = Texture_Load(L"resources/over_bg.png");
}

void Over_Finalize()
{
    //Texture_AllRelease();
}

void Over_Update(double elapsed_time)
{
    if (KeyLogger_IsTrigger(KK_ENTER))
    {
        Fade_Start(1.0, true);
    }

    if (Fade_GetState() == FADE_STATE_FINISHED_OUT)
    {
        Scene_Change(SCENE_TITLE);
    }
}

void Over_Draw()
{
    float sw = (float)Direct3D_GetBackBufferWidth();
    float sh = (float)Direct3D_GetBackBufferHeight();
    Sprite_Draw(g_OverBgTexId, 0.0f, 0.0f, sw, sh, false, { 1,1,1,1 });
}
