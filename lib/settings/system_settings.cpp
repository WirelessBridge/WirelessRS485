/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "system_settings.h"

#include "cmsis_os.h"
#include "main.h"

#include "logger.h"

extern "C" {
extern osMutexId_t systemSettingsMutex;
}

SettingsUpdateCallbackData SystemSettings::s_callbacks[MAX_CALLBACKS] = {};
Settings SystemSettings::s_settings{};
bool SystemSettings::s_isLoaded = false;

const Settings& SystemSettings::getSettings()
{
    if (!s_isLoaded) {
        loadSettings();
    }
    return s_settings;
}

void SystemSettings::updateSettings(const Settings& settings, bool storeToFlash)
{
    osMutexAcquire(systemSettingsMutex, osWaitForever);
    s_settings = settings;
    osMutexRelease(systemSettingsMutex);
    if (storeToFlash) {
        storeSettings();
    }
    for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i].callback != nullptr) {
            s_callbacks[i].callback(s_callbacks[i].userData);
        }
    }
}

void SystemSettings::loadSettings()
{
    osMutexAcquire(systemSettingsMutex, osWaitForever);
    const uint32_t offset = findActualDataOffset();
    if (offset != 0xFFFFFFFF) {
        Settings* loadedSettings = reinterpret_cast<Settings*>(SETTINGS_FLASH_ADDR + offset);
        s_settings = *loadedSettings;
        LOG_INFO("<<< Loaded successfully! <<<\n");
    } else {
        LOG_WARNING("No stored settings found! Default settings will be stored and used!\n");
        eraseSettingsFlashPage();
        storeSettings();
    }
    s_isLoaded = true;
    osMutexRelease(systemSettingsMutex);
}

bool SystemSettings::storeSettings()
{
    osMutexAcquire(systemSettingsMutex, osWaitForever);
    uint32_t offset = findFreeSpaceOffset();
    if (offset + sizeof(Settings) > SETTINGS_FLASH_SIZE) {
        LOG_WARNING("Flash page is full and will be erased!\n");
        eraseSettingsFlashPage();
        offset = 0;
    }

    HAL_StatusTypeDef res = HAL_FLASH_Unlock();
    if (res != HAL_OK) {
        LOG_ERROR("Flash unlock error: %d\n", res);
        osMutexRelease(systemSettingsMutex);
        return false;
    }
    const uint32_t* data = reinterpret_cast<const uint32_t*>(&s_settings);
    const uint32_t startAddress = SETTINGS_FLASH_ADDR + offset;
    for (uint32_t i = 0; i < sizeof(Settings); i += 4) {
        res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, startAddress + i, *data++);
        if (res != HAL_OK) {
            LOG_ERROR("Failed to store data on address 0x%08X! Error code: %d\n", startAddress + i, res);
            osMutexRelease(systemSettingsMutex);
            return false;
        }
    }

    res = HAL_FLASH_Lock();
    if (res != HAL_OK) {
        LOG_ERROR("Flash lock error: %d\n", res);
    } else {
        LOG_INFO(">>> Stored successfully! >>>\n");
    }
    osMutexRelease(systemSettingsMutex);
    return res == HAL_OK;
}

bool SystemSettings::registerCallback(SettingsUpdatedCallback callback, void* userData)
{ 
    for (uint8_t i = 0; i < MAX_CALLBACKS; i++) {
        if (s_callbacks[i].callback == nullptr) {
            s_callbacks[i].callback = callback;
            s_callbacks[i].userData = userData;
            LOG_DEBUG_2("Callback registered successfully! Index: %d\n", i);
            return true;
        }
    }
    LOG_WARNING("Failed to register callback! Max callbacks count reached!\n");
    return false;
}

uint32_t SystemSettings::findFreeSpaceOffset()
{
    uint32_t result = SETTINGS_FLASH_SIZE;
    for (uint32_t offset = 0; offset < 1024; offset += sizeof(Settings)) {
        Settings* settings = reinterpret_cast<Settings*>(SETTINGS_FLASH_ADDR + offset);
        if (settings->endMarker == 0xFFFFFFFF) {
            result = offset;
            break;
        }
    }
    LOG_DEBUG_2("Free space offset: %d\n", result);
    return result;
}

uint32_t SystemSettings::findActualDataOffset()
{
    uint32_t result = 0xFFFFFFFF;
    for (uint32_t offset = 0; offset < 1024; offset += sizeof(Settings)) {
        Settings* settings = reinterpret_cast<Settings*>(SETTINGS_FLASH_ADDR + offset);
        if (settings->endMarker == SETTINGS_END_MARKER) {
            result = offset;
        } else {
            break;
        }
    }
    LOG_DEBUG_2("Actual data offset: %d\n", result);
    return result;
}

bool SystemSettings::eraseSettingsFlashPage()
{
    HAL_StatusTypeDef res = HAL_FLASH_Unlock();
    if (res != HAL_OK) {
        LOG_ERROR("Failed to unlock flash! Error code: %d\n", res);
        return false;
    }

    FLASH_EraseInitTypeDef eraseInit;
    eraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
    eraseInit.PageAddress = SETTINGS_FLASH_ADDR;
    eraseInit.NbPages = 1;
    uint32_t pageError = 0;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);

    res = HAL_FLASHEx_Erase(&eraseInit, &pageError);
    if (res != HAL_OK) {
        LOG_ERROR("Failed to erase flash! Error code: %d\n", res);
        HAL_FLASH_Lock();
        return false;
    }

    res = HAL_FLASH_Lock();
    if (res != HAL_OK) {
        LOG_ERROR("Failed to lock flash! Error code: %d\n", res);
        return false;
    }

    LOG_DEBUG_2("Flash page erased successfully!\n");
    return true;
}
