// player_sm_condition.cpp — 临时占位实现（简化版）
// 目的：消除链接错误 + 支持最常用的简单条件（如 "move.mag > 0.1"）
// 之后会换成完整的 DSL/回调模块。

#include "player_sm_condition.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdlib>
#include <cctype>
#include <algorithm>

// ----------------- 简易存储 -----------------
static float g_defaultTriggerBuffer = 0.15f;

static std::unordered_map<std::string, float> sFloats;     // 变量：float
static std::unordered_map<std::string, bool>  sBools;      // 变量：bool
static std::unordered_map<std::string, double> sTriggers;  // 触发器：最近一次置位的“标记”，这里不实现时间，只做“一次性消费”

static std::unordered_map<std::string, CondBoolFn>  sBoolFns;   // 回调（未真正用到）
static std::unordered_map<std::string, CondFloatFn> sFloatFns;

static std::vector<std::string> sBoolExprs;   // 编译后的“句柄”就是索引
static std::vector<std::string> sFloatExprs;

// ----------------- 工具函数 -----------------
static inline std::string trim(std::string s) {
    auto issp = [](int ch) { return std::isspace(ch) != 0; };
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [&](int c) { return !issp(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [&](int c) { return !issp(c); }).base(), s.end());
    return s;
}

static inline bool ieq(const std::string& a, const char* b) {
    if (a.size() != std::strlen(b)) return false;
    for (size_t i = 0; i < a.size(); ++i) if (std::tolower(a[i]) != std::tolower(b[i])) return false;
    return true;
}

static double getFloatVar(const std::string& name, bool* ok = nullptr) {
    auto it = sFloats.find(name);
    if (it != sFloats.end()) { if (ok) *ok = true; return it->second; }
    // 支持回调 float()
    if (!name.empty() && name.back() == ')') {
        auto lp = name.find('(');
        if (lp != std::string::npos) {
            std::string fn = name.substr(0, lp);
            auto itf = sFloatFns.find(fn);
            if (itf != sFloatFns.end()) { if (ok) *ok = true; return itf->second(); }
        }
    }
    if (ok) *ok = false;
    return 0.0;
}

static bool getBoolVar(const std::string& name, bool* ok = nullptr) {
    auto it = sBools.find(name);
    if (it != sBools.end()) { if (ok) *ok = true; return it->second; }
    if (ieq(name, "true")) { if (ok) *ok = true; return true; }
    if (ieq(name, "false")) { if (ok) *ok = true; return false; }
    // 支持回调 Bool()
    if (!name.empty() && name.back() == ')') {
        auto lp = name.find('(');
        if (lp != std::string::npos) {
            std::string fn = name.substr(0, lp);
            auto itb = sBoolFns.find(fn);
            if (itb != sBoolFns.end()) { if (ok) *ok = true; return itb->second(); }
        }
    }
    if (ok) *ok = false;
    return false;
}

// 非完整解析器：仅支持 这些模式：
//  1) <var> <op> <number>  （op: >, >=, <, <=, ==, !=）
//  2) [!] <boolVar>
//  3) 以及简单的 "A && B" / "A || B"（不支持括号与优先级，左到右求值）
// 已足够应对 "move.mag > 0.1"、"move.mag <= 0.1" 这样的条件。
static bool evalSimpleBoolExpr(std::string expr) {
    expr = trim(expr);
    if (expr.empty()) return true;

    // 尝试解析与或（非常简单的左到右，无括号）
    // 优先分割成 "X || Y"；若无，则按 "X && Y"
    auto posOr = expr.find("||");
    if (posOr != std::string::npos) {
        auto L = trim(expr.substr(0, posOr));
        auto R = trim(expr.substr(posOr + 2));
        return evalSimpleBoolExpr(L) || evalSimpleBoolExpr(R);
    }
    auto posAnd = expr.find("&&");
    if (posAnd != std::string::npos) {
        auto L = trim(expr.substr(0, posAnd));
        auto R = trim(expr.substr(posAnd + 2));
        return evalSimpleBoolExpr(L) && evalSimpleBoolExpr(R);
    }

    // 一元取反
    if (!expr.empty() && expr[0] == '!') {
        return !evalSimpleBoolExpr(trim(expr.substr(1)));
    }

    // 二元比较: 依次尝试 <= >= == != > <
    struct Op { const char* s; int len; } ops[] = {
        {"<=",2}, {">=",2}, {"==",2}, {"!=",2}, {">",1}, {"<",1}
    };
    for (auto& op : ops) {
        auto p = expr.find(op.s);
        if (p != std::string::npos) {
            auto L = trim(expr.substr(0, p));
            auto R = trim(expr.substr(p + op.len));
            char* endp = nullptr;
            double rhs = std::strtod(R.c_str(), &endp);
            bool okL = false;
            double lhs = getFloatVar(L, &okL);
            if (!okL || endp == R.c_str()) {
                // 非数字比较/或未知变量，尝试布尔比较
                bool bl = false, br = false;
                bl = getBoolVar(L, &okL);
                br = getBoolVar(R, &bl);
                if (op.len == 2 && op.s[0] == '=') return bl == br;
                if (op.len == 2 && op.s[0] == '!') return bl != br;
                // 其他比较对 bool 不合理，返回 false
                return false;
            }
            if (op.len == 2 && op.s[0] == '<' && op.s[1] == '=') return lhs <= rhs;
            else if (op.len == 2 && op.s[0] == '>' && op.s[1] == '=') return lhs >= rhs;
            else if (op.len == 2 && op.s[0] == '=' && op.s[1] == '=') return lhs == rhs;
            else if (op.len == 2 && op.s[0] == '!' && op.s[1] == '=') return lhs != rhs;
            else if (op.len == 1 && op.s[0] == '>')                 return lhs > rhs;
            else if (op.len == 1 && op.s[0] == '<')                 return lhs < rhs;
        }
    }

    // 没有操作符：当作布尔变量 / Bool 回调
    bool ok = false;
    bool bv = getBoolVar(expr, &ok);
    if (ok) return bv;

    // 当作 float 变量，非零即真
    double fv = getFloatVar(expr, &ok);
    if (ok) return fv != 0.0;

    // 实在解析不了，保守返回 false
    return false;
}

