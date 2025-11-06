// player_state.cpp
#include "player_state.h"
#include "player_sm_json.h"
#include "player_sm_condition.h"
#include "direct3d.h"
#include "debug_text.h"
#include <Windows.h>
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
bool PlayerSM_LoadConfigJSON(const wchar_t* jsonPath)
{
    using namespace smjson;
    Value root; std::string err;

    if (!jsonPath || !*jsonPath) {
        OutputDebugStringA("[PlayerSM] JSON path is null/empty\n");
        return false;
    }

    if (!ParseFileUTF8(jsonPath, root, &err)) {
        OutputDebugStringA(("[PlayerSM] JSON parse failed: " + err + "\n").c_str());
        return false;
    }

    SMConfig cfg{};
    g_stateIndex.clear();

    // -------- defaults --------
    if (auto d = root.find("defaults"); d && d->isObject()) {
        if (auto tb = d->find("trigger_buffer"); tb && tb->isNumber())
            cfg.defaultTriggerBuffer = (float)tb->getNumber(0.15);
    }
    // 先同步默认缓冲（稍后也会再次覆盖一次，确保一致）
    Cond_SetTriggerBufferDefault(cfg.defaultTriggerBuffer);

    // -------- states --------
    auto stA = root.find("states");
    if (!stA || !stA->isArray()) {
        OutputDebugStringA("[PlayerSM] missing states[]\n");
        return false;
    }
    for (auto& js : stA->arr) {
        if (!js.isObject()) continue;
        SMState st{};

        auto n = js.find("name");
        if (!n || !n->isString()) {
            OutputDebugStringA("[PlayerSM] state without name\n");
            continue;
        }
        st.name = n->getString();

        if (auto c = js.find("clip"); c && c->isString()) {
            auto s = c->getString();
            st.clip.assign(s.begin(), s.end());   // UTF-8 → wstring（逐字节）
        }

        st.loop = js.find("loop") ? js.find("loop")->getBool(true) : true;

        if (auto rm = js.find("root_motion"); rm && rm->isString())
            st.useRootMotion = _stricmp(rm->getString().c_str(), "use_delta") == 0;

        if (auto ls = js.find("length_sec"); ls && ls->isNumber())
            st.lengthSec = (float)ls->getNumber(0.0);

        st.locomotionAllowed = js.find("locomotion") ? js.find("locomotion")->getBool(false) : false;

        if (auto ui = js.find("uninterruptible"); ui && ui->isArray()) {
            for (auto& seg : ui->arr) {
                if (seg.isArray() && seg.arr.size() == 2) {
                    float a = (float)seg.arr[0].getNumber(0.0);
                    float b = (float)seg.arr[1].getNumber(0.0);
                    st.uninterruptible.emplace_back(a, b);
                }
            }
        }

        g_stateIndex[st.name] = (int)cfg.states.size();
        cfg.states.push_back(std::move(st));
    }

    if (cfg.states.empty()) {
        OutputDebugStringA("[PlayerSM] no states parsed\n");
        return false;
    }

    // -------- transitions --------
    auto trA = root.find("transitions");
    if (!trA || !trA->isArray()) {
        OutputDebugStringA("[PlayerSM] missing transitions[]\n");
        return false;
    }

    int decl = 0;
    for (auto& jt : trA->arr) {
        if (!jt.isObject()) continue;
        SMTransition tr{};

        std::string from = jt.find("from") && jt.find("from")->isString()
            ? jt.find("from")->getString() : std::string("Any");
        std::string to = jt.find("to") && jt.find("to")->isString()
            ? jt.find("to")->getString() : std::string();

        if (to.empty()) {
            OutputDebugStringA("[PlayerSM] transition without 'to'\n");
            continue;
        }

        int fromIdx = idx_or_any(from);
        if (fromIdx == -2) {
            OutputDebugStringA(("[PlayerSM] unknown from: " + from + "\n").c_str());
            continue;
        }
        auto itTo = g_stateIndex.find(to);
        if (itTo == g_stateIndex.end()) {
            OutputDebugStringA(("[PlayerSM] unknown to: " + to + "\n").c_str());
            continue;
        }
        tr.from = fromIdx;
        tr.to = itTo->second;

        // conditions[]
        if (auto cs = jt.find("conditions"); cs && cs->isArray()) {
            for (auto& ce : cs->arr) {
                if (!ce.isString()) continue;
                auto s = ce.getString();
                CondExpr h{};
                if (Cond_CompileBool(s.c_str(), &h)) {
                    tr.conds.push_back(h);
                    tr.condStrs.push_back(s);   // 仅用于 Debug HUD；如无此字段可删掉本行
                }
                else {
                    OutputDebugStringA(("[PlayerSM] cond compile failed: " + s + "\n").c_str());
                }
            }
        }

        // trigger + buffer
        if (auto tg = jt.find("trigger"); tg && tg->isString())
            tr.trigger = tg->getString();
        if (auto bf = jt.find("buffer");  bf && bf->isNumber())
            tr.bufferSec = (float)bf->getNumber(-1.0);

        // window（归一化多段）
        if (auto win = jt.find("window"); win && win->isArray()) {
            for (auto& seg : win->arr) {
                if (seg.isArray() && seg.arr.size() == 2) {
                    tr.window.emplace_back(
                        (float)seg.arr[0].getNumber(0.0),
                        (float)seg.arr[1].getNumber(1.0)
                    );
                }
            }
        }

        // 其他
        if (auto du = jt.find("duration"); du && du->isNumber())
            tr.duration = (float)du->getNumber(0.0);

        if (auto cv = jt.find("curve"); cv && cv->isString()) {
            std::string s = cv->getString();
            if (_stricmp(s.c_str(), "ease_in") == 0) tr.curve = "ease_in";
            else if (_stricmp(s.c_str(), "ease_out") == 0) tr.curve = "ease_out";
            else if (_stricmp(s.c_str(), "ease_in_out") == 0) tr.curve = "ease_in_out";
            else tr.curve = "linear";
        }

        tr.canInterrupt = jt.find("can_interrupt") ? jt.find("can_interrupt")->getBool(true) : true;
        tr.force = jt.find("force") ? jt.find("force")->getBool(false) : false;
        tr.priority = jt.find("priority") && jt.find("priority")->isNumber()
            ? (int)jt.find("priority")->getNumber(0) : 0;
        tr.declOrder = decl++;

        cfg.transitions.push_back(std::move(tr));
    }

    // -------- initial_state --------
    std::string initName = "Idle";
    if (auto d = root.find("defaults"); d && d->isObject()) {
        if (auto ini = d->find("initial_state"); ini && ini->isString())
            initName = ini->getString();
    }
    auto itInit = g_stateIndex.find(initName);
    cfg.initial = (itInit == g_stateIndex.end()) ? 0 : itInit->second;

    // -------- 应用配置 --------
    g_cfg = std::move(cfg);
    g_cur = g_cfg.initial;
    g_timeInState = 0.0;
    g_moveMag = 0.0f;

    // 再次同步触发器缓冲默认值（双保险）
    Cond_SetTriggerBufferDefault(g_cfg.defaultTriggerBuffer);

    char buf[256];
    sprintf_s(buf, "[PlayerSM] JSON loaded: %zu states, %zu transitions, init=%s\n",
        g_cfg.states.size(), g_cfg.transitions.size(),
        g_cfg.states[g_cfg.initial].name.c_str());
    OutputDebugStringA(buf);
    return true;
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