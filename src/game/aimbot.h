#pragma once
#include "../driver/hv_interface.h"
#include "entity.h"
#include "feature_manager.h"
#include "math.h"
#include <vector>


struct AimbotConfig {
    bool enabled = false;
    float fovDegrees = 5.0f;      
    float smooth = 0.5f;          
    float aggression = 1.0f;      
    int aimKey = 0x02; // default (right click)
    bool usePrediction = true;
    bool compensateRecoil = true; 
    float recoilStrength = 0.7f;  
    bool aggressiveWhenClose = true; 
    bool lockOnTarget = false;    
    bool drawFovCircle = true;
    int boneMode = 0;             
};

class Aimbot : public IFeature {
private:
    DriverInterfaceV3* driver;
    AimbotConfig config;

    
    uintptr_t currentTargetAddress = 0;

    
    bool IsHoldKeyActive() const;
    bool FindBestTarget(const std::vector<Entity>& entities, uintptr_t localPlayer, Entity& outTarget, Vector3& outTargetPos);
    Vector3 ApplyPrediction(const Vector3& targetPos, const Entity& target, const Vector3& localPos);
    Vector2 WorldToScreenSafe(const Vector3& worldPos) const;
    void MoveMouseTowards(const Vector2& targetScreenPos);

public:
    explicit Aimbot(DriverInterfaceV3* drv);
    ~Aimbot() = default;

    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override;
    bool IsEnabled() const override { return config.enabled; }
    void SetEnabled(bool e) override { config.enabled = e; }
    const char* GetName() const override { return "Aimbot"; }

    
    AimbotConfig& GetConfig() { return config; }

    
    void RenderMenu();
};
