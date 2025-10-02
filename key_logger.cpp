/*==============================================================================

　 キーボード入力の機能[key_logger.cpp]
														 Author : Youhei Sato
														 Date   : 2025/06/27
--------------------------------------------------------------------------------

==============================================================================*/
#include "key_logger.h"

static Keyboard_State g_PrevState{};
static Keyboard_State g_TriggerState{};
static Keyboard_State g_ReleaseState{};


void KeyLogger_Initialize()
{
	Keyboard_Initialize();
}

void KeyLogger_Update()
{
	const Keyboard_State* pState = Keyboard_GetState(); // 現在のキー入力状態を取得（押されているキーを取得）
	LPBYTE pn = (LPBYTE)pState;             // 現在のキー状態をバイト配列として扱う（比較のため）
	LPBYTE pp = (LPBYTE)&g_PrevState;       // 前のフレームのキー状態（押下・離上の判定に使う）
	LPBYTE pt = (LPBYTE)&g_TriggerState;    // 今回のフレームで「新たに押された」キーを記録する配列
	LPBYTE pr = (LPBYTE)&g_ReleaseState;    // 今回のフレームで「離された（押していたが今は離された）」キーを記録する配列

	for (int i = 0; i < sizeof(Keyboard_State); i++)
	{
		// 0が押してる、1が押してない
		// 0 1 -> 1
		// 1 0 -> 0
		// 1 1 -> 0
		// 0 0 -> 0
		pt[i] = (pp[i] ^ pn[i]) & pn[i];
		// 0 1 -> 0
		// 1 0 -> 1
		// 1 1 -> 0
		// 0 0 -> 0
		pr[i] = (pp[i] ^ pn[i]) & pn[i];
	}

	g_PrevState = *pState; // キー状態更新
}

bool KeyLogger_IsPressed(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key);
}

bool KeyLogger_IsTrigger(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &g_TriggerState);
}

bool KeyLogger_IsRelease(Keyboard_Keys key)
{
	return Keyboard_IsKeyDown(key, &g_ReleaseState);
}
