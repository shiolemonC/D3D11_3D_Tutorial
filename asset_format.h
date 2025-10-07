// ====== 通用 ======
struct FileHeader {
    char     magic[4];    // 'MESH' / 'MATL' / 'SKEL' / 'ANIM'
    uint32_t version;     // 0x00010000
    uint32_t byteSize;
    uint32_t reserved;
};

struct AABB { float minv[3]; float maxv[3]; };

enum MeshFlags : uint32_t {
    HAS_TANGENT = 1u << 0,
    HAS_SKIN = 1u << 1,   // <<< 新增：网格含骨权重
};

// ====== 网格（v0/v1 兼容）======
struct MeshHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexStride;     // v0: 48 (pos3+nrm3+tang4+uv2)
    // v1: 56 (v0 + boneIdx u8x4 + boneW u8x4)
    uint32_t submeshCount;
    uint32_t flags;            // HAS_TANGENT / HAS_SKIN
    AABB     bounds;
    // v1 追加：若 HAS_SKIN，便于 sanity check
    uint32_t jointCount;       // = skeleton joint 数；v0 可为 0
    uint32_t _pad0[3];
};

struct Submesh {
    uint32_t indexOffset;
    uint32_t indexCount;
    uint32_t materialIndex;
    AABB     bounds;
};

// ====== 材质（与旧版一致）======
struct MaterialHeader { uint32_t materialCount; };

struct MaterialRec {
    char   name[64];
    char   baseColorTex[96];
    char   normalTex[96];
    char   mrTex[96];
    char   emissiveTex[96];
    float  baseColorFactor[4];
    float  metallicFactor;
    float  roughnessFactor;
    float  emissiveStrength;
    uint32_t flags;
};

// ====== 骨架：.skel ======
struct SkeletonHeader {
    uint32_t jointCount;
    uint32_t _pad[3];
};

struct JointRec {
    char    name[64];
    int32_t parent;         // -1 为根
    float   invBind[16];    // 行主/列主保持一致性即可，读写统一
    float   bindLocalT[3];  uint32_t _pad0;
    float   bindLocalR[4];
    float   bindLocalS[3];  uint32_t _pad1;
};

// ====== 动画：.anim（逐帧姿势版本，简单可用）======
struct AnimHeader {
    uint32_t jointCount;
    float    durationSec;
    float    sampleRate;    // e.g. 30
    uint32_t frameCount;    // = floor(duration * sampleRate) + 1
};

struct AnimTRS {
    float T[3];  uint32_t _pad0;
    float R[4];                 // quaternion (x,y,z,w)
    float S[3];  uint32_t _pad1;
};
// 紧随其后：AnimTRS pose[frameCount][jointCount]