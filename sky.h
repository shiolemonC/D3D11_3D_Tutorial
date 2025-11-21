#pragma once
#include <DirectXMath.h>

void Sky_Initialize();
void Sky_Finalize();
void Sky_SetPosition(const DirectX::XMFLOAT3& position);
void Sky_Draw();