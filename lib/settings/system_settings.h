/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include <stdint.h>
#include "lora_types.h"

#define SETTINGS_FLASH_SIZE 1024
#define SETTINGS_FLASH_ADDR 0x0800FC00
#define SETTINGS_END_MARKER 0x11AF44C2

using SettingsUpdatedCallback = void (*)(void*);

struct SettingsUpdateCallbackData
{
    SettingsUpdatedCallback callback = nullptr;
    void* userData = nullptr;
};

enum Mode : uint8_t
{
    BRIDGE_MODE,
    TEST_RS485_MODE,
    TEST_RF_MODE,
    NOICE_MEASUREMENT_MODE
};

struct Settings
{
    Mode workMode = BRIDGE_MODE;

    uint32_t rsBaudrate = 9600;
    uint8_t rsStopBits = 0;

    uint32_t loraFrequency = 435000000;
    uint16_t loraPreambleLen = 8;
    LoraBW loraBandwidth = BW_125_KHZ;
    LoraSF loraSpreadingFactor = SF_7;
    LoraCR loraCodingRate = CR_4_5;
    uint8_t loraTxPower = 14;
    uint8_t loraSyncWord = 0x12;
    uint32_t checkNoisePeriod = 500;
    uint32_t endMarker = SETTINGS_END_MARKER;
};

class SystemSettings
{
    static constexpr uint8_t MAX_CALLBACKS = 10;
public:
    static const Settings& getSettings();
    static void updateSettings(const Settings& settings, bool storeToFlash = false);
    static bool storeSettings();
    static bool registerCallback(SettingsUpdatedCallback callback, void* userData = nullptr);

private:
    static void loadSettings();
    static uint32_t findFreeSpaceOffset();
    static uint32_t findActualDataOffset();
    static bool eraseSettingsFlashPage();

private:
    static SettingsUpdateCallbackData s_callbacks[MAX_CALLBACKS];

    static Settings s_settings;
    static bool s_isLoaded;

};
