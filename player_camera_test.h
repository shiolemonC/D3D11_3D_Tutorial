#ifndef PLAYER_CAMERA_TEST_H
#define PLAYER_CAMERA_TEST_H

#include <DirectXMath.h>

void PlayerCameraTest_Initialize();
void PlayerCameraTest_Finalize();
void PlayerCameraTest_Update(double elapsed_time);

const DirectX::XMFLOAT4X4& PlayerCameraTest_GetMatrix();
const DirectX::XMFLOAT4X4& PlayerCameraTest_GetPerspectiveMatrix();
const DirectX::XMFLOAT3& PlayerCameraTest_GetFront();
const DirectX::XMFLOAT3& PlayerCameraTest_GetPosition();

#endif // !PLAYER_CAMERA_TEST_H
