// player_sm_condition.h
#pragma once
#include <cstdint>

using CondExpr = uint32_t;

// 生命周期
bool  Cond_Init(float defaultTriggerBufferSec);
void  Cond_Shutdown();

// 变量输入（FSM 每帧或事件驱动写入）
void  Cond_SetFloat(const char* name, float v);   // e.g., "move.mag"
void  Cond_SetBool(const char* name, bool  v);   // e.g., "grounded"
void  Cond_SetTimeNorm(float t01);                // 便捷写入 "time.norm"

// 触发器（带缓冲）
void  Cond_FireTrigger(const char* trigName);     // e.g., "Attack"
// 测试并可选择消费触发器（bufferSec<0 使用全局默认；consume=true 命中后清除）
bool  Cond_TestTrigger(const char* trigName, float bufferSec, bool consume);
void  Cond_SetTriggerBufferDefault(float sec);

// 回调注册（复杂条件留到子模块实现）
using CondBoolFn = bool  (*)(void);
using CondFloatFn = float (*)(void);
void  Cond_RegisterBool(const char* name, CondBoolFn  fn);
void  Cond_RegisterFloat(const char* name, CondFloatFn fn);

// 表达式编译/评估（FSM 加载 JSON 时编译一次，运行时只 Eval）
bool  Cond_CompileBool(const char* expr, CondExpr* outHandle);
bool  Cond_CompileFloat(const char* expr, CondExpr* outHandle);
bool  Cond_EvalBool(CondExpr h);
float Cond_EvalFloat(CondExpr h);