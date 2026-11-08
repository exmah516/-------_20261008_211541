// 仅供离线宿主验收：导出真实SDK签名，状态/计数放在测试专用共享内存。
#include <windows.h>
static unsigned char* metrics() {
    static unsigned char* view=[] {
        wchar_t name[256]={}; GetEnvironmentVariableW(L"VI_FAKE_SDK_MAP",name,256);
        HANDLE map=OpenFileMappingW(FILE_MAP_ALL_ACCESS,FALSE,name);
        auto* result=static_cast<unsigned char*>(MapViewOfFile(map,FILE_MAP_ALL_ACCESS,0,0,128)); CloseHandle(map); return result;
    }(); return view;
}
extern "C" {
__declspec(dllexport) int openDevice(unsigned int sn) { return sn==582?0:1; }
__declspec(dllexport) void closeDevice() {}
__declspec(dllexport) int startServoLoop(int(__stdcall*)(void*),void*) { return 0; }
__declspec(dllexport) int stopServoLoop() { return 0; }
__declspec(dllexport) void enableForces(bool,int) {}
__declspec(dllexport) int deviceStatus(int id) { return *reinterpret_cast<int*>(metrics()+id*4); }
__declspec(dllexport) int getSerialNumber(int id) { return id==0?582:587; }
__declspec(dllexport) void getEncVel(double* v,int) { v[0]=1;v[1]=2; }
#ifndef VI_NO_JOINTS
__declspec(dllexport) void getJoints(double* v,int id) {
    v[0]=.125;v[1]=.25; InterlockedIncrement(reinterpret_cast<LONG*>(metrics()+8+id*4));
}
#endif
__declspec(dllexport) void getEncoders(int* v,int) { v[0]=10;v[1]=20; }
__declspec(dllexport) void getSwitch(unsigned char* v,int) { *v=0; }
__declspec(dllexport) void sendForce(double* v,double torque,int id) {
    auto* out=reinterpret_cast<double*>(metrics()+16+id*32);
    out[0]=v[0];out[1]=v[1];out[2]=v[2];out[3]=torque;
}
}
