// player_state.cpp
#include "player_state.h"
#include "player_sm_condition.h"
#include "direct3d.h"
#include "debug_text.h"
#include <sstream>
#include <unordered_map>
#include <string>
#include <cstring>
#include <cassert>
#include <cmath>
#include <cwchar>

// ---------- 内部数据结构 ----------
struct SMState {
    std::string  name;            // UTF-8
    std::wstring clip;            // 绑定的动画名
    bool   loop = true;
    bool   useRootMotion = false; // none/use_delta
    float  lengthSec = 0.0f;      // 可选：clip 长度（秒），window 计算用
    // 不可打断窗口（归一化时间、多段）
    std::vector<std::pair<float, float>> uninterruptible;
    bool   locomotionAllowed = false;
};

struct SMTransition {
    int    from = -1;             // -1 表示 Any
    int    to = -1;
    // 条件：编译后的 Cond 句柄（全部需满足）
    std::vector<CondExpr> conds;
    std::vector<std::string> condStrs; //for debug
    // 触发器
    std::string trigger;          // 空串表示无触发器
    float bufferSec = -1.0f;      // <0 使用全局默认
    // 时间窗口（归一化，多段）
    std::vector<std::pair<float, float>> window;
    float duration = 0.0f;        // 融合时间（秒，=0 硬切）
    const char* curve = "linear";
    bool  canInterrupt = true;
    bool  force = false;
    int   priority = 0;
    // 保留声明次序用于 tie-break
    int   declOrder = 0;
};

struct SMConfig {
    std::vector<SMState>      states;
    std::vector<SMTransition> transitions;
    int   initial = 0;            // 初始状态索引
    float defaultTriggerBuffer = 0.15f;
};

static SMConfig g_cfg;
static std::unordered_map<std::string, int> g_stateIndex; // name->idx
static int    g_cur = -1;
static double g_timeInState = 0.0;
static float  g_moveMag = 0.0f;


#if defined(DEBUG) || defined(_DEBUG)
static hal::DebugText * g_pDT_SM = nullptr;
static void EnsureDT_SM() {
   if (!g_pDT_SM) {
           // 起始绘制位置放低一点，避免与 Camera_DebugDraw 重叠
           g_pDT_SM = new hal::DebugText(
               Direct3D_GetDevice(), Direct3D_GetContext(),
               L"consolab_ascii_512.png",
               Direct3D_GetBackBufferWidth(), Direct3D_GetBackBufferHeight(),
                     /*originX*/ 0.0f, /*originY*/ 160.0f,
                     /*gridX*/ 0, /*gridY*/ 0,
                     /*cellOffsetX*/ 0.0f, /*cellH*/ 16.0f);
       
   }
   
}
#endif

// ---------- 工具 ----------
static inline float clamp01(float x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }
static inline bool in_windows(float t01, const std::vector<std::pair<float, float>>& ws) {
    if (ws.empty()) return true;
    for (auto& w : ws) if (t01 >= w.first && t01 <= w.second) return true;
    return false;
}
static float current_time_norm() {
    if (g_cur < 0) return 0.0f;
    const auto& st = g_cfg.states[g_cur];
    if (st.lengthSec > 0.0f) {
        float t = float(g_timeInState / st.lengthSec);
        return st.loop ? (t - std::floor(t)) : clamp01(t);
    }
    // 未知长度：默认放行 window
    return 0.0f;
}
static bool in_uninterruptible(double tState) {
    const auto& st = g_cfg.states[g_cur];
    if (st.uninterruptible.empty() || st.lengthSec <= 0.0f) return false;
    float t01 = clamp01(float(tState / st.lengthSec));
    for (auto& w : st.uninterruptible) if (t01 >= w.first && t01 <= w.second) return true;
    return false;
}
static int idx_or_any(const std::string& name) {
    if (name == "Any" || name == "any" || name == "*") return -1;
    auto it = g_stateIndex.find(name);
    return (it == g_stateIndex.end()) ? -2 : it->second; // -2: 未知
}

// ---------- 对外：加载配置 ----------
bool PlayerSM_LoadConfigJSON(const wchar_t* /*jsonPath*/)
{
    // 为避免此阶段引入 JSON 依赖，这里先返回 false。
    // 你可以先调用 PlayerSM_LoadConfigDefaults() 立即使用；
    // 等我们实现条件子模块时一并加上 JSON 解析（按你定的 Schema）。
    return false;
}

