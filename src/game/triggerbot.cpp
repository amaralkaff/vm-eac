#include "triggerbot.h"
#include "math.h"
#include <cmath>
#include <algorithm>
#include "../OS-ImGui/imgui/imgui.h"
#include <mutex>

TriggerBot::TriggerBot(DriverInterfaceV3* drv, Overlay* ovl) 
        : driver(drv), overlay(ovl), enabled(false), lastTriggerTime(0), 
            gen(rd()), delayLevel(TriggerDelayLevel::LEVEL_3), boneMode(TriggerBoneMode::HEAD_BODY),
            showHitboxes(true), usePrediction(true), extraPadding(0.0f), closeRangeThreshold(3.0f),
            triggerKey(VK_XBUTTON2), isSelectingKey(false) {
}

TriggerBot::~TriggerBot() {
}

int TriggerBot::GetRandomDelay() {
    
    int minDelay, maxDelay;
    
    switch (delayLevel) {
        case TriggerDelayLevel::LEVEL_1:
            minDelay = 200; maxDelay = 300;
            break;
        case TriggerDelayLevel::LEVEL_2:
            minDelay = 100; maxDelay = 200;
            break;
        case TriggerDelayLevel::LEVEL_3:
            minDelay = 50; maxDelay = 100;
            break;
        case TriggerDelayLevel::LEVEL_4:
            minDelay = 25; maxDelay = 50;
            break;
        case TriggerDelayLevel::LEVEL_5:
            minDelay = 10; maxDelay = 25;
            break;
        default:
            minDelay = 50; maxDelay = 100;
            break;
    }
    
    std::uniform_int_distribution<> dist(minDelay, maxDelay);
    return dist(gen);
}

std::vector<int> TriggerBot::GetActiveBones() {
    switch (boneMode) {
        case TriggerBoneMode::BODY_ONLY:
            return { 2, 3 }; 
        case TriggerBoneMode::HEAD_ONLY:
            return { 0 }; 
        case TriggerBoneMode::HEAD_BODY:
            return { 0, 2 }; 
        case TriggerBoneMode::FULL_BODY:
            return { 0, 1, 2, 3 }; 
        default:
            return { 0, 2 }; 
    }
}

TriggerVec3 TriggerBot::GetBoxDimensionsForBone(int boneIndex) {
    switch (boneIndex) {
        case 0: 
            return { 5.0f, 5.0f, 5.0f };
        case 1: 
            return { 4.0f, 4.0f, 4.0f };
        case 2: 
            return { 7.0f, 7.0f, 10.0f };
        case 3: 
            return { 8.0f, 8.0f, 12.0f };
        default:
            return { 6.0f, 6.0f, 8.0f };
    }
}

std::vector<TriggerVec3> TriggerBot::CalculateBoxCorners(const TriggerVec3& bonePos, const TriggerVec3& dimensions) {
    return {
        
        {bonePos.x + dimensions.x, bonePos.y + dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y + dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y - dimensions.y, bonePos.z + dimensions.z},
        {bonePos.x + dimensions.x, bonePos.y - dimensions.y, bonePos.z + dimensions.z},
        
        {bonePos.x + dimensions.x, bonePos.y + dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y + dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x - dimensions.x, bonePos.y - dimensions.y, bonePos.z - dimensions.z},
        {bonePos.x + dimensions.x, bonePos.y - dimensions.y, bonePos.z - dimensions.z}
    };
}

TriggerVec3 TriggerBot::CalculatePredictedPosition(const TriggerVec3& bonePos, const TriggerVec3& velocity, float projectileSpeed) {
    if (projectileSpeed <= 0.0f) return bonePos;

    
    extern Vector3 g_LocalPos;
    TriggerVec3 localPos = { g_LocalPos.x, g_LocalPos.y, g_LocalPos.z };

    
    float distance = sqrtf(
        (bonePos.x - localPos.x) * (bonePos.x - localPos.x) +
        (bonePos.y - localPos.y) * (bonePos.y - localPos.y) +
        (bonePos.z - localPos.z) * (bonePos.z - localPos.z));

    
    if (distance < 600.0f) { 
        return bonePos;
    }

    
    float timeToHit = distance / projectileSpeed;

    
    if (timeToHit <= 0.0f || timeToHit > 0.8f) {
        return bonePos;
    }

    
    TriggerVec3 predicted = {
        bonePos.x + velocity.x * timeToHit,
        bonePos.y + velocity.y * timeToHit,
        bonePos.z + velocity.z * timeToHit
    };

    
    extern float g_ProjectileScale;
    float drop = 0.5f * g_ProjectileScale * timeToHit * timeToHit;
    predicted.z -= drop;

    return predicted;
}

