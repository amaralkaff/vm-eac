#pragma once
#include "feature_manager.h"


class NameToggleManager : public IFeature {
private:
    bool showNames = true; 
    
public:
    NameToggleManager() = default;
    
    
    const char* GetName() const override { return "Names"; }
    bool IsEnabled() const override { return showNames; }
    void SetEnabled(bool enabled) override { showNames = enabled; }
    
    void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) override {
        
    }
    
    
    void Toggle() {
        showNames = !showNames;
    }
    
    
    bool ShouldShowNames() const {
        return showNames;
    }
};
