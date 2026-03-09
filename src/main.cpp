#include <Windows.h>
#include <winternl.h>
#include <vector>
#include <thread>
#include <mutex>
#include <time.h>
#include <fstream>
#include <conio.h>
#include <stdarg.h>
#include "driver/hv_interface.h"
#include "memory/memory_reader.h"
#include "memory/offsets.h"
#include "game/entity.h"
#include "game/math.h"
#include "OS-ImGui/OS-ImGui.h"
#include "overlay/gamebar_hijack.h"
#include "security/polym.h"
#include "security/protec.h"

#include "game/feature_manager.h"
#include "game/triggerbot.h"
#include "game/esp_feature.h"
#include "game/distance_manager.h"
#include "game/aimbot.h"
#include <random>

// Stubs for removed auth/license code
namespace RuntimeSync { inline bool UpdateRuntimeState() { return true; } inline bool VerifyHeader() { return true; } }
namespace LzStage { struct LicenseResponse {}; inline bool StageCheck(LicenseResponse&) { return true; } }

DriverInterfaceV3* g_Driver = nullptr;
Overlay* g_Overlay = nullptr;
DWORD g_ApexPid = 0;
uintptr_t g_ApexBase = 0;

FeatureManager* g_FeatureManager = nullptr;

Aimbot* g_Aimbot = nullptr;
TriggerBot* g_TriggerBot = nullptr;
ESPFeature* g_ESP = nullptr;
DistanceManager* g_DistanceManager = nullptr;

bool g_IsInFiringRange = false;

std::vector<Entity> g_Entities;
std::mutex g_EntityMutex;
Vector3 g_LocalPos;
uintptr_t g_LocalPlayerAddress = 0;
int g_LocalTeam = 0;
Matrix g_ViewMatrix;
bool g_IsRunning = true;

Vector3 g_PunchAngles;

float g_ClosestEnemyDistance = 999999.0f;
int g_AdaptiveSleepTime = 33;

float g_ProjectileSpeed = 30000.0f;
float g_ProjectileScale = 1.0f;

Vector2 g_ScreenSize = { 1920, 1080 };

bool g_ShowMenu = false;

bool g_TestMouseEnabled = false;
std::atomic_bool g_TestMouseRunning(false);
std::thread g_TestMouseThread;

int g_TargetFPS = 30;
int g_SleepTime = 33;   




void SecurityWatchdog() {
    
    while (g_IsRunning) {
        Sleep(5000);
    }
}


void ProcessSettingsHotkeys() {
    
}

bool InitializeDriver() {
    g_Driver = new DriverInterfaceV3();
    if (!g_Driver->Initialize()) {
        delete g_Driver;
        g_Driver = nullptr;
        return false;
    }
    
    
    
    g_Driver->SetMouseSignature(0, 0);
    
    return true;
}

bool WaitForApex() {
    const char* dots[] = { ".", "..", "...", ".", "..", "..." };
    int dotIndex = 0;
    
    const wchar_t* processName = L"r5apex_dx12.exe";
    
    while (true) {
        g_ApexPid = g_Driver->GetProcessId(processName);
        if (g_ApexPid) break;
        
        
        printf("\r  [*] Waiting for Apex%s   ", dots[dotIndex]);
        fflush(stdout);
        dotIndex = (dotIndex + 1) % 6;
        
        Sleep(500);
    }
    printf("\r   Apex Found! PID: %d              \n", g_ApexPid);
    
    g_Driver->SetCurrentPid(g_ApexPid);
    return true;
}

bool GetModuleBase() {
    const wchar_t* moduleName = L"r5apex_dx12.exe";
    
    g_ApexBase = g_Driver->GetModuleBase(g_ApexPid, moduleName);
    
    if (!g_ApexBase || g_ApexBase < 0x10000) {
        return false;
    }
    
    g_Driver->GetProcessCr3(g_ApexPid);
    return true;
}

bool InitializeFeatures() {
    g_FeatureManager = new FeatureManager();
    
    g_Overlay = new Overlay();

    g_ESP = new ESPFeature();
    g_FeatureManager->RegisterFeature("f1", g_ESP, 0);
    
    g_DistanceManager = new DistanceManager();
    g_FeatureManager->RegisterFeature("f2", g_DistanceManager, 0);
    
    g_TriggerBot = new TriggerBot(g_Driver, g_Overlay);
    g_FeatureManager->RegisterFeature("f3", g_TriggerBot, 0);

    g_Aimbot = new Aimbot(g_Driver);
    g_FeatureManager->RegisterFeature("f4", g_Aimbot, 0);
    
    return true;
}

