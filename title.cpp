/*==============================================================================

　 タイトル制御 [title.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/11
--------------------------------------------------------------------------------

==============================================================================*/
#include "title.h"
#include "fade.h"
#include "scene.h"
#include "texture.h"
#include "sprite.h"
#include "key_logger.h"
#include "direct3d.h"


enum TitleState
{
    TITLE_STATE_FADE_IN,
    TITLE_STATE_FIGHTER_SHOW,
};

static TitleState g_State = {};

static int g_TitleBgTexId = -1;
static int g_TitleTexId = -1;

void Title_Initialize()
{
    g_TitleBgTexId = Texture_Load(L"resources/title_bg.png");
    g_TitleTexId = Texture_Load(L"resources/title_title.png");
}

void Title_Finalize()
{
    //Texture_AllRelease();
}

void Title_Update(double elapsed_time)
{
    if (KeyLogger_IsTrigger(KK_ENTER))
    {
        Scene_Change(SCENE_GAME);
    }
}

void Title_Draw()
{
    float sw = (float)Direct3D_GetBackBufferWidth();
    float sh = (float)Direct3D_GetBackBufferHeight();
    Sprite_Draw(g_TitleBgTexId, 0.0f, 0.0f, sw, sh, false, { 1,1,1,1 });

    //Sprite_Draw(g_TitleTexId, 0.0f, 0.0f);
}
