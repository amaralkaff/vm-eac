#pragma once
#include <Windows.h>
#include <map>
#include <string>
#include <chrono>


class IFeature {
public:
    virtual ~IFeature() = default;
    virtual void Update(uintptr_t localPlayerPtr, uintptr_t gameBase) = 0;
    virtual bool IsEnabled() const = 0;
    virtual void SetEnabled(bool enabled) = 0;
    virtual const char* GetName() const = 0;
};


class FeatureManager {
private:
    std::map<int, IFeature*> featureBindings;  
    std::map<std::string, IFeature*> features;  
    std::map<int, int> hotkeyIdMap;            
    
    HWND targetWindow = nullptr;
    int nextHotkeyId = 1;
    
public:
    FeatureManager() = default;
    ~FeatureManager() {
        
        if (targetWindow) {
            for (auto& [vkKey, hotkeyId] : hotkeyIdMap) {
                UnregisterHotKey(targetWindow, hotkeyId);
            }
        }
        
        
        features.clear();
        featureBindings.clear();
        hotkeyIdMap.clear();
    }
    
    
    void SetTargetWindow(HWND hwnd) {
        targetWindow = hwnd;
    }
    
    
    void RegisterFeature(const char* name, IFeature* feature, int vkKey) {
        features[name] = feature;
        
        
        if (targetWindow) {
            int hotkeyId = nextHotkeyId++;
            
            
            if (RegisterHotKey(targetWindow, hotkeyId, MOD_NOREPEAT, vkKey)) {
                featureBindings[hotkeyId] = feature;
                hotkeyIdMap[vkKey] = hotkeyId;
            }
        }
    }
    
    
    void ProcessHotkeyMessage(int hotkeyId) {
        auto it = featureBindings.find(hotkeyId);
        if (it != featureBindings.end()) {
            IFeature* feature = it->second;
            bool currentState = feature->IsEnabled();
            feature->SetEnabled(!currentState);
        }
    }
    
    
    void ProcessHotkeys() {
        
    }
    
    
    void UpdateAllFeatures(uintptr_t localPlayerPtr, uintptr_t gameBase) {
        for (auto& [name, feature] : features) {
            if (feature->IsEnabled()) {
                feature->Update(localPlayerPtr, gameBase);
            }
        }
    }
    
    
    IFeature* GetFeature(const char* name) {
        auto it = features.find(name);
        return (it != features.end()) ? it->second : nullptr;
    }
    
    
    const std::map<std::string, IFeature*>& GetAllFeatures() const {
        return features;
    }
};