bool TriggerBot::CheckBoneOnCrosshair(Entity& entity, int boneIndex) {
    try {
        if (!overlay) return false;
        
        
        if (!entity.valid || entity.address == 0) return false;
        if (entity.health <= 0) return false;
        
        
        Vector3 bonePos = entity.position;

        switch (boneIndex) {
            case 0: 
                bonePos.z += 60.0f;
                break;
            case 1: 
                bonePos.z += 50.0f;
                break;
            case 2: 
                bonePos.z += 35.0f;
                break;
            case 3: 
                bonePos.z += 10.0f;
                break;
            default:
                bonePos.z += 35.0f;
                break;
        }

        
        TriggerVec3 bonePosVec3 = { bonePos.x, bonePos.y, bonePos.z };

        
        if (usePrediction) {
            extern float g_ProjectileSpeed;
            TriggerVec3 velocityVec3 = { entity.velocity.x, entity.velocity.y, entity.velocity.z };
            bonePosVec3 = CalculatePredictedPosition(bonePosVec3, velocityVec3, g_ProjectileSpeed);
        }

        
        TriggerVec3 boxDimensions = GetBoxDimensionsForBone(boneIndex);

        
        std::vector<TriggerVec3> corners = CalculateBoxCorners(bonePosVec3, boxDimensions);

        extern Matrix g_ViewMatrix;
        extern Vector2 g_ScreenSize;

        
        float minX = FLT_MAX, maxX = -FLT_MAX;
        float minY = FLT_MAX, maxY = -FLT_MAX;
        bool anyPointVisible = false;

        for (const auto& c : corners) {
            Vector3 worldCorner = { c.x, c.y, c.z };
            Vector2 screenPos = WorldToScreen(worldCorner, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
            if (screenPos.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                anyPointVisible = true;
                if (screenPos.x < minX) minX = screenPos.x;
                if (screenPos.x > maxX) maxX = screenPos.x;
                if (screenPos.y < minY) minY = screenPos.y;
                if (screenPos.y > maxY) maxY = screenPos.y;
            }
        }

        if (!anyPointVisible) return false;

        
        float distanceMeters = entity.distance / 39.37f;
        if (distanceMeters <= closeRangeThreshold && closeRangeThreshold > 0.0f) {
            float expansionFactor = 1.0f + (closeRangeThreshold - distanceMeters) / closeRangeThreshold;
            float width = maxX - minX;
            float height = maxY - minY;
            float centerX = (minX + maxX) * 0.5f;
            float centerY = (minY + maxY) * 0.5f;

            minX = centerX - width * 0.5f * expansionFactor;
            maxX = centerX + width * 0.5f * expansionFactor;
            minY = centerY - height * 0.5f * expansionFactor;
            maxY = centerY + height * 0.5f * expansionFactor;
        }

        
        float screenCenterX = g_ScreenSize.x * 0.5f;
        float screenCenterY = g_ScreenSize.y * 0.5f;

        return (screenCenterX >= minX && screenCenterX <= maxX &&
                screenCenterY >= minY && screenCenterY <= maxY);
        
    } catch (...) {
        return false;
    }
}

bool TriggerBot::ShouldTrigger() {
    try {
        
        extern std::vector<Entity> g_Entities;
        extern std::mutex g_EntityMutex;
        extern Vector3 g_LocalPos;
        extern int g_LocalTeam;
        
        
        static DWORD lastCheckTime = 0;
        DWORD now = GetTickCount();
        if (now - lastCheckTime < 16) { 
            return false;
        }
        lastCheckTime = now;
        
        std::vector<Entity> entities;
        {
            std::lock_guard<std::mutex> lock(g_EntityMutex);
            if (g_Entities.empty()) return false;
            entities = g_Entities; 
        }
        
        std::vector<int> bones = GetActiveBones();
        
        static float maxTriggerDistanceMeters = 200.0f;
        
        for (const auto& entity : entities) {
            
            if (!entity.valid) continue;
            if (entity.address == 0) continue;
            if (entity.health <= 0) continue;
            if (entity.isKnocked) continue;
            
            if (!entity.isVisible) continue;
            
            
            if (entity.team == g_LocalTeam && !entity.isDummy) continue;
            
            
            float distMeters = entity.distance / 39.37f;
            if (distMeters > maxTriggerDistanceMeters) continue;
            
            
            for (int boneIndex : bones) {
                
                Entity& entityRef = const_cast<Entity&>(entity);
                if (CheckBoneOnCrosshair(entityRef, boneIndex)) {
                    return true;
                }
            }
        }
    } catch (...) {
        
    }
    
    return false;
}

void TriggerBot::Update(uintptr_t localPlayerPtr, uintptr_t gameBase) {
    try {
        if (!enabled || !driver) return;
        
        
        if (triggerKey != 0 && !(GetAsyncKeyState(triggerKey) & 0x8000)) {
            return;
        }
        
        DWORD currentTime = GetTickCount64();

        
        static const DWORD globalMinCooldownMs = 120; 

        
        int randomDelay = GetRandomDelay();

        
        DWORD requiredDelay = (DWORD)randomDelay;
        if (requiredDelay < globalMinCooldownMs) {
            requiredDelay = globalMinCooldownMs;
        }

        if (currentTime - lastTriggerTime < requiredDelay) return;
        
        
        
        
        if (ShouldTrigger()) {
            
            if (driver->InjectMouseClick()) {
                lastTriggerTime = currentTime;
            }
        }
    } catch (...) {
        
    };
}

void TriggerBot::RenderMenu() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::Checkbox("Enable Triggerbot", &enabled);
    ImGui::PopStyleVar();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    
    ImGui::Text("Trigger Key:");
    ImGui::SameLine();
    
    char keyName[32];
    if (isSelectingKey) {
        strcpy_s(keyName, "Press any key...");
    } else {
        if (triggerKey == 0) {
            strcpy_s(keyName, "None");
        } else {
            
            switch (triggerKey) {
                case VK_LBUTTON: strcpy_s(keyName, "Left Mouse"); break;
                case VK_RBUTTON: strcpy_s(keyName, "Right Mouse"); break;
                case VK_MBUTTON: strcpy_s(keyName, "Middle Mouse"); break;
                case VK_XBUTTON1: strcpy_s(keyName, "Mouse 4"); break;
                case VK_XBUTTON2: strcpy_s(keyName, "Mouse 5"); break;
                case VK_SHIFT: strcpy_s(keyName, "Shift"); break;
                case VK_CONTROL: strcpy_s(keyName, "Control"); break;
                case VK_MENU: strcpy_s(keyName, "Alt"); break;
                default: sprintf_s(keyName, "Key %d", triggerKey); break;
            }
        }
    }
    
    if (ImGui::Button(keyName, ImVec2(120, 20))) {
        isSelectingKey = true;
    }
    
    if (isSelectingKey) {
        
        for (int i = 3; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                
                if (i == VK_LBUTTON) continue; 
                
                triggerKey = i;
                isSelectingKey = false;
                break;
            }
        }
        
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            isSelectingKey = false;
        }
    }
    
    ImGui::Spacing();
    
    
    ImGui::Text("Delay Level:");
    const char* delayLevels[] = {
        "Level 1 (200-300ms - Very Safe)",
        "Level 2 (100-200ms - Safe)",
        "Level 3 (50-100ms - Balanced)",
        "Level 4 (25-50ms - Aggressive)",
        "Level 5 (10-25ms - Very Aggressive)"
    };
    int currentLevel = static_cast<int>(delayLevel);
    if (ImGui::Combo("##DelayLevel", &currentLevel, delayLevels, 5)) {
        delayLevel = static_cast<TriggerDelayLevel>(currentLevel);
    }
    
    ImGui::Spacing();
    
    
    ImGui::Text("Target Bones:");
    const char* boneModes[] = {
        "Body Only (Chest + Pelvis)",
        "Head Only",
        "Head + Body (Priority Head)",
        "Full Body (All Bones)"
    };
    int currentMode = static_cast<int>(boneMode);
    if (ImGui::Combo("##BoneMode", &currentMode, boneModes, 4)) {
        boneMode = static_cast<TriggerBoneMode>(currentMode);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    
    ImGui::Text("Visual Settings:");
    ImGui::Checkbox("Show Hitboxes", &showHitboxes);
    ImGui::Checkbox("Use Prediction", &usePrediction);

    ImGui::Spacing();

    
    static float maxTriggerDistanceMeters = 200.0f;
    ImGui::Text("Max Trigger Distance (m):");
    ImGui::SliderFloat("##MaxTriggerDist", &maxTriggerDistanceMeters, 10.0f, 250.0f, "%.0fm");
}

