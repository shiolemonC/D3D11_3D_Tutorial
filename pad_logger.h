
#ifndef PAD_LOGGER_H
#define PAD_LOGGER_H

#include <windows.h>
#include <Xinput.h>
#include<DirectXMath.h>

void PadLogger_Initialize();

void PadLogger_Update();

bool PadLogger_IsPressed(DWORD user_index, WORD buttons);

bool PadLogger_IsTrigger(DWORD user_index, WORD buttons);

bool PadLogger_IsRelease(DWORD user_index, WORD buttons);

DirectX::XMFLOAT2 PadLogger_GetLeftThumbStick(DWORD user_index);

float PadLogger_GetLeftTrigger(DWORD user_index);

float PadLogger_GetRightTrigger(DWORD user_index);

void PadLogger_VibrationEnable(DWORD user_index, bool enable);

#endif // !PAD_LOGGER_H
