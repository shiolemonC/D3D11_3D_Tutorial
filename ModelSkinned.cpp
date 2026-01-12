#include "ModelSkinned.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <string>
#include <Windows.h>

#include "asset_format.h"     // 你 AssetCooker 的公共头（含 JointRec / SkeletonHeader 等）
#include "shader3d.h"         // 用来设置 b0~b4（已存在）
#include "texture.h"          // Texture_Load / Texture_SetTexture
#include "sampler.h"          // Sampler_SetFillterAnisotropic 等
#include "direct3d.h"

using namespace DirectX;
namespace fs = std::filesystem;

// ---------------------------------------------------------
// 内部全局状态（隐藏在实现里，Draw() 无参数）
// ---------------------------------------------------------
static ID3D11Device* gDev = nullptr;
static ID3D11DeviceContext* gCtx = nullptr;

static ID3D11Buffer* gVB = nullptr;
static ID3D11Buffer* gIB = nullptr;
static UINT                  gIndexCount = 0;
static DXGI_FORMAT           gIndexFormat = DXGI_FORMAT_R16_UINT;

static UINT                  gVertexStride = 0;
static UINT                  gVertexCount = 0;

static ID3D11VertexShader* gVS = nullptr;
static ID3D11InputLayout* gIL = nullptr;

// b5：骨矩阵数组
static ID3D11Buffer* gCBBones = nullptr;
static const UINT            MAX_BONES = 128;
static uint32_t               gSkelJointCount = 0;

// 可选：光照常量（与现有 Shader3d 配合）
static ID3D11Buffer* gCBAmbient = nullptr;     // VS b3
static ID3D11Buffer* gCBDirectional = nullptr; // VS b4

// Velocity-Driven 开关：为“动画位移”使用时清除根的局部 XZ 平移
static bool                  gZeroRootTransXZ = false;

static std::string gZeroExtraXZ_NameUTF8;
static int         gZeroExtraXZ_Index = -1;

// 世界矩阵（由上层设置）
static XMMATRIX              gWorld = XMMatrixIdentity();

// 贴图
static int                   gTexId = -1;

// 骨架
struct Joint {
    int        parent = -1;
    XMFLOAT4X4 invBind;     // 行主存储
    XMFLOAT3   bindT{ 0,0,0 };
    XMFLOAT4   bindR{ 0,0,0,1 };
    XMFLOAT3   bindS{ 1,1,1 };
    std::string name;       // ★ 新增：骨骼名（UTF-8）
};
static std::vector<Joint>    gJoints;

// 动画帧缓存
static float                 gSampleRate = 30.0f;
static float                 gDurationSec = 0.0f;
static uint32_t              gFrameCount = 0;
static bool                  gLoop = true;
static float                 gPlayback = 1.0f;
static float                 gTime = 0.0f;   // 当前时间（秒）

static std::vector<AnimTRS>  gAnimFrames;    // 连续存储：frame 0..N-1，每帧 jointCount 个 AnimTRS

// 每帧递归计算出的全局矩阵（行主）
static std::vector<XMFLOAT4X4> gPalette;
static std::vector<XMMATRIX>   g_temp_globals;

// —— 入场对齐 + ΔYaw 抽取 —— 
static bool  gRootYawAlignEnabled = false;
static float gRootYawAlignTarget = 0.0f; // 目标 yaw（弧度），一般=Idle首帧
static float gRootYawStart = 0.0f; // 本剪辑首帧 yaw（弧度）

// —— 节点世界层的 yaw 修正（NodeFix），乘在 world 之前 —— 
static float gNodeYawFixRad = 0.0f;

// —— MotionRoot（可配置）——
static std::string gMotionRootNameUTF8 = "mixamorig:Hips"; // 缺省：Hips
static int         gMotionRootIndex = -1;

// --动画融合用参数
static std::vector<AnimTRS> gCrossFadeSourcePose; // 旧姿势快照
static float gCrossFadeWeight = 1.0f;             // 当前混合权重（0~1）

// 安全释放
#ifndef SAFE_RELEASE
template<typename T>
static void SAFE_RELEASE(T*& p) { if (p) { p->Release(); p = nullptr; } }
#endif

static inline float AngleDelta(float a) {
    const float PI = 3.14159265358979323846f;
    const float TWO_PI = 6.283185307179586f;
    while (a > PI) a -= TWO_PI;
    while (a <= -PI) a += TWO_PI;
    return a;
}

static void EnsureD3D() {
    if (!gDev || !gCtx) {
        gDev = Direct3D_GetDevice();
        gCtx = Direct3D_GetContext();
    }
}

//
static void ResolveZeroExtraXZ()
{
    gZeroExtraXZ_Index = -1;
    if (gJoints.empty()) return;
    if (gZeroExtraXZ_NameUTF8.empty()) return;

    // 先按指定名找
    for (size_t i = 0; i < gJoints.size(); ++i) {
        if (gJoints[i].name == gZeroExtraXZ_NameUTF8) {
            gZeroExtraXZ_Index = (int)i;
            break;
        }
    }

    // 找不到就做一点常见兜底（可选，但很实用）
    if (gZeroExtraXZ_Index < 0) {
        static const char* kFallback[] = { "mixamorig:Hips","Hips","hips","Pelvis","pelvis" };
        for (auto s : kFallback) {
            for (size_t i = 0; i < gJoints.size(); ++i) {
                if (gJoints[i].name == s) { gZeroExtraXZ_Index = (int)i; break; }
            }
            if (gZeroExtraXZ_Index >= 0) break;
        }
    }

#if defined(DEBUG) || defined(_DEBUG)
    if (gZeroExtraXZ_Index >= 0) {
        char buf[256];
        sprintf_s(buf, "[Skinned] ZeroExtraXZ = #%d (%s)\n",
            gZeroExtraXZ_Index, gJoints[gZeroExtraXZ_Index].name.c_str());
        OutputDebugStringA(buf);
    }
#endif
}

// 前置
static XMMATRIX MakeLocalMatrix(const AnimTRS& t);
static void ComputeGlobalBindPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform);
static void ComputeAnimationPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform, const AnimTRS* currentFramePose);

// ---------------------------------------------------------
// 读文件小工具
// ---------------------------------------------------------
static bool ReadAll(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    size_t n = size_t(f.tellg()); f.seekg(0, std::ios::beg);
    out.resize(n);
    if (n && !f.read((char*)out.data(), n)) return false;
    return true;
}