void TriggerBot::RenderHitboxes() {
    if (!showHitboxes || !enabled) return;
    
    
    extern std::vector<Entity> g_Entities;
    extern std::mutex g_EntityMutex;
    extern int g_LocalTeam;
    extern Matrix g_ViewMatrix;
    extern Vector2 g_ScreenSize;
    
    std::vector<Entity> entities;
    {
        std::lock_guard<std::mutex> lock(g_EntityMutex);
        entities = g_Entities;
    }
    
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    std::vector<int> bones = GetActiveBones();
    
    for (auto& entity : entities) {
        
        if (entity.team == g_LocalTeam) continue;
        if (!entity.valid) continue;
        
        for (int boneIndex : bones) {
            
            
            Vector3 bonePos = entity.position;

            switch (boneIndex) {
                case 0: 
                    bonePos.z += 60.0f;
                    break;
                case 1: 
                    bonePos.z += 50.0f;
                    break;
                case 2: 
                    bonePos.z += 35.0f;
                    break;
                case 3: 
                    bonePos.z += 10.0f;
                    break;
                default:
                    bonePos.z += 35.0f;
                    break;
            }

            TriggerVec3 bonePosVec3 = { bonePos.x, bonePos.y, bonePos.z };
            TriggerVec3 boxDimensions = GetBoxDimensionsForBone(boneIndex);
            boxDimensions.x *= (1.0f + extraPadding);
            boxDimensions.y *= (1.0f + extraPadding);
            boxDimensions.z *= (1.0f + extraPadding);
            
            std::vector<TriggerVec3> corners = CalculateBoxCorners(bonePosVec3, boxDimensions);
            
            
            ImU32 color = CheckBoneOnCrosshair(entity, boneIndex) ? 
                         IM_COL32(255, 0, 0, 180) : IM_COL32(0, 255, 255, 180);
            
            for (size_t i = 0; i < 4; i++) {
                
                Vector3 c1 = { corners[i].x, corners[i].y, corners[i].z };
                Vector3 c2 = { corners[(i + 1) % 4].x, corners[(i + 1) % 4].y, corners[(i + 1) % 4].z };
                Vector3 c3 = { corners[i + 4].x, corners[i + 4].y, corners[i + 4].z };
                Vector3 c4 = { corners[((i + 1) % 4) + 4].x, corners[((i + 1) % 4) + 4].y, corners[((i + 1) % 4) + 4].z };
                
                Vector2 p1, p2;
                
                p1 = WorldToScreen(c1, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c2, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
                
                p1 = WorldToScreen(c3, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c4, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
                
                Vector3 c5 = { corners[i].x, corners[i].y, corners[i].z };
                Vector3 c6 = { corners[i + 4].x, corners[i + 4].y, corners[i + 4].z };
                p1 = WorldToScreen(c5, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                p2 = WorldToScreen(c6, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                if (p1.IsValid(g_ScreenSize.x, g_ScreenSize.y) && p2.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    drawList->AddLine(ImVec2(p1.x, p1.y), ImVec2(p2.x, p2.y), color, 1.0f);
                }
            }
        }
    }
}
