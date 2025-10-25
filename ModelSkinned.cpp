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

static ID3D11Buffer* gCBAmbient = nullptr;     // VS b3
static ID3D11Buffer* gCBDirectional = nullptr; // VS b4

//给动画位移用的
static bool gZeroRootTransXZ = false;

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

static bool DecomposeRowMajor(const XMMATRIX& M, XMVECTOR& S, XMVECTOR& R, XMVECTOR& T) {
    return XMMatrixDecompose(&S, &R, &T, M);
}

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
        //j.invBind = Float16_ColMajor_to_RowMajor(jr->invBind);
        memcpy(&j.invBind, jr->invBind, sizeof(float) * 16);
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


// === 辅助：拿根索引 ===
int ModelSkinned_GetRootJointIndex()
{
    for (int i = 0; i < (int)gJoints.size(); ++i)
        if (gJoints[i].parent == -1) return i;
    return -1;
}

// === 辅助：按时间 t 线性插值采样某关节的 TRS（只用到T；S/R 保留以防以后扩展） ===
static AnimTRS SampleJointTRS_Linear(int joint, float tSec)
{
    AnimTRS out{};
    if (gFrameCount == 0 || gJoints.empty()) return out;

    const float rate = gSampleRate;             // 帧率（来自 .anim）
    const float f = tSec * rate;             // 连续帧坐标
    int   f0 = (int)floorf(f);
    float a = f - f0;                          // 插值因子
    auto wrap = [&](int x) {                    // 循环动画时的包裹
        if (gFrameCount == 0) return 0;
        while (x < 0) x += (int)gFrameCount;
        while (x >= (int)gFrameCount) x -= (int)gFrameCount;
        return x;
        };
    int f1 = wrap(f0 + 1);
    f0 = wrap(f0);

    const size_t J = gJoints.size();
    const AnimTRS* base = gAnimFrames.data();

    const AnimTRS& A = base[(size_t)f0 * J + joint];
    const AnimTRS& B = base[(size_t)f1 * J + joint];

    // 线性插值平移/缩放、四元数做球面插值（稳一点）
    out.T[0] = A.T[0] + (B.T[0] - A.T[0]) * a;
    out.T[1] = A.T[1] + (B.T[1] - A.T[1]) * a;
    out.T[2] = A.T[2] + (B.T[2] - A.T[2]) * a;

    out.S[0] = A.S[0] + (B.S[0] - A.S[0]) * a;
    out.S[1] = A.S[1] + (B.S[1] - A.S[1]) * a;
    out.S[2] = A.S[2] + (B.S[2] - A.S[2]) * a;

    DirectX::XMVECTOR qA = DirectX::XMVectorSet(A.R[0], A.R[1], A.R[2], A.R[3]);
    DirectX::XMVECTOR qB = DirectX::XMVectorSet(B.R[0], B.R[1], B.R[2], B.R[3]);
    DirectX::XMVECTOR q = DirectX::XMQuaternionSlerp(qA, qB, a);
    DirectX::XMFLOAT4 qf;
    DirectX::XMStoreFloat4(&qf, q);
    out.R[0] = qf.x; out.R[1] = qf.y; out.R[2] = qf.z; out.R[3] = qf.w;

    return out;
}