static XMMATRIX MakeLocalMatrix(const AnimTRS& t) {
    XMVECTOR T = XMVectorSet(t.T[0], t.T[1], t.T[2], 0.0f);
    XMVECTOR R = XMVectorSet(t.R[0], t.R[1], t.R[2], t.R[3]);
    XMVECTOR S = XMVectorSet(t.S[0], t.S[1], t.S[2], 1.0f);
    return XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(T);
}

// ---------------------------------------------------------
// 从 .mat 尝试读取第一条材质的 baseColorTex 并加载（可选）
// ---------------------------------------------------------
static bool TryLoadBaseColorFromMat(const std::wstring& matPathW) {
    if (matPathW.empty()) return false;
    std::ifstream f(matPathW, std::ios::binary);
    if (!f) return false;

    FileHeader fh{};                     // from asset_format.h
    if (!f.read((char*)&fh, sizeof(fh))) return false;
    if (std::memcmp(fh.magic, "MATL", 4) != 0) return false;

    MaterialHeader mh{};
    if (!f.read((char*)&mh, sizeof(mh))) return false;
    if (mh.materialCount == 0) return false;

    MaterialRec rec{};
    if (!f.read((char*)&rec, sizeof(rec))) return false;

    if (rec.baseColorTex[0]) {
        fs::path folder = fs::path(matPathW).parent_path();
        std::string rel(rec.baseColorTex, rec.baseColorTex + strnlen(rec.baseColorTex, sizeof(rec.baseColorTex)));
        fs::path texPath = folder / fs::path(rel);
        gTexId = Texture_Load(texPath.wstring().c_str());
        return (gTexId >= 0);
    }
    return false;
}

// ---------------------------------------------------------
// 加载 .mesh（v1 带皮肤）：创建 VB/IB & InputLayout
// ---------------------------------------------------------
static bool LoadMeshV1(const std::wstring& meshPathW) {
    EnsureD3D();
    std::vector<uint8_t> bin;
    if (!ReadAll(meshPathW, bin)) return false;

    const uint8_t* p = bin.data();
    const uint8_t* e = bin.data() + bin.size();
    auto need = [&](size_t n) { return (size_t)(e - p) >= n; };

    if (!need(sizeof(FileHeader))) return false;
    auto fh = (const FileHeader*)p; p += sizeof(FileHeader);
    if (std::memcmp(fh->magic, "MESH", 4) != 0) return false;

    if (!need(sizeof(MeshHeader))) return false;
    auto mh = (const MeshHeader*)p; p += sizeof(MeshHeader);

    if ((mh->flags & HAS_SKIN) == 0) return false;

    const UINT stride = mh->vertexStride; // 56（你的格式）
    const UINT vcount = mh->vertexCount;
    const UINT icount = mh->indexCount;
    const bool idx32 = (mh->vertexCount > 65535);

    gVertexStride = stride;
    gVertexCount = vcount;

    // 如果你当前的 VS/IL 仍然是“固定格式”的（例如你写死按 56 的布局建 IL）
    // 那么 stride 变化会导致渲染全错 —— 先直接报错退出，避免“透明人”难查。
    if (gVertexStride == 0)
    {
        OutputDebugStringA("[ModelSkinned] ERROR: vertexStride==0\n");
        return false;
    }

    // 如果你确定 shader/input-layout 只支持 56，这里建议先强约束：
    // （等你将来支持多 layout 再删掉这段）
    if (gVertexStride != 56)
    {
        char buf[256];
        sprintf_s(buf, "[ModelSkinned] ERROR: vertexStride=%u (expected 56). Update IL/VS or recook mesh.\n",
            gVertexStride);
        OutputDebugStringA(buf);
        return false;
    }

    size_t vbBytes = size_t(vcount) * stride;
    if (!need(vbBytes)) return false;
    const void* vbData = p; p += vbBytes;

    size_t ibBytes = size_t(icount) * (idx32 ? 4 : 2);
    if (!need(ibBytes)) return false;
    const void* ibData = p; p += ibBytes;

    size_t sbBytes = sizeof(Submesh) * mh->submeshCount;
    if (need(sbBytes)) p += sbBytes; // 暂不拆材质组

    // VB
    SAFE_RELEASE(gVB);
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = UINT(vbBytes);
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vbData;
    if (FAILED(gDev->CreateBuffer(&bd, &sd, &gVB))) return false;

    // IB
    SAFE_RELEASE(gIB);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = UINT(ibBytes);
    sd.pSysMem = ibData;
    if (FAILED(gDev->CreateBuffer(&bd, &sd, &gIB))) return false;

    gIndexCount = icount;
    gIndexFormat = idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

    // VS + IL
    std::vector<uint8_t> vsbin;
    if (!ReadAll(L"shader_vertex_skinned_3d.cso", vsbin)) return false;

    SAFE_RELEASE(gVS);
    if (FAILED(gDev->CreateVertexShader(vsbin.data(), vsbin.size(), nullptr, &gVS))) return false;

    SAFE_RELEASE(gIL);
    D3D11_INPUT_ELEMENT_DESC descs[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,      0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R8G8B8A8_UNORM,     0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(gDev->CreateInputLayout(descs, ARRAYSIZE(descs),
        vsbin.data(), vsbin.size(), &gIL))) return false;

    return true;
}

