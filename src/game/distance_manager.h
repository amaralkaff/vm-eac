#pragma once
#include "feature_manager.h"


class DistanceManager : public IFeature {
private:
    float maxDistance = 500.0f; 
    
public:
    DistanceManager() = default;
    
    
    const char* GetName() const override { return "Distance"; }
    bool IsEnabled() const override { return true; } 
    void SetEnabled(bool enabled) override { }
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override {
        
    }
    
    
    void CycleDistance() {
        if (maxDistance == 250.0f) maxDistance = 500.0f;
        else if (maxDistance == 500.0f) maxDistance = 1000.0f;
        else maxDistance = 250.0f;
    }
    
    
    void SetMaxDistance(float distance) {
        if (distance >= 100.0f && distance <= 1000.0f) {
            maxDistance = distance;
        }
    }
    
    
    float GetMaxDistance() const {
        return maxDistance;
    }
    
    
    const char* GetDistanceText() const {
        static char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.0fm", maxDistance);
        return buffer;
    }
};