// === 采样根关节在区间 [t, t+dt] 的“局部位移 ΔT” ===
// 说明：该函数使用“内部当前时间 gTime”作为起点，所以需要在 AnimatorRegistry_Update 里
//       先调用它，再调用 ModelSkinned_Update(dt) 推进时间（否则会错位）
bool ModelSkinned_SampleRootDelta_Local(float dt, DirectX::XMFLOAT3* outDeltaT)
{
    if (!outDeltaT) return false;
    outDeltaT->x = outDeltaT->y = outDeltaT->z = 0.0f;

    if (gFrameCount == 0 || gJoints.empty()) return false;
    const int root = ModelSkinned_GetRootJointIndex();
    if (root < 0) return false;

    // 采样 t0 与 t1 的“根局部平移”，做差即可
    const float t0 = gTime;
    const float t1 = gLoop ? (t0 + dt) : std::min(t0 + dt, gDurationSec);

    AnimTRS R0 = SampleJointTRS_Linear(root, t0);
    AnimTRS R1 = SampleJointTRS_Linear(root, t1);

    outDeltaT->x = (R1.T[0] - R0.T[0]);
    outDeltaT->y = (R1.T[1] - R0.T[1]);
    outDeltaT->z = (R1.T[2] - R0.T[2]);
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

    // b5: bones
    if (!CreateCBBones()) return false;

    // b3: ambient(float4)
    {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(DirectX::XMFLOAT4);
        if (FAILED(gDev->CreateBuffer(&bd, nullptr, &gCBAmbient))) return false;

        const DirectX::XMFLOAT4 amb{ 0.25f, 0.25f, 0.25f, 1.0f }; // 适度环境光
        gCtx->UpdateSubresource(gCBAmbient, 0, nullptr, &amb, 0, 0);
    }
    // b4: dir(vec, color)
    {
        struct DirCB { DirectX::XMFLOAT4 dir; DirectX::XMFLOAT4 color; };
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(DirCB);
        if (FAILED(gDev->CreateBuffer(&bd, nullptr, &gCBDirectional))) return false;

        // 光从上往下（注意 VS 里用 dot(-direction, n)）
        DirCB d{};
        d.dir = DirectX::XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
        d.color = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        gCtx->UpdateSubresource(gCBDirectional, 0, nullptr, &d, 0, 0);
    }

    return true;
}

bool ModelSkinned_Load(const ModelSkinnedDesc& d) {
    // mesh
    if (!LoadMeshV1(d.meshPath)) return false;
    // skeleton
    if (!LoadSkel(d.skelPath)) return false;
    // anim (optional)
    auto RebuildBindLocalsFromInvBind = [&]() {
        const size_t J = gJoints.size();
        if (J == 0) return;

        // ---------- 1) 用“当前 bindLocal（尽管不可靠）”先递归出每根骨骼的 BindGlobal ----------
        g_temp_globals.resize(J);
        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                ComputeGlobalBindPoseRecursively(j, XMMatrixIdentity());
            }
        }

        // 计算每根骨骼的 Mj = Bj * InvBj，并对 16 个分量分别做平均（简单但足够稳）
        double sum[16] = { 0.0 };
        for (size_t j = 0; j < J; ++j) {
            XMMATRIX Bj = g_temp_globals[j];
            XMMATRIX InvBj = XMLoadFloat4x4(&gJoints[j].invBind); // 注意：继续沿用你现在“memcpy 加载”的行主矩阵
            XMMATRIX Mj = Bj * InvBj;

            XMFLOAT4X4 fm; XMStoreFloat4x4(&fm, Mj);
            const float* p = &fm._11;
            for (int k = 0; k < 16; ++k) sum[k] += p[k];
        }
        float avg[16];
        for (int k = 0; k < 16; ++k) avg[k] = float(sum[k] / double(J));

        // 平均后的 MeshGlobalAtBind
        XMFLOAT4X4 avgM{}; std::memcpy(&avgM._11, avg, sizeof(avg));
        XMMATRIX MeshGlobalAtBind = XMLoadFloat4x4(&avgM);

        // ---------- 2) 用 InvBind 反推“每根骨骼的绑定时全局矩阵” ----------
        std::vector<XMMATRIX> G(J);
        for (size_t j = 0; j < J; ++j) {
            XMMATRIX InvBj = XMLoadFloat4x4(&gJoints[j].invBind);
            // 注意：Inv（InvBj）就是 Bj；MeshGlobalAtBind * Bj 这一步不是恒等，
            // 它把坐标系统一到了“网格绑定时的全局坐标系”下
            XMMATRIX Bj = MeshGlobalAtBind * XMMatrixInverse(nullptr, InvBj);
            G[j] = Bj;
        }

        // ---------- 3) 统一 parent 关系，重算本地 TRS 并回填 ----------
        for (size_t j = 0; j < J; ++j) {
            XMMATRIX parentG = (gJoints[j].parent >= 0) ? G[gJoints[j].parent] : XMMatrixIdentity();
            XMMATRIX local = XMMatrixInverse(nullptr, parentG) * G[j];

            XMVECTOR S, R, T;
            if (!XMMatrixDecompose(&S, &R, &T, local)) continue;

            XMStoreFloat3(&gJoints[j].bindT, T);
            XMStoreFloat4(&gJoints[j].bindR, R);
            XMStoreFloat3(&gJoints[j].bindS, S);
        }
        };

    // 调用一次修正
    RebuildBindLocalsFromInvBind();

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

    SAFE_RELEASE(gCBDirectional);
    SAFE_RELEASE(gCBAmbient);
    SAFE_RELEASE(gCBBones);
    SAFE_RELEASE(gVB);
    SAFE_RELEASE(gIB);
    SAFE_RELEASE(gVS);
    SAFE_RELEASE(gIL);
    //SAFE_RELEASE(gCBBones);
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
void ModelSkinned_SetZeroRootTranslationXZ(bool enable) { gZeroRootTransXZ = enable; }

