/*==============================================================================

sampler
--------------------------------------------------------------------------------

==============================================================================*/

#include <d3d11.h>
#ifndef SAMPLER_H
#define SAMPLER_H

void Sampler_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void Sampler_Finalize();

void Sampler_SetFillterPoint();
void Sampler_SetFillterLinear();
void Sampler_SetFillterAnisotropic();

#endif // !SAMPLER_H
