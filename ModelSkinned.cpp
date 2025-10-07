#include "ModelSkinned.h"
#include <DirectXMath.h>
#include <d3d11.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <Windows.h>

#include "asset_format.h"     // 你 AssetCooker 的公共头
#include "shader3d.h"         // 用来设置 b0~b4（已存在）
#include "texture.h"          // Texture_Load / Texture_SetTexture
#include "sampler.h"          // Sampler_SetFillterAnisotropic 等

using namespace DirectX;
namespace fs = std::filesystem;

// ---------------------------------------------------------
// 内部全局状态（按你的偏好：隐藏在实现里，Draw() 无参数）
// ---------------------------------------------------------
static ID3D11Device* gDev = nullptr;
static ID3D11DeviceContext* gCtx = nullptr;

static ID3D11Buffer* gVB = nullptr;
static ID3D11Buffer* gIB = nullptr;
static UINT                 gIndexCount = 0;
static DXGI_FORMAT          gIndexFormat = DXGI_FORMAT_R16_UINT;

static ID3D11VertexShader* gVS = nullptr;
static ID3D11InputLayout* gIL = nullptr;

// b5：骨矩阵数组（行主 -> HLSL 可列主用 mul ）
static ID3D11Buffer* gCBBones = nullptr;
static const UINT           MAX_BONES = 128;

// 世界矩阵
static XMMATRIX             gWorld = XMMatrixIdentity();

// 贴图
static int                  gTexId = -1;

// 骨架
struct Joint {
    int parent = -1;
    XMFLOAT4X4 invBind; // 从 .skel 读出（列主写入的 16 float），这里按行主存储方便
    XMFLOAT3   bindT{ 0,0,0 };
    XMFLOAT4   bindR{ 0,0,0,1 };
    XMFLOAT3   bindS{ 1,1,1 };
};
static std::vector<Joint>   gJoints;

// 动画
static float gSampleRate = 30.0f;   // 从 .anim 读
static float gDurationSec = 0.0f;
static uint32_t gFrameCount = 0;
static bool   gLoop = true;
static float  gPlayback = 1.0f;
static float  gTime = 0.0f;    // 当前时间（秒）

static std::vector<AnimTRS> gAnimFrames; // 连续存储：frame 0..N-1，每帧 jointCount 个 AnimTRS

// 临时：每帧计算的调色板（骨矩阵，行主）
static std::vector<XMFLOAT4X4> gPalette;

// 工具：SAFE_RELEASE
template<typename T> static void SAFE_RELEASE(T*& p) { if (p) { p->Release(); p = nullptr; } }

// ▼▼▼ 新增这个打印矩阵的函数 ▼▼▼
static void PrintMatrix(const char* name, const XMMATRIX& M) {
    printf("Matrix: %s\n", name);
    for (int r = 0; r < 4; ++r) {
        printf("  [ %10.6f, %10.6f, %10.6f, %10.6f ]\n",
            M.r[r].m128_f32[0], M.r[r].m128_f32[1], M.r[r].m128_f32[2], M.r[r].m128_f32[3]);
    }
}
// ▲▲▲ 新增结束 ▲▲▲
// 
// 工具用
static std::vector<XMMATRIX> g_temp_globals;

static XMMATRIX MakeLocalMatrix(const AnimTRS& t);

// 递归函数，用于正确计算每个骨骼的全局绑定姿势矩阵
static void ComputeGlobalBindPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform)
{
    // 1. 获取当前骨骼的局部绑定姿势矩阵
    const auto& joint = gJoints[boneIndex];
    XMVECTOR T = XMLoadFloat3(&joint.bindT);
    XMVECTOR R = XMLoadFloat4(&joint.bindR);
    XMVECTOR S = XMLoadFloat3(&joint.bindS);
    XMMATRIX localBindPose = XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(T);

    // 2. 计算当前骨骼的全局矩阵
    XMMATRIX globalTransform = localBindPose * parentGlobalTransform;
    g_temp_globals[boneIndex] = globalTransform;

    // 3. 递归处理所有子骨骼
    for (size_t i = 0; i < gJoints.size(); ++i) {
        if (gJoints[i].parent == boneIndex) {
            ComputeGlobalBindPoseRecursively(i, globalTransform);
        }
    }
}

