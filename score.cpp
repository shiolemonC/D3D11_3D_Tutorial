/*==============================================================================

　 スコア管理 [score.cpp]
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/09
--------------------------------------------------------------------------------

==============================================================================*/
#include "score.h"
#include "texture.h"
#include "sprite.h"
#include <algorithm>

static unsigned int g_Score = 0;
static unsigned int g_ViewScore = 0;

static int g_digit = 1;
static unsigned int g_CounterStop = 1;
static float g_x = 0.0f;
static float g_y = 0.0f;
static int g_ScoreTexId = -1;
static constexpr int SCORE_FONT_WIDTH = 32;
static constexpr int SCORE_FONT_HEIGHT = 32;

static void drawNumber(float x, float y, int number);

void Score_Initialize(float x, float y, int digit)
{
    g_Score = 0;
    g_ViewScore = 0;
    g_digit = digit;
    g_x = x;
    g_y = y;

    for (int i = 0; i < digit; i++)
    {
        g_CounterStop *= 10;
    }
    g_CounterStop--;

    g_ScoreTexId = Texture_Load(L"consolab_ascii_512.png");
}

void Score_Finalize()
{
}

void Score_Update() // 1点1点得点増えるの演出
{
    g_ViewScore = std::min(g_ViewScore + 1, g_Score);
}

void Score_Draw()
{
    unsigned int temp = std::min(g_ViewScore, g_CounterStop);

    for (int i = 0; i < g_digit; i++)
    {
        int n = temp % 10;
        float x = g_x + (g_digit - i - 1) * SCORE_FONT_WIDTH;

        drawNumber(x, g_y, n);

        temp /= 10;
    }
}

unsigned int Score_GetScore()
{
	return g_Score;
}

void Score_AddScore(int score)
{
    g_ViewScore = g_Score;
    g_Score += score;
}

void Score_Reset()
{
    g_Score = 0;
}

static void drawNumber(float x, float y, int number)
{
    Sprite_Draw(g_ScoreTexId, x, y, SCORE_FONT_WIDTH * number, SCORE_FONT_HEIGHT, SCORE_FONT_WIDTH, SCORE_FONT_HEIGHT);
}