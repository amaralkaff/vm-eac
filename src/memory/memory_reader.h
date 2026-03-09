#pragma once
#include "../driver/hv_interface.h"


extern DriverInterfaceV3* g_Driver;
extern DWORD g_ApexPid;
extern uintptr_t g_ApexBase;


template<typename T>
inline T Read(uintptr_t address) {
    T value{};
    if (g_Driver && g_ApexPid) {
        g_Driver->ReadMemory(g_ApexPid, address, &value, sizeof(T));
    }
    return value;
}


template<typename T>
inline bool ReadArray(uintptr_t address, T* buffer, size_t count) {
    if (g_Driver && g_ApexPid) {
        size_t totalSize = sizeof(T) * count;
        // Use bulk read for large reads (>64 bytes) to reduce VM-exits
        if (totalSize > 64) {
            return g_Driver->ReadMemoryBulk(g_ApexPid, address, buffer, totalSize);
        }
        return g_Driver->ReadMemory(g_ApexPid, address, buffer, totalSize);
    }
    return false;
}


inline bool ReadBulk(uintptr_t address, void* buffer, size_t size) {
    if (g_Driver && g_ApexPid) {
        return g_Driver->ReadMemoryBulk(g_ApexPid, address, buffer, size);
    }
    return false;
}
