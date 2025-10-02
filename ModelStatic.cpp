//#define NOMINMAX
#include "ModelStatic.h"

#include <vector>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error "No <filesystem>; enable C++17."
#endif
#include <cstdio>
#include <cstring>

#include "shader3d.h"  // Shader3d_Begin/SetWorldMatrix
#include "texture.h"   // Texture_Load(const wchar_t*), Texture_SetTexture(int)
#include "sampler.h"

using namespace DirectX;

// ================== 与你 VS/PS 匹配的顶点 ==================
struct VertexForYourShader {
    float px, py, pz;            // POSITION
    float nx, ny, nz;      // NORMAL
    float cr, cg, cb, ca;        // COLOR（这里固定白）
    float u, v;                  // TEXCOORD0
};

struct ModelStatic {
    ID3D11Buffer* vb = nullptr;
    ID3D11Buffer* ib = nullptr;
    DXGI_FORMAT   idxFmt = DXGI_FORMAT_R16_UINT;
    UINT          indexCount = 0;
    int           texId = -1;    // PS t0
};

// --------- 全局静态 ----------
static ModelStatic          s_models[8]{};
static ID3D11Device* s_dev = nullptr;
static ID3D11DeviceContext* s_ctx = nullptr;
static int                  s_whiteTexId = -1;

// “默认模型”封装
static std::wstring         s_defMeshPath;
static std::wstring         s_defMatPath;
static std::wstring         s_defOverrideTex;
static int                  s_defHandle = -1;
static XMMATRIX             s_defWorld = XMMatrixIdentity();

// --------- 小工具 ----------
static bool ReadAll(FILE* f, void* p, size_t n) { return std::fread(p, 1, n, f) == n; }

static void EnsureWhiteTexture() {
    if (s_whiteTexId >= 0) return;
    s_whiteTexId = Texture_Load(L"resources/white.png"); // 放一张 1x1 白图
}

static int TryLoadTextureNearMat(const std::wstring& matPathW, const char* fileName)
{
    if (!fileName || !fileName[0]) return -1;
    size_t len = strnlen(fileName, 512);
    std::wstring fnW; fnW.reserve(len);
    for (size_t i = 0; i < len; ++i) fnW.push_back(wchar_t(static_cast<unsigned char>(fileName[i])));

    fs::path matP(matPathW);
    fs::path dir = matP.parent_path();

    fs::path cand1 = dir / fnW;
    if (fs::exists(cand1)) return Texture_Load(cand1.c_str());

    fs::path cand2 = dir / L"Textures" / fnW;
    if (fs::exists(cand2)) return Texture_Load(cand2.c_str());

    return -1;
}

static int LoadBaseColorFromMat(const std::wstring& matPathW)
{
    if (matPathW.empty()) return -1;

    FILE* f = nullptr;
    if (_wfopen_s(&f, matPathW.c_str(), L"rb") != 0 || !f) return -1;

    FileHeader fh{};
    if (!ReadAll(f, &fh, sizeof(fh))) { std::fclose(f); return -1; }
    if (std::string(fh.magic, fh.magic + 4) != "MATL") { std::fclose(f); return -1; }

    MaterialHeader mh{};
    if (!ReadAll(f, &mh, sizeof(mh))) { std::fclose(f); return -1; }
    if (mh.materialCount == 0) { std::fclose(f); return -1; }

    MaterialRec rec{};
    if (!ReadAll(f, &rec, sizeof(rec))) { std::fclose(f); return -1; }
    std::fclose(f);

    if (rec.baseColorTex[0] != '\0') {
        int id = TryLoadTextureNearMat(matPathW, rec.baseColorTex);
        if (id >= 0) return id;
    }
    return -1;
}

static bool LoadMeshToVBIB(const std::wstring& meshPathW,
    std::vector<VertexForYourShader>& outVB,
    std::vector<uint8_t>& outIB,
    DXGI_FORMAT& outFmt,
    UINT& outIndexCount)
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, meshPathW.c_str(), L"rb") != 0 || !f) return false;

    FileHeader fh{};
    if (!ReadAll(f, &fh, sizeof(fh))) { std::fclose(f); return false; }
    if (std::string(fh.magic, fh.magic + 4) != "MESH") { std::fclose(f); return false; }

    MeshHeader mh{};
    if (!ReadAll(f, &mh, sizeof(mh))) { std::fclose(f); return false; }

    const size_t vbBytes = size_t(mh.vertexCount) * mh.vertexStride;
    std::vector<uint8_t> vbRaw(vbBytes);
    if (!ReadAll(f, vbRaw.data(), vbBytes)) { std::fclose(f); return false; }

    const bool use32 = (mh.vertexCount > 65535);
    const size_t ibBytes = size_t(mh.indexCount) * (use32 ? 4 : 2);
    outIB.resize(ibBytes);
    if (!ReadAll(f, outIB.data(), ibBytes)) { std::fclose(f); return false; }

    if (mh.submeshCount) {
        std::vector<Submesh> tmp(mh.submeshCount);
        ReadAll(f, tmp.data(), sizeof(Submesh) * mh.submeshCount);
    }
    std::fclose(f);

    if (mh.vertexStride < 48) return false;

    outVB.resize(mh.vertexCount);
    for (uint32_t i = 0; i < mh.vertexCount; ++i) {
        const float* src = reinterpret_cast<const float*>(
            vbRaw.data() + size_t(i) * mh.vertexStride);

        outVB[i].px = src[0];
        outVB[i].py = src[1];
        outVB[i].pz = src[2];

        outVB[i].nx = src[3];
        outVB[i].ny = src[4];
        outVB[i].nz = src[5];

        outVB[i].cr = 1.0f; outVB[i].cg = 1.0f; outVB[i].cb = 1.0f; outVB[i].ca = 1.0f;

        outVB[i].u = src[10];
        outVB[i].v = src[11];
    }

    outFmt = use32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
    outIndexCount = mh.indexCount;
    return true;
}

