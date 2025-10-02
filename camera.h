/*==============================================================================

Å@ camera controller
                                                         Author : Youhei Sato
                                                         Date   : 2025/07/03
--------------------------------------------------------------------------------

==============================================================================*/

#ifndef CAMERA_H
#define CAMERA_H

#include <DirectXMath.h>

void Camera_Initialize(
    const DirectX::XMFLOAT3& position, 
    const DirectX::XMFLOAT3& front, 
    const DirectX::XMFLOAT3& right);
void Camera_Initialize();
void Camera_Finalize();
void Camera_Update(double elapsed_time);

const DirectX::XMFLOAT4X4& Camera_GetMatrix();
const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix();
const DirectX::XMFLOAT3& Camera_GetFront();
const DirectX::XMFLOAT3& Camera_GetPosition();

float Camera_GetFov();

void Camera_DebugDraw();

#endif // !CAMERA_H
