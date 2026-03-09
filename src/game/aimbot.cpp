#include "aimbot.h"
#include "math.h"
#include "entity.h"
#include "../OS-ImGui/imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <mutex>
#include <random>

namespace RuntimeSync { inline bool VerifyHeader() { return true; } }

extern std::vector<Entity> g_Entities;
extern std::mutex g_EntityMutex;
extern Vector3 g_LocalPos;
extern int g_LocalTeam;
extern Matrix g_ViewMatrix;
extern Vector2 g_ScreenSize;
extern float g_ProjectileSpeed;
extern float g_ProjectileScale;
extern Vector3 g_PunchAngles; 

Aimbot::Aimbot(DriverInterfaceV3* drv)
    : driver(drv) {
}

bool Aimbot::IsHoldKeyActive() const {
    if (config.aimKey == 0) return false;
    return (GetAsyncKeyState(config.aimKey) & 0x8000) != 0;
}

Vector2 Aimbot::WorldToScreenSafe(const Vector3& worldPos) const {
    return WorldToScreen(worldPos, g_ViewMatrix, g_ScreenSize.x, g_ScreenSize.y);
}

Vector3 Aimbot::ApplyPrediction(const Vector3& targetPos, const Entity& target, const Vector3& localPos) {
    if (!config.usePrediction) {
        return targetPos;
    }

    float dx = targetPos.x - localPos.x;
    float dy = targetPos.y - localPos.y;
    float dz = targetPos.z - localPos.z;
    float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (distance < 600.0f || g_ProjectileSpeed <= 0.0f) {
        return targetPos;
    }

    float timeToHit = distance / g_ProjectileSpeed;
    if (timeToHit <= 0.0f || timeToHit > 0.8f) {
        return targetPos;
    }

    Vector3 predicted = {
        targetPos.x + target.velocity.x * timeToHit,
        targetPos.y + target.velocity.y * timeToHit,
        targetPos.z + target.velocity.z * timeToHit
    };

    float drop = 0.5f * g_ProjectileScale * timeToHit * timeToHit;
    predicted.z -= drop;
    return predicted;
}

bool Aimbot::FindBestTarget(const std::vector<Entity>& entities, uintptr_t localPlayer, Entity& outTarget, Vector3& outTargetPos) {
    if (!localPlayer) return false;

    float bestFov = config.fovDegrees;
    bool found = false;

    for (const auto& ent : entities) {
        if (!ent.valid) continue;
        if (ent.address == 0) continue;
        if (ent.health <= 0) continue;
        if (ent.isKnocked) continue;
        if (ent.team == g_LocalTeam && !ent.isDummy) continue;
        if (!ent.isVisible) continue;

        // Base bone depending on config
        Vector3 targetPos = ent.position;
        switch (config.boneMode) {
            case 1: 
                targetPos.z += 35.0f;
                break;
            case 2: 
                targetPos.z += 47.5f;
                break;
            case 0:
            default: 
                targetPos.z += 60.0f;
                break;
        }

        
        targetPos = ApplyPrediction(targetPos, ent, g_LocalPos);

        
        if (config.compensateRecoil) {
            float strength = std::clamp(config.recoilStrength, 0.0f, 1.0f);
            
            
            targetPos.z += g_PunchAngles.x * 0.3f * strength;
        }

        Vector2 screenPos = WorldToScreenSafe(targetPos);
        if (!screenPos.IsValid(g_ScreenSize.x, g_ScreenSize.y)) continue;

        
        float screenCenterX = g_ScreenSize.x * 0.5f;
        float screenCenterY = g_ScreenSize.y * 0.5f;
        float dx = screenPos.x - screenCenterX;
        float dy = screenPos.y - screenCenterY;

        float distPixels = std::sqrt(dx * dx + dy * dy);
        if (distPixels <= 0.0f) continue;

        
        float fov = std::atan2(distPixels, g_ScreenSize.y) * (180.0f / 3.14159265f) * 2.0f;
        if (fov < bestFov) {
            bestFov = fov;
            outTarget = const_cast<Entity&>(ent);
            outTargetPos = targetPos;
            found = true;
        }
    }

    return found;
}

void Aimbot::MoveMouseTowards(const Vector2& targetScreenPos) {
    if (!driver) return;

    static DWORD lastMoveTime = 0;
    DWORD now = GetTickCount();

    
    if (lastMoveTime != 0) {
        DWORD deltaMs = now - lastMoveTime;
        if (deltaMs < 5) {
            return;
        }
    }

    float screenCenterX = g_ScreenSize.x * 0.5f;
    float screenCenterY = g_ScreenSize.y * 0.5f;

    float dx = targetScreenPos.x - screenCenterX;
    float dy = targetScreenPos.y - screenCenterY;

    float distPixels = std::sqrt(dx * dx + dy * dy);
    if (distPixels <= 0.5f) {
        return;
    }

    
    
    
    float clampedSmooth = std::clamp(config.smooth, 0.0f, 1.0f);

    float adaptiveFactor = 1.0f;
    if (distPixels > 250.0f) {
        adaptiveFactor = 1.1f;
    } else if (distPixels < 80.0f && config.aggressiveWhenClose) {
        adaptiveFactor = 0.5f; 
    }

    float aggression = std::clamp(config.aggression, 0.5f, 2.0f);

    float smoothFactor = clampedSmooth * adaptiveFactor / aggression;
    smoothFactor = std::clamp(smoothFactor, 0.0f, 1.0f);

    if (smoothFactor > 0.0f) {
        dx *= (1.0f - smoothFactor);
        dy *= (1.0f - smoothFactor);
    }

    
    float maxStep = 12.0f * aggression;
    float stepLen = std::sqrt(dx * dx + dy * dy);
    if (stepLen > maxStep && stepLen > 0.0f) {
        float scale = maxStep / stepLen;
        dx *= scale;
        dy *= scale;
    }

    
    static std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> jitterDist(-0.15f, 0.15f);
    dx += jitterDist(rng);
    dy += jitterDist(rng);

    int moveX = static_cast<int>(dx);
    int moveY = static_cast<int>(dy);

    if (moveX == 0 && moveY == 0) return;

    driver->InjectMouseMove(moveX, moveY);
    lastMoveTime = now;
}

