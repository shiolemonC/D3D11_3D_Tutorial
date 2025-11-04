#include "camera.h"
#include <DirectXMath.h>
#include "debug_ostream.h"
#include "direct3d.h"
#include "shader3d.h"
#include "key_logger.h"
#include "debug_text.h"
#include <sstream>
using namespace DirectX;

static bool g_ExternalControl = false;

static XMFLOAT3 g_CameraPos{ 0.0f, 0.0f, -5.0f };
static XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };
static XMFLOAT3 g_CameraUp{ 0.0f, 1.0f, 0.0f };
static XMFLOAT3 g_CameraRight{ 1.0f, 0.0f, 0.0f };
static constexpr float CAMERA_MOVE_SPEED = 3.0f;
static constexpr float CAMERA_ROTATION_SPEED = XMConvertToRadians(30); //radian speed
static XMFLOAT4X4 g_CameraMatrix;
static XMFLOAT4X4 g_PerspectiveMatrix;
static float g_Fov = XMConvertToRadians(60);
static hal::DebugText* g_pDT = nullptr;

void Camera_Initialize(
	const DirectX::XMFLOAT3& position, 
	const DirectX::XMFLOAT3& front, 
	const DirectX::XMFLOAT3& right)
{
	Camera_Initialize();
	g_CameraPos = position;
	XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&front));
	XMVECTOR r = XMVector3Normalize(XMLoadFloat3(&right) * XMVECTOR { 1.0, 0.0f, 1.0f });
	XMVECTOR u = XMVector3Normalize(XMVector3Cross(f, r));
	XMStoreFloat3(&g_CameraFront, f);
	XMStoreFloat3(&g_CameraRight, r);
	XMStoreFloat3(&g_CameraUp, u);

}

void Camera_Initialize()
{
	g_CameraPos = { 0.0f, 0.0f, -5.0f };
	g_CameraFront = { 0.0f, 0.0f, 1.0f };
	g_CameraUp = { 0.0f, 1.0f, 0.0f };
	g_CameraRight = { 1.0f, 0.0f, 0.0f };
	g_Fov = XMConvertToRadians(60);

	XMStoreFloat4x4(&g_CameraMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&g_PerspectiveMatrix, XMMatrixIdentity());

#if defined(DEBUG) || defined(_DEBUG)
	// Debug Mode
	g_pDT = new hal::DebugText(Direct3D_GetDevice(), Direct3D_GetContext(),
		L"consolab_ascii_512.png",
		Direct3D_GetBackBufferWidth(),
		Direct3D_GetBackBufferHeight(),
		0.0f, 32.0f,
		0, 0,
		0.0f, 16.0f);

#endif
}

void Camera_Finalize()
{
	delete g_pDT;
}

