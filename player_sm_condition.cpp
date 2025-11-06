// player_sm_condition.cpp — 正式版（DSL + 触发器缓冲 + 回调）
// 兼容头文件 player_sm_condition.h 的现有接口
#include "player_sm_condition.h"

#include <unordered_map>
#include <vector>
#include <string>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <algorithm>
#include <cassert>

// ------------------------------ 基础存储 ------------------------------
static float g_defaultTriggerBuffer = 0.15f;

// 标量变量
static std::unordered_map<std::string, float> sFloats;   // e.g., "move.mag", "time.norm"
static std::unordered_map<std::string, bool>  sBools;    // e.g., "grounded"

// 触发器：最近一次“点火”的时间（秒）
static std::unordered_map<std::string, double> sTriggerLastFireSec;

// 回调
static std::unordered_map<std::string, CondBoolFn>  sBoolFns;
static std::unordered_map<std::string, CondFloatFn> sFloatFns;

// 全局时钟（steady_clock）
static inline double nowSec() {
    using clock = std::chrono::steady_clock;
    static const auto t0 = clock::now();
    auto t = clock::now();
    return std::chrono::duration<double>(t - t0).count();
}

// ------------------------------ 词法与编译 ------------------------------
enum class Tok {
    END, IDENT, NUMBER,
    LP, RP,
    NOT, AND, OR,
    GT, GE, LT, LE, EQ, NE
};

struct Token {
    Tok   t{ Tok::END };
    std::string s;   // IDENT 原文
    float num{ 0.0f }; // NUMBER
};

struct Lexer {
    const char* p{};
    Token cur{};
    explicit Lexer(const char* src) : p(src ? src : "") { next(); }

    static bool isIdent1(char c) { return std::isalpha((unsigned char)c) || c == '_' || c == '.'; }
    static bool isIdent(char c) { return std::isalnum((unsigned char)c) || c == '_' || c == '.'; }

    void skipWs() { while (*p && std::isspace((unsigned char)*p)) ++p; }

    void next() {
        skipWs();
        if (!*p) { cur = { Tok::END }; return; }
        char c = *p;

        // identifiers / function names
        if (isIdent1(c)) {
            const char* b = p++;
            while (isIdent(*p)) ++p;
            cur.t = Tok::IDENT; cur.s.assign(b, p);
            skipWs();
            if (*p == '(') { /*留给解析阶段判断是否函数*/ }
            return;
        }

        // number
        if (std::isdigit((unsigned char)c) || (c == '.' && std::isdigit((unsigned char)p[1]))) {
            char* endp = nullptr;
            cur.t = Tok::NUMBER; cur.num = std::strtof(p, &endp);
            p = endp; return;
        }

        // operators / parens
        if (c == '(') { ++p; cur = { Tok::LP }; return; }
        if (c == ')') { ++p; cur = { Tok::RP }; return; }
        if (c == '!' && p[1] == '=') { p += 2; cur = { Tok::NE }; return; }
        if (c == '=' && p[1] == '=') { p += 2; cur = { Tok::EQ }; return; }
        if (c == '<' && p[1] == '=') { p += 2; cur = { Tok::LE }; return; }
        if (c == '>' && p[1] == '=') { p += 2; cur = { Tok::GE }; return; }
        if (c == '&' && p[1] == '&') { p += 2; cur = { Tok::AND }; return; }
        if (c == '|' && p[1] == '|') { p += 2; cur = { Tok::OR };  return; }
        if (c == '>') { ++p; cur = { Tok::GT }; return; }
        if (c == '<') { ++p; cur = { Tok::LT }; return; }
        if (c == '!') { ++p; cur = { Tok::NOT }; return; }

        // unknown -> skip
        ++p; next();
    }
};

// RPN opcodes
enum class Op {
    PUSH_NUM,        // f
    PUSH_VAR_F,      // ident
    PUSH_VAR_B,      // ident
    CALL_F,          // ident()
    CALL_B,          // ident()
    TO_BOOL,         // number->bool (x!=0)
    TO_FLOAT,        // bool->float (b?1:0)
    NOT_, AND_, OR_,
    GT_, GE_, LT_, LE_, EQ_, NE_   // 数值比较结果为 bool
};

struct Instr {
    Op op{};
    float f{ 0 };
    std::string s; // ident
};

struct Compiled {
    bool isBool = true;          // 这是一条 bool 表达式
    std::vector<Instr> code;     // RPN
};