// ---------------------------------------------------------
// 加载 .skel（★ 修好骨骼名读取）
// ---------------------------------------------------------
static bool LoadSkel(const std::wstring& skelPathW) {
    std::vector<uint8_t> bin;
    if (!ReadAll(skelPathW, bin)) return false;

    const uint8_t* p = bin.data();
    const uint8_t* e = bin.data() + bin.size();
    auto need = [&](size_t n) { return (size_t)(e - p) >= n; };

    if (!need(sizeof(FileHeader))) return false;
    auto fh = (const FileHeader*)p; p += sizeof(FileHeader);
    if (std::memcmp(fh->magic, "SKEL", 4) != 0) return false;

    if (!need(sizeof(SkeletonHeader))) return false;

    auto sh = (const SkeletonHeader*)p; p += sizeof(SkeletonHeader);

    gSkelJointCount = sh->jointCount;

    // ★ 关键：超出上限直接报错（否则后面 palette 截断会导致蒙皮错乱/透明）
    if (gSkelJointCount == 0)
    {
        OutputDebugStringA("[ModelSkinned] ERROR: skel jointCount==0\n");
        return false;
    }

    if (gSkelJointCount > MAX_BONES)
    {
        char buf[256];
        sprintf_s(buf, "[ModelSkinned] ERROR: skel jointCount=%u > MAX_BONES=%u. Increase MAX_BONES and HLSL CB size.\n",
            (unsigned)gSkelJointCount, (unsigned)MAX_BONES);
        OutputDebugStringA(buf);
        return false;
    }

    gJoints.clear();
    gJoints.resize(gSkelJointCount);

    if (!need(sizeof(JointRec) * gSkelJointCount)) return false;
    if (!need(sizeof(JointRec) * sh->jointCount)) return false;

    for (uint32_t i = 0; i < sh->jointCount; ++i) {
        auto jr = (const JointRec*)p; p += sizeof(JointRec);

        Joint j{};
        j.parent = jr->parent;

        // invBind（asset 写的是按列主存 float[16]；在这里直接 memcpy 到 XMFLOAT4X4(行主存)
        // 你当前工程里下游按“行主”用，所以一直保持 memcpy 的方式（与之前一致）。
        std::memcpy(&j.invBind, jr->invBind, sizeof(float) * 16);

        j.bindT = XMFLOAT3(jr->bindLocalT[0], jr->bindLocalT[1], jr->bindLocalT[2]);
        j.bindR = XMFLOAT4(jr->bindLocalR[0], jr->bindLocalR[1], jr->bindLocalR[2], jr->bindLocalR[3]);
        j.bindS = XMFLOAT3(jr->bindLocalS[0], jr->bindLocalS[1], jr->bindLocalS[2]);

        // ★ 关键：读骨骼名（假定 JointRec 里有 char name[64]；这是 AssetCooker 常见做法）
        // 如果你们的 JointRec 用的是 nameOffset/哈希，请把这里改成相应读取方式。
        {
            // 兼容保护：把 name 视作以 0 结尾的 C 字符串
            const size_t kMax = sizeof(jr->name);
            size_t len = strnlen(jr->name, kMax);
            j.name.assign(jr->name, jr->name + len);
        }

        gJoints[i] = std::move(j);
    }

    gPalette.resize(std::max<size_t>(1, gJoints.size()));

    // 解析一次 MotionRoot（默认策略可解析出 Hips）
    ModelSkinned_ResolveMotionRoot();
    ResolveZeroExtraXZ();   // ★ 新增：骨骼变化后，重解析“额外清零骨骼”
    return true;
}

// ---------------------------------------------------------
// 加载 .anim（可选）
// ---------------------------------------------------------
static bool LoadAnim(const std::wstring& animPathW) {
    gAnimFrames.clear();
    gFrameCount = 0; gDurationSec = 0.0f;

    if (animPathW.empty()) return true; // 没动画也不报错

    std::vector<uint8_t> bin;
    if (!ReadAll(animPathW, bin)) return false;

    const uint8_t* p = bin.data();
    const uint8_t* e = bin.data() + bin.size();
    auto need = [&](size_t n) { return (size_t)(e - p) >= n; };

    if (!need(sizeof(FileHeader))) return false;
    auto fh = (const FileHeader*)p; p += sizeof(FileHeader);
    if (std::memcmp(fh->magic, "ANIM", 4) != 0) return false;

    if (!need(sizeof(AnimHeader))) return false;
    auto ah = (const AnimHeader*)p; p += sizeof(AnimHeader);

    if (ah->jointCount != gJoints.size()) {
        // 数量不匹配：先严格处理（如需容错可在此处做重映射）
        return false;
    }

    if (gSkelJointCount == 0)
    {
        OutputDebugStringA("[ModelSkinned] ERROR: LoadAnim before LoadSkel (gSkelJointCount==0)\n");
        return false;
    }

    if (ah->jointCount != gSkelJointCount)
    {
        char buf[256];
        sprintf_s(buf, "[ModelSkinned] ERROR: anim jointCount=%u != skel jointCount=%u (asset mismatch / mixed cook?)\n",
            (unsigned)ah->jointCount, (unsigned)gSkelJointCount);
        OutputDebugStringA(buf);
        return false;
    }

    gSampleRate = ah->sampleRate;
    gFrameCount = ah->frameCount;
    gDurationSec = ah->durationSec;

    size_t framesBytes = size_t(gFrameCount) * size_t(ah->jointCount) * sizeof(AnimTRS);
    if (!need(framesBytes)) return false;

    gAnimFrames.resize(gFrameCount * ah->jointCount);
    std::memcpy(gAnimFrames.data(), p, framesBytes);
    return true;
}

// ---------------------------------------------------------
// 递归计算
// ---------------------------------------------------------
static void ComputeGlobalBindPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform)
{
    const auto& joint = gJoints[boneIndex];
    XMVECTOR T = XMLoadFloat3(&joint.bindT);
    XMVECTOR R = XMLoadFloat4(&joint.bindR);
    XMVECTOR S = XMLoadFloat3(&joint.bindS);
    XMMATRIX localBindPose = XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(T);

    XMMATRIX globalTransform = localBindPose * parentGlobalTransform;
    g_temp_globals[boneIndex] = globalTransform;

    for (size_t i = 0; i < gJoints.size(); ++i) {
        if (gJoints[i].parent == (int)boneIndex) {
            ComputeGlobalBindPoseRecursively(i, globalTransform);
        }
    }
}

static void ComputeAnimationPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform, const AnimTRS* currentFramePose)
{
    const AnimTRS& jointPose = currentFramePose[boneIndex];
    XMMATRIX localTransform = MakeLocalMatrix(jointPose);

    XMMATRIX globalTransform = localTransform * parentGlobalTransform;
    g_temp_globals[boneIndex] = globalTransform;

    for (size_t i = 0; i < gJoints.size(); ++i) {
        if (gJoints[i].parent == (int)boneIndex) {
            ComputeAnimationPoseRecursively(i, globalTransform, currentFramePose);
        }
    }
}

// ---------------------------------------------------------
// 常量缓冲
// ---------------------------------------------------------
static bool CreateCBBones() {
    EnsureD3D();
    SAFE_RELEASE(gCBBones);
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth = ((UINT)MAX_BONES * sizeof(XMFLOAT4X4) + 255) & ~255u;
    return SUCCEEDED(gDev->CreateBuffer(&bd, nullptr, &gCBBones));
}

