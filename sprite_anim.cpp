/*==============================================================================

   スプライトアニメーション描画 [sprite_anim.cpp]
														 Author : Youhei Sato
														 Date   : 2025/06/17
--------------------------------------------------------------------------------

==============================================================================*/
#include "sprite_anim.h"
#include "sprite.h"
#include "texture.h"
#include <DirectXMath.h>
using namespace DirectX;


struct AnimPatternData
{
	int m_TextureId = -1;    // テクスチャID
	int m_PatternMax = 0;    // アニメのパターン数
	int m_HPatternMax = 0;    // アニメの横方向のパターン数
	XMUINT2 m_StartPosition = { 0, 0 }; // アニメーションのスタート座標
	XMUINT2 m_PatternSize = { 0, 0 };   // 1パターンのサイズ
	double m_seconds_per_pattern = 0.1; // 1パターンの再生時間
	bool m_IsLooped = true;  // ループするか
};

struct AnimPlayData
{
	int m_PatternId = -1;    // アニメーションパターンID、使うパターンはどっち
	int m_PatternNum = 0;    // 現在再生中のパターン番号、常に変わっている
	double m_Accumulated_time = 0.0; // 累積時間
	bool m_IsStopped = false;
};

static constexpr int ANIM_PATTERN_MAX = 128;
static AnimPatternData g_AnimPattern[ANIM_PATTERN_MAX];
static constexpr int ANIM_PLAY_MAX = 256;
static AnimPlayData g_AnimPlayData[ANIM_PLAY_MAX];

void SpriteAnim_Initialize()
{
	// アニメーションパターン管理情報を初期化（全て利用していない）状況にする

	for (AnimPatternData& data : g_AnimPattern)
	{
		data.m_TextureId = -1; // -1だったら使えない
	}

	for (AnimPlayData& data : g_AnimPlayData)
	{
		data.m_PatternId = -1; // -1だったら使えない
		data.m_IsStopped = false;
	}
}

void SpriteAnim_Finalize()
{
}

void SpriteAnim_Update(double elapsed_time)
{
	// 0.1秒以上たったらアニメーションを進める
	for (int i = 0; i < ANIM_PLAY_MAX; i++)
	{
		if (g_AnimPlayData[i].m_PatternId < 0) continue; // データがないところで再生必要がない

		// m_PatternIdを使えば、同じアニメを複数の再生データで使い回せる
		// g_AnimPattern[i]にすると固定されて、柔軟に切り替えられなくなる
		AnimPatternData* pAnimPatternData = &g_AnimPattern[g_AnimPlayData[i].m_PatternId]; // 長すぎて縮めようにする

		if (g_AnimPlayData[i].m_Accumulated_time >= pAnimPatternData->m_seconds_per_pattern) // 累積時間がパターンの再生時間を超えたら
		{
			g_AnimPlayData[i].m_PatternNum++; // パターン数を次のパターンに更新

			if (g_AnimPlayData[i].m_PatternNum >= pAnimPatternData->m_PatternMax) // 最後のパターンに来たら
			{
				if (pAnimPatternData->m_IsLooped)
				{
					g_AnimPlayData[i].m_PatternNum = 0; // ループするなら最初のパターン番号に戻す
				}
				else
				{
					g_AnimPlayData[i].m_PatternNum = pAnimPatternData->m_PatternMax - 1; // ループしないとそのまま保つ
					g_AnimPlayData[i].m_IsStopped = true;
				}
			}
			
			g_AnimPlayData[i].m_Accumulated_time -= pAnimPatternData->m_seconds_per_pattern;
		}

		g_AnimPlayData[i].m_Accumulated_time += elapsed_time; // 経過時間を加算する
	}
}

void SpriteAnim_Draw(int playid, float dx, float dy, float dw, float dh)
{
	int anim_pattern_id = g_AnimPlayData[playid].m_PatternId;
	AnimPatternData* pAnimPatternData = &g_AnimPattern[anim_pattern_id];

	// 同一行の画像をアニメにする、sprite.cppの関数を使う
	Sprite_Draw(pAnimPatternData->m_TextureId,
		dx, dy, dw, dh,
		pAnimPatternData->m_StartPosition.x + pAnimPatternData->m_PatternSize.x * (g_AnimPlayData[playid].m_PatternNum % pAnimPatternData->m_HPatternMax),
		pAnimPatternData->m_StartPosition.y + pAnimPatternData->m_PatternSize.y * (g_AnimPlayData[playid].m_PatternNum / pAnimPatternData->m_HPatternMax),
		pAnimPatternData->m_PatternSize.x,
		pAnimPatternData->m_PatternSize.y
	);
}

int SpriteAnim_RegisterPattern(int texId, int patternMax, int h_pattern_max, double seconds_per_pattern,
	const DirectX::XMUINT2& pattern_size,
	const DirectX::XMUINT2& start_position, bool is_looped)
{
	// 前がInitialize()の中に書いているコードが別の関数に独立する

	for (int i = 0; i < ANIM_PATTERN_MAX; i++)
	{
		// 空いてる場所を探す
		if (g_AnimPattern[i].m_TextureId >= 0) continue;

		g_AnimPattern[i].m_TextureId = texId;
		g_AnimPattern[i].m_PatternMax = patternMax;
		g_AnimPattern[i].m_HPatternMax = h_pattern_max;
		g_AnimPattern[i].m_seconds_per_pattern = seconds_per_pattern;
		g_AnimPattern[i].m_PatternSize = pattern_size;
		g_AnimPattern[i].m_StartPosition = start_position;
		g_AnimPattern[i].m_IsLooped = is_looped;

		return i; // 管理番号を戻す
	}

	return -1;
}

int SpriteAnim_CreatePlayer(int anim_pattern_id)
{
	for (int i = 0; i < ANIM_PLAY_MAX; i++)
	{
		if (g_AnimPlayData[i].m_PatternId >= 0) continue; // 空いてない
		
		g_AnimPlayData[i].m_PatternId = anim_pattern_id;
		g_AnimPlayData[i].m_Accumulated_time = 0.0;
		g_AnimPlayData[i].m_PatternNum = 0;

		// アニメ再生データを再利用する場合、前回の停止状態が残っている可能性がある。
		// 新しく再生を開始するため、明示的に再生中（false）にリセットする必要がある。
		g_AnimPlayData[i].m_IsStopped = false;

		return i;
	}

	return -1;
}

bool SpriteAnim_IsStopped(int index)
{
	return g_AnimPlayData[index].m_IsStopped;
}

void SpriteAnim_DestroyPlayer(int index)
{
	g_AnimPlayData[index].m_PatternId = -1;
}