Vector3 GetLocalPlayerPosition() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return Vector3();
    return Read<Vector3>(localPlayer + OFF_LOCAL_ORIGIN);
}

int GetLocalPlayerTeam() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return 0;
    return Read<int>(localPlayer + OFF_TEAM_NUMBER);
}

void UpdateWeaponInfo() {
    uintptr_t localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
    if (!localPlayer) return;
    
    
    uintptr_t weaponHandle = Read<uintptr_t>(localPlayer + OFF_WEAPON_HANDLE);
    if (!weaponHandle || weaponHandle == 0xFFFFFFFF) {
        g_ProjectileSpeed = 30000.0f; 
        g_ProjectileScale = 1.0f;
        return;
    }
    
    
    uintptr_t weaponEntity = Read<uintptr_t>(g_ApexBase + OFF_ENTITY_LIST + ((weaponHandle & 0xFFFF) << 5));
    if (!weaponEntity) {
        g_ProjectileSpeed = 30000.0f;
        g_ProjectileScale = 1.0f;
        return;
    }
    
    
    g_ProjectileSpeed = Read<float>(weaponEntity + OFF_PROJECTILESPEED);
    g_ProjectileScale = Read<float>(weaponEntity + OFF_PROJECTILESCALE);
    
    
    if (g_ProjectileSpeed <= 0 || g_ProjectileSpeed > 100000.0f) {
        g_ProjectileSpeed = 30000.0f;
    }
    if (g_ProjectileScale <= 0 || g_ProjectileScale > 10.0f) {
        g_ProjectileScale = 1.0f;
    }
}

Matrix GetViewMatrix() {
    uintptr_t viewRender = Read<uintptr_t>(g_ApexBase + OFF_VIEWRENDER);
    if (!viewRender) return Matrix();
    
    
    uintptr_t viewMatrixPtr = Read<uintptr_t>(viewRender + OFF_VIEWMATRIX);
    if (!viewMatrixPtr) return Matrix();
    
    Matrix matrix;
    ReadArray<float>(viewMatrixPtr, matrix.matrix, 16);
    return matrix;
}


void MemoryLoop() {
    
    
    
    
    int loopCounter = 0;
    
    while (g_IsRunning) {
        
        
        if (++loopCounter >= 1000) {
            loopCounter = 0;
            if (!RuntimeSync::UpdateRuntimeState()) {
                
                g_IsRunning = false;
                ExitProcess(0);
            }
        }
        
        if (g_ApexBase != 0) {
            uintptr_t localPlayer = 0;
            
            try {
                
                localPlayer = Read<uintptr_t>(g_ApexBase + OFF_LOCAL_PLAYER);
                
                
                if (!localPlayer || localPlayer < 0x10000 || localPlayer > 0x7FFFFFFFFFFF) {
                    Sleep(g_AdaptiveSleepTime);
                    continue;
                }
                
                
                Vector3 localPos = GetLocalPlayerPosition();
                if (localPos.x == 0 && localPos.y == 0 && localPos.z == 0) {
                    Sleep(g_AdaptiveSleepTime);
                    continue; 
                }
                
                int localTeam = GetLocalPlayerTeam();
                if (localTeam < 0 || localTeam > 50) {
                    Sleep(g_AdaptiveSleepTime);
                    continue; 
                }
                
                Matrix viewMatrix = GetViewMatrix();

                
                Vector3 punch = Read<Vector3>(localPlayer + OFF_PUNCH_ANGLES);
                
                
                UpdateWeaponInfo();
                
                
                auto entities = EntityScanner::ScanEntities(64);
                
                
                
                float closestDist = 999999.0f;
                for (auto& ent : entities) {
                    if (ent.valid && ent.team != localTeam) {
                        ent.CalculateDistance(localPos);
                        if (ent.distance < closestDist) {
                            closestDist = ent.distance;
                        }
                    }
                }
                
                
                float closestMeters = closestDist / 39.37f;
                g_ClosestEnemyDistance = closestMeters;
                
                
                
                
                
                
                if (closestMeters < 50.0f) {
                    g_AdaptiveSleepTime = 10;  
                } else if (closestMeters < 100.0f) {
                    g_AdaptiveSleepTime = 16;  
                } else if (closestMeters < 200.0f) {
                    g_AdaptiveSleepTime = 22;  
                } else {
                    g_AdaptiveSleepTime = 33;  
                }
                
                
                {
                    std::lock_guard<std::mutex> lock(g_EntityMutex);
                    g_LocalPos = localPos;
                    g_LocalPlayerAddress = localPlayer;
                    g_LocalTeam = localTeam;
                    g_ViewMatrix = viewMatrix;
                    g_PunchAngles = punch;
                    g_Entities = entities;
                }
                
                
                if (g_FeatureManager && localPlayer) {
                    g_FeatureManager->UpdateAllFeatures(localPlayer, g_ApexBase);
                }
            } catch (...) {
                
                
            }
        }
        
        
        TimeHelper::SleepFor(g_AdaptiveSleepTime, 5);
    }
}


