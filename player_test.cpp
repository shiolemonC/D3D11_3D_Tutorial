#include "player_test.h"
#include <DirectXMath.h>
#include "model.h"
#include "key_logger.h"
#include "light.h"
#include "camera.h"
using namespace DirectX;

static XMFLOAT3 g_PlayerPosition{};
static XMFLOAT3 g_PlayerFront{0.0f, 0.0f, 1.0f};
static XMFLOAT3 g_PlayerVelocity{};
static MODEL* g_pPlayerModel{ nullptr };
static bool g_IsJump = false;



void Player_Test_Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front)
{
	g_PlayerPosition = position;
	g_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
	XMStoreFloat3(&g_PlayerFront, XMVector3Normalize(XMLoadFloat3(&front)));

	g_pPlayerModel = ModelLoad("resources/test.fbx");
}

void Player_Finalize()
{
	ModelRelease(g_pPlayerModel);
}

void Player_Update(double elapsed_time)
{
	XMVECTOR position = XMLoadFloat3(&g_PlayerPosition);
	XMVECTOR velocity = XMLoadFloat3(&g_PlayerVelocity);


	if (KeyLogger_IsTrigger(KK_J) && !g_IsJump)
	{
		velocity += XMVECTOR{ 0.0f, 50.0f, 0.0f };
		g_IsJump = true;
	}

	XMVECTOR gdir{ 0.0f, 1.0f, 0.0f};
	velocity += gdir * -9.8f * 25.0f * (float)elapsed_time;
	position += velocity * (float)elapsed_time;

	if (XMVectorGetY(position) < 15.0f)
	{
		position -= velocity * (float)elapsed_time;
		velocity *= {1.0f, 0.0f, 1.0f};
		g_IsJump = false;
	}

	XMStoreFloat3(&g_PlayerPosition, position);
	XMStoreFloat3(&g_PlayerVelocity, velocity);
}

void Player_Draw()
{
	Light_SetSpecularWorld(Camera_GetPosition(), 2.0f, { 0.1f, 0.1f, 0.1f, 1.0f });

	XMMATRIX t = XMMatrixTranslation(g_PlayerPosition.x, g_PlayerPosition.y, g_PlayerPosition.z);
	XMMATRIX world = t;
	world *= XMMatrixScaling(0.1f, 0.1f, 0.1f);
	ModelDraw(g_pPlayerModel, world);

}

const DirectX::XMFLOAT3& Player_Test_GetPosition()
{
	return g_PlayerPosition;
}

const DirectX::XMFLOAT3& Player_Test_GetFront()
{
	return g_PlayerFront;
}
