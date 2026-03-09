#pragma once
#include "math.h"
#include "../memory/offsets.h"
#include "../memory/memory_reader.h"
#include <string>
#include <cstring>
#include <vector>

extern uintptr_t g_ApexBase;
extern DWORD g_ApexPid;
extern DriverInterfaceV3* g_Driver;
extern float g_ClosestEnemyDistance;
extern int g_AdaptiveSleepTime;

extern bool g_IsInFiringRange;




struct Entity {
    
    int index;
    uintptr_t address;
    uint64_t valid;
    
    
    Vector3 position;
    Vector3 velocity;
    
    
    int health;
    int maxHealth;
    int shield;
    int maxShield;
    int armorType;
    int team;
    int bleedoutState;
    
    
    bool isVisible;
    bool isKnocked;
    bool isDummy;
    bool isPlayer;
    
    
    float distance;
    
    
    std::string signifierName;
    
    Entity() : index(-1), address(0), valid(0), position(), velocity(),
               health(0), maxHealth(0), shield(0), maxShield(0), armorType(0),
               team(0), bleedoutState(0), isVisible(false), isKnocked(false),
               isDummy(false), isPlayer(false), distance(0.0f) {}
    
    
    bool IsValid() const {
        return address != 0 &&
               address != 0xCCCCCCCCCCCCCCCC &&
               address != 0xCDCDCDCDCDCDCDCD &&
               address < 0x7FFFFFFFFFFF &&
               address > 0x1000;
    }
    
    
    bool IsDummy() const {
        return signifierName == "npc_dummie";
    }
    
    
    void UpdateFromBuffer(const uint8_t* buffer, size_t bufferSize) {
        valid = 0;
        if (!buffer || bufferSize < 0x700) return;
        
        int lifeState = *(int*)(buffer + OFF_LIFE_STATE);
        if (lifeState != 0) return;

        health = *(int*)(buffer + OFF_HEALTH);
        if (health <= 0 || health > 1000) return;

        team = *(int*)(buffer + OFF_TEAM_NUMBER);
        if (team < 0 || team > 100) return;

        position = *(Vector3*)(buffer + OFF_LOCAL_ORIGIN);
        if (position.x == 0 && position.y == 0 && position.z == 0) return;

        maxHealth = *(int*)(buffer + OFF_MAXHEALTH);
        shield = *(int*)(buffer + OFF_SHIELD);
        maxShield = *(int*)(buffer + OFF_MAXSHIELD);
        
        valid = 1;
    }
    
    
    void UpdateExtendedData() {
        if (!valid || !address || !g_Driver) return;
        
        g_Driver->ReadMemory(g_ApexPid, address + OFF_ARMORTYPE, &armorType, sizeof(int));
        
        if (g_Driver->ReadMemory(g_ApexPid, address + OFF_BLEEDOUT_STATE, &bleedoutState, sizeof(int))) {
            
            isKnocked = isDummy ? false : (bleedoutState > 0);
        }
        
        float lastVisibleTime = 0.f;
        if (g_Driver->ReadMemory(g_ApexPid, address + OFF_LAST_VISIBLE_TIME, &lastVisibleTime, sizeof(float))) {
            isVisible = (lastVisibleTime > 0);
        }
    }

    void CalculateDistance(const Vector3& localPos) {
        distance = position.Distance(localPos);
    }
    
    bool IsValidForESP(const Vector3& localPos, int localTeam, float maxDistance = 25000.0f) {
        if (!valid) return false;
        CalculateDistance(localPos);
        if (distance > maxDistance) return false;
        if (team == localTeam && !isDummy) return false;
        return true;
    }

    Vector3 GetBonePosition(int boneIndex) {
        if (!address || !g_Driver) return Vector3();
        
        uintptr_t boneArray = 0;
        if (!g_Driver->ReadMemory(g_ApexPid, address + OFF_BONES, &boneArray, sizeof(boneArray)))
            return Vector3();
        
        struct Matrix3x4 { float m[3][4]; };
        Matrix3x4 boneMatrix;
        if (!g_Driver->ReadMemory(g_ApexPid, boneArray + (boneIndex * sizeof(Matrix3x4)), &boneMatrix, sizeof(boneMatrix)))
            return Vector3();
        
        return Vector3(boneMatrix.m[0][3], boneMatrix.m[1][3], boneMatrix.m[2][3]);
    }
};




