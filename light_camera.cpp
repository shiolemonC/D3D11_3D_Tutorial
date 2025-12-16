#include "light_camera.h"
using namespace DirectX;

static XMFLOAT3 g_CameraPos{ 0.0f, 0.0f, -5.0f };
static XMFLOAT3 g_CameraFront{ 0.0f, 0.0f, 1.0f };

static XMFLOAT4X4 g_LightCameraMatrix{};
static XMFLOAT4X4 g_LightPerspectiveMatrix{};

void LightCamera_Initialize(const DirectX::XMFLOAT3& world_direction, const DirectX::XMFLOAT3& position)
{
}

void LightCamera_Finalize()
{
}

void LightCamera_SetPosition(const DirectX::XMFLOAT3& position)
{
}

void LightCamera_SetFront(const DirectX::XMFLOAT3& position)
{
}

const DirectX::XMFLOAT4X4& LightCamera_GetViewMatrix()
{
	return g_LightCameraMatrix;
}

const DirectX::XMFLOAT4X4& LightCamera_GetProjectionMatrix()
{
	return g_LightPerspectiveMatrix;
}