void RenderMenu() {
    if (!g_ShowMenu) return;
    
    
    static int activeTab = 0;
    
    ImGuiStyle* style = &ImGui::GetStyle();
    
    
    ImVec4 originalSeparator = style->Colors[ImGuiCol_Separator];
    ImVec4 originalBorder = style->Colors[ImGuiCol_Border];
    ImVec2 originalItemSpacing = style->ItemSpacing;
    ImVec2 originalFramePadding = style->FramePadding;
    float originalFrameBorderSize = style->FrameBorderSize;
    
    
    style->WindowPadding = ImVec2(6, 6);
    style->Colors[ImGuiCol_Separator] = ImVec4(0, 0, 0, 0); 
    style->Colors[ImGuiCol_Border] = ImVec4(0, 0, 0, 0);    
    style->FrameBorderSize = 0.0f;                           
    
    
    ImVec4 bg_color = ImVec4(10.0f / 255.0f, 10.0f / 255.0f, 10.0f / 255.0f, 1.00f);
    ImVec4 button_color = ImVec4(12.f/255.f, 12.f/255.f, 12.f/255.f, 1.0f);
    ImVec4 button_hover = ImVec4(22.f/255.f, 22.f/255.f, 22.f/255.f, 1.0f);
    ImVec4 button_active = ImVec4(20.f/255.f, 20.f/255.f, 20.f/255.f, 1.0f);
    
        
        ImGui::SetNextWindowSize(ImVec2(580.f, 480.f));
        if (ImGui::Begin("##m", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoTitleBar)) {        ImGui::BeginChild("Complete Border", ImVec2(568.f, 468.f), false);
        {
            ImVec2 window_pos = ImGui::GetWindowPos();
            ImVec2 window_size = ImGui::GetWindowSize();
            ImVec2 image_size = ImVec2(568.f, 468.f);
            ImVec2 image_pos;
            image_pos.x = window_pos.x + (window_size.x - image_size.x) * 0.5f;
            image_pos.y = window_pos.y + (window_size.y - image_size.y) * 0.5f;
            
            
            ImGui::GetWindowDrawList()->AddRectFilled(image_pos, 
                ImVec2(image_pos.x + image_size.x, image_pos.y + image_size.y), 
                IM_COL32(10, 10, 10, 255));
        }
        ImGui::EndChild();
        
        ImGui::SameLine(6.f);
        
        style->Colors[ImGuiCol_ChildBg] = ImColor(0, 0, 0, 0);
        style->ItemSpacing = ImVec2(0.f, 0.f);

        ImGui::BeginChild("Menu", ImVec2(568.f, 468.f), false);
        {
            ImGui::Columns(2, nullptr, false); 
            ImGui::SetColumnWidth(0, 64.f);
            style->ItemSpacing = ImVec2(0.f, -1.f);
            
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 size = ImVec2(64, 468.f);
            
            
            ImU32 color = IM_COL32(12, 12, 12, 255);
            ImVec2 shifted_pos = ImVec2(pos.x, pos.y + 1);
            draw_list->AddRectFilled(shifted_pos, ImVec2(shifted_pos.x + size.x, shifted_pos.y + size.y), color);
            
            ImVec2 cursor_pos = ImGui::GetCursorPos();
            
            
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f); 
            ImGui::PushStyleColor(ImGuiCol_Button, button_color);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, button_hover);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, button_active);
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0)); 
            
            
            if (ImGui::GetIO().Fonts->Fonts.Size > 2) {
                ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[2]);
            }
            
            
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 2;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("V", ImVec2(64.f, 56.f))) activeTab = 0;
            }
            
            
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 60;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("M", ImVec2(64.f, 56.f))) activeTab = 1;
            }
            
            
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 120;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("A", ImVec2(64.f, 56.f))) activeTab = 2;
            }

            
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 180;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("T", ImVec2(64.f, 56.f))) activeTab = 3;
            }
            
            
            {
                ImVec2 btn_pos = cursor_pos;
                btn_pos.y += 240;
                btn_pos.x -= 2;
                ImGui::SetCursorPos(btn_pos);
                if (ImGui::Button("S", ImVec2(64.f, 56.f))) activeTab = 4;
            }
            
            if (ImGui::GetIO().Fonts->Fonts.Size > 2) {
                ImGui::PopFont();
            }
            ImGui::PopStyleColor(4); 
            ImGui::PopStyleVar(2);    
            
            ImGui::NextColumn();
            
            
            ImGui::BeginChild("MainContent");
            {
                style->ItemSpacing = ImVec2(6, 4); 
                style->FramePadding = ImVec2(6, 3);
                
                if (activeTab == 0) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Text("VISUALS");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    
                    if (g_ESP) {
                        bool espEnabled = g_ESP->IsEnabled();
                        ImGui::Text("ESP Status");
                        if (ImGui::Checkbox("##espenable", &espEnabled)) {
                            g_ESP->SetEnabled(espEnabled);
                        }
                        ImGui::SameLine();
                        ImGui::Text(espEnabled ? "Enabled" : "Disabled");
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    
                    ImGui::Text("Max Distance");
                    static int maxDistance = 500;
                    if (g_DistanceManager) {
                        maxDistance = (int)g_DistanceManager->GetMaxDistance();
                    }
                    if (ImGui::SliderInt("##maxdist", &maxDistance, 100, 1000, "%dm")) {
                        if (g_DistanceManager) {
                            g_DistanceManager->SetMaxDistance((float)maxDistance);
                        }
                    }
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    
                    ImGui::Text("ESP Features");
                    ImGui::Spacing();
                    
                    if (g_ESP) {
                        ImGui::Checkbox("Box ESP", &g_ESP->config.showBoxes);
                        ImGui::Checkbox("Health Bars", &g_ESP->config.showHealthBars);
                        ImGui::Checkbox("Shield Bars", &g_ESP->config.showShieldBars);
                        ImGui::Checkbox("Distance", &g_ESP->config.showDistances);
                        ImGui::Checkbox("Knocked State", &g_ESP->config.showKnocked);
                    }
                }
                else if (activeTab == 1) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Text("MISC");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Status");
                    ImGui::Spacing();
                    ImGui::BulletText("Driver Connected");
                    ImGui::BulletText("Game: %s", g_ApexBase ? "Apex Legends" : "Waiting...");

                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();

                    
                    ImGui::Text("Test Input");
                    ImGui::Text("ALT (hold) - Random Mouse Move");
                    ImGui::Checkbox("Enable ALT test", &g_TestMouseEnabled);
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Info");
                    ImGui::Spacing();
                    ImGui::BulletText("v10.0");
                    ImGui::BulletText("SP");
                    ImGui::BulletText("OK");
                }
                else if (activeTab == 2) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Text("AIMBOT");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    if (g_Aimbot) {
                        g_Aimbot->RenderMenu();
                    }
                }
                else if (activeTab == 3) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Text("TRIGGERBOT");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    if (g_TriggerBot) {
                        g_TriggerBot->RenderMenu();
                    }
                }
                else if (activeTab == 4) 
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
                    ImGui::Text("SETTINGS");
                    ImGui::PopStyleColor();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Performance");
                    ImGui::SliderInt("Base FPS", &g_TargetFPS, 30, 240);
                    
                    
                    ImGui::Text("Adaptive Timing:");
                    ImGui::BulletText("Closest Enemy: %.0fm", g_ClosestEnemyDistance);
                    ImGui::BulletText("Current Rate: %dms", g_AdaptiveSleepTime);
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                    
                    ImGui::Text("Overlay Info");
                    ImGui::Spacing();
                    ImGui::BulletText("FPS: %.1f", ImGui::GetIO().Framerate);
                    ImGui::BulletText("Hotkey: HOME");
                }
            }
            ImGui::EndChild();
            
            ImGui::Columns(1);
        }
        ImGui::EndChild();
    }
    ImGui::End();
    
    
    style->Colors[ImGuiCol_Separator] = originalSeparator;
    style->Colors[ImGuiCol_Border] = originalBorder;
    style->ItemSpacing = originalItemSpacing;
    style->FramePadding = originalFramePadding;
    style->FrameBorderSize = originalFrameBorderSize;
}


