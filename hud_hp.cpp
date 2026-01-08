#include "hud_hp.h"

#include <algorithm>
#include <d3d11.h>
#include <DirectXMath.h>

#include "direct3d.h"
#include "sprite.h"
#include "player.h"
#include "boss.h"

using namespace DirectX;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) do { if (x) { (x)->Release(); (x) = nullptr; } } while(0)
#endif

namespace
{
    constexpr float kChipHoldSec = 0.25f;
    constexpr float kChipSpeedPerSec = 1.15f;

    // Player HUD (top-left)
    constexpr float kPlayerX = 20.0f;
    constexpr float kPlayerY = 20.0f;
    constexpr float kPlayerW = 220.0f;
    constexpr float kPlayerH = 18.0f;

    // Boss HUD (center-bottom)
    constexpr float kBossW = 520.0f;
    constexpr float kBossH = 20.0f;
    constexpr float kBossBottomMargin = 80.0f;

    constexpr float kBorder = 2.0f;

    ID3D11DepthStencilState* g_dsNoDepth = nullptr;

    float Clamp01(float v) { return std::max(0.0f, std::min(1.0f, v)); }

    float RatioFromHP(int hp, int maxHp)
    {
        if (maxHp <= 0) return 0.0f;
        return Clamp01(static_cast<float>(hp) / static_cast<float>(maxHp));
    }

    struct ChipBar
    {
        float front = 1.0f;
        float chip = 1.0f;
        float hold = 0.0f;

        void Reset(float r)
        {
            r = Clamp01(r);
            front = r;
            chip = r;
            hold = 0.0f;
        }

        void UpdateTo(float target, double dt)
        {
            target = Clamp01(target);
            float fdt = static_cast<float>(dt);

            // Heal: snap
            if (target >= front) {
                front = target;
                chip = target;
                hold = 0.0f;
                return;
            }

            // Damage: front snaps, chip holds then shrinks
            if (target < front) {
                front = target;
                hold = kChipHoldSec;
            }

            if (hold > 0.0f) {
                hold -= fdt;
                return;
            }

            if (chip > front) {
                chip -= kChipSpeedPerSec * fdt;
                if (chip < front) chip = front;
            }
        }
    };

    ChipBar g_player;
    ChipBar g_boss;
    bool g_inited = false;

    void EnsureNoDepthState()
    {
        if (g_dsNoDepth) return;

        ID3D11Device* dev = Direct3D_GetDevice();
        if (!dev) return;

        D3D11_DEPTH_STENCIL_DESC ds{};
        ds.DepthEnable = FALSE;
        ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        ds.DepthFunc = D3D11_COMPARISON_ALWAYS;
        ds.StencilEnable = FALSE;

        dev->CreateDepthStencilState(&ds, &g_dsNoDepth);
    }

    void DrawBar(float x, float y, float w, float h, const ChipBar& bar)
    {
        const XMFLOAT4 borderColor{ 0,0,0,1 };
        const XMFLOAT4 bgColor{ 0.35f, 0.05f, 0.05f, 1.0f };
        const XMFLOAT4 chipColor{ 1.00f, 0.55f, 0.55f, 1.0f };
        const XMFLOAT4 frontColor{ 1.00f, 0.00f, 0.00f, 1.0f }; // 大红色

        Sprite_DrawRect(x - kBorder, y - kBorder, w + kBorder * 2, h + kBorder * 2, borderColor);
        Sprite_DrawRect(x, y, w, h, bgColor);

        float chipW = w * Clamp01(bar.chip);
        float frontW = w * Clamp01(bar.front);

        if (chipW > frontW + 0.5f) Sprite_DrawRect(x, y, chipW, h, chipColor);
        if (frontW > 0.0f)         Sprite_DrawRect(x, y, frontW, h, frontColor);
    }
}

void HudHP_Initialize()
{
    EnsureNoDepthState();
    g_player.Reset(RatioFromHP(Player_GetHP(), Player_GetMaxHP()));
    g_boss.Reset(RatioFromHP(Boss_GetHP(), Boss_GetMaxHP()));
    g_inited = true;
}

void HudHP_Update(double dt)
{
    if (!g_inited) HudHP_Initialize();
    g_player.UpdateTo(RatioFromHP(Player_GetHP(), Player_GetMaxHP()), dt);
    g_boss.UpdateTo(RatioFromHP(Boss_GetHP(), Boss_GetMaxHP()), dt);
}

void HudHP_Draw()
{
    if (!g_inited) HudHP_Initialize();

    EnsureNoDepthState();

    ID3D11DeviceContext* ctx = Direct3D_GetContext();
    if (ctx && g_dsNoDepth)
    {
        ID3D11DepthStencilState* prev = nullptr;
        UINT prevRef = 0;
        ctx->OMGetDepthStencilState(&prev, &prevRef);
        ctx->OMSetDepthStencilState(g_dsNoDepth, 0);

        // 关键：HUD 自己调用 Sprite_Begin，完全归 game 管，不依赖 main 的 Sprite_Begin
        Sprite_Begin();

        DrawBar(kPlayerX, kPlayerY, kPlayerW, kPlayerH, g_player);

        float screenW = (float)Direct3D_GetBackBufferWidth();
        float screenH = (float)Direct3D_GetBackBufferHeight();
        float bx = (screenW - kBossW) * 0.5f;
        float by = (screenH - kBossBottomMargin);
        DrawBar(bx, by, kBossW, kBossH, g_boss);

        ctx->OMSetDepthStencilState(prev, prevRef);
        SAFE_RELEASE(prev);
    }
    else
    {
        // fallback（万一 depth state 创建失败也能画）
        Sprite_Begin();

        DrawBar(kPlayerX, kPlayerY, kPlayerW, kPlayerH, g_player);

        float screenW = (float)Direct3D_GetBackBufferWidth();
        float screenH = (float)Direct3D_GetBackBufferHeight();
        float bx = (screenW - kBossW) * 0.5f;
        float by = (screenH - kBossBottomMargin);
        DrawBar(bx, by, kBossW, kBossH, g_boss);
    }
}
