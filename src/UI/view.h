/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "system_settings.h"
#include <stddef.h>

class SSD1306;
class Gpio;
struct LoraLinkData;

class View
{
    using MenuItemHandler = void(*)(View*);

public:

    enum PressedBtn : uint8_t
    {
        BTN_NONE,
        BTN_MAIN,
        BTN_DOWN,
        BTN_UP
    };

private:
    static constexpr uint8_t LINE_BUFFER_SIZE = 22;
    static constexpr uint8_t LINE_TEXT_HEIGHT = 8;
    static constexpr uint8_t MAX_MENU_ITEMS_ON_PAGE = 3;

    template<typename T, size_t N>
    static constexpr size_t get_array_size(T (&)[N])
    {
        return N;
    }

    enum MenuItemType : uint8_t
    {
        SUB_MENU,
        STR_VALUE,
        INT_VALUE,
        INT_RANGE,
        HEX_RANGE,
        CALL_FUNCTION
    };

    enum Screen : uint8_t
    {
        MAIN_SCREEN,
        MENU_SCREEN,
        VALUE_CHANGE_SCREEN,
        SAVE_CHANGES_POPUP,
        SAVE_TO_FLASH_POPUP,
        WAIT_SCREEN
    };
    
    static constexpr const char* MODES[] = {"BRIDGE", "TEST-RS", "TEST-RF", "NOICE-FLOOR"};
    static constexpr uint32_t BAUD_RATES[] = {2400, 4800, 9600, 19200, 38400, 57600, 115200};
    static constexpr const char* LORA_CR[] = {"4/5", "4/6", "4/7", "4/8"};
    static constexpr const char* LORA_BW[] = {"7.8", "10.4", "15.6", "20.8", "31.25", "41.7", "62.5", "125", "250", "500"};

    struct MenuItem
    {
        MenuItemType type = SUB_MENU;
        char title[20];
        char** strValues = nullptr;
        uint32_t* intValues = nullptr;
        MenuItemHandler handler = nullptr;
        uint32_t minRangeValue;
        uint32_t maxRangeValue;
        uint32_t rangeStep = 1;
        uint32_t rangeValue;
        uint16_t maxIndex = 0;
        uint8_t index = 0;
    };

    struct Menu
    {
        char title[20];
        MenuItem items[10];
        uint8_t itemsCount;
        uint8_t uiStartPosition = 0;
        uint8_t uiPosition = 0;
    };

public:
    View(SSD1306* display, Gpio* mainBtnGpio, Gpio* downBtnGpio, Gpio* upBtnGpio);
    ~View();

    void startThread();
    bool isWaiting() const;

    void onLoraPacketTransmitted();
    void onLoraPacketReceived(const LoraLinkData& linkData);
    void onLoraNoiseFloorUpdated(int16_t rssi);

private:
    void update();
    void createMenu();
    inline uint8_t getMenuItemPosition(const Menu& menu) const { return menu.uiStartPosition + menu.uiPosition; }
    void setMenuItemPosition(Menu& menu, uint8_t pos);
    void incMenuItemPosition(Menu& menu);
    void decMenuItemPosition(Menu& menu);
    void selectMenuItem(const MenuItem& item);

    void showMenu();
    void showChangeValueScreen(const MenuItem& item);
    void updateLocalSettings();
    bool checkSettingsChanges();
    void updateMenu();
    void updateFromSystemSettings();
    void saveChangesToFlash();
    void showMainScreen();
    void updateMainScreen();
    void showSaveChangesPopup();
    void showSaveToFlashPopup();
    void showWaitScreen();

private:

    SSD1306* m_display;
    Gpio* m_mainBtnGpio;
    Gpio* m_downBtnGpio;
    Gpio* m_upBtnGpio;
    PressedBtn m_pressedBtn;
    Menu m_menu[3];
    Settings m_localSettings;
    LoraLinkData m_loraLinkData;
    uint32_t m_loraTxPacketsCount;
    uint32_t m_loraRxPacketsCount;
    int16_t m_currentRssi;

    Screen m_screen;
    uint8_t m_menuIndex;

};