void RenderESP() {
    
    try {
        
        g_ScreenSize.x = ImGui::GetIO().DisplaySize.x;
        g_ScreenSize.y = ImGui::GetIO().DisplaySize.y;
        
        
        static bool homeKeyWasPressed = false;
        bool homeKeyIsPressed = (GetAsyncKeyState(VK_HOME) & 0x8000) != 0;
        
        if (homeKeyIsPressed && !homeKeyWasPressed) {
            g_ShowMenu = !g_ShowMenu;
        }
        homeKeyWasPressed = homeKeyIsPressed;

        
        if (g_TestMouseEnabled && g_Driver) {
            bool altDown = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0; 

            if (altDown && !g_TestMouseRunning.load(std::memory_order_relaxed)) {
                g_TestMouseRunning.store(true, std::memory_order_relaxed);
                if (!g_TestMouseThread.joinable()) {
                    g_TestMouseThread = std::thread([] {
                        g_Driver->TestRandomMouseMoveLoop(g_TestMouseRunning);
                    });
                }
            } else if (!altDown && g_TestMouseRunning.load(std::memory_order_relaxed)) {
                g_TestMouseRunning.store(false, std::memory_order_relaxed);
                if (g_TestMouseThread.joinable()) {
                    g_TestMouseThread.join();
                }
            }
        } else if (!g_TestMouseEnabled && g_TestMouseRunning.load(std::memory_order_relaxed)) {
            
            g_TestMouseRunning.store(false, std::memory_order_relaxed);
            if (g_TestMouseThread.joinable()) {
                g_TestMouseThread.join();
            }
        }
        
        
        RenderMenu();
        
        if (g_ApexBase == 0) return;
        
        
        std::vector<Entity> entities;
        Vector3 localPos;
        int localTeam;
        Matrix viewMatrix;
        
        {
            std::lock_guard<std::mutex> lock(g_EntityMutex);
            entities = g_Entities;
            localPos = g_LocalPos;
            localTeam = g_LocalTeam;
            viewMatrix = g_ViewMatrix;
        }
        
        
        if (localPos.x == 0 && localPos.y == 0 && localPos.z == 0) {
            return; 
        }
    
        
        if (g_ESP && g_ESP->IsEnabled()) {
            ImDrawList* drawList = ImGui::GetBackgroundDrawList();
            
            
            float maxDistanceMeters = g_DistanceManager ? g_DistanceManager->GetMaxDistance() : 500.0f;
            float maxDistanceInches = maxDistanceMeters * 39.37f; 
            
            for (auto& entity : entities) {
                if (!entity.IsValidForESP(localPos, localTeam, maxDistanceInches))
                    continue;
                
                
                Vector2 head = WorldToScreen(entity.position + Vector3(0, 0, 70), viewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                Vector2 feet = WorldToScreen(entity.position, viewMatrix, g_ScreenSize.x, g_ScreenSize.y);
                
                if (!head.IsValid(g_ScreenSize.x, g_ScreenSize.y) || !feet.IsValid(g_ScreenSize.x, g_ScreenSize.y)) {
                    
                    continue;
                }
                
                float height = feet.y - head.y;
                float width = height / 2.0f;
                float x = feet.x - width / 2.0f;
                float y = head.y;
                
                ImVec2 topLeft(x, y);
                ImVec2 bottomRight(x + width, y + height);
                
                
                ImU32 boxColor;
                if (entity.isDummy) {
                    boxColor = IM_COL32(255, 165, 0, 255); 
                } else if (entity.isKnocked) {
                    boxColor = IM_COL32(128, 128, 128, 255); 
                } else if (entity.isVisible) {
                    boxColor = IM_COL32(255, 255, 0, 255); 
                } else {
                    boxColor = IM_COL32(255, 0, 0, 255); 
                }
                
                
                if (g_ESP->config.showBoxes) {
                    
                    drawList->AddRect(topLeft, bottomRight, IM_COL32(0, 0, 0, 255), 0, 0, g_ESP->config.boxThickness + 1.0f);
                    drawList->AddRect(topLeft, bottomRight, boxColor, 0, 0, g_ESP->config.boxThickness);
                }
                
                
                if (g_ESP->config.showHealthBars || g_ESP->config.showShieldBars) {
                    g_ESP->DrawHealthAndShieldBar(drawList, topLeft, bottomRight, 
                        entity.health, entity.maxHealth, entity.shield, entity.maxShield, entity.armorType);
                }
                
                
                float distanceMeters = entity.distance / 39.37f; 
                g_ESP->DrawPlayerInfo(drawList, ImVec2(x, y - 16), entity, distanceMeters, false);
            }
        }

        
        if (g_Aimbot && g_Aimbot->IsEnabled()) {
            AimbotConfig& cfg = g_Aimbot->GetConfig();
            if (cfg.drawFovCircle && cfg.fovDegrees > 0.1f) {
                ImDrawList* drawList = ImGui::GetBackgroundDrawList();
                float screenCenterX = g_ScreenSize.x * 0.5f;
                float screenCenterY = g_ScreenSize.y * 0.5f;

                
                float fovRad = cfg.fovDegrees * 3.14159265f / 180.0f;
                float radius = std::tan(fovRad * 0.5f) * g_ScreenSize.y;
                radius = std::clamp(radius, 10.0f, g_ScreenSize.y * 0.5f);

                ImU32 col = IM_COL32(255, 255, 255, 60);
                drawList->AddCircle(ImVec2(screenCenterX, screenCenterY), radius, col, 64, 1.0f);
            }
        }
    
        
        if (g_TriggerBot && g_TriggerBot->IsEnabled()) {
            g_TriggerBot->RenderHitboxes();
        }

        
    
    } catch (...) {
        
        
    }
}

int RunClient();


namespace LzStage {
    struct LicenseResponse;
    bool StageCheck(LicenseResponse& outResponse);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    LzStage::LicenseResponse authResp{};
    if (!LzStage::StageCheck(authResp)) {
        return 0;
    }

    
    
    if (!RuntimeSync::UpdateRuntimeState()) {
        ExitProcess(0);
    }

    return RunClient();
}

int RunClient() {
    AllocConsole();
    FILE* fDummy;
    freopen_s(&fDummy, "CONOUT$", "w", stdout);
    freopen_s(&fDummy, "CONOUT$", "w", stderr);
    
    
    char _rT[16];
    srand((unsigned)time(NULL) ^ GetTickCount());
    for (int i = 0; i < 8; i++) _rT[i] = 'a' + (rand() % 26);
    _rT[8] = 0;
    SetConsoleTitleA(_rT);

    printf("\n");
    printf("  [*] Ready\n");
    printf("  [*] Key: HOME\n");
    printf("\n");

    if (!RuntimeSync::VerifyHeader()) {
        ExitProcess(0);
    }

    printf("  Driver init..\n");
    if (!InitializeDriver()) {
        printf("  Failed\n");
        _getch();
        return 1;
    }
    
    if (!WaitForApex()) {
        if (g_Driver) {
            g_Driver->Cleanup();
            delete g_Driver;
        }
        return 1;
    }
    
    
    bool baseFound = false;
    for (int i = 0; i < 10; i++) {
        if (GetModuleBase()) {
            baseFound = true;
            break;
        }
        Sleep(2000);
    }
    if (!baseFound) {
        printf("  Base not found\n");
        _getch();
        return 1;
    }

    
    if (!InitializeFeatures()) {
        printf("  Init fail\n");
        _getch();
        return 1;
    }
    
    
    std::thread memoryThread(MemoryLoop);
    memoryThread.detach();
    
    
    HWND apexWindow = NULL;
    const char* windowClass = "Respawn001";
    while (!apexWindow) {
        apexWindow = FindWindowA(windowClass, NULL);
        Sleep(500);
    }
    
    
    ShowWindow(GetConsoleWindow(), SW_HIDE);
    
    Sleep(1000);
    

    // Xbox Game Bar overlay hijack - renders in Game Bar's window band
    // Uses CreateWindowInBand to appear above fullscreen games
    GameBarOverlay overlay;
    overlay.Run( apexWindow, RenderESP );
    
    
    g_IsRunning = false;
    
    
    if (g_TriggerBot) { delete g_TriggerBot; g_TriggerBot = nullptr; }
    if (g_Aimbot) { delete g_Aimbot; g_Aimbot = nullptr; }
    if (g_ESP) { delete g_ESP; g_ESP = nullptr; }
    if (g_DistanceManager) { delete g_DistanceManager; g_DistanceManager = nullptr; }
    if (g_FeatureManager) { delete g_FeatureManager; g_FeatureManager = nullptr; }
    
    if (g_Driver) {
        g_Driver->Cleanup();
        delete g_Driver;
        g_Driver = nullptr;
    }
    
    return 0;
}
