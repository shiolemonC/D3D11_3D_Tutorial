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


enum TitleState
{
    TITLE_STATE_FADE_IN,
    TITLE_STATE_FIGHTER_SHOW,
};

static TitleState g_State = {};

static int g_TitleBgTexId = -1;

void Title_Initialize()
{
    g_TitleBgTexId = Texture_Load(L"resources/Starting_Title_2.png");
}

void Title_Finalize()
{
    //Texture_AllRelease();
}

void Title_Update(double elapsed_time)
{
    if (KeyLogger_IsTrigger(KK_ENTER))
    {
        Fade_Start(1.0, true);
    }

    if (Fade_GetState() == FADE_STATE_FINISHED_OUT)
    {
        Scene_Change(SCENE_GAME);
    }
}

void Title_Draw()
{
    Sprite_Draw(g_TitleBgTexId, 0.0f, 0.0f);
}
