#include "AnimatorRegistry.h"
#include "BossAnimatorRegistry.h"

// cd /d D:\AssetCooker\AssetCooker\build\msvc-x64-release\Debug
//  AssetCooker.exe "D:\GP11Project\AnimSourceFile\sword_roll.fbx" "D:\GP11Project\GameSample05_3D\resources\player_anim\cooked"
//  AssetCooker.exe "D:\GP11Project\AnimSourceFile\mutant_idle.fbx" "D:\GP11Project\GameSample05_3D\resources\player_anim\cooked"
// 把你素材的固定路径、默认播放策略都集中在这里注册
void AnimRegister()
{
    AnimatorRegistry_Clear();

    // 例1：Idle（完全用动画，循环）
    {
        AnimClipDesc c{};
        c.name = L"Idle";
        //c.meshPath = L"D:/AssetCooker/resources/Cooked/idle_test.mesh";
        //c.skelPath = L"D:/AssetCooker/resources/Cooked/idle_test.skel";
        //c.animPath = L"D:/AssetCooker/resources/Cooked/idle_test.anim";
        //c.matPath = L"D:/AssetCooker/resources/Cooked/idle_test.mat";
        //// 如需强制贴图（覆盖 .mat）：
        //c.baseColorOverride = L"D:/AssetCooker/resources/test/ninja_T.fbm/Ch24_1001_Diffuse.png";

        c.meshPath = L"resources/player_anim/cooked/sword_idle.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_idle.skel";
        c.animPath = L"resources/player_anim/cooked/sword_idle.anim";
        c.matPath  = L"resources/player_anim/cooked/sword_idle.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::None;   // Idle 其实没有位移，但作为例子
        // ★ 指定此动画用 Hips 作为 motion-root（常见于 Mixamo）
        c.motionRootNameUTF8 = "root";
        AnimatorRegistry_Register(c);
    }

    // 例2：Run（逻辑速度驱动，不用动画位移；loop）
    {
        AnimClipDesc c{};
        c.name = L"Walk";
        c.meshPath = L"resources/player_anim/cooked/sword_run_60.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_run_60.skel";
        c.animPath = L"resources/player_anim/cooked/sword_run_60.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_run_60.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::None;
        //c.velocity = 2.0f;
        // ★ 行走通常也用 Hips
        c.motionRootNameUTF8 = "root";
        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Attack";
        c.meshPath = L"resources/player_anim/cooked/sword_attack.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_attack.skel";
        c.animPath = L"resources/player_anim/cooked/sword_attack.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_attack.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Attack_2";
        c.meshPath = L"resources/player_anim/cooked/sword_attack_2.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_attack_2.skel";
        c.animPath = L"resources/player_anim/cooked/sword_attack_2.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_attack_2.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Attack_3";
        c.meshPath = L"resources/player_anim/cooked/sword_block_attack.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_block_attack.skel";
        c.animPath = L"resources/player_anim/cooked/sword_block_attack.anim";
        c.matPath = L"resources/player_anim/cooked/sword_block_attack.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Parry";
        c.meshPath = L"resources/player_anim/cooked/sword_block.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_block.skel";
        c.animPath = L"resources/player_anim/cooked/sword_block.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_block.mat";
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Block";
        c.meshPath = L"resources/player_anim/cooked/sword_parry.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_parry.skel";
        c.animPath = L"resources/player_anim/cooked/sword_parry.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_parry.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    // GuardHold reuses the successful block animation with a slower playback rate.
    {
        AnimClipDesc c{};
        c.name = L"GuardHold";
        c.meshPath = L"resources/player_anim/cooked/sword_parry.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_parry.skel";
        c.animPath = L"resources/player_anim/cooked/sword_parry.anim";
        c.matPath = L"resources/player_anim/cooked/sword_parry.mat";
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";
        c.loop = false;
        c.playbackRate = 0.65f;
        c.rmType = RootMotionType::None;
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Block_Attack";
        c.meshPath = L"resources/player_anim/cooked/sword_block_attack.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_block_attack.skel";
        c.animPath = L"resources/player_anim/cooked/sword_block_attack.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_block_attack.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Finish_Attack";
        c.meshPath = L"resources/player_anim/cooked/sword_finish.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_finish.skel";
        c.animPath = L"resources/player_anim/cooked/sword_finish.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_finish.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 2.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Roll";
        c.meshPath = L"resources/player_anim/cooked/sword_roll.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_roll.skel";
        c.animPath = L"resources/player_anim/cooked/sword_roll.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_roll.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        //c.velocity = 5.0f;
           // ★ 若希望攻击不受 Hips 摇摆影响，改用 "Root"（按你的骨骼名来）
    //   也可以继续用 "mixamorig:Hips"，视资源而定。
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Hit_Light";
        c.meshPath = L"resources/player_anim/cooked/sword_hit_light.mesh";
        c.skelPath = L"resources/player_anim/cooked/sword_hit_light.skel";
        c.animPath = L"resources/player_anim/cooked/sword_hit_light.anim";
        c.matPath =  L"resources/player_anim/cooked/sword_hit_light.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Paladin_diffuse.png";
        c.normalOverride = L"resources/player_anim/textures/Paladin_normal.png";

        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        c.motionRootNameUTF8 = "root";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Hit_Medium";
        c.meshPath = L"resources/player_anim/cooked/player_attack.mesh";
        c.skelPath = L"resources/player_anim/cooked/player_attack.skel";
        c.animPath = L"resources/player_anim/cooked/player_attack.anim";
        c.matPath = L"resources/player_anim/cooked/player_attack.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        c.motionRootNameUTF8 = "Armature";

        AnimatorRegistry_Register(c);
    }

    {
        AnimClipDesc c{};
        c.name = L"Hit_Heavy";
        c.meshPath = L"resources/player_anim/cooked/player_attack.mesh";
        c.skelPath = L"resources/player_anim/cooked/player_attack.skel";
        c.animPath = L"resources/player_anim/cooked/player_attack.anim";
        c.matPath = L"resources/player_anim/cooked/player_attack.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta;
        c.motionRootNameUTF8 = "Armature";

        AnimatorRegistry_Register(c);
    }




    // 注册完可（可选）调用一次加载校验
    AnimatorRegistry_LoadAll();
}

void BossAnimRegister()
{
    BossAnimatorRegistry_Clear();

    // 占位 Idle
    {
        AnimClipDesc c{};
        c.name = L"Boss_Idle";
        //c.meshPath = L"resources/player_anim/cooked/player_idle_test.mesh";
        //c.skelPath = L"resources/player_anim/cooked/player_idle_test.skel";
        //c.animPath = L"resources/player_anim/cooked/player_idle_test.anim";
        //c.matPath =  L"resources/player_anim/cooked/player_idle_test.mat";
        c.meshPath = L"resources/player_anim/cooked/mutant_idle.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_idle.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_idle.anim";
        c.matPath =  L"resources/player_anim/cooked/mutant_idle.mat";
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::None;   // Idle 不需要 RootMotion
        c.motionRootNameUTF8 = "root";        // 或 "mixamorig:Hips"，看你 Boss 的骨骼名

        BossAnimatorRegistry_Register(c);
    }

    // 占位 Chase（行走/跑步）
    {
        AnimClipDesc c{};
        c.name = L"Boss_Chase";
        c.meshPath = L"resources/player_anim/cooked/mutant_run.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_run.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_run.anim";
        c.matPath =  L"resources/player_anim/cooked/mutant_run.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = true;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::None;   // 先用逻辑位移追踪
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // 占位 Attack（单次攻击）
    {
        AnimClipDesc c{};
        c.name = L"Boss_Attack";
        c.meshPath = L"resources/player_anim/cooked/mutant_swipe.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_swipe.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_swipe.anim";
        c.matPath =  L"resources/player_anim/cooked/mutant_swipe.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;  // 攻击不循环
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta; // 第一版不做冲刺 RootMotion
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // Spell cast: reuse single attack animation asset with a separate event track.
    {
        AnimClipDesc c{};
        c.name = L"Boss_CastSpell";
        c.meshPath = L"resources/player_anim/cooked/mutant_swipe.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_swipe.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_swipe.anim";
        c.matPath = L"resources/player_anim/cooked/mutant_swipe.mat";
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::None;
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // 强力Attack
    {
        AnimClipDesc c{};
        c.name = L"Boss_Combo";
        c.meshPath = L"resources/player_anim/cooked/mutant_combo.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_combo.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_combo.anim";
        c.matPath  = L"resources/player_anim/cooked/mutant_combo.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;  // 攻击不循环
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta; // 第一版不做冲刺 RootMotion
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // 开场吼叫
    {
        AnimClipDesc c{};
        c.name = L"Boss_Roar";
        c.meshPath = L"resources/player_anim/cooked/mutant_roar.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_roar.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_roar.anim";
        c.matPath =  L"resources/player_anim/cooked/mutant_roar.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;  // 攻击不循环
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta; // 第一版不做冲刺 RootMotion
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // 受击动作
    {
        AnimClipDesc c{};
        c.name = L"Boss_Hit";
        c.meshPath = L"resources/player_anim/cooked/mutant_hit.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_hit.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_hit.anim";
        c.matPath =  L"resources/player_anim/cooked/mutant_hit.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;  // 攻击不循环
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta; // 第一版不做冲刺 RootMotion
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    // 死亡动作
    {
        AnimClipDesc c{};
        c.name = L"Boss_Die";
        c.meshPath = L"resources/player_anim/cooked/mutant_die.mesh";
        c.skelPath = L"resources/player_anim/cooked/mutant_die.skel";
        c.animPath = L"resources/player_anim/cooked/mutant_die.anim";
        c.matPath  = L"resources/player_anim/cooked/mutant_die.mat";
        // 如需强制贴图（覆盖 .mat）：
        c.baseColorOverride = L"resources/player_anim/cooked/Textures/Mutant_diffuse.png";
        c.normalOverride = L"resources/player_anim/cooked/Textures/Mutant_normal.png";
        c.loop = false;  // 攻击不循环
        c.playbackRate = 1.0f;
        c.rmType = RootMotionType::UseZDelta; // 第一版不做冲刺 RootMotion
        c.motionRootNameUTF8 = "root";

        BossAnimatorRegistry_Register(c);
    }

    BossAnimatorRegistry_LoadAll();
}
