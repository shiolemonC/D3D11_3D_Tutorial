#include "AnimatorRegistry.h"

// 把你素材的固定路径、默认播放策略都集中在这里注册
void AnimRegister()
{
    AnimatorRegistry_Clear();

    // 例1：Idle（完全用动画，循环）
    {
        AnimClipDesc c{};
        c.name = L"Idle";
        c.meshPath = L"D:/AssetCooker/resources/Cooked/idle_test.mesh";
        c.skelPath = L"D:/AssetCooker/resources/Cooked/idle_test.skel";
        c.animPath = L"D:/AssetCooker/resources/Cooked/idle_test.anim";
        c.matPath = L"D:/AssetCooker/resources/Cooked/idle_test.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"D:/AssetCooker/resources/test/ninja_T.fbm/Ch24_1001_Diffuse.png";

        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseAnimDelta;   // Idle 其实没有位移，但作为例子
        AnimatorRegistry_Register(c);
    }

    // 例2：Run（逻辑速度驱动，不用动画位移；loop）
    {
        AnimClipDesc c{};
        c.name = L"Walk";
        c.meshPath = L"D:/AssetCooker/resources/Cooked/walk_test.mesh";
        c.skelPath = L"D:/AssetCooker/resources/Cooked/walk_test.skel";
        c.animPath = L"D:/AssetCooker/resources/Cooked/walk_test.anim";
        c.matPath = L"D:/AssetCooker/resources/Cooked/walk_test.mat";
        c.baseColorOverride = L"D:/AssetCooker/resources/test/ninja_T.fbm/Ch24_1001_Diffuse.png";
        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::VelocityDriven;
        AnimatorRegistry_Register(c);
    }


    // 注册完可（可选）调用一次加载校验
    AnimatorRegistry_LoadAll();
}