#pragma once
#include <cstdint>

// 文件通用头
struct FileHeader {
    char     magic[4];   // 'MESH' / 'MATL' / ...
    uint32_t version;    // 例如 0x00010000
    uint32_t byteSize;   // 整个文件大小（字节数）
    uint32_t reserved;   // 先留空
};

// 包围盒
struct AABB {
    float minv[3];
    float maxv[3];
};

// Mesh 文件头
enum MeshFlags : uint32_t { HAS_TANGENT = 1u << 0 };

struct MeshHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexStride;    // 例如 40 bytes (pos3+nrm3+tang4+uv2)
    uint32_t submeshCount;
    uint32_t flags;           // HAS_TANGENT
    AABB     bounds;
};

struct Submesh {
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t materialIndex;   // 指向下面的材质表
    AABB     bounds;
};

// 材质文件头
struct MaterialHeader {
    uint32_t materialCount;
};

struct MaterialRec {
    char  name[64];
    char  baseColorTex[96];
    char  normalTex[96];
    char  mrTex[96];
    char  emissiveTex[96];
    float baseColorFactor[4]; // 若无贴图可用颜色
    float metallicFactor;
    float roughnessFactor;
    float emissiveStrength;
    uint32_t flags; // 双面/Alpha 模式等（先 0）
}; 
