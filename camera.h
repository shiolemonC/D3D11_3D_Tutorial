/*==============================================================================

　 camera controller
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

// 开/关相机外部控制（开后将忽略自由相机的键鼠输入）
void Camera_EnableExternalControl(bool on);
// 由外部系统直接设定相机姿态（位置/前/上）
void Camera_SetPose(const DirectX::XMFLOAT3& pos,
    const DirectX::XMFLOAT3& front,
    const DirectX::XMFLOAT3& up);

#endif // !CAMERA_H
