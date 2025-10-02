/*==============================================================================

Å@ cubeÇÃï\é¶

--------------------------------------------------------------------------------

==============================================================================*/

#ifndef CUBE_H
#define CUBE_H
#include <DirectXMath.h>
#include <d3d11.h>

void Cube_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);
void Cube_Finalize(void);

void Cube_Update(double elapsed_time);

void Cube_Draw(const DirectX::XMMATRIX& mtxWorld);

#endif // !CUBE_H
