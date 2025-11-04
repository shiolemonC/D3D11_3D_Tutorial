#include "player_camera_test.h"
#include "player_test.h"
#include <DirectXMath.h>
#include "debug_ostream.h"
#include "debug_text.h"
#include <sstream>
#include "shader3d.h"
#include "direct3d.h"
using namespace DirectX;

static XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };
static XMFLOAT3 g_CameraPos{0.0f, 0.0f, 0.0f };

static XMFLOAT4X4 g_CameraMatrix;
static XMFLOAT4X4 g_PerspectiveMatrix;
static float g_Fov = XMConvertToRadians(60);
static hal::DebugText* g_pDT = nullptr;

void PlayerCameraTest_Initialize()
{
}

void PlayerCameraTest_Finalize()
{
}

void PlayerCameraTest_Update(double elapsed_time)
{
	XMVECTOR position = XMLoadFloat3(&Player_Test_GetPosition()) - XMLoadFloat3(&Player_Test_GetFront()) * 5.0f;
	XMVECTOR target = XMLoadFloat3(&Player_Test_GetPosition());
	XMVECTOR front = XMVector3Normalize(target - position);
	XMStoreFloat3(&g_CameraPos, position);
	XMStoreFloat3(&g_CameraFront, front);

	XMMATRIX mtxView = XMMatrixLookAtLH(
		position,
		target,
		{0.0f, 1.0f, 0.0f});

	Shader3d_SetViewMatrix(mtxView);

	//Perspective array
	//NearZ一定要大于0 是距离
	float aspectRatio = (float)Direct3D_GetBackBufferWidth() / Direct3D_GetBackBufferHeight();
	float nearZ = 0.1f;
	float farZ = 100.0f;
	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(g_Fov, aspectRatio, nearZ, farZ);

	//XMStoreFloat4x4(&g_PerspectiveMatrix, mtxPerspective);
	Shader3d_SetProjectionMatrix(mtxPerspective);
}

const DirectX::XMFLOAT4X4& PlayerCameraTest_GetMatrix()
{
	return g_CameraMatrix;
}

const DirectX::XMFLOAT4X4& PlayerCameraTest_GetPerspectiveMatrix()
{
	return g_CameraMatrix;
}

const DirectX::XMFLOAT3& PlayerCameraTest_GetFront()
{
	return g_CameraFront;
}

const DirectX::XMFLOAT3& PlayerCameraTest_GetPosition()
{
	return g_CameraPos;
}
