/*==============================================================================

Å@mesh_field

--------------------------------------------------------------------------------

==============================================================================*/

#ifndef MESHFIELD
#define MESHFIELD

#include <DirectXMath.h>
#include <d3d11.h>

void MeshField_Initialize(ID3D11Device* pDevice, ID3D11DeviceContext* pContext);

void MeshField_Finalize(void);

void MeshField_Draw();

#endif // !MESHFIELD
