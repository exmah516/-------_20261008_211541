#pragma once
#include <windows.h>
#include <chrono>
#include <cstdint>
namespace baseline {
inline ULONGLONG now_ms=1000;
inline DWORD tick() { return static_cast<DWORD>(now_ms); }
inline ULONGLONG tick64() { return now_ms; }
inline BOOL counter(LARGE_INTEGER* p) { p->QuadPart=static_cast<LONGLONG>(now_ms*1000); return TRUE; }
inline BOOL frequency(LARGE_INTEGER* p) { p->QuadPart=1000000; return TRUE; }
inline void sleep(DWORD ms) { now_ms+=ms; }
}
#define GetTickCount baseline::tick
#define GetTickCount64 baseline::tick64
#define QueryPerformanceCounter baseline::counter
#define QueryPerformanceFrequency baseline::frequency
#define Sleep baseline::sleep