// ================== 对外：初始化/结束 ==================
bool ModelStatic_Initialize(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    s_dev = dev; s_ctx = ctx;
    EnsureWhiteTexture();
    s_defWorld = XMMatrixIdentity();
    return (s_dev && s_ctx);
}

void ModelStatic_Finalize()
{
    ModelStatic_UnloadDefault();
    for (auto& m : s_models) {
        if (m.vb) { m.vb->Release(); m.vb = nullptr; }
        if (m.ib) { m.ib->Release(); m.ib = nullptr; }
        m = {};
    }
    s_dev = nullptr; s_ctx = nullptr;
}

// ================== 多模型接口（保留） ==================
bool ModelStatic_Load(const ModelStaticDesc& desc, int* outHandle)
{
    if (!s_dev || !s_ctx || !outHandle) return false;

    int h = -1; for (int i = 0; i < (int)std::size(s_models); ++i) if (!s_models[i].vb) { h = i; break; }
    if (h < 0) return false;

    std::vector<VertexForYourShader> verts;
    std::vector<uint8_t> ib;
    DXGI_FORMAT fmt = DXGI_FORMAT_R16_UINT;
    UINT icount = 0;
    if (!LoadMeshToVBIB(desc.meshPath, verts, ib, fmt, icount)) return false;

    D3D11_BUFFER_DESC vbd{}; vbd.Usage = D3D11_USAGE_DEFAULT; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.ByteWidth = UINT(verts.size() * sizeof(VertexForYourShader));
    D3D11_SUBRESOURCE_DATA vinit{ verts.data(),0,0 };
    if (FAILED(s_dev->CreateBuffer(&vbd, &vinit, &s_models[h].vb))) return false;

    D3D11_BUFFER_DESC ibd{}; ibd.Usage = D3D11_USAGE_DEFAULT; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.ByteWidth = UINT(ib.size());
    D3D11_SUBRESOURCE_DATA iinit{ ib.data(),0,0 };
    if (FAILED(s_dev->CreateBuffer(&ibd, &iinit, &s_models[h].ib))) {
        s_models[h].vb->Release(); s_models[h].vb = nullptr; return false;
    }

    s_models[h].idxFmt = fmt;
    s_models[h].indexCount = icount;

    int tex = -1;
    if (!desc.matPath.empty()) tex = LoadBaseColorFromMat(desc.matPath);
    if (tex < 0) { EnsureWhiteTexture(); tex = s_whiteTexId; }
    s_models[h].texId = tex;

    *outHandle = h;
    return true;
}

void ModelStatic_Unload(int handle)
{
    if (handle < 0 || handle >= (int)std::size(s_models)) return;
    auto& m = s_models[handle];
    if (m.vb) { m.vb->Release(); m.vb = nullptr; }
    if (m.ib) { m.ib->Release(); m.ib = nullptr; }
    m = {};
}

void ModelStatic_Draw(int handle, const XMMATRIX& world)
{
    if (handle < 0 || handle >= (int)std::size(s_models)) return;
    const auto& m = s_models[handle];
    if (!m.vb || !m.ib) return;

    Shader3d_Begin();
    if (m.texId >= 0) Texture_SetTexture(m.texId);

    UINT stride = sizeof(VertexForYourShader), offset = 0;
    s_ctx->IASetVertexBuffers(0, 1, &m.vb, &stride, &offset);
    s_ctx->IASetIndexBuffer(m.ib, m.idxFmt, 0);
    s_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    Shader3d_SetWorldMatrix(world);
    s_ctx->DrawIndexed(m.indexCount, 0, 0);
}

bool ModelStatic_OverrideBaseColorTex(int handle, const wchar_t* texturePath)
{
    if (handle < 0 || handle >= (int)std::size(s_models) || !texturePath || !texturePath[0]) return false;
    int id = Texture_Load(texturePath);
    if (id < 0) return false;
    s_models[handle].texId = id;
    return true;
}

// ================== 默认模型封装 ==================
void ModelStatic_SetDefaultPaths(const wchar_t* meshPath,
    const wchar_t* matPath,
    const wchar_t* overrideDiffuse)
{
    s_defMeshPath = meshPath ? meshPath : L"";
    s_defMatPath = matPath ? matPath : L"";
    s_defOverrideTex = overrideDiffuse ? overrideDiffuse : L"";
}

bool ModelStatic_LoadDefault()
{
    if (s_defMeshPath.empty()) return false;
    ModelStaticDesc d; d.meshPath = s_defMeshPath; d.matPath = s_defMatPath;
    if (!ModelStatic_Load(d, &s_defHandle)) return false;

    if (!s_defOverrideTex.empty())
        ModelStatic_OverrideBaseColorTex(s_defHandle, s_defOverrideTex.c_str());

    return true;
}

void ModelStatic_SetWorld(const XMMATRIX& world)
{
    s_defWorld = world;
}

void ModelStatic_Draw()
{
    if (s_defHandle < 0) return;
    ModelStatic_Draw(s_defHandle, s_defWorld);
}

void ModelStatic_UnloadDefault()
{
    if (s_defHandle >= 0) {
        ModelStatic_Unload(s_defHandle);
        s_defHandle = -1;
    }
}