// ---------------------------------------------------------
// Public API
// ---------------------------------------------------------
bool ModelSkinned_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    gDev = dev; gCtx = ctx;

    // b5: bones
    if (!CreateCBBones()) return false;

    // b3: ambient
    {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(XMFLOAT4);
        if (FAILED(gDev->CreateBuffer(&bd, nullptr, &gCBAmbient))) return false;

        const XMFLOAT4 amb{ 0.25f, 0.25f, 0.25f, 1.0f };
        gCtx->UpdateSubresource(gCBAmbient, 0, nullptr, &amb, 0, 0);
    }
    // b4: directional
    {
        struct DirCB { XMFLOAT4 dir; XMFLOAT4 color; };
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(DirCB);
        if (FAILED(gDev->CreateBuffer(&bd, nullptr, &gCBDirectional))) return false;

        DirCB d{};
        d.dir = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f); // 从上往下
        d.color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        gCtx->UpdateSubresource(gCBDirectional, 0, nullptr, &d, 0, 0);
    }
    return true;
}

bool ModelSkinned_Load(const ModelSkinnedDesc& d) {
    if (!LoadMeshV1(d.meshPath)) return false;
    if (!LoadSkel(d.skelPath))   return false;

    // 可选：用 InvBind 反推 bindLocal 一致性（保持你当前稳定做法）
    {
        const size_t J = gJoints.size();
        if (J) {
            g_temp_globals.resize(J);
            // 先用 bindLocal 递归出 Bj
            for (size_t j = 0; j < J; ++j)
                if (gJoints[j].parent == -1)
                    ComputeGlobalBindPoseRecursively(j, XMMatrixIdentity());

            // 用平均 MeshGlobalAtBind 统一坐标后回填（略去细节注释，逻辑与之前一致）
            double sum[16] = { 0 };
            for (size_t j = 0; j < J; ++j) {
                XMMATRIX Bj = g_temp_globals[j];
                XMMATRIX InvB = XMLoadFloat4x4(&gJoints[j].invBind);
                XMMATRIX Mj = Bj * InvB;
                XMFLOAT4X4 fm; XMStoreFloat4x4(&fm, Mj);
                const float* p = &fm._11;
                for (int k = 0; k < 16; ++k) sum[k] += p[k];
            }
            float avg[16]; for (int k = 0; k < 16; ++k) avg[k] = float(sum[k] / double(J));
            XMFLOAT4X4 favg{}; std::memcpy(&favg._11, avg, sizeof(avg));
            XMMATRIX MeshGlobalAtBind = XMLoadFloat4x4(&favg);

            std::vector<XMMATRIX> G(J);
            for (size_t j = 0; j < J; ++j) {
                XMMATRIX InvBj = XMLoadFloat4x4(&gJoints[j].invBind);
                XMMATRIX Bj = MeshGlobalAtBind * XMMatrixInverse(nullptr, InvBj);
                G[j] = Bj;
            }
            for (size_t j = 0; j < J; ++j) {
                XMMATRIX parentG = (gJoints[j].parent >= 0) ? G[gJoints[j].parent] : XMMatrixIdentity();
                XMMATRIX local = XMMatrixInverse(nullptr, parentG) * G[j];
                XMVECTOR S, R, T; if (!XMMatrixDecompose(&S, &R, &T, local)) continue;
                XMStoreFloat3(&gJoints[j].bindT, T);
                XMStoreFloat4(&gJoints[j].bindR, R);
                XMStoreFloat3(&gJoints[j].bindS, S);
            }
        }
    }

    if (!LoadAnim(d.animPath)) return false;

    // 贴图：override > .mat > none
    if (!d.baseColorTexOverride.empty()) {
        gTexId = Texture_Load(d.baseColorTexOverride.c_str());
    }
    else {
        std::wstring matPath = d.matPath.empty()
            ? fs::path(d.meshPath).replace_extension(L".mat").wstring()
            : d.matPath;
        if (!TryLoadBaseColorFromMat(matPath)) {
            gTexId = -1;
        }
    }

    gWorld = XMMatrixIdentity();
    gTime = 0.0f;
    return true;
}

void ModelSkinned_Finalize() {
    SAFE_RELEASE(gCBDirectional);
    SAFE_RELEASE(gCBAmbient);
    SAFE_RELEASE(gCBBones);
    SAFE_RELEASE(gVB);
    SAFE_RELEASE(gIB);
    SAFE_RELEASE(gVS);
    SAFE_RELEASE(gIL);

    gJoints.clear();
    gAnimFrames.clear();
    gPalette.clear();
    g_temp_globals.clear();

    gIndexCount = 0;
    gTexId = -1;
    gMotionRootIndex = -1;
}

void ModelSkinned_Update(double dtSec) {
    if (gFrameCount == 0 || gJoints.empty()) return;
    gTime += float(dtSec) * gPlayback;
    if (gLoop) {
        if (gDurationSec > 0.0f) {
            while (gTime >= gDurationSec) gTime -= gDurationSec;
            while (gTime < 0.0f)         gTime += gDurationSec;
        }
    }
    else {
        gTime = std::clamp(gTime, 0.0f, gDurationSec);
    }
}

void ModelSkinned_SetWorldMatrix(const XMMATRIX& world) { gWorld =  world; }
//XMMatrixScaling(100.0f, 100.0f, 100.0f)
void ModelSkinned_SetLoop(bool loop) { gLoop = loop; }
void ModelSkinned_SetPlaybackRate(float rate) { gPlayback = rate; }
void ModelSkinned_Seek(float t) { gTime = t; }
bool ModelSkinned_LoadAnimOnly(const std::wstring& p) { return LoadAnim(p); }

int ModelSkinned_GetRootJointIndex() {
    for (int i = 0; i < (int)gJoints.size(); ++i)
        if (gJoints[i].parent == -1) return i;
    return -1;
}