static float evalSimpleFloatExpr(const std::string& expr) {
    std::string s = trim(expr);
    bool ok = false;
    double v = getFloatVar(s, &ok);
    if (ok) return (float)v;
    char* endp = nullptr;
    double num = std::strtod(s.c_str(), &endp);
    if (endp != s.c_str()) return (float)num;
    return 0.0f;
}

// ----------------- 对外 API 实现 -----------------
bool Cond_Init(float defaultTriggerBufferSec) {
    g_defaultTriggerBuffer = defaultTriggerBufferSec;
    sFloats.clear(); sBools.clear(); sTriggers.clear();
    sBoolExprs.clear(); sFloatExprs.clear();
    sBoolFns.clear(); sFloatFns.clear();
    return true;
}
void Cond_Shutdown() {
    sFloats.clear(); sBools.clear(); sTriggers.clear();
    sBoolExprs.clear(); sFloatExprs.clear();
    sBoolFns.clear(); sFloatFns.clear();
}

void Cond_SetFloat(const char* name, float v) { sFloats[std::string(name)] = v; }
void Cond_SetBool(const char* name, bool  v) { sBools[std::string(name)] = v; }
void Cond_SetTimeNorm(float t01) { sFloats["time.norm"] = t01; }

void Cond_FireTrigger(const char* trigName) { sTriggers[std::string(trigName)] = 1.0; }

// 这里先忽略“缓冲时间”的时序语义：如果被点火过且未消费，则返回真并可选择消费。
// 目前你的默认配置没有用到 trigger，所以足够用。之后做正式版再加入时间轴。
bool Cond_TestTrigger(const char* trigName, float /*bufferSec*/, bool consume) {
    auto it = sTriggers.find(std::string(trigName));
    if (it == sTriggers.end()) return false;
    if (consume) sTriggers.erase(it);
    return true;
}
void Cond_SetTriggerBufferDefault(float sec) { g_defaultTriggerBuffer = sec; }

void Cond_RegisterBool(const char* name, CondBoolFn fn) { sBoolFns[std::string(name)] = fn; }
void Cond_RegisterFloat(const char* name, CondFloatFn fn) { sFloatFns[std::string(name)] = fn; }

bool Cond_CompileBool(const char* expr, CondExpr* outHandle) {
    if (!outHandle) return false;
    sBoolExprs.push_back(std::string(expr ? expr : ""));
    *outHandle = (CondExpr)(sBoolExprs.size() - 1);
    return true;
}
bool Cond_CompileFloat(const char* expr, CondExpr* outHandle) {
    if (!outHandle) return false;
    sFloatExprs.push_back(std::string(expr ? expr : ""));
    *outHandle = (CondExpr)(sFloatExprs.size() - 1);
    return true;
}
bool  Cond_EvalBool(CondExpr h) {
    if (h >= sBoolExprs.size()) return false;
    return evalSimpleBoolExpr(sBoolExprs[(size_t)h]);
}
float Cond_EvalFloat(CondExpr h) {
    if (h >= sFloatExprs.size()) return 0.0f;
    return evalSimpleFloatExpr(sFloatExprs[(size_t)h]);
}