class EntityScanner {
private:
    
    static inline int s_dummyIndices[100];
    static inline int s_dummyCount = 0;
    static inline bool s_scanned = false;

public:
    
    static bool CheckFiringRange() { return false; }

    static void ClearAll() {
        s_dummyCount = 0;
        s_scanned = false;
    }

    
    static void DoFullDummyScan() { }

    static std::vector<Entity> ScanEntities(int maxEntities = 64) {
        std::vector<Entity> entities;
        
        if (!g_Driver || !g_ApexPid || !g_ApexBase) {
            return entities;
        }
        
        
        bool inFiringRange = false;
        g_IsInFiringRange = false;

        
        Vector3 localPos;
        int localTeam = 0;
        uintptr_t localPlayer = 0;

        g_Driver->ReadMemory(g_ApexPid, g_ApexBase + OFF_LOCAL_PLAYER, &localPlayer, sizeof(localPlayer));
        if (localPlayer > 0x10000 && localPlayer < 0x7FFFFFFFFFFF) {
            g_Driver->ReadMemory(g_ApexPid, localPlayer + OFF_LOCAL_ORIGIN, &localPos, sizeof(localPos));
            g_Driver->ReadMemory(g_ApexPid, localPlayer + OFF_TEAM_NUMBER, &localTeam, sizeof(localTeam));
        }

        uintptr_t entityListBase = g_ApexBase + OFF_ENTITY_LIST;

        
        for (int i = 1; i <= 70; ++i) {
            uintptr_t entityPtr = 0;
            if (!g_Driver->ReadMemory(g_ApexPid, entityListBase + (i << 5), &entityPtr, sizeof(entityPtr))) {
                continue;
            }

            if (entityPtr < 0x10000 || entityPtr > 0x7FFFFFFFFFFF) {
                continue;
            }

            
            char name[16] = { 0 };
            if (!g_Driver->ReadMemory(g_ApexPid, entityPtr + OFF_NAME, name, 15)) {
                continue;
            }
            
            if (name[0] == '\0') {
                continue;
            }

            uint8_t buffer[0x700];
            if (!g_Driver->ReadMemoryBulk(g_ApexPid, entityPtr, buffer, sizeof(buffer))) {
                continue;
            }

            Entity entity;
            entity.index = i;
            entity.address = entityPtr;
            entity.isPlayer = true;
            entity.isDummy = false;
            entity.UpdateFromBuffer(buffer, sizeof(buffer));

            if (entity.valid && entity.health > 0) {
                entity.CalculateDistance(localPos);
                entity.UpdateExtendedData();
                entities.push_back(entity);
            }
        }

        
        if (inFiringRange) {
            if (!s_scanned) {
                DoFullDummyScan();
            }

            for (int j = 0; j < s_dummyCount; ++j) {
                int i = s_dummyIndices[j];

                uintptr_t entityPtr = 0;
                if (!g_Driver->ReadMemory(g_ApexPid, entityListBase + (i << 5), &entityPtr, sizeof(entityPtr))) {
                    continue;
                }

                if (entityPtr < 0x10000 || entityPtr > 0x7FFFFFFFFFFF) {
                    continue;
                }

                uint8_t buffer[0x700];
                if (!g_Driver->ReadMemoryBulk(g_ApexPid, entityPtr, buffer, sizeof(buffer))) {
                    continue;
                }

                Entity entity;
                entity.index = i;
                entity.address = entityPtr;
                entity.isPlayer = false;
                entity.isDummy = true;
                entity.UpdateFromBuffer(buffer, sizeof(buffer));

                if (entity.valid && entity.health > 0) {
                    entity.CalculateDistance(localPos);
                    entity.UpdateExtendedData();
                    entities.push_back(entity);
                }
            }
        }

        return entities;
    }
    
    
    static std::vector<Entity> ScanEntitiesBatch(int maxEntities = 64) {
        return ScanEntities(maxEntities);
    }
    
    
    static size_t GetCachedDummyCount() {
        return s_dummyCount;
    }
    
    
    static void ForceRescan() {
        s_scanned = false;
        s_dummyCount = 0;
    }
};