void PlayerSM_LoadConfigDefaults()
{
    g_cfg = {};
    g_stateIndex.clear();

    // States
    SMState S_idle;
    S_idle.name = "Idle";
    S_idle.clip = L"Idle";
    S_idle.loop = true;
    S_idle.useRootMotion = false;
    S_idle.locomotionAllowed = false;
    S_idle.lengthSec = 1.0f; // 先给 1s；真实长度等接入后更新
    g_stateIndex[S_idle.name] = int(g_cfg.states.size());
    g_cfg.states.push_back(S_idle);

    SMState S_move;
    S_move.name = "Move";
    S_move.clip = L"Walk";
    S_move.loop = true;
    S_move.useRootMotion = false;
    S_move.lengthSec = 1.0f;
    S_move.locomotionAllowed = true;
    g_stateIndex[S_move.name] = int(g_cfg.states.size());
    g_cfg.states.push_back(S_move);

    g_cfg.initial = 0;
    g_cfg.defaultTriggerBuffer = 0.15f;

    // 通知子模块默认缓冲
    Cond_Init(g_cfg.defaultTriggerBuffer);

    // Transitions
    int decl = 0;

    // Idle -> Move （move.mag > 0.1）
    SMTransition T_im;
    T_im.from = g_stateIndex["Idle"];
    T_im.to = g_stateIndex["Move"];
    {
        CondExpr h{};
        Cond_CompileBool("move.mag > 0.1", &h);   // ★ 子模块就绪后由其解析
        T_im.conds.push_back(h);
        T_im.condStrs.emplace_back("move.mag > 0.1");
    }
    T_im.window = { {0.00f,1.00f} };
    T_im.duration = 0.18f; T_im.curve = "ease_in_out";
    T_im.canInterrupt = true; T_im.force = false;
    T_im.priority = 10; T_im.declOrder = decl++;
    g_cfg.transitions.push_back(T_im);

    // Move -> Idle （move.mag <= 0.1）
    SMTransition T_mi;
    T_mi.from = g_stateIndex["Move"];
    T_mi.to = g_stateIndex["Idle"];
    {
        CondExpr h{};
        Cond_CompileBool("move.mag <= 0.1", &h);
        T_mi.conds.push_back(h);
        T_mi.condStrs.emplace_back("move.mag <= 0.1");
    }
    T_mi.window = { {0.00f,1.00f} };
    T_mi.duration = 0.12f; T_mi.curve = "ease_out";
    T_mi.canInterrupt = true; T_mi.force = false;
    T_mi.priority = 10; T_mi.declOrder = decl++;
    g_cfg.transitions.push_back(T_mi);

    PlayerSM_Reset();
}

void PlayerSM_Reset()
{
    g_cur = g_cfg.initial;
    g_timeInState = 0.0;
    g_moveMag = 0.0f;
}

// ---------- 输入同步 ----------
void PlayerSM_SetMoveInput(float x, float z)
{
    float m = std::sqrt(x * x + z * z);
    if (m > 1e-6f) {
        if (m > 1.0f) m = 1.0f; // 归一化至 0..1
    }
    else m = 0.0f;
    g_moveMag = m;

    // 同步给子模块变量
    Cond_SetFloat("move.mag", g_moveMag);
}
void PlayerSM_SetBool(const char* name, bool v) { Cond_SetBool(name, v); }
void PlayerSM_SetFloat(const char* name, float v) { Cond_SetFloat(name, v); }
void PlayerSM_FireTrigger(const char* name) { Cond_FireTrigger(name); }

// ---------- 主更新 ----------
PlayerSMOutput PlayerSM_Update(double dt)
{
    PlayerSMOutput out{};
    if (g_cur < 0 || g_cur >= (int)g_cfg.states.size()) {
        // 配置未载入：回退 Idle
        PlayerSM_LoadConfigDefaults();
    }

    // 推进时间 & 写入归一化时间
    g_timeInState += dt;
    Cond_SetTimeNorm(current_time_norm());

    const auto& curSt = g_cfg.states[g_cur];

    // 收集候选（当前 + Any）
    std::vector<int> cand;
    for (int i = 0; i < (int)g_cfg.transitions.size(); ++i) {
        const auto& tr = g_cfg.transitions[i];
        if (tr.from == g_cur || tr.from == -1) cand.push_back(i);
    }

    // 过滤与择优
    int best = -1;
    int bestPri = -999999;
    bool curLocked = in_uninterruptible(g_timeInState);

    float t01 = current_time_norm();
    for (int idx : cand) {
        const auto& tr = g_cfg.transitions[idx];
        if (tr.to < 0 || tr.to >= (int)g_cfg.states.size()) continue;

        // 不可打断窗口
        if (curLocked && !tr.force && !tr.canInterrupt) continue;

        // 时间窗口
        if (!in_windows(t01, tr.window)) continue;

        // 触发器（如声明）
        if (!tr.trigger.empty()) {
            if (!Cond_TestTrigger(tr.trigger.c_str(), tr.bufferSec, /*consume*/true))
                continue;
        }

        // 条件（全部满足）
        bool ok = true;
        for (auto h : tr.conds) {
            if (!Cond_EvalBool(h)) { ok = false; break; }
        }
        if (!ok) continue;

        // 选优：priority > force > can_interrupt > 声明顺序
        int score = tr.priority * 1000
            + (tr.force ? 100 : 0)
            + (tr.canInterrupt ? 10 : 0)
            - tr.declOrder; // 声明越早，值越大（这里取负使得越早越优先）

        if (score > bestPri) {
            bestPri = score; best = idx;
        }
    }

    bool changed = false;
    float blendSec = 0.0f;
    const char* blendCurve = "linear";
    int next = g_cur;

    if (best >= 0) {
        const auto& tr = g_cfg.transitions[best];
        next = tr.to;
        changed = (next != g_cur);
        blendSec = tr.duration;
        blendCurve = tr.curve;
    }

    if (changed) {
        g_cur = next;
        g_timeInState = 0.0;
    }

    const auto& st = g_cfg.states[g_cur];
    out.state = st.name.c_str();
    out.clip = st.clip.c_str();
    out.changed = changed;
    out.blendSeconds = blendSec;
    out.blendCurve = blendCurve;
    out.useRootMotion = st.useRootMotion;
    out.locomotionActive = st.locomotionAllowed;

    return out;
}