// 全部编译后的表达式池（CondExpr 是索引）
static std::vector<Compiled> sCompiled;

// precedence（shunting-yard）
static int prec(Tok t) {
    switch (t) {
    case Tok::OR:  return 1;
    case Tok::AND: return 2;
    case Tok::EQ: case Tok::NE: return 3;
    case Tok::LT: case Tok::LE: case Tok::GT: case Tok::GE: return 4;
    case Tok::NOT: return 5;
    default: return 0;
    }
}
static bool rightAssoc(Tok t) { return t == Tok::NOT; }

// 将 IDENT 判定为“bool变量/回调”还是“float变量/回调”在运行时完成：
// - 变量若只在 float 表里找到 → 按 float
// - 若只在 bool 表里找到/或是 true/false → 按 bool
// - 函数：优先按注册的回调类型决定
// 编译期只保留名字和调用形式（是否带()）

static bool compileToRPN_bool(const char* expr, Compiled* out) {
    Lexer lx(expr);
    std::vector<Instr> output;
    std::vector<Token> opstack; // 运算符栈（存 Token）

    auto pushIdent = [&](const Token& tk) {
        // 可能是 ident() 调用，也可能是变量
        // 偷看一个 '('
        const char* p = lx.p; // 保存
        Lexer look(expr); // 这种方式不好用；改成手工检查
        (void)look;
        };

    // 我们手动解析，处理括号与一元 !
    Token last = { Tok::END };
    while (lx.cur.t != Tok::END) {
        Token t = lx.cur;

        if (t.t == Tok::NUMBER) {
            output.push_back({ Op::PUSH_NUM, t.num, {} });
            last = t; lx.next(); continue;
        }
        if (t.t == Tok::IDENT) {
            // 看看是否是函数形式 ident()
            // 需要从当前 lexer 的指针向前找一个紧随的 '('，但我们无法轻易回看。
            // 简单做法：创建一个临时指针从当前 p 开始跳过空白检查 '('
            const char* p = lx.p;
            // 回到 IDENT 末尾位置（lx.p 已经在 IDENT 末尾）
            const char* q = p;
            while (*q && std::isspace((unsigned char)*q)) ++q;
            bool isCall = (*q == '(');
            if (isCall) {
                // 吃掉 "(" 和可选的 ")"
                lx.next(); // 把当前 IDENT 消费掉
                // 消费 "("
                if (lx.cur.t == Tok::LP) lx.next();
                // 暂时只支持无参函数，遇到 RP 直接过
                if (lx.cur.t == Tok::RP) lx.next();
                // 记录为调用（bool/float 在运行时判定）
                Instr insF{ Op::CALL_F, 0.0f, t.s };
                Instr insB{ Op::CALL_B, 0.0f, t.s };
                // 先放一个“占位”：在执行期根据是否注册为 FloatFn/BoolFn 决定
                // 为避免执行期分支，我们保留两个指令都不合适。这里采用：统一先按 CALL_F 编译，
                // 执行时若找不到 FloatFn 再尝试 BoolFn（并在需要处做 bool/float 转换）。
                output.push_back(insF);
                last = t;
                continue;
            }
            else {
                // 变量（布尔或浮点运行时再判定）
                lx.next();
                // 同上，统一先按 float 变量编译，执行时再查表
                output.push_back({ Op::PUSH_VAR_F, 0.0f, t.s });
                last = t; continue;
            }
        }

        if (t.t == Tok::LP) {
            opstack.push_back(t); lx.next(); continue;
        }
        if (t.t == Tok::RP) {
            while (!opstack.empty() && opstack.back().t != Tok::LP) {
                Token op = opstack.back(); opstack.pop_back();
                switch (op.t) {
                case Tok::NOT: output.push_back({ Op::NOT_ }); break;
                case Tok::AND: output.push_back({ Op::AND_ }); break;
                case Tok::OR:  output.push_back({ Op::OR_ });  break;
                case Tok::GT:  output.push_back({ Op::GT_ });  break;
                case Tok::GE:  output.push_back({ Op::GE_ });  break;
                case Tok::LT:  output.push_back({ Op::LT_ });  break;
                case Tok::LE:  output.push_back({ Op::LE_ });  break;
                case Tok::EQ:  output.push_back({ Op::EQ_ });  break;
                case Tok::NE:  output.push_back({ Op::NE_ });  break;
                default: break;
                }
            }
            if (!opstack.empty() && opstack.back().t == Tok::LP) opstack.pop_back();
            lx.next(); continue;
        }

        // 运算符
        if (t.t == Tok::NOT || t.t == Tok::AND || t.t == Tok::OR ||
            t.t == Tok::GT || t.t == Tok::GE || t.t == Tok::LT || t.t == Tok::LE ||
            t.t == Tok::EQ || t.t == Tok::NE) {
            while (!opstack.empty()) {
                Token top = opstack.back();
                if (top.t == Tok::LP) break;
                int p1 = prec(t.t), p2 = prec(top.t);
                if ((!rightAssoc(t.t) && p1 <= p2) || (rightAssoc(t.t) && p1 < p2)) {
                    opstack.pop_back();
                    switch (top.t) {
                    case Tok::NOT: output.push_back({ Op::NOT_ }); break;
                    case Tok::AND: output.push_back({ Op::AND_ }); break;
                    case Tok::OR:  output.push_back({ Op::OR_ });  break;
                    case Tok::GT:  output.push_back({ Op::GT_ });  break;
                    case Tok::GE:  output.push_back({ Op::GE_ });  break;
                    case Tok::LT:  output.push_back({ Op::LT_ });  break;
                    case Tok::LE:  output.push_back({ Op::LE_ });  break;
                    case Tok::EQ:  output.push_back({ Op::EQ_ });  break;
                    case Tok::NE:  output.push_back({ Op::NE_ });  break;
                    default: break;
                    }
                }
                else break;
            }
            opstack.push_back(t); lx.next(); continue;
        }

        // 未知token，跳过
        lx.next();
    }

    while (!opstack.empty()) {
        Token top = opstack.back(); opstack.pop_back();
        switch (top.t) {
        case Tok::NOT: output.push_back({ Op::NOT_ }); break;
        case Tok::AND: output.push_back({ Op::AND_ }); break;
        case Tok::OR:  output.push_back({ Op::OR_ });  break;
        case Tok::GT:  output.push_back({ Op::GT_ });  break;
        case Tok::GE:  output.push_back({ Op::GE_ });  break;
        case Tok::LT:  output.push_back({ Op::LT_ });  break;
        case Tok::LE:  output.push_back({ Op::LE_ });  break;
        case Tok::EQ:  output.push_back({ Op::EQ_ });  break;
        case Tok::NE:  output.push_back({ Op::NE_ });  break;
        default: break;
        }
    }

    out->isBool = true;
    out->code.swap(output);
    return true;
}

