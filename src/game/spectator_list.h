#pragma once
#include "../memory/offsets.h"
#include "../memory/memory_reader.h"
#include "../overlay/OS-ImGui/imgui/imgui.h"
#include <string>
#include <vector>


extern uintptr_t g_ApexBase;
extern DWORD g_ApexPid;
extern DriverInterfaceV3* g_Driver;

struct SpectatorInfo {
    std::string name;
    int index;
    
    SpectatorInfo(const std::string& n, int idx) : name(n), index(idx) {}
};

class SpectatorList {
private:
    std::vector<SpectatorInfo> spectators;
    float lastUpdateTime;
    const float UPDATE_INTERVAL = 1.0f; 
    
public:
    SpectatorList() : lastUpdateTime(0.0f) {}
    
    void Update(uintptr_t localPlayerAddress) {
        
        float currentTime = ImGui::GetTime();
        if (currentTime - lastUpdateTime < UPDATE_INTERVAL) {
            return;
        }
        lastUpdateTime = currentTime;
        
        spectators.clear();
        
        
        if (!g_Driver || !g_ApexBase || !g_ApexPid || !localPlayerAddress) {
            return;
        }
        
        
        uintptr_t observerList = 0;
        
        
        try {
            if (!g_Driver->ReadMemory(g_ApexPid, g_ApexBase + OFF_OBSERVER_LIST, &observerList, sizeof(observerList))) {
                return; 
            }
            
            
            if (observerList == 0 || observerList < 0x10000 || observerList > 0x7FFFFFFFFFFF) {
                return; 
            }
        } catch (...) {
            
            return;
        }
        
        
        for (int i = 1; i < 100; i++) {
            
            uintptr_t entityListAddr = g_ApexBase + OFF_ENTITY_LIST + (static_cast<uint64_t>(i) * 0x20);
            uintptr_t entityAddress = 0;
            
            if (!g_Driver->ReadMemory(g_ApexPid, entityListAddr, &entityAddress, sizeof(entityAddress))) {
                continue;
            }
            
            if (entityAddress == 0 || entityAddress < 0x10000) {
                continue;
            }
            
            
            int lifeState = 0;
            if (!g_Driver->ReadMemory(g_ApexPid, entityAddress + OFF_LIFE_STATE, &lifeState, sizeof(lifeState))) {
                continue;
            }
            
            
            if (lifeState == 0) {
                continue;
            }
            
            
            int playerData = 0;
            if (!g_Driver->ReadMemory(g_ApexPid, entityAddress + 0x38, &playerData, sizeof(playerData))) {
                continue;
            }
            
            
            int specIndex = 0;
            uintptr_t specIndexAddr = observerList + static_cast<uint64_t>(playerData) * 8 + OFF_OBSERVER_ARRAY;
            if (!g_Driver->ReadMemory(g_ApexPid, specIndexAddr, &specIndex, sizeof(specIndex))) {
                continue;
            }
            
            
            uintptr_t spectatorTarget = 0;
            uintptr_t targetAddr = g_ApexBase + OFF_ENTITY_LIST + (static_cast<uint64_t>(specIndex & 0xFFFF) * 0x20);
            if (!g_Driver->ReadMemory(g_ApexPid, targetAddr, &spectatorTarget, sizeof(spectatorTarget))) {
                continue;
            }
            
            
            if (spectatorTarget == localPlayerAddress) {
                
                uintptr_t nameListAddress = g_ApexBase + OFF_NAME_LIST + (static_cast<uint64_t>(i) * 24);
                uintptr_t namePtr = 0;
                
                if (g_Driver->ReadMemory(g_ApexPid, nameListAddress, &namePtr, sizeof(namePtr))) {
                    if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                        char nameBuffer[32] = {0};
                        if (g_Driver->ReadMemory(g_ApexPid, namePtr, nameBuffer, 32)) {
                            nameBuffer[31] = '\0';
                            
                            
                            bool isValid = true;
                            int len = 0;
                            for (int j = 0; j < 32; j++) {
                                if (nameBuffer[j] == '\0') break;
                                if (nameBuffer[j] >= 0x20 && nameBuffer[j] <= 0x7E) {
                                    len++;
                                } else {
                                    isValid = false;
                                    break;
                                }
                            }
                            
                            if (isValid && len > 0 && len <= 31) {
                                nameBuffer[len] = '\0';
                                spectators.push_back(SpectatorInfo(std::string(nameBuffer), i));
                            }
                        }
                    }
                }
            }
        }
    }
    
    void Render() {
        if (spectators.empty()) {
            return;
        }
        
        
        ImVec2 pos(20, 20);
        ImDrawList* drawList = ImGui::GetBackgroundDrawList();
        
        
        float boxWidth = 250.0f;
        float boxHeight = 30.0f + (spectators.size() * 20.0f);
        
        drawList->AddRectFilled(
            pos,
            ImVec2(pos.x + boxWidth, pos.y + boxHeight),
            IM_COL32(0, 0, 0, 180),
            5.0f
        );
        
        
        drawList->AddRect(
            pos,
            ImVec2(pos.x + boxWidth, pos.y + boxHeight),
            IM_COL32(255, 0, 0, 255),
            5.0f,
            0,
            2.0f
        );
        
        
        ImVec2 textPos(pos.x + 10, pos.y + 8);
        char title[64];
        sprintf_s(title, "SPECTATORS [%d]", static_cast<int>(spectators.size()));
        drawList->AddText(textPos, IM_COL32(255, 0, 0, 255), title);
        
        
        textPos.y += 22;
        for (const auto& spec : spectators) {
            drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), spec.name.c_str());
            textPos.y += 20;
        }
    }
    
    int GetSpectatorCount() const {
        return static_cast<int>(spectators.size());
    }
};
