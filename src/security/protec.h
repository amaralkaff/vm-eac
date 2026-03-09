#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <random>

class SysHelper {
public:
    static std::wstring GetProcName() {
        static const std::vector<std::wstring> names = {
            L"dwm.exe",                         
            L"RuntimeBroker.exe",               
            L"SearchApp.exe",                   
            L"StartMenuExperienceHost.exe",    
            L"TextInputHost.exe",               
            L"ShellExperienceHost.exe",        
            L"SecurityHealthSystray.exe",      
            L"audiodg.exe",                    
            L"dasHost.exe",                    
            L"fontdrvhost.exe"                  
        };
        
        static std::random_device rd;
        static std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(names.size() - 1));
        
        return names[dis(gen)];
    }
    
    static bool CheckStatus() {
        return IsDebuggerPresent();
    }
};
