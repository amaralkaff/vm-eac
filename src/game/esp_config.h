#pragma once
#include "../OS-ImGui/imgui/imgui.h"


struct ESPConfig {
    
    bool enabled = true;
    bool showBoxes = true;
    bool showHealthBars = true;
    bool showShieldBars = true;
    bool showNames = true;
    bool showDistances = true;
    bool showWeapons = false;
    bool showLevels = false;
    bool showSkeleton = false;
    bool showKnocked = true; 
    
    
    float maxDistance = 500.0f; 
    float boxThickness = 1.5f;
    float skeletonThickness = 1.5f;
    
    
    ImVec4 enemyColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); 
    ImVec4 teamColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f);  
    ImVec4 visibleColor = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); 
    ImVec4 knockedColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f); 
    
    
    ImVec4 shieldColors[5] = {
        ImVec4(0.5f, 0.5f, 0.5f, 1.0f), 
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 
        ImVec4(0.27f, 0.89f, 1.0f, 1.0f), 
        ImVec4(0.88f, 0.27f, 1.0f, 1.0f), 
        ImVec4(1.0f, 0.87f, 0.24f, 1.0f)  
    };
    
    
    bool hideOnScreenshot = true; 
};