// float 表达式目前支持：数字 / 变量 / 无参回调（不做 +-* /），够用时再扩展
static bool compileToRPN_float(const char* expr, Compiled* out) {
    Lexer lx(expr);
    std::vector<Instr> output;

    while (lx.cur.t != Tok::END) {
        Token t = lx.cur;
        if (t.t == Tok::NUMBER) { output.push_back({ Op::PUSH_NUM, t.num, {} }); lx.next(); continue; }
        if (t.t == Tok::IDENT) {
            // 检查 ident()
            const char* q = lx.p;
            while (*q && std::isspace((unsigned char)*q)) ++q;
            bool isCall = (*q == '(');
            lx.next();
            if (isCall) {
                if (lx.cur.t == Tok::LP) lx.next();
                if (lx.cur.t == Tok::RP) lx.next();
                output.push_back({ Op::CALL_F, 0.0f, t.s });
            }
            else {
                output.push_back({ Op::PUSH_VAR_F, 0.0f, t.s });
            }
            continue;
        }
        if (t.t == Tok::LP) { lx.next(); continue; }
        if (t.t == Tok::RP) { lx.next(); continue; }
        // 其他忽略
        lx.next();
    }

    out->isBool = false;
    out->code.swap(output);
    return true;
}

// ------------------------------ 执行期 ------------------------------
static inline bool asBool(float f) { return std::abs(f) > 1e-6f; }
static inline float asFloat(bool b) { return b ? 1.0f : 0.0f; }