// 采样线性插值（辅助）
static AnimTRS SampleJointTRS_Linear(int joint, float tSec) {
    AnimTRS out{};
    if (gFrameCount == 0 || gJoints.empty()) return out;

    const float rate = gSampleRate;
    const float f = tSec * rate;
    int   f0 = (int)floorf(f);
    float a = f - f0;

    auto wrap = [&](int x) {
        if (gFrameCount == 0) return 0;
        while (x < 0) x += (int)gFrameCount;
        while (x >= (int)gFrameCount) x -= (int)gFrameCount;
        return x;
        };
    int f1 = wrap(f0 + 1);
    f0 = wrap(f0);

    const size_t J = gJoints.size();
    const AnimTRS& A = gAnimFrames[(size_t)f0 * J + joint];
    const AnimTRS& B = gAnimFrames[(size_t)f1 * J + joint];

    out.T[0] = A.T[0] + (B.T[0] - A.T[0]) * a;
    out.T[1] = A.T[1] + (B.T[1] - A.T[1]) * a;
    out.T[2] = A.T[2] + (B.T[2] - A.T[2]) * a;

    out.S[0] = A.S[0] + (B.S[0] - A.S[0]) * a;
    out.S[1] = A.S[1] + (B.S[1] - A.S[1]) * a;
    out.S[2] = A.S[2] + (B.S[2] - A.S[2]) * a;

    XMVECTOR qA = XMVectorSet(A.R[0], A.R[1], A.R[2], A.R[3]);
    XMVECTOR qB = XMVectorSet(B.R[0], B.R[1], B.R[2], B.R[3]);
    XMVECTOR q = XMQuaternionSlerp(qA, qB, a);
    XMFLOAT4 qf; XMStoreFloat4(&qf, q);
    out.R[0] = qf.x; out.R[1] = qf.y; out.R[2] = qf.z; out.R[3] = qf.w;
    return out;
}

// === 以 MotionRoot 为根：采样位移 ΔT ===
bool ModelSkinned_SampleRootDelta_Local(float dt, XMFLOAT3* outDeltaT) {
    if (!outDeltaT) return false;
    outDeltaT->x = outDeltaT->y = outDeltaT->z = 0.0f;

    if (gFrameCount == 0 || gJoints.empty()) return false;
    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = ModelSkinned_GetRootJointIndex();
    if (root < 0) return false;

    const float t0 = gTime;
    const float t1 = gLoop ? (t0 + dt) : std::min(t0 + dt, gDurationSec);

    AnimTRS R0 = SampleJointTRS_Linear(root, t0);
    AnimTRS R1 = SampleJointTRS_Linear(root, t1);

    outDeltaT->x = (R1.T[0] - R0.T[0]);
    outDeltaT->y = (R1.T[1] - R0.T[1]);
    outDeltaT->z = (R1.T[2] - R0.T[2]);
    return true;
}

// === 以 MotionRoot 为根：采样 ΔYaw（局部） ===
bool ModelSkinned_SampleRootYawDelta(float dt, float* outDeltaYaw) {
    if (!outDeltaYaw) return false;
    *outDeltaYaw = 0.0f;

    if (gFrameCount == 0 || gJoints.empty()) return false;
    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = ModelSkinned_GetRootJointIndex();
    if (root < 0) return false;

    const float t0 = gTime;
    const float t1 = gLoop ? (t0 + dt) : std::min(t0 + dt, gDurationSec);

    const AnimTRS R0 = SampleJointTRS_Linear(root, t0);
    const AnimTRS R1 = SampleJointTRS_Linear(root, t1);

    auto YawFromQuat = [](const AnimTRS& r)->float {
        XMVECTOR q = XMQuaternionNormalize(XMVectorSet(r.R[0], r.R[1], r.R[2], r.R[3]));
        XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
        f = XMVector3Normalize(f);
        XMFLOAT3 fv; XMStoreFloat3(&fv, f);
        return std::atan2f(fv.x, fv.z);
        };

    const float y0 = YawFromQuat(R0);
    const float y1 = YawFromQuat(R1);
    *outDeltaYaw = AngleDelta(y1 - y0);
    return true;
}

// —— 入场对齐 —— 
void ModelSkinned_SetRootYawAlignTarget(float yawTargetRad) { gRootYawAlignTarget = yawTargetRad; gRootYawAlignEnabled = true; }
void ModelSkinned_ResetRootYawTrack(float yawStartRad) { gRootYawStart = yawStartRad; }

// —— Node 层修正 —— 
void  ModelSkinned_SetNodeYawFix(float r) { gNodeYawFixRad = r; }
float ModelSkinned_GetNodeYawFix() { return gNodeYawFixRad; }

