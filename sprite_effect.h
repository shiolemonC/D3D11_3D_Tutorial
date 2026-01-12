#pragma once
#include <DirectXMath.h>

// 两种特效（你也可以后面扩展更多）
enum class SpriteEffectKind : int
{
    Hit = 0,
    Parry = 1,
};

// sprite sheet 的配置
struct SpriteSheetDesc
{
    const wchar_t* path = nullptr;

    int cols = 1;             // 每行多少帧
    int rows = 1;             // 总共有多少行
    int frameCount = 0;       // 0 表示 cols*rows；否则用你指定的帧数（常见最后一行没填满）

    double secondsPerFrame = 0.05;
    bool looped = false;

    DirectX::XMUINT2 startPx = { 0, 0 };        // 起始像素（有些图集不是从(0,0)开始）
    DirectX::XMUINT2 frameSizePx = { 0, 0 };    // (0,0) 表示自动算：texW/cols, texH/rows
};

// 初始化/结束（初始化时会读取两张贴图并注册 Pattern）
bool SpriteEffect_Initialize(const SpriteSheetDesc& hit, const SpriteSheetDesc& parry);
void SpriteEffect_Finalize();

// 每帧更新：只负责“播完就销毁实例”
void SpriteEffect_Update(double dt);

// 绘制所有实例（建议在不透明物体之后、HUD之前调用）
void SpriteEffect_Draw();

// 触发：生成一个实例（会占用一个 SpriteAnim Player）
void SpriteEffect_SpawnHit(const DirectX::XMFLOAT3& pos,
    const DirectX::XMFLOAT2& scale = { 1.0f, 1.0f },
    const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f });

void SpriteEffect_SpawnParry(const DirectX::XMFLOAT3& pos,
    const DirectX::XMFLOAT2& scale = { 1.0f, 1.0f },
    const DirectX::XMFLOAT2& pivot = { 0.0f, 0.0f });

// 可选：清空所有实例
void SpriteEffect_ClearAll();