static float fetchVarF(const std::string& name, bool* have) {
    auto itf = sFloats.find(name);
    if (itf != sFloats.end()) { if (have) *have = true; return itf->second; }
    // true/false 作为 float
    if (name == "true") { if (have) *have = true; return 1.0f; }
    if (name == "false") { if (have) *have = true; return 0.0f; }
    // bool变量转float
    auto itb = sBools.find(name);
    if (itb != sBools.end()) { if (have) *have = true; return asFloat(itb->second); }
    // 回调 float()
    auto itfn = sFloatFns.find(name);
    if (itfn != sFloatFns.end()) { if (have) *have = true; return itfn->second(); }
    // 回调 bool() 也允许作为 float
    auto itbn = sBoolFns.find(name);
    if (itbn != sBoolFns.end()) { if (have) *have = true; return asFloat(itbn->second()); }
    if (have) *have = false;
    return 0.0f;
}
static bool fetchVarB(const std::string& name, bool* have) {
    if (name == "true") { if (have) *have = true; return true; }
    if (name == "false") { if (have) *have = true; return false; }
    auto itb = sBools.find(name);
    if (itb != sBools.end()) { if (have) *have = true; return itb->second; }
    auto itf = sFloats.find(name);
    if (itf != sFloats.end()) { if (have) *have = true; return asBool(itf->second); }
    auto itbn = sBoolFns.find(name);
    if (itbn != sBoolFns.end()) { if (have) *have = true; return itbn->second(); }
    auto itfn = sFloatFns.find(name);
    if (itfn != sFloatFns.end()) { if (have) *have = true; return asBool(itfn->second()); }
    if (have) *have = false;
    return false;
}

static bool evalBool(const Compiled& c) {
    std::vector<float> fs;  fs.reserve(16);
    std::vector<bool>  bs;  bs.reserve(16);
    for (const auto& ins : c.code) {
        switch (ins.op) {
        case Op::PUSH_NUM: fs.push_back(ins.f); break;
        case Op::PUSH_VAR_F: {
            bool ok = false; float v = fetchVarF(ins.s, &ok);
            fs.push_back(v);
        } break;
        case Op::PUSH_VAR_B: {
            bool ok = false; bool b = fetchVarB(ins.s, &ok);
            bs.push_back(b);
        } break;
        case Op::CALL_F: {
            // 优先 float 回调，没有则尝试 bool 回调
            auto itf = sFloatFns.find(ins.s);
            if (itf != sFloatFns.end()) fs.push_back(itf->second());
            else {
                auto itb = sBoolFns.find(ins.s);
                fs.push_back((itb != sBoolFns.end()) ? asFloat(itb->second()) : 0.0f);
            }
        } break;
        case Op::CALL_B: {
            auto itb = sBoolFns.find(ins.s);
            if (itb != sBoolFns.end()) bs.push_back(itb->second());
            else {
                auto itf = sFloatFns.find(ins.s);
                bs.push_back((itf != sFloatFns.end()) ? asBool(itf->second()) : false);
            }
        } break;
        case Op::TO_BOOL: {
            if (!fs.empty()) { bs.push_back(asBool(fs.back())); fs.pop_back(); }
        } break;
        case Op::TO_FLOAT: {
            if (!bs.empty()) { fs.push_back(asFloat(bs.back())); bs.pop_back(); }
        } break;
        case Op::NOT_: {
            bool a = !bs.back(); bs.back() = a;
        } break;
        case Op::AND_: {
            bool b2 = bs.back(); bs.pop_back();
            bool b1 = bs.back(); bs.pop_back();
            bs.push_back(b1 && b2);
        } break;
        case Op::OR_: {
            bool b2 = bs.back(); bs.pop_back();
            bool b1 = bs.back(); bs.pop_back();
            bs.push_back(b1 || b2);
        } break;
        case Op::GT_: {
            float b2 = fs.back(); fs.pop_back();
            float b1 = fs.back(); fs.pop_back();
            bs.push_back(b1 > b2);
        } break;
        case Op::GE_: {
            float b2 = fs.back(); fs.pop_back();
            float b1 = fs.back(); fs.pop_back();
            bs.push_back(b1 >= b2);
        } break;
        case Op::LT_: {
            float b2 = fs.back(); fs.pop_back();
            float b1 = fs.back(); fs.pop_back();
            bs.push_back(b1 < b2);
        } break;
        case Op::LE_: {
            float b2 = fs.back(); fs.pop_back();
            float b1 = fs.back(); fs.pop_back();
            bs.push_back(b1 <= b2);
        } break;
        case Op::EQ_: {
            // EQ 支持“数值==数值”的常见用法；当需要比较布尔时也能工作
            if (fs.size() >= 2) {
                float b2 = fs.back(); fs.pop_back();
                float b1 = fs.back(); fs.pop_back();
                bs.push_back(b1 == b2);
            }
            else {
                bool y2 = bs.back(); bs.pop_back();
                bool y1 = bs.back(); bs.pop_back();
                bs.push_back(y1 == y2);
            }
        } break;
        case Op::NE_: {
            if (fs.size() >= 2) {
                float b2 = fs.back(); fs.pop_back();
                float b1 = fs.back(); fs.pop_back();
                bs.push_back(b1 != b2);
            }
            else {
                bool y2 = bs.back(); bs.pop_back();
                bool y1 = bs.back(); bs.pop_back();
                bs.push_back(y1 != y2);
            }
        } break;
        }
    }
    if (!bs.empty()) return bs.back();
    if (!fs.empty()) return asBool(fs.back());
    return false;
}