// ---------------------------------------------------------
// Draw
// ---------------------------------------------------------
void ModelSkinned_Draw() {
    if (!gVB || !gIB || !gVS || !gIL) return;

    const size_t J = gJoints.size();
    if (J == 0) return;

    g_temp_globals.resize(J);

    if (gFrameCount > 0) {
        // 取当前帧
        float frameF = gTime * gSampleRate;
        uint32_t f0 = (uint32_t)std::floor(frameF);
        if (f0 >= gFrameCount) f0 = gFrameCount - 1;

        const AnimTRS* currentFramePose = gAnimFrames.data() + size_t(f0) * J;



        // 只读/可写姿态双指针
        const AnimTRS* poseRO = currentFramePose;
        std::vector<AnimTRS> tempPose;
        AnimTRS* poseRW = nullptr;

        // MotionRoot
        int root = ModelSkinned_GetMotionRootIndex();
        if (root < 0) root = ModelSkinned_GetRootJointIndex();
        if (root < 0) root = 0;


//#if defined(_DEBUG) || defined(DEBUG)
//        static int sPoseLog = 0;
//        if ((sPoseLog++ % 30) == 0)
//        {
//            // motion root index（Draw 里本来就有 root 这个局部变量）
//            int dbgRoot = root;
//
//            // 取第0帧做对比基准（注意是“同一个关节”）
//            const AnimTRS* frame0Pose = gAnimFrames.data(); // f=0 的开头
//            const AnimTRS& r0 = frame0Pose[dbgRoot];
//            const AnimTRS& rN = currentFramePose[dbgRoot];
//
//            auto LogTRS = [&](const char* tag, const AnimTRS& t, const AnimTRS& base)
//                {
//                    float dx = t.T[0] - base.T[0];
//                    float dy = t.T[1] - base.T[1];
//                    float dz = t.T[2] - base.T[2];
//                    char b[256];
//                    sprintf_s(b,
//                        "[Pose][%s] root=%d(%s) T=(%.5f,%.5f,%.5f) dT0=(%.5f,%.5f,%.5f) zeroXZ=%d\n",
//                        tag,
//                        dbgRoot, gJoints[dbgRoot].name.c_str(),
//                        t.T[0], t.T[1], t.T[2],
//                        dx, dy, dz,
//                        (int)gZeroRootTransXZ
//                    );
//                    OutputDebugStringA(b);
//                };
//
//            // 1) 清零前（RAW）
//            LogTRS("RAW", rN, r0);
//
//            // 2) 如果你创建了 tempPose 并且 finalPose 指向它，就打印清零后（SKIN）
//            //    finalPose 在你函数里后面会被定义：const AnimTRS* finalPose = poseRW ? ... : poseRO;
//            //    所以你可以把下面这句挪到 finalPose 定义之后再打
//        }
//#endif

        // 清除根 XZ 平移（Velocity-driven 模式）
        if (gZeroRootTransXZ && J > 0) {
            tempPose.assign(currentFramePose, currentFramePose + J);
            tempPose[root].T[0] = 0.0f;
            tempPose[root].T[2] = 0.0f;
            poseRW = tempPose.data();
        }

        // ★ 新增：额外清零某骨骼 XZ（比如 mixamorig:Hips），避免“动画自带位移”叠加
        if (gZeroExtraXZ_Index >= 0 && gZeroExtraXZ_Index < (int)J && gZeroExtraXZ_Index != root) {
            if (!poseRW) {
                tempPose.assign(currentFramePose, currentFramePose + J);
                poseRW = tempPose.data();
            }
            poseRW[gZeroExtraXZ_Index].T[0] = 0.0f;
            poseRW[gZeroExtraXZ_Index].T[2] = 0.0f;
        }

        // 根局部旋转：入场对齐（可选是否保留Δ，当前默认不保留，以便把 Δ 交给 RootMotion）
        if (gRootYawAlignEnabled && J > 0) {
            if (!poseRW) { tempPose.assign(currentFramePose, currentFramePose + J); poseRW = tempPose.data(); }

            auto AngleWrap = [](float a)->float {
                const float PI = 3.14159265358979323846f, TWO = 6.283185307179586f;
                while (a > PI) a -= TWO;
                while (a <= -PI) a += TWO;
                return a;
                };
            auto YawFromLocal = [](const AnimTRS& t)->float {
                XMVECTOR q = XMQuaternionNormalize(XMVectorSet(t.R[0], t.R[1], t.R[2], t.R[3]));
                XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
                f = XMVector3Normalize(f);
                XMFLOAT3 fv; XMStoreFloat3(&fv, f);
                return std::atan2f(fv.x, fv.z);
                };

            const float yawCurr = YawFromLocal(poseRW[root]);
            const float visualOffset = AngleWrap(gRootYawAlignTarget - gRootYawStart);
            const float deltaCum = AngleWrap(yawCurr - gRootYawStart);
            const float fixYaw = AngleWrap(visualOffset - deltaCum);

            XMVECTOR qLocal = XMQuaternionNormalize(XMVectorSet(
                poseRW[root].R[0], poseRW[root].R[1], poseRW[root].R[2], poseRW[root].R[3]));
            XMVECTOR qFix = XMQuaternionRotationRollPitchYaw(0.0f, fixYaw, 0.0f);
            XMVECTOR qOut = XMQuaternionMultiply(qFix, qLocal);

            XMFLOAT4 out; XMStoreFloat4(&out, qOut);
            poseRW[root].R[0] = out.x; poseRW[root].R[1] = out.y;
            poseRW[root].R[2] = out.z; poseRW[root].R[3] = out.w;
        }

        const AnimTRS* finalPose = poseRW ? (const AnimTRS*)poseRW : poseRO;

        const AnimTRS* poseForSkinning = finalPose;

        // 有快照 & 权重不是1，就做混合
        std::vector<AnimTRS> blendedPose;
        if (!gCrossFadeSourcePose.empty() && gCrossFadeWeight < 1.0f) {
            blendedPose.assign(finalPose, finalPose + J);

            const float w = gCrossFadeWeight;

            for (size_t j = 0; j < J; ++j) {
                const AnimTRS& src = gCrossFadeSourcePose[j]; // 旧姿势
                AnimTRS& dst = blendedPose[j];                // 新动画姿势

                // 平移 lerp
                dst.T[0] = src.T[0] + (dst.T[0] - src.T[0]) * w;
                dst.T[1] = src.T[1] + (dst.T[1] - src.T[1]) * w;
                dst.T[2] = src.T[2] + (dst.T[2] - src.T[2]) * w;

                // 缩放 lerp
                dst.S[0] = src.S[0] + (dst.S[0] - src.S[0]) * w;
                dst.S[1] = src.S[1] + (dst.S[1] - src.S[1]) * w;
                dst.S[2] = src.S[2] + (dst.S[2] - src.S[2]) * w;

                // 旋转 nlerp（线性插值后归一化）
                // 旧姿势四元数
                XMVECTOR qA = XMVectorSet(src.R[0], src.R[1], src.R[2], src.R[3]);
                // 新姿势四元数
                XMVECTOR qB = XMVectorSet(dst.R[0], dst.R[1], dst.R[2], dst.R[3]);

                // 先规整一下，避免积累误差
                qA = XMQuaternionNormalize(qA);
                qB = XMQuaternionNormalize(qB);

                // ★ 优弧选择：如果 dot<0，则翻转新四元数
                float dot = XMVectorGetX(XMVector4Dot(qA, qB));
                if (dot < 0.0f) {
                    qB = XMVectorNegate(qB);
                }

                // 做 nlerp（或者 slerp 都行，这里用 nlerp 即可）
                XMVECTOR q = XMVectorLerp(qA, qB, w);
                q = XMQuaternionNormalize(q);

                XMFLOAT4 qf;
                XMStoreFloat4(&qf, q);
                dst.R[0] = qf.x;
                dst.R[1] = qf.y;
                dst.R[2] = qf.z;
                dst.R[3] = qf.w;
            }

            poseForSkinning = blendedPose.data();
        }
        else {
            poseForSkinning = finalPose;
        }


        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                ComputeAnimationPoseRecursively(j, XMMatrixIdentity(), poseForSkinning);
            }
        }
    }
    else {
        // 无动画：展示 bind pose
        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                ComputeGlobalBindPoseRecursively(j, XMMatrixIdentity());
            }
        }
    }

    // 调色板：invBind * currentGlobal
    for (size_t j = 0; j < J; ++j) {
        XMMATRIX invB = XMLoadFloat4x4(&gJoints[j].invBind);
        XMMATRIX M = invB * g_temp_globals[j];
        XMStoreFloat4x4(&gPalette[j], XMMatrixTranspose(M));
    }

    // 上传到 VS b5
    D3D11_MAPPED_SUBRESOURCE mp{};
    if (SUCCEEDED(gCtx->Map(gCBBones, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
        size_t copyJ = std::min(J, size_t(MAX_BONES));
        std::memcpy(mp.pData, gPalette.data(), copyJ * sizeof(XMFLOAT4X4));
        gCtx->Unmap(gCBBones, 0);
    }

    // 绑定着色器 & 常量
    Shader3d_Begin();
    Shader3d_SetColor({ 1,1,1,1 });

    gCtx->VSSetShader(gVS, nullptr, 0);
    gCtx->IASetInputLayout(gIL);

    // world 乘以 NodeYawFix（不要写回 gWorld，避免累乘）
    const float nodeFix = ModelSkinned_GetNodeYawFix();


    const XMMATRIX W = XMMatrixRotationY(nodeFix) * gWorld;
    Shader3d_SetWorldMatrix(W);
    //const XMMATRIX Wfix = XMMatrixRotationX(+XM_PIDIV2); // 或 -XM_PIDIV2
    //Shader3d_SetWorldMatrix(Wfix* W);

    // VS b5（避免被 Shader3d_Begin 覆盖）
    gCtx->VSSetConstantBuffers(5, 1, &gCBBones);

    // 纹理/采样
    if (gTexId >= 0) Texture_SetTexture(gTexId);
    Sampler_SetFillterAnisotropic();

    // Draw
    if (!gVB || !gIB || gIndexCount == 0 || gVertexStride == 0)
    {
        // 可选：打一个一次性 log
        // OutputDebugStringA("[ModelSkinned] Draw skipped: VB/IB not ready\n");
        return;
    }

    UINT stride = gVertexStride;
    UINT offset = 0;
    gCtx->IASetVertexBuffers(0, 1, &gVB, &stride, &offset);

    gCtx->IASetIndexBuffer(gIB, gIndexFormat, 0);
    gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 在 shadow pass 中，强制保证 PS 为空（任何人绑回去都没用）
    if (Shader3d_IsShadowPass())   // 你加个getter返回 g_isShadowPass
    {
        ID3D11DeviceContext* ctx = Direct3D_GetContext();
        ctx->PSSetShader(nullptr, nullptr, 0);
    }

    gCtx->DrawIndexed(gIndexCount, 0, 0);
}

// ---------------------------------------------------------
// DEBUG & Meta
// ---------------------------------------------------------
static int MS_FindTrueRootIndex() {
    for (size_t i = 0; i < gJoints.size(); ++i)
        if (gJoints[i].parent == -1) return (int)i;
    return gJoints.empty() ? -1 : 0;
}
static float MS_YawFromLocalQuat(float qx, float qy, float qz, float qw) {
    XMVECTOR q = XMQuaternionNormalize(XMVectorSet(qx, qy, qz, qw));
    XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
    f = XMVector3Normalize(f);
    XMFLOAT3 fv; XMStoreFloat3(&fv, f);
    return std::atan2f(fv.x, fv.z);
}

bool ModelSkinned_DebugGetRootYaw_F0(float* yaw0) {
    if (!yaw0) return false;
    if (gFrameCount <= 0 || gJoints.empty() || gAnimFrames.empty()) return false;

    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = MS_FindTrueRootIndex();
    if (root < 0) return false;

    const AnimTRS& r0 = gAnimFrames[root]; // 第0帧
    *yaw0 = MS_YawFromLocalQuat(r0.R[0], r0.R[1], r0.R[2], r0.R[3]);
    return true;
}
bool ModelSkinned_DebugGetRootYaw_Current(float* yawNow) {
    if (!yawNow) return false;
    if (gFrameCount <= 0 || gJoints.empty() || gAnimFrames.empty()) return false;

    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = MS_FindTrueRootIndex();
    if (root < 0) return false;

    const size_t J = gJoints.size();
    float frameF = gTime * gSampleRate;
    uint32_t f0 = (uint32_t)floorf(frameF);
    if (f0 >= (uint32_t)gFrameCount) f0 = (uint32_t)gFrameCount - 1;

    const AnimTRS& rc = gAnimFrames[size_t(f0) * J + root];
    *yawNow = MS_YawFromLocalQuat(rc.R[0], rc.R[1], rc.R[2], rc.R[3]);
    return true;
}

uint32_t ModelSkinned_GetFrameCount() { return gFrameCount; }
float    ModelSkinned_GetSampleRate() { return gSampleRate; }

// 计算“第0帧 MotionRoot 的模型空间 yaw”
bool ModelSkinned_ComputeRootYaw_ModelSpace_FirstFrame(float* outRad) {
    if (!outRad) return false;
    const size_t J = gJoints.size();
    if (J == 0 || gFrameCount == 0) return false;

    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = MS_FindTrueRootIndex();
    if (root < 0) root = 0;

    const AnimTRS* pose0 = gAnimFrames.data(); // f0
    for (size_t j = 0; j < J; ++j)
        if (gJoints[j].parent == -1)
            ComputeAnimationPoseRecursively(j, XMMatrixIdentity(), pose0);

    XMMATRIX M = g_temp_globals[root];
    XMVECTOR f = XMVector3Normalize(XMVector3TransformNormal(XMVectorSet(0, 0, 1, 0), M));
    float yaw = std::atan2f(XMVectorGetX(f), XMVectorGetZ(f));
    *outRad = yaw;
    return true;
}

void ModelSkinned_SetZeroRootTranslationXZ(bool enable)
{
    gZeroRootTransXZ = enable;
}

void ModelSkinned_SetZeroTranslationXZBoneByName(const char* utf8Name)
{
    if (!utf8Name || utf8Name[0] == '\0') {
        gZeroExtraXZ_NameUTF8.clear();
        gZeroExtraXZ_Index = -1;
        return;
    }

    gZeroExtraXZ_NameUTF8 = utf8Name;
    ResolveZeroExtraXZ();
}

// ---------------------------------------------------------
// MotionRoot 选择/解析
// ---------------------------------------------------------
bool  ModelSkinned_SetMotionRootByName(const char* utf8Name) {
    if (!utf8Name) return false;
    gMotionRootNameUTF8 = utf8Name;
    if (!gJoints.empty()) ModelSkinned_ResolveMotionRoot();
    return true;
}
int   ModelSkinned_GetMotionRootIndex() {
    if (gMotionRootIndex >= 0 && gMotionRootIndex < (int)gJoints.size())
        return gMotionRootIndex;
    ModelSkinned_ResolveMotionRoot();
    return gMotionRootIndex;
}
const char* ModelSkinned_GetMotionRootName() {
    return gMotionRootNameUTF8.c_str();
}

void  ModelSkinned_ResolveMotionRoot() {
    gMotionRootIndex = -1;
    if (gJoints.empty()) return;

    // 1) 指定名优先
    if (!gMotionRootNameUTF8.empty()) {
        for (size_t i = 0; i < gJoints.size(); ++i) {
            if (gJoints[i].name == gMotionRootNameUTF8) {
                gMotionRootIndex = (int)i;
                break;
            }
        }
    }
    // 2) 常见别名
    if (gMotionRootIndex < 0) {
        static const char* kCandidates[] = {
            "mixamorig:Hips","Hips","Root","root","Armature","Motion","motion"
        };
        for (auto s : kCandidates) {
            for (size_t i = 0; i < gJoints.size(); ++i) {
                if (gJoints[i].name == s) { gMotionRootIndex = (int)i; break; }
            }
            if (gMotionRootIndex >= 0) break;
        }
    }
    // 3) 退回真正根
    if (gMotionRootIndex < 0) {
        for (size_t i = 0; i < gJoints.size(); ++i) {
            if (gJoints[i].parent == -1) { gMotionRootIndex = (int)i; break; }
        }
    }

#if defined(DEBUG) || defined(_DEBUG)
    if (gMotionRootIndex >= 0) {
        char buf[256];
        sprintf_s(buf, "[Skinned] player MotionRoot = #%d (%s)\n",
            gMotionRootIndex, gJoints[gMotionRootIndex].name.c_str());
        OutputDebugStringA(buf);
    }
    else {
        OutputDebugStringA("[Skinned] player MotionRoot resolve FAILED\n");
    }
#endif
}

void ModelSkinned_CaptureCurrentLocalPose()
{
    const size_t J = gJoints.size();
    if (gFrameCount == 0 || J == 0) {
        gCrossFadeSourcePose.clear();
        return;
    }

    gCrossFadeSourcePose.resize(J);

    // 以下逻辑基本 copy 自 Draw() 顶部那段采样代码
    float frameF = gTime * gSampleRate;
    uint32_t f0 = (uint32_t)std::floor(frameF);
    if (f0 >= gFrameCount) f0 = gFrameCount - 1;

    const AnimTRS* currentFramePose = gAnimFrames.data() + size_t(f0) * J;


    const AnimTRS* poseRO = currentFramePose;
    std::vector<AnimTRS> tempPose;
    AnimTRS* poseRW = nullptr;

    // MotionRoot 计算和 Draw 保持一致
    int root = ModelSkinned_GetMotionRootIndex();
    if (root < 0) root = ModelSkinned_GetRootJointIndex();
    if (root < 0) root = 0;

    // ZeroRootTransXZ（VelocityDriven 时清 XZ 平移）
    if (gZeroRootTransXZ && J > 0) {
        tempPose.assign(currentFramePose, currentFramePose + J);
        tempPose[root].T[0] = 0.0f;
        tempPose[root].T[2] = 0.0f;
        poseRW = tempPose.data();
    }

    if (gZeroExtraXZ_Index >= 0 && gZeroExtraXZ_Index < (int)J /* && gZeroExtraXZ_Index != root */) {
        if (!poseRW) {
            tempPose.assign(currentFramePose, currentFramePose + J);
            poseRW = tempPose.data();
        }
        poseRW[gZeroExtraXZ_Index].T[0] = 0.0f;
        poseRW[gZeroExtraXZ_Index].T[2] = 0.0f;

#if defined(DEBUG) || defined(_DEBUG)
        // 添加日志验证清除生效
        static int sDbg = 0;
        if ((sDbg++ % 30) == 0) {
            char buf[256];
            sprintf_s(buf, "[Draw] Cleared XZ for bone #%d (%s)\n",
                gZeroExtraXZ_Index,
                gJoints[gZeroExtraXZ_Index].name.c_str());
            OutputDebugStringA(buf);
        }
#endif

    }

    // RootYawAlign（跟 Draw 中那段用的 AngleWrap / YawFromLocal 完全一致）
    if (gRootYawAlignEnabled && J > 0) {
        if (!poseRW) {
            tempPose.assign(currentFramePose, currentFramePose + J);
            poseRW = tempPose.data();
        }

        auto AngleWrap = [](float a)->float {
            const float PI = 3.14159265358979323846f, TWO = 6.283185307179586f;
            while (a > PI) a -= TWO;
            while (a <= -PI) a += TWO;
            return a;
            };
        auto YawFromLocal = [](const AnimTRS& t)->float {
            XMVECTOR q = XMQuaternionNormalize(XMVectorSet(t.R[0], t.R[1], t.R[2], t.R[3]));
            XMVECTOR f = XMVector3Rotate(XMVectorSet(0, 0, 1, 0), q);
            f = XMVector3Normalize(f);
            XMFLOAT3 fv; XMStoreFloat3(&fv, f);
            return std::atan2f(fv.x, fv.z);
            };

        const float yawCurr = YawFromLocal(poseRW[root]);
        const float visualOffset = AngleWrap(gRootYawAlignTarget - gRootYawStart);
        const float deltaCum = AngleWrap(yawCurr - gRootYawStart);
        const float fixYaw = AngleWrap(visualOffset - deltaCum);

        XMVECTOR qLocal = XMQuaternionNormalize(XMVectorSet(
            poseRW[root].R[0], poseRW[root].R[1], poseRW[root].R[2], poseRW[root].R[3]));
        XMVECTOR qFix = XMQuaternionRotationRollPitchYaw(0.0f, fixYaw, 0.0f);
        XMVECTOR qOut = XMQuaternionMultiply(qFix, qLocal);

        XMFLOAT4 out; XMStoreFloat4(&out, qOut);
        poseRW[root].R[0] = out.x; poseRW[root].R[1] = out.y;
        poseRW[root].R[2] = out.z; poseRW[root].R[3] = out.w;
    }

    const AnimTRS* finalPose = poseRW ? (const AnimTRS*)poseRW : poseRO;

    // 拷贝到快照
    std::copy(finalPose, finalPose + J, gCrossFadeSourcePose.begin());
}

void ModelSkinned_SetCrossFadeWeight(float w)
{
    gCrossFadeWeight = w;
}

void ModelSkinned_ClearCrossFadeSource()
{
    gCrossFadeSourcePose.clear();
}

void ModelSkinned_GetWorldTranslation(DirectX::XMFLOAT3* outT)
{
    if (!outT) return;
    DirectX::XMFLOAT4X4 m;
    DirectX::XMStoreFloat4x4(&m, gWorld); // 你内部保存的 world 矩阵
    outT->x = m._41;
    outT->y = m._42;
    outT->z = m._43;
}
