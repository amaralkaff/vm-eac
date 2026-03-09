#pragma once
#include "../driver/hv_interface.h"
#include "entity.h"
#include "../overlay/overlay.h"
#include "feature_manager.h"
#include <random>
#include <vector>


enum class TriggerDelayLevel {
    LEVEL_1 = 0,  
    LEVEL_2 = 1,  
    LEVEL_3 = 2,  
    LEVEL_4 = 3,  
    LEVEL_5 = 4   
};


enum class TriggerBoneMode {
    BODY_ONLY = 0,     
    HEAD_ONLY = 1,     
    HEAD_BODY = 2,     
    FULL_BODY = 3      
};


struct TriggerVec3 {
    float x, y, z;
};

class TriggerBot : public IFeature {
private:
    bool enabled;
    DWORD lastTriggerTime;
    DriverInterfaceV3* driver;
    Overlay* overlay;
    
    
    TriggerDelayLevel delayLevel;
    TriggerBoneMode boneMode;
    bool showHitboxes;
    bool usePrediction;
    float extraPadding;
    float closeRangeThreshold;
    
    
    int triggerKey;
    bool isSelectingKey;
    
    
    std::random_device rd;
    std::mt19937 gen;
    
    
    int GetRandomDelay();
    
    
    std::vector<int> GetActiveBones();
    
    
    TriggerVec3 GetBoxDimensionsForBone(int boneIndex);
    std::vector<TriggerVec3> CalculateBoxCorners(const TriggerVec3& bonePos, const TriggerVec3& dimensions);
    
    
    TriggerVec3 CalculatePredictedPosition(const TriggerVec3& bonePos, const TriggerVec3& velocity, float projectileSpeed);
    
    
    bool ShouldTrigger();
    bool CheckBoneOnCrosshair(Entity& entity, int boneIndex);
    
public:
    TriggerBot(DriverInterfaceV3* drv, Overlay* ovl);
    ~TriggerBot();
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override;
    void RenderMenu();
    void RenderHitboxes();

    
    bool IsEnabled() const override { return enabled; }
    void SetEnabled(bool e) override { enabled = e; }
    const char* GetName() const override { return "TriggerBot"; }
};