static float evalFloat(const Compiled& c) {
    std::vector<float> fs;  fs.reserve(16);
    for (const auto& ins : c.code) {
        switch (ins.op) {
        case Op::PUSH_NUM: fs.push_back(ins.f); break;
        case Op::PUSH_VAR_F: {
            bool ok = false; float v = fetchVarF(ins.s, &ok);
            fs.push_back(v);
        } break;
        case Op::CALL_F: {
            auto itf = sFloatFns.find(ins.s);
            if (itf != sFloatFns.end()) fs.push_back(itf->second());
            else {
                auto itb = sBoolFns.find(ins.s);
                fs.push_back((itb != sBoolFns.end()) ? asFloat(itb->second()) : 0.0f);
            }
        } break;
        default: break;
        }
    }
    if (fs.empty()) return 0.0f;
    return fs.back();
}

// ------------------------------ 对外 API ------------------------------
bool Cond_Init(float defaultTriggerBufferSec) {
    g_defaultTriggerBuffer = defaultTriggerBufferSec;
    sFloats.clear(); sBools.clear(); sTriggerLastFireSec.clear();
    sBoolFns.clear(); sFloatFns.clear();
    sCompiled.clear();
    return true;
}
void Cond_Shutdown() {
    sFloats.clear(); sBools.clear(); sTriggerLastFireSec.clear();
    sBoolFns.clear(); sFloatFns.clear();
    sCompiled.clear();
}

void Cond_SetFloat(const char* name, float v) { sFloats[std::string(name)] = v; }
void Cond_SetBool(const char* name, bool v) { sBools[std::string(name)] = v; }
void Cond_SetTimeNorm(float t01) { sFloats["time.norm"] = t01; }

void Cond_FireTrigger(const char* trigName) {
    sTriggerLastFireSec[std::string(trigName)] = nowSec();
}

bool Cond_TestTrigger(const char* trigName, float bufferSec, bool consume) {
    auto it = sTriggerLastFireSec.find(std::string(trigName));
    if (it == sTriggerLastFireSec.end()) return false;
    double buf = (bufferSec >= 0.0f) ? bufferSec : g_defaultTriggerBuffer;
    bool ok = (nowSec() - it->second) <= buf;
    if (ok && consume) sTriggerLastFireSec.erase(it);
    return ok;
}

void Cond_SetTriggerBufferDefault(float sec) { g_defaultTriggerBuffer = sec; }

void Cond_RegisterBool(const char* name, CondBoolFn fn) { sBoolFns[std::string(name)] = fn; }
void Cond_RegisterFloat(const char* name, CondFloatFn fn) { sFloatFns[std::string(name)] = fn; }

bool Cond_CompileBool(const char* expr, CondExpr* outHandle) {
    if (!outHandle) return false;
    Compiled c;
    if (!compileToRPN_bool(expr ? expr : "", &c)) return false;
    sCompiled.push_back(std::move(c));
    *outHandle = (CondExpr)(sCompiled.size() - 1);
    return true;
}
bool Cond_CompileFloat(const char* expr, CondExpr* outHandle) {
    if (!outHandle) return false;
    Compiled c;
    if (!compileToRPN_float(expr ? expr : "", &c)) return false;
    sCompiled.push_back(std::move(c));
    *outHandle = (CondExpr)(sCompiled.size() - 1);
    return true;
}

bool Cond_EvalBool(CondExpr h) {
    if (h >= sCompiled.size()) return false;
    const auto& c = sCompiled[(size_t)h];
    if (!c.isBool) {
        // 若误编译成 float，按“非零即真”
        return asBool(evalFloat(c));
    }
    return evalBool(c);
}
float Cond_EvalFloat(CondExpr h) {
    if (h >= sCompiled.size()) return 0.0f;
    const auto& c = sCompiled[(size_t)h];
    if (c.isBool) {
        // 若误编译成 bool，将其转换为 0/1
        return asFloat(evalBool(c));
    }
    return evalFloat(c);
}
