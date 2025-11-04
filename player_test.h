#include <DirectXMath.h>

#ifndef PLAYER_TEST_H
#define PLAYER_TEST_H

void Player_Test_Initialize(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& front);
void Player_Finalize();
void Player_Update(double elapsed_time);
void Player_Draw();

const DirectX::XMFLOAT3& Player_Test_GetPosition();
const DirectX::XMFLOAT3& Player_Test_GetFront();
#endif // !PLAYER_TEST_H