void Camera_Update(double elapsed_time)
{
	XMVECTOR front = XMLoadFloat3(&g_CameraFront);
	XMVECTOR right = XMLoadFloat3(&g_CameraRight);
	XMVECTOR up = XMLoadFloat3(&g_CameraUp);
	XMVECTOR position = XMLoadFloat3(&g_CameraPos);

	if(!g_ExternalControl)
	{
		if (KeyLogger_IsPressed(KK_W))
		{
			//position += front * CAMERA_MOVE_SPEED * elapsed_time;
			position += XMVector3Normalize(front * XMVECTOR{ 1,0,1 }) * (float)CAMERA_MOVE_SPEED * elapsed_time;
		}

		if (KeyLogger_IsPressed(KK_A))
		{
			position += -right * (float)CAMERA_MOVE_SPEED * elapsed_time;

		}

		if (KeyLogger_IsPressed(KK_S))
		{
			//position += -front * CAMERA_MOVE_SPEED * elapsed_time;
			position += XMVector3Normalize(front * XMVECTOR{ 1,0,1 }) * (float)-CAMERA_MOVE_SPEED * elapsed_time;

		}

		if (KeyLogger_IsPressed(KK_D))
		{
			position += right * (float)CAMERA_MOVE_SPEED * elapsed_time;
		}


		if (KeyLogger_IsPressed(KK_SPACE))
		{
			//position += up * CAMERA_MOVE_SPEED * elapsed_time;
			position += XMVECTOR{ 0.0f, 1.0f, 0.0f } *(float)CAMERA_MOVE_SPEED * elapsed_time;
		}

		if (KeyLogger_IsPressed(KK_LEFTCONTROL))
		{
			//position += -up * CAMERA_MOVE_SPEED * elapsed_time;
			position += XMVECTOR{ 0.0f, -1.0f, 0.0f } *(float)CAMERA_MOVE_SPEED * elapsed_time;
		}


		if (KeyLogger_IsPressed(KK_UP))
		{
			XMMATRIX rotation = XMMatrixRotationAxis(right, (float)(-CAMERA_ROTATION_SPEED * elapsed_time));
			front = XMVector3TransformNormal(front, rotation);
			front = XMVector3Normalize(front);
			up = XMVector3Normalize(XMVector3Cross(front, right));
		}

		if (KeyLogger_IsPressed(KK_DOWN))
		{
			XMMATRIX rotation = XMMatrixRotationAxis(right, (float)(CAMERA_ROTATION_SPEED * elapsed_time));
			front = XMVector3TransformNormal(front, rotation);
			front = XMVector3Normalize(front);
			up = XMVector3Normalize(XMVector3Cross(front, right));
		}

		if (KeyLogger_IsPressed(KK_LEFT))
		{
			//XMMATRIX rotation = XMMatrixRotationAxis(up, -CAMERA_ROTATION_SPEED * elapsed_time);
			XMMATRIX rotation = XMMatrixRotationY((float)(-CAMERA_ROTATION_SPEED * elapsed_time));
			up = XMVector3Normalize(XMVector3TransformNormal(up, rotation));
			front = XMVector3TransformNormal(front, rotation);
			front = XMVector3Normalize(front);
			//right = XMVector3Normalize(XMVector3Cross(up, front) * XMVECTOR{1.0f, 0.0f, 1.0f});
			right = XMVector3Normalize(XMVector3Cross(up, front));
		}

		if (KeyLogger_IsPressed(KK_RIGHT))
		{
			//XMMATRIX rotation = XMMatrixRotationAxis(up, CAMERA_ROTATION_SPEED * elapsed_time);
			XMMATRIX rotation = XMMatrixRotationY((float)(CAMERA_ROTATION_SPEED * elapsed_time));
			up = XMVector3Normalize(XMVector3TransformNormal(up, rotation));
			front = XMVector3TransformNormal(front, rotation);
			front = XMVector3Normalize(front);
			//right = XMVector3Normalize(XMVector3Cross(up, front) * XMVECTOR { 1.0f, 0.0f, 1.0f });
			right = XMVector3Normalize(XMVector3Cross(up, front));
		}

		if (KeyLogger_IsPressed(KK_Z))
		{
			g_Fov -= XMConvertToRadians(15) * elapsed_time;
		}

		if (KeyLogger_IsPressed(KK_C))
		{
			g_Fov += XMConvertToRadians(15) * elapsed_time;
		}

		//store the result
		XMStoreFloat3(&g_CameraPos, position);
		XMStoreFloat3(&g_CameraFront, front);
		XMStoreFloat3(&g_CameraRight, right);
		XMStoreFloat3(&g_CameraUp, up);

		//view matrix
		//XMVECTOR eye = XMLoadFloat3(&g_CameraPos);
		XMVECTOR at = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
		//XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	}
	XMMATRIX mtxView = XMMatrixLookAtLH(position, position + front, up);

	XMStoreFloat4x4(&g_CameraMatrix, mtxView);

	Shader3d_SetViewMatrix(mtxView);

	//Perspective array
	//NearZ一定要大于0 是距离
	float aspectRatio = (float)Direct3D_GetBackBufferWidth() / Direct3D_GetBackBufferHeight();
	float nearZ = 0.1f;
	float farZ = 100.0f;
	XMMATRIX mtxPerspective = XMMatrixPerspectiveFovLH(g_Fov, aspectRatio, nearZ, farZ);

	XMStoreFloat4x4(&g_PerspectiveMatrix, mtxPerspective);
	Shader3d_SetProjectionMatrix(mtxPerspective);
}

const DirectX::XMFLOAT4X4& Camera_GetMatrix()
{
	return g_CameraMatrix;
}

const DirectX::XMFLOAT4X4& Camera_GetPerspectiveMatrix()
{
	return g_PerspectiveMatrix;
}

const DirectX::XMFLOAT3& Camera_GetFront()
{
	return g_CameraFront;
}

const DirectX::XMFLOAT3& Camera_GetPosition()
{
	return g_CameraPos;
}

float Camera_GetFov()
{
	return g_Fov;
}

void Camera_DebugDraw()
{
#if defined(DEBUG) || defined(_DEBUG) // debug buildだけで有効

	std::stringstream ss;

	ss << "Camera pos: x = " << g_CameraPos.x;
	ss << " y = " << g_CameraPos.y;
	ss << " z = " << g_CameraPos.z << std::endl;

	ss << "Camera front: x = " << g_CameraFront.x;
	ss << " y = " << g_CameraFront.y;
	ss << " z = " << g_CameraFront.z << std::endl;

	ss << "Camera right: x = " << g_CameraRight.x;
	ss << " y = " << g_CameraRight.y;
	ss << " z = " << g_CameraRight.z << std::endl;

	ss << "Camera up: x = " << g_CameraUp.x;
	ss << " y = " << g_CameraUp.y;
	ss << " z = " << g_CameraUp.z << std::endl;

	ss << "Camera fov: x = " << g_Fov << std::endl;

	g_pDT->SetText(ss.str().c_str(), {0.0f, 1.0f, 0.0f, 1.0f});
	g_pDT->Draw();
	g_pDT->Clear();
#endif
}

void Camera_EnableExternalControl(bool on) { g_ExternalControl = on; }

void Camera_SetPose(const XMFLOAT3& pos, const XMFLOAT3& front, const XMFLOAT3& up)
{
	g_CameraPos = pos;
	g_CameraFront = front;
	g_CameraUp = up;
	// 重新计算 right
	using namespace DirectX;
	XMVECTOR f = XMVector3Normalize(XMLoadFloat3(&g_CameraFront));
	XMVECTOR u = XMVector3Normalize(XMLoadFloat3(&g_CameraUp));
	XMVECTOR r = XMVector3Normalize(XMVector3Cross(f, u));
	XMStoreFloat3(&g_CameraFront, f);
	XMStoreFloat3(&g_CameraUp, u);
	XMStoreFloat3(&g_CameraRight, r);
}
