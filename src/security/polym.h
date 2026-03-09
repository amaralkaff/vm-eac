#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>

class TimeHelper {
public:
    static std::wstring GetRandName() {
        static const wchar_t* prefixes[] = {
            L"Intel", L"AMD", L"Nvidia", L"Realtek", L"Microsoft", L"Windows"
        };
        static const wchar_t* suffixes[] = {
            L"Mgr", L"Svc", L"Helper", L"Monitor", L"Agent", L"Host", L"Ctrl"
        };
        static const wchar_t* types[] = {
            L"Audio", L"Video", L"Net", L"Disk", L"Usb", L"Pci", L"Acpi"
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        
        std::uniform_int_distribution<> prefixDis(0, _countof(prefixes) - 1);
        std::uniform_int_distribution<> suffixDis(0, _countof(suffixes) - 1);
        std::uniform_int_distribution<> typeDis(0, _countof(types) - 1);
        
        std::wstring name = prefixes[prefixDis(gen)];
        name += types[typeDis(gen)];
        name += suffixes[suffixDis(gen)];
        
        return name;
    }
    
    static void SleepFor(int baseMs, int jitterMs = 0) {
        if (jitterMs <= 0) {
            Sleep(baseMs);
            return;
        }
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(-jitterMs, jitterMs);
        
        int actualSleep = baseMs + dis(gen);
        if (actualSleep < 1) actualSleep = 1;
        
        Sleep(actualSleep);
    }
    
    static void DoWork() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100, 1000);
        
        volatile int dummy = 0;
        int iterations = dis(gen);
        
        for (int i = 0; i < iterations; i++) {
            dummy += i;
            dummy ^= (i << 2);
            dummy -= (i >> 1);
        }
    }
};