static void ComputeAnimationPoseRecursively(size_t boneIndex, const XMMATRIX& parentGlobalTransform, const AnimTRS* currentFramePose)
{
    // 1. 从当前动画帧数据中，获取此骨骼的局部变换矩阵
    const AnimTRS& jointPose = currentFramePose[boneIndex];
    XMMATRIX localTransform = MakeLocalMatrix(jointPose); // MakeLocalMatrix 函数已存在，无需修改

    // 2. 计算当前骨骼的全局变换矩阵
    XMMATRIX globalTransform = localTransform * parentGlobalTransform;

    // 3. 存储结果
    g_temp_globals[boneIndex] = globalTransform;

    // 4. 递归处理所有子骨骼
    for (size_t i = 0; i < gJoints.size(); ++i) {
        if (gJoints[i].parent == boneIndex) {
            ComputeAnimationPoseRecursively(i, globalTransform, currentFramePose);
        }
    }
}

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

// 将列主（AssetCooker 写入的 float[16]）拷到 XMFLOAT4X4（行主存储）
static XMFLOAT4X4 Float16_ColMajor_to_RowMajor(const float* m16) {
    XMFLOAT4X4 M{};
    // 行主存储 M(r,c) = m16[c*4 + r]
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            (&M.m[0][0])[r * 4 + c] = m16[c * 4 + r];
    return M;
}

// TRS -> 行主矩阵
static XMMATRIX MakeLocalMatrix(const AnimTRS& t) {
    XMVECTOR T = XMVectorSet(t.T[0], t.T[1], t.T[2], 0.0f);
    XMVECTOR R = XMVectorSet(t.R[0], t.R[1], t.R[2], t.R[3]);
    XMVECTOR S = XMVectorSet(t.S[0], t.S[1], t.S[2], 1.0f);
    return XMMatrixScalingFromVector(S) * XMMatrixRotationQuaternion(R) * XMMatrixTranslationFromVector(T);
}

