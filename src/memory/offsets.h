#pragma once
#include <cstdint>

// Updated from ap3xdump.txt - Patch 3/4/2026

#define OFF_LOCAL_PLAYER 0x282d298
#define OFF_ENTITY_LIST 0x644d138
#define OFF_NAME_LIST 0x8e47640
#define OFF_VIEWRENDER 0x3f372e0
#define OFF_VIEWMATRIX 0x11a350
#define OFF_LEVEL 0x1f7b9f4


#define OFF_HEALTH 0x0324
#define OFF_MAXHEALTH 0x0468
#define OFF_SHIELD 0x01a0
#define OFF_MAXSHIELD 0x01a4
#define OFF_ARMORTYPE 0x4814
#define OFF_LOCAL_ORIGIN 0x017c
#define OFF_TEAM_NUMBER 0x0334
#define OFF_LIFE_STATE 0x0690
#define OFF_BLEEDOUT_STATE 0x27c0
#define OFF_LAST_VISIBLE_TIME 0x1a64


#define OFF_SIGNIFIER_NAME 0x0470        // m_iSignifierName
#define OFF_NAME 0x0479                  // m_iName


#define OFF_BONES (0xda8 + 0x48)


#define OFF_ABSVELOCITY 0x0170           // Vec3
#define OFF_WEAPON_HANDLE 0x19c4
#define OFF_WEAPON_INDEX 0x15f0
#define OFF_PROJECTILESPEED 0x2820
#define OFF_PROJECTILESCALE (OFF_PROJECTILESPEED + 0x8)
#define OFF_LAST_AIMEDAT_TIME 0x1a6c


#define OFF_VIEW_ANGLES   (0x2614 - 0x14)
#define OFF_BREATH_ANGLES (OFF_VIEW_ANGLES - 0x10)
#define OFF_PUNCH_ANGLES  0x2518
#define OFF_YAW           (0x230c - 0x8)


#define OFF_INATTACK    0x3f39bd0        // [Buttons]->in_attack
#define OFF_IN_RELOAD   0x3f3a3c8        // [Buttons]->in_reload

#define OFF_TIME_BASE            0x2168  // timeBase
#define OFF_TRAVERSAL_START_TIME 0x2bd4  // m_traversalStartTime
#define OFF_TRAVERSAL_PROGRESS   0x2bcc  // m_traversalProgress
#define OFF_Wall_RunStart_TIME   0x2c14  // m_wallClimbSetUp
#define OFF_Wall_RunClear_TIME   0x2c15  // m_wallHanging
#define OFF_SET_SKYDRIVESTATE    0x4878  // m_skydiveState
#define OFF_SET_IN_DUCKSTATE     0x2abc  // m_duckState


#define OFF_FLAGS            0x00c8      // m_fFlags

