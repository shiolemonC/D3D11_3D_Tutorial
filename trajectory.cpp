/*================================================================================================
   軌跡エフェクトの描画　[trajectory.h]

   Author: Kunii Michito
   Date: 2025/09/03

--------------------------------------------------------------------------------------------------
================================================================================================*/
#include "trajectory.h"
#include "texture.h"
#include "sprite.h"
#include "direct3d.h"
using namespace DirectX;

struct Trajectory
{
	XMFLOAT2 position;
	XMFLOAT4 color;
	float size;
	double lifeTime;
	double birthTime; // 0.0だったら未使用
};

static constexpr unsigned int TRAJECTORY_MAX = 16384;
static Trajectory g_Trajectorys[TRAJECTORY_MAX] = {};
static int g_TrajectoryTexId = -1;
static double g_Time = 0.0;

void Trajectory_Initialize()
{
	for (Trajectory& t : g_Trajectorys) {
		t.birthTime = 0.0;
	}

	g_Time = 0.0;

	g_TrajectoryTexId = Texture_Load(L"bullet.jpg");
}

void Trajectory_Finalize()
{
}

void Trajectory_Update(double elapsed_time)
{
	for (Trajectory& t : g_Trajectorys) {

		if (t.birthTime == 0.0) continue;

		double time = g_Time - t.birthTime;

		if (time > t.lifeTime) {
			t.birthTime = 0.0; // 寿命が尽きた
		}
	}

	g_Time += elapsed_time;
}

void Trajectory_Draw()
{
	Direct3D_SetAlphaBlendAdd();

	for (const Trajectory& t : g_Trajectorys) {

		if (t.birthTime == 0.0) continue;

		double time = g_Time - t.birthTime;
		float racio = (float)(time / t.lifeTime);
		float size = t.size * (1.0f - racio);
		float harf_size = size * 0.5f;
		XMFLOAT4 color = t.color;
		color.w = t.color.w * (1.0f - racio);

		Sprite_Draw(g_TrajectoryTexId, t.position.x - harf_size, t.position.y - harf_size, size, size, false, color);
	}

	Direct3D_SetAlphaBlendTransparent();
}

void Trajectory_Create(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT4& color, float size, double lifeTime)
{
	for (Trajectory& t : g_Trajectorys) {

		if (t.birthTime != 0.0) continue;

		t.birthTime = g_Time;
		t.lifeTime = lifeTime;
		t.color = color;
		t.position = position;
		t.size = size;
		break;
	}
}