const char* PlayerSM_GetCurrentStateName() {
    if (g_cur < 0 || g_cur >= (int)g_cfg.states.size()) return "Unknown";
    return g_cfg.states[g_cur].name.c_str();
}

void PlayerSM_DebugDraw()
{
#if defined(DEBUG) || defined(_DEBUG)
    EnsureDT_SM();
    if (!g_pDT_SM) return;

    // 汇总当前状态信息
    std::stringstream ss;
    ss.setf(std::ios::fixed); ss.precision(3);

    const char* stateName = (g_cur >= 0 && g_cur < (int)g_cfg.states.size())
        ? g_cfg.states[g_cur].name.c_str() : "Unknown";
    const auto& st = (g_cur >= 0 && g_cur < (int)g_cfg.states.size())
        ? g_cfg.states[g_cur] : SMState{};
    float t01 = current_time_norm();

    ss << "[PlayerSM]\n";
    ss << " State      : " << stateName << "\n";
    ss << " Clip       : " << (st.clip.empty() ? "<none>" : std::string(st.clip.begin(), st.clip.end())) << "\n";
    ss << " Time(sec)  : " << g_timeInState << "  (norm=" << t01 << ")\n";
    ss << " Loop       : " << (st.loop ? "true" : "false") << "\n";
    ss << " RootMotion : " << (st.useRootMotion ? "true" : "false") << "\n";
    ss << " Locomotion : " << (st.locomotionAllowed ? "true" : "false") << "\n";
    ss << " move.mag   : " << g_moveMag << "\n";

    // 列出候选转移（当前+Any），标注窗口与条件
    ss << " Candidates :\n";
    for (int i = 0; i < (int)g_cfg.transitions.size(); ++i) {
        const auto& tr = g_cfg.transitions[i];
        if (!(tr.from == g_cur || tr.from == -1)) continue;
        if (tr.to < 0 || tr.to >= (int)g_cfg.states.size()) continue;

        const char* toName = g_cfg.states[tr.to].name.c_str();
        ss << "  -> " << toName
            << "  prio=" << tr.priority
            << "  dur=" << tr.duration
            << (tr.force ? "  [force]" : "")
            << (tr.canInterrupt ? "" : "  [no-int]")
            << "\n";

        // window
        ss << "     window : ";
        if (tr.window.empty()) ss << "[0,1]";
        else {
            for (size_t k = 0; k < tr.window.size(); ++k) {
                ss << "[" << tr.window[k].first << "," << tr.window[k].second << "]";
                if (k + 1 < tr.window.size()) ss << " U ";
            }
        }
        ss << "\n";

        // 条件字符串
        if (!tr.condStrs.empty()) {
            ss << "     cond   : ";
            for (size_t k = 0; k < tr.condStrs.size(); ++k) {
                ss << tr.condStrs[k];
                if (k + 1 < tr.condStrs.size()) ss << " && ";
            }
            ss << "\n";
        }

        // 触发器
        if (!tr.trigger.empty()) {
            ss << "     trigger: " << tr.trigger;
            if (tr.bufferSec >= 0.0f) ss << "  (buf=" << tr.bufferSec << "s)";
            ss << "\n";
        }
    }

    g_pDT_SM->SetText(ss.str().c_str(), { 0.2f, 1.0f, 0.2f, 1.0f });
    g_pDT_SM->Draw();
    g_pDT_SM->Clear();
#endif
}