// ---------------------------------------------------------
// 从 .mat 尝试读取第一条材质的 baseColorTex 并加载
// ---------------------------------------------------------
static bool TryLoadBaseColorFromMat(const std::wstring& matPathW) {
    if (matPathW.empty()) return false;
    std::ifstream f(matPathW, std::ios::binary);
    if (!f) return false;

    FileHeader fh{};
    if (!f.read((char*)&fh, sizeof(fh))) return false;
    if (std::memcmp(fh.magic, "MATL", 4) != 0) return false;

    MaterialHeader mh{};
    if (!f.read((char*)&mh, sizeof(mh))) return false;
    if (mh.materialCount == 0) return false;

    MaterialRec rec{};
    if (!f.read((char*)&rec, sizeof(rec))) return false;

    if (rec.baseColorTex[0]) {
        // 组合路径：<mat同目录>/<rec.baseColorTex>
        fs::path folder = fs::path(matPathW).parent_path();
        // rec.baseColorTex 是 char[96]，转宽字串
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

    // ▼▼▼ 在这里加入决定性的检查代码 ▼▼▼
    char buffer[256];
    sprintf_s(buffer, 256, "--- MESH FILE CHECK --- Vertex Stride in header: %u\n", mh->vertexStride);
    OutputDebugStringA(buffer); // 这条信息会打印到 Visual Studio 的“输出”窗口

    if (mh->vertexStride != 56) {
        OutputDebugStringA("--- MESH FILE CHECK --- FATAL ERROR: Vertex Stride is NOT 56! This is the wrong .mesh file!\n");
        __debugbreak(); // 如果 stride 不是 56，程序会在这里中断
    }
    // ▲▲▲ 检查代码结束 ▲▲▲


    if ((mh->flags & HAS_SKIN) == 0) {
        // 必须是 v1（含蒙皮）
        // 你也可以容错：静态也画，但这里我们严格点
        return false;
    }

    const UINT stride = mh->vertexStride; // 56
    const UINT vcount = mh->vertexCount;
    const UINT icount = mh->indexCount;
    const bool idx32 = (mh->vertexCount > 65535);

    size_t vbBytes = size_t(vcount) * stride;
    if (!need(vbBytes)) return false;
    const void* vbData = p; p += vbBytes;

    size_t ibBytes = size_t(icount) * (idx32 ? 4 : 2);
    if (!need(ibBytes)) return false;
    const void* ibData = p; p += ibBytes;

    size_t sbBytes = sizeof(Submesh) * mh->submeshCount;
    if (need(sbBytes)) p += sbBytes; // 暂不拆材质组，你需要的话可存下来

    // 创建 VB
    SAFE_RELEASE(gVB);
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = UINT(vbBytes);
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vbData;
    if (FAILED(gDev->CreateBuffer(&bd, &sd, &gVB))) return false;

    // 创建 IB
    SAFE_RELEASE(gIB);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.ByteWidth = UINT(ibBytes);
    sd.pSysMem = ibData;
    if (FAILED(gDev->CreateBuffer(&bd, &sd, &gIB))) return false;

    gIndexCount = icount;
    gIndexFormat = idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;

    // 准备输入布局（与你 HLSL 的语义一致）
    // POSITION (float3), NORMAL (float3), TANGENT (float4), TEXCOORD (float2),
    // BLENDINDICES (R8G8B8A8_UINT), BLENDWEIGHT (R8G8B8A8_UNORM)
    // 都在同一个 slot 0，按 stride=mh->vertexStride 走
    // 注意：AlignedByteOffset 我们用 D3D11_APPEND_ALIGNED_ELEMENT，前提是 AssetCooker 按上述顺序紧密写入
    // （它就是这么写的：48 + 8 = 56）
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
// 加载 .skel
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

    gJoints.clear(); gJoints.resize(sh->jointCount);
    if (!need(sizeof(JointRec) * sh->jointCount)) return false;

    for (uint32_t i = 0; i < sh->jointCount; ++i) {
        auto jr = (const JointRec*)p; p += sizeof(JointRec);
        Joint j{};
        j.parent = jr->parent;
        j.invBind = Float16_ColMajor_to_RowMajor(jr->invBind);
        j.bindT = XMFLOAT3(jr->bindLocalT[0], jr->bindLocalT[1], jr->bindLocalT[2]);
        j.bindR = XMFLOAT4(jr->bindLocalR[0], jr->bindLocalR[1], jr->bindLocalR[2], jr->bindLocalR[3]);
        j.bindS = XMFLOAT3(jr->bindLocalS[0], jr->bindLocalS[1], jr->bindLocalS[2]);
        gJoints[i] = j;
    }

    gPalette.resize(std::max<size_t>(1, gJoints.size()));
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
        // 骨骼数量和动画通道不匹配也允许，但这里先严格些
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
// 创建骨矩阵常量缓冲
// ---------------------------------------------------------
static bool CreateCBBones() {
    SAFE_RELEASE(gCBBones);
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.ByteWidth = ((UINT)MAX_BONES * sizeof(XMFLOAT4X4) + 255) & ~255u; // 常量缓冲 256 对齐
    return SUCCEEDED(gDev->CreateBuffer(&bd, nullptr, &gCBBones));
}

// ---------------------------------------------------------
// 对外 API
// ---------------------------------------------------------
bool ModelSkinned_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx) {
    gDev = dev; gCtx = ctx;
    return CreateCBBones();
}

bool ModelSkinned_Load(const ModelSkinnedDesc& d) {
    // mesh
    if (!LoadMeshV1(d.meshPath)) return false;
    // skeleton
    if (!LoadSkel(d.skelPath)) return false;
    // anim (optional)
    if (!LoadAnim(d.animPath)) return false;

    // texture: override > .mat > none
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
    SAFE_RELEASE(gVB);
    SAFE_RELEASE(gIB);
    SAFE_RELEASE(gVS);
    SAFE_RELEASE(gIL);
    SAFE_RELEASE(gCBBones);
    gJoints.clear();
    gAnimFrames.clear();
    gIndexCount = 0;
    gTexId = -1;
}

void ModelSkinned_Update(double dtSec) {
    if (gFrameCount == 0 || gJoints.empty()) return;
    gTime += float(dtSec) * gPlayback;
    if (gLoop) {
        if (gDurationSec > 0.0f) {
            while (gTime >= gDurationSec) gTime -= gDurationSec;
            while (gTime < 0.0f) gTime += gDurationSec;
        }
    }
    else {
        gTime = std::clamp(gTime, 0.0f, gDurationSec);
    }
}

void ModelSkinned_SetWorldMatrix(const XMMATRIX& world) {
    gWorld = world;
}

void ModelSkinned_SetLoop(bool loop) { gLoop = loop; }
void ModelSkinned_SetPlaybackRate(float rate) { gPlayback = rate; }
void ModelSkinned_Seek(float t) { gTime = t; }

void ModelSkinned_Draw() {
    if (!gVB || !gIB || !gVS || !gIL) return;

    // 1) 计算骨骼变换矩阵
    const size_t J = gJoints.size();
    if (J == 0) return;

    g_temp_globals.resize(J);

    // 根据是否有动画数据，选择不同的计算路径
    if (gFrameCount > 0) {
        // 路径A：有动画，计算当前动画姿势
        // 简单地取最近的帧
        float frameF = gTime * gSampleRate;
        uint32_t f0 = (uint32_t)std::floor(frameF);
        if (f0 >= gFrameCount) f0 = gFrameCount - 1;

        const AnimTRS* currentFramePose = gAnimFrames.data() + size_t(f0) * J;

        // 从根骨骼开始，递归计算所有骨骼的全局矩阵
        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                // 递归的起点是单位矩阵。
                // 因为正确的场景根变换已由AssetCooker烘焙进骨骼的局部变换数据中。
                ComputeAnimationPoseRecursively(j, XMMatrixIdentity(), currentFramePose);
            }
        }
    }
    else {
        // 路径B：没有动画，计算并显示绑定姿势
        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                ComputeGlobalBindPoseRecursively(j, XMMatrixIdentity());
            }
        }
    }

    // 2) 基于计算出的全局矩阵，生成最终给到Shader的调色板矩阵
    for (size_t j = 0; j < J; ++j) {
        XMMATRIX invB = XMLoadFloat4x4(&gJoints[j].invBind);

        // 最终矩阵 = 逆绑定矩阵 * 当前姿势的全局矩阵
        XMMATRIX M = invB * g_temp_globals[j];

        // 为上传到HLSL，将行主序的XMMATRIX转置为列主序
        XMStoreFloat4x4(&gPalette[j], XMMatrixTranspose(M));
    }

    // 3) 上传骨骼矩阵数据到常量缓冲区 (b5)
    D3D11_MAPPED_SUBRESOURCE mp{};
    if (SUCCEEDED(gCtx->Map(gCBBones, 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
        size_t copyJ = std::min(J, size_t(MAX_BONES));
        std::memcpy(mp.pData, gPalette.data(), copyJ * sizeof(XMFLOAT4X4));
        gCtx->Unmap(gCBBones, 0);
    }
    gCtx->VSSetConstantBuffers(5, 1, &gCBBones);

    // 4) 绑定渲染状态 (包括默认的VS/PS, CBs 0-4等)
    Shader3d_Begin();
    // 用我们蒙皮专用的VS和IL，覆盖掉Shader3d_Begin()中设置的默认项
    gCtx->VSSetShader(gVS, nullptr, 0);
    gCtx->IASetInputLayout(gIL);

    // 5) 绑定顶点和索引缓冲
    UINT stride = 56;
    UINT offset = 0;
    gCtx->IASetVertexBuffers(0, 1, &gVB, &stride, &offset);
    gCtx->IASetIndexBuffer(gIB, gIndexFormat, 0);
    gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 6) 设置世界矩阵 (通过Shader3d模块)
    Shader3d_SetWorldMatrix(gWorld);

    // 7) 绑定贴图和采样器
    if (gTexId >= 0) Texture_SetTexture(gTexId);
    Sampler_SetFillterAnisotropic();

    // 8) 发出绘制指令
    gCtx->DrawIndexed(gIndexCount, 0, 0);
}