void Aimbot::Update(uintptr_t localPlayerPtr, uintptr_t gameBase) {
 
    if (!RuntimeSync::VerifyHeader()) {
        ExitProcess(0);
    }

    try {
        if (!config.enabled) return;
        if (!driver) return;
        if (!IsHoldKeyActive()) return;

        std::vector<Entity> entities;
        {
            std::lock_guard<std::mutex> lock(g_EntityMutex);
            entities = g_Entities;
        }

        if (entities.empty()) return;

        Entity bestTarget;
        Vector3 bestPos;

        
        if (config.lockOnTarget && currentTargetAddress != 0) {
            bool foundExisting = false;
            for (const auto& ent : entities) {
                if (!ent.valid || ent.address == 0) continue;
                if (ent.address != currentTargetAddress) continue;
                if (ent.health <= 0 || ent.isKnocked) continue;

                bestTarget = const_cast<Entity&>(ent);
                bestPos = ent.position;
                foundExisting = true;
                break;
            }

            if (!foundExisting) {
                currentTargetAddress = 0;
            }
        }

        if (currentTargetAddress == 0 || !config.lockOnTarget) {
            if (!FindBestTarget(entities, localPlayerPtr, bestTarget, bestPos)) return;
            currentTargetAddress = config.lockOnTarget ? bestTarget.address : 0;
        }

        Vector2 screenPos = WorldToScreenSafe(bestPos);
        if (!screenPos.IsValid(g_ScreenSize.x, g_ScreenSize.y)) return;

        MoveMouseTowards(screenPos);
    } catch (...) {
        
    }
}

void Aimbot::RenderMenu() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
    ImGui::Checkbox("Enable Aimbot", &config.enabled);
    ImGui::PopStyleVar();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("FOV (degrees)");
    ImGui::SliderFloat("##AimbotFOV", &config.fovDegrees, 1.0f, 20.0f, "%.1f");

    ImGui::Spacing();

    ImGui::Text("Smoothing");
    ImGui::SliderFloat("##AimbotSmooth", &config.smooth, 0.0f, 1.0f, "%.2f");

    ImGui::Spacing();

    ImGui::Text("Aggression");
    ImGui::SliderFloat("##AimbotAgg", &config.aggression, 0.5f, 2.0f, "%.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    
    ImGui::Text("Target Bone");
    const char* boneOptions[] = { "Head", "Chest", "Nearest" };
    ImGui::Combo("##AimbotBone", &config.boneMode, boneOptions, IM_ARRAYSIZE(boneOptions));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    
    static bool selectingKey = false;

    ImGui::Text("Aim Key (hold)");
    ImGui::SameLine();

    char keyName[32];
    if (selectingKey) {
        strcpy_s(keyName, "Press any key...");
    } else {
        if (config.aimKey == 0) {
            strcpy_s(keyName, "None");
        } else {
            switch (config.aimKey) {
                case VK_LBUTTON: strcpy_s(keyName, "Left Mouse"); break;
                case VK_RBUTTON: strcpy_s(keyName, "Right Mouse"); break;
                case VK_MBUTTON: strcpy_s(keyName, "Middle Mouse"); break;
                case VK_XBUTTON1: strcpy_s(keyName, "Mouse 4"); break;
                case VK_XBUTTON2: strcpy_s(keyName, "Mouse 5"); break;
                case VK_SHIFT: strcpy_s(keyName, "Shift"); break;
                case VK_CONTROL: strcpy_s(keyName, "Control"); break;
                case VK_MENU: strcpy_s(keyName, "Alt"); break;
                default: sprintf_s(keyName, "Key %d", config.aimKey); break;
            }
        }
    }

    if (ImGui::Button(keyName, ImVec2(140, 24))) {
        selectingKey = true;
    }

    if (selectingKey) {
        for (int i = 3; i < 255; i++) {
            if (GetAsyncKeyState(i) & 0x8000) {
                if (i == VK_LBUTTON) continue;
                config.aimKey = i;
                selectingKey = false;
                break;
            }
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            selectingKey = false;
        }
    }

    ImGui::Spacing();

    ImGui::Checkbox("Use Prediction", &config.usePrediction);
    ImGui::Checkbox("Draw FOV Circle", &config.drawFovCircle);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Recoil");
    ImGui::Checkbox("Enable Recoil Compensation", &config.compensateRecoil);
    ImGui::SliderFloat("##AimbotRecoilStr", &config.recoilStrength, 0.0f, 1.0f, "Strength %.2f");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Aggressive When Close", &config.aggressiveWhenClose);
    ImGui::Checkbox("Lock On Target", &config.lockOnTarget);
}
