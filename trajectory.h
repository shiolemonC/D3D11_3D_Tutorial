/*================================================================================================
   軌跡エフェクトの描画　[trajectory.h]

   Author: Kunii Michito
   Date: 2025/9/03

--------------------------------------------------------------------------------------------------
================================================================================================*/
#ifndef TRAJECTORY_H
#define TRAJECTORY_H

#include <DirectXMath.h>

void Trajectory_Initialize();
void Trajectory_Finalize();
void Trajectory_Update(double elapsed_time);
void Trajectory_Draw();

void Trajectory_Create(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT4& color, float size, double lifeTime);

#endif // TRAJECTORY_H
