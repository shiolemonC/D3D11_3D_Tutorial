#pragma comment(lib, "xinput.lib")

#include "pad_logger.h"
using namespace DirectX;

static WORD g_ButtonPrev[4]{};
static WORD g_ButtonTrigger[4]{};
static WORD g_ButtonRelease[4]{};
static XINPUT_STATE g_State[4]{};

void PadLogger_Initialize()
{
}

void PadLogger_Update()
{
	for (int i = 0; i < 4; i++)
	{
		XInputGetState(i, &g_State[i]);

		g_ButtonTrigger[i] = (g_ButtonPrev[i] ^ g_State[i].Gamepad.wButtons) & g_State[i].Gamepad.wButtons;
		g_ButtonRelease[i] = (g_ButtonPrev[i] ^ g_State[i].Gamepad.wButtons) & g_ButtonPrev[i];

		g_ButtonPrev[i] = g_State[i].Gamepad.wButtons;
	}
}

bool PadLogger_IsPressed(DWORD user_index, WORD buttons)
{
	return g_State[user_index].Gamepad.wButtons & buttons;
}

bool PadLogger_IsTrigger(DWORD user_index, WORD buttons)
{
	return g_ButtonTrigger[user_index] & buttons;
}

bool PadLogger_IsRelease(DWORD user_index, WORD buttons)
{
	return g_ButtonRelease[user_index] & buttons;
}

DirectX::XMFLOAT2 PadLogger_GetLeftThumbStick(DWORD user_index)
{
	float x = 0.0f, y = 0.0f;
	if (g_State[user_index].Gamepad.sThumbLX < 0)
	{
		if (g_State[user_index].Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			x = (float)g_State[user_index].Gamepad.sThumbLX / 32768.0f;
		}
		else
		{
			x = 0.0f;
		}
	}
	else 
	{
		if (g_State[user_index].Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) 
		{
			x = (float)g_State[user_index].Gamepad.sThumbLX / 32767.0f;
		}
		else 
		{
			x = 0.0f;
		}
	}

	if (g_State[user_index].Gamepad.sThumbLY < 0)
	{
		if (g_State[user_index].Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			y = (float)g_State[user_index].Gamepad.sThumbLY / 32768.0f;
		}
		else
		{
			y = 0.0f;
		}
	}
	else
	{
		if (g_State[user_index].Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			y = (float)g_State[user_index].Gamepad.sThumbLY / 32767.0f;
		}
		else
		{
			y = 0.0f;
		}
	}

	if (XMVectorGetX(XMVector2LengthSq({ x,y })) > 1.0f)
	{
		XMFLOAT2 ret;
		XMStoreFloat2(&ret, XMVector2Normalize({ x, y }));
		return ret;
	}
	return { x,y };
}

float PadLogger_GetLeftTrigger(DWORD user_index)
{
	return (float)g_State[user_index].Gamepad.bLeftTrigger/255.0f;
}

float PadLogger_GetRightTrigger(DWORD user_index)
{
	return (float)g_State[user_index].Gamepad.bRightTrigger / 255.0f;
}

void PadLogger_VibrationEnable(DWORD user_index, bool enable)
{
	XINPUT_VIBRATION xv
	{
		enable ? 65535 : 0,
		enable ? 65535 : 0,
	};
	XInputSetState(user_index, &xv);
}