bool ModelSkinned_LoadAnimOnly(const std::wstring& animPath)
{
    return LoadAnim(animPath);
}

void ModelSkinned_Draw() {
    if (!gVB || !gIB || !gVS || !gIL) return;

    // 1) 关节数检查
    const size_t J = gJoints.size();
    if (J == 0) return;

    // 让临时数组有空间（后面复用）
    g_temp_globals.resize(J);

    // ====== A. 绑定姿势一致性检查（仅跑一次）======
    {
        static bool s_checked = false;
        if (!s_checked) {
            // 1) 用当前 bindLocal 递归得出每根骨骼的绑定全局矩阵 Bj
            for (size_t j = 0; j < J; ++j)
                if (gJoints[j].parent == -1)
                    ComputeGlobalBindPoseRecursively(j, XMMatrixIdentity());

            // 2) 计算 Mj = InvBj * Bj
            std::vector<XMMATRIX> Ms(J);
            for (size_t j = 0; j < J; ++j) {
                XMMATRIX Bj = g_temp_globals[j];
                XMMATRIX InvB = XMLoadFloat4x4(&gJoints[j].invBind);
                Ms[j] = InvB * Bj;
            }

            // 3) 相对 Frobenius & 去缩放的正交误差（记录最差索引）
            auto frob = [](const XMFLOAT4X4& m) {
                double s = 0; const float* p = &m._11; for (int k = 0; k < 16; ++k) s += double(p[k]) * double(p[k]); return std::sqrt(s);
                };
            auto ortho_err_noscale = [](const XMMATRIX& M)->double {
                XMFLOAT4X4 f; XMStoreFloat4x4(&f, M);
                double c0[3] = { f._11,f._21,f._31 }, c1[3] = { f._12,f._22,f._32 }, c2[3] = { f._13,f._23,f._33 };
                auto norm = [](const double c[3]) {return std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]); };
                auto dot = [](const double a[3], const double b[3]) {return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
                double n0 = std::max(1e-12, norm(c0)), n1 = std::max(1e-12, norm(c1)), n2 = std::max(1e-12, norm(c2));
                for (int i = 0; i < 3; ++i) { c0[i] /= n0; c1[i] /= n1; c2[i] /= n2; }
                double RtR[3][3] = { {dot(c0,c0),dot(c0,c1),dot(c0,c2)},
                                  {dot(c1,c0),dot(c1,c1),dot(c1,c2)},
                                  {dot(c2,c0),dot(c2,c1),dot(c2,c2)} };
                double s = 0; for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) { double t = RtR[i][j] - (i == j ? 1.0 : 0.0); s += t * t; }
                return std::sqrt(s);
                };

            // 先算平均矩阵用于相对误差
            double sum[16] = { 0 };
            for (size_t j = 0; j < J; ++j) { XMFLOAT4X4 fm; XMStoreFloat4x4(&fm, Ms[j]); const float* p = &fm._11; for (int k = 0; k < 16; ++k) sum[k] += p[k]; }
            float avgf[16]; for (int k = 0; k < 16; ++k) avgf[k] = float(sum[k] / double(J));
            XMFLOAT4X4 favg{}; memcpy(&favg._11, avgf, sizeof(avgf));
            XMMATRIX Mavg = XMLoadFloat4x4(&favg);
            XMFLOAT4X4 favgF; XMStoreFloat4x4(&favgF, Mavg);
            double normAvg = std::max(1e-12, frob(favgF));

            double worstRel = 0.0, worstOrtho = 0.0;
            size_t worstRelJ = 0, worstOrthoJ = 0;
            for (size_t j = 0; j < J; ++j) {
                XMFLOAT4X4 fM; XMStoreFloat4x4(&fM, Ms[j]);
                XMFLOAT4X4 fD; XMStoreFloat4x4(&fD, Ms[j] - Mavg);
                double rel = frob(fD) / normAvg;
                if (rel > worstRel) { worstRel = rel; worstRelJ = j; }
                double o = ortho_err_noscale(Ms[j]);
                if (o > worstOrtho) { worstOrtho = o; worstOrthoJ = j; }
            }

            char buf[256];
            sprintf_s(buf, "[SKIN CHECK] rel-Frob(worst)=%.6g @j=%zu  ortho-noscale(worst)=%.6g @j=%zu\n",
                worstRel, worstRelJ, worstOrtho, worstOrthoJ);
            OutputDebugStringA(buf);

            // 4) 对“最差骨骼”打印 3 列范数与两两点积（看是缩放大，还是剪切大）
            auto dump3x3 = [](const XMMATRIX& M, const char* tag) {
                XMFLOAT4X4 f; XMStoreFloat4x4(&f, M);
                double c0[3] = { f._11,f._21,f._31 }, c1[3] = { f._12,f._22,f._32 }, c2[3] = { f._13,f._23,f._33 };
                auto norm = [](const double c[3]) {return std::sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]); };
                auto dot = [](const double a[3], const double b[3]) {return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
                char b2[256];
                sprintf_s(b2, "[SKIN CHECK] %s |col norms| = (%.4f, %.4f, %.4f),  dots = (c0·c1=%.4f, c0·c2=%.4f, c1·c2=%.4f)\n",
                    tag, norm(c0), norm(c1), norm(c2), dot(c0, c1), dot(c0, c2), dot(c1, c2));
                OutputDebugStringA(b2);
                };
            dump3x3(Ms[worstOrthoJ], "InvB*B of worst-ortho joint");

            s_checked = true;

            // 你也可以按需只在超过阈值时再详细 dump
            s_checked = true;
        }
    }

    // 根据是否有动画数据，选择不同的计算路径
    if (gFrameCount > 0) {
        // 路径A：有动画，计算当前动画姿势
        // 简单地取最近的帧
        float frameF = gTime * gSampleRate;
        uint32_t f0 = (uint32_t)std::floor(frameF);
        if (f0 >= gFrameCount) f0 = gFrameCount - 1;

        const AnimTRS* currentFramePose = gAnimFrames.data() + size_t(f0) * J;

        // 可选：VelocityDriven 时把根局部平移的 XZ 清零（保留 Y 抖动）
        const AnimTRS* pose = currentFramePose;
        std::vector<AnimTRS> tempPose;
        if (gZeroRootTransXZ && J > 0) {
            tempPose.assign(currentFramePose, currentFramePose + J);
            // root = 0 号关节；只清 XZ，保留 Y（上下起伏对步态有帮助）
            tempPose[0].T[0] = 0.0f;  // X
            tempPose[0].T[2] = 0.0f;  // Z
            pose = tempPose.data();
        }

        // ↓↓↓ 把后续递归的输入从 currentFramePose 换成 pose
        for (size_t j = 0; j < J; ++j) {
            if (gJoints[j].parent == -1) {
                ComputeAnimationPoseRecursively(j, XMMatrixIdentity(), pose);
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
    Shader3d_Begin();      // ← 先让它把 b0~b4 等统一好

    Shader3d_SetColor({ 1,1,1,1 });

    gCtx->VSSetShader(gVS, nullptr, 0);
    gCtx->IASetInputLayout(gIL);
    Shader3d_SetWorldMatrix(gWorld);

    ID3D11Buffer* cbs34[2] = { gCBAmbient, gCBDirectional };
    gCtx->VSSetConstantBuffers(3, 2, cbs34);

    // ！！现在再把 b5 绑回去（避免被 Begin 清掉/覆盖）
    gCtx->VSSetConstantBuffers(5, 1, &gCBBones);

    // 然后再设纹理/采样、VB/IB、Draw
    if (gTexId >= 0) Texture_SetTexture(gTexId);
    Sampler_SetFillterAnisotropic();


    UINT stride = 56, offset = 0;
    gCtx->IASetVertexBuffers(0, 1, &gVB, &stride, &offset);
    gCtx->IASetIndexBuffer(gIB, gIndexFormat, 0);
    gCtx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    gCtx->DrawIndexed(gIndexCount, 0, 0);
}