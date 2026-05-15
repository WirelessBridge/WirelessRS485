/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "view.h"

#include "cmsis_os.h"
#include "gpio_facade.h"
#include "logger.h"
#include "lora_types.h"
#include "ssd1306.h"

extern "C" {
extern osSemaphoreId_t viewUpdateSemaphore;
}

View::View(SSD1306* display, Gpio* mainBtnGpio, Gpio* downBtnGpio, Gpio* upBtnGpio)
    : m_display(display)
    , m_mainBtnGpio(mainBtnGpio)
    , m_downBtnGpio(downBtnGpio)
    , m_upBtnGpio(upBtnGpio)
    , m_pressedBtn(BTN_NONE)
    , m_menu{}
    , m_localSettings{}
    , m_loraLinkData{}
    , m_loraTxPacketsCount(0)
    , m_loraRxPacketsCount(0)
    , m_currentRssi(0)
    , m_screen(MENU_SCREEN)
    , m_menuIndex(0)
{}

View::~View() = default;

void View::startThread()
{
    m_display->init();
    createMenu();
    updateLocalSettings();
    showMainScreen();
    m_mainBtnGpio->registerCallback(
        [](void* userData) {
            if (View* inst = static_cast<View*>(userData); inst != nullptr && inst->m_pressedBtn == BTN_NONE) {
                inst->m_pressedBtn = View::BTN_MAIN;
                osSemaphoreRelease(viewUpdateSemaphore);
            }
        },
        this);

    m_downBtnGpio->registerCallback(
        [](void* userData) {
            if (View* inst = static_cast<View*>(userData); inst != nullptr && inst->m_pressedBtn == BTN_NONE) {
                inst->m_pressedBtn = View::BTN_DOWN;
                osSemaphoreRelease(viewUpdateSemaphore);
            }
        },
        this);

    m_upBtnGpio->registerCallback(
        [](void* userData) {
            if (View* inst = static_cast<View*>(userData); inst != nullptr && inst->m_pressedBtn == BTN_NONE) {
                inst->m_pressedBtn = View::BTN_UP;
                osSemaphoreRelease(viewUpdateSemaphore);
            }
        },
        this);

    for (;;) {
        m_pressedBtn = BTN_NONE;
        osSemaphoreAcquire(viewUpdateSemaphore, osWaitForever);
        update();
        osDelay(1);
    }
}

void View::update()
{
    if (m_screen == WAIT_SCREEN) {
        return;
    } else if (m_screen == MAIN_SCREEN) {
        updateMainScreen();
    }
    Menu& menu = m_menu[m_menuIndex];
    MenuItem& item = menu.items[getMenuItemPosition(menu)];
    MenuItem& loraFreq = m_menu[2].items[0];
    switch (m_pressedBtn) {
    case BTN_MAIN:
        if (m_screen == MENU_SCREEN) {
            selectMenuItem(item);
        } else if (m_screen == VALUE_CHANGE_SCREEN) {
            showMenu();
        } else if (m_screen == SAVE_CHANGES_POPUP) {
            saveChangesToFlash();
        } else {
            m_menuIndex = 0;
            showMenu();
        }
        break;
    case BTN_DOWN:
        if (m_screen == MAIN_SCREEN) {
            switch (m_localSettings.workMode) {
            case NOICE_MEASUREMENT_MODE:
                if (loraFreq.rangeValue < loraFreq.maxRangeValue) {
                    loraFreq.rangeValue += loraFreq.rangeStep;
                    updateLocalSettings();
                    SystemSettings::updateSettings(m_localSettings);
                }
                break;
            default:
                break;
            }
        } else if (m_screen == MENU_SCREEN) {
            incMenuItemPosition(menu);
            showMenu();
        } else if (m_screen == VALUE_CHANGE_SCREEN) {
            if (item.type == INT_RANGE || item.type == HEX_RANGE) {
                item.rangeValue =
                    item.rangeValue <= item.maxRangeValue - item.rangeStep ? item.rangeValue + item.rangeStep : item.rangeValue;
            } else {
                item.index = item.index < item.maxIndex ? item.index + 1 : item.index;
            }
            showChangeValueScreen(item);
        } else if (m_screen == SAVE_CHANGES_POPUP) {
            updateLocalSettings();
            SystemSettings::updateSettings(m_localSettings);
            showMainScreen();
        } else if (m_screen == SAVE_TO_FLASH_POPUP) {
            saveChangesToFlash();
        }
        break;
    case BTN_UP:
        if (m_screen == MAIN_SCREEN) {
            switch (m_localSettings.workMode) {
            case NOICE_MEASUREMENT_MODE:
                if (loraFreq.rangeValue > loraFreq.minRangeValue) {
                    loraFreq.rangeValue -= loraFreq.rangeStep;
                    updateLocalSettings();
                    SystemSettings::updateSettings(m_localSettings);
                }
                break;
            default:
                break;
            }
        } else if (m_screen == MENU_SCREEN) {
            decMenuItemPosition(menu);
            showMenu();
        } else if (m_screen == VALUE_CHANGE_SCREEN) {
            if (item.type == INT_RANGE || item.type == HEX_RANGE) {
                item.rangeValue =
                    item.rangeValue >= item.minRangeValue + item.rangeStep ? item.rangeValue - item.rangeStep : item.rangeValue;
            } else {
                item.index = item.index > 0 ? item.index - 1 : item.index;
            }
            showChangeValueScreen(item);
        } else if (m_screen == SAVE_CHANGES_POPUP) {
            updateFromSystemSettings();
            showMainScreen();
        } else if (m_screen == SAVE_TO_FLASH_POPUP) {
            showMenu();
        }
        break;
    default:
        break;
    }
}

bool View::isWaiting() const
{
    return m_screen == WAIT_SCREEN;
}

void View::onLoraPacketTransmitted()
{
    // m_loraTxPacketsCount++;
    //osSemaphoreRelease(viewUpdateSemaphore);
}

void View::onLoraPacketReceived(const LoraLinkData& linkData)
{
    // m_loraRxPacketsCount++;
    m_loraLinkData = linkData;
    osSemaphoreRelease(viewUpdateSemaphore);
}

void View::onLoraNoiseFloorUpdated(int16_t rssi)
{
    m_currentRssi = rssi;
    osSemaphoreRelease(viewUpdateSemaphore);
}

void View::createMenu()
{
    const auto& settings = SystemSettings::getSettings();

    // --------- Main menu ------------
    auto& mainMenu = m_menu[0];
    strcpy(mainMenu.title, "MAIN MENU");
    mainMenu.itemsCount = 6;

    auto& mode = mainMenu.items[0];
    strcpy(mode.title, "MODE");
    mode.type = STR_VALUE;
    mode.strValues = const_cast<char**>(MODES);
    mode.maxIndex = get_array_size(MODES) - 1;
    mode.index = settings.workMode;

    auto& rsSettings = mainMenu.items[1];
    strcpy(rsSettings.title, "RS485 SETTINGS");
    rsSettings.type = SUB_MENU;
    rsSettings.index = 1;

    auto& radioSettings = mainMenu.items[2];
    strcpy(radioSettings.title, "RADIO SETTINGS");
    radioSettings.type = SUB_MENU;
    radioSettings.index = 2;

    auto& checkNoise = mainMenu.items[3];
    strcpy(checkNoise.title, "CHECK NOISE");
    checkNoise.type = INT_RANGE;
    checkNoise.minRangeValue = 100;
    checkNoise.rangeValue = settings.checkNoisePeriod;
    checkNoise.maxRangeValue = 10000;
    checkNoise.rangeStep = 100;

    auto& exit = mainMenu.items[4];
    strcpy(exit.title, "EXIT");
    exit.type = CALL_FUNCTION;
    exit.handler = [](View* inst) {
        if (inst->checkSettingsChanges()) {
            inst->showSaveChangesPopup();
        } else {
            inst->showMainScreen();
        }
    };

    auto& saveToFlash = mainMenu.items[5];
    strcpy(saveToFlash.title, "SAVE TO FLASH");
    saveToFlash.type = CALL_FUNCTION;
    saveToFlash.handler = [](View* inst) {
        inst->showSaveToFlashPopup();
    };

    // --------- RS485 settings ------------
    auto& rsMenu = m_menu[1];
    strcpy(rsMenu.title, "RS485 SETTINGS");
    rsMenu.itemsCount = 3;

    auto& baudrate = rsMenu.items[0];
    strcpy(baudrate.title, "BAUDRATE");
    baudrate.type = INT_VALUE;
    baudrate.intValues = const_cast<uint32_t*>(BAUD_RATES);
    baudrate.maxIndex = get_array_size(BAUD_RATES) - 1;
    for (size_t i = 0; i < get_array_size(BAUD_RATES); i++) {
        if (BAUD_RATES[i] == settings.rsBaudrate) {
            baudrate.index = i;
            break;
        }
    }

    auto& stopBits = rsMenu.items[1];
    strcpy(stopBits.title, "STOP BITS");
    stopBits.type = INT_RANGE;
    stopBits.minRangeValue = 0;
    stopBits.maxRangeValue = 2;
    stopBits.rangeValue = settings.rsStopBits;

    auto& rsBack = rsMenu.items[2];
    strcpy(rsBack.title, "BACK");
    rsBack.type = SUB_MENU;
    rsBack.index = 0;

    // --------- LoRA settings ------------
    auto& loraMenu = m_menu[2];
    strcpy(loraMenu.title, "LoRA SETTINGS");
    loraMenu.itemsCount = 8;

    auto& freq = loraMenu.items[0];
    strcpy(freq.title, "FREQUENCY");
    freq.type = INT_RANGE;
    freq.minRangeValue = 433;
    freq.maxRangeValue = 510;
    freq.rangeValue = settings.loraFrequency / 1000000;
    freq.rangeStep = 1;

    auto& txPower = loraMenu.items[1];
    strcpy(txPower.title, "TX POWER");
    txPower.type = INT_RANGE;
    txPower.minRangeValue = 14;
    txPower.rangeValue = settings.loraTxPower;
    txPower.maxRangeValue = 20;

    auto& bandwidth = loraMenu.items[2];
    strcpy(bandwidth.title, "BANDWITH");
    bandwidth.type = STR_VALUE;
    bandwidth.strValues = const_cast<char**>(LORA_BW);
    bandwidth.maxIndex = get_array_size(LORA_BW) - 1;
    bandwidth.index = settings.loraBandwidth;

    auto& codingRate = loraMenu.items[3];
    strcpy(codingRate.title, "CODING RATE");
    codingRate.type = STR_VALUE;
    codingRate.strValues = const_cast<char**>(LORA_CR);
    codingRate.maxIndex = get_array_size(LORA_CR) - 1;
    codingRate.index = settings.loraCodingRate;

    auto& spreadFactor = loraMenu.items[4];
    strcpy(spreadFactor.title, "SPREAD FACTOR");
    spreadFactor.type = INT_RANGE;
    spreadFactor.minRangeValue = SF_6;
    spreadFactor.rangeValue = settings.loraSpreadingFactor;
    spreadFactor.maxRangeValue = SF_12;

    auto& preambleLen = loraMenu.items[5];
    strcpy(preambleLen.title, "PREAMBLE LENGTH");
    preambleLen.type = INT_RANGE;
    preambleLen.minRangeValue = 6;
    preambleLen.rangeValue = settings.loraPreambleLen;
    preambleLen.maxRangeValue = 1000;

    auto& syncWord = loraMenu.items[6];
    strcpy(syncWord.title, "SYNC WORD");
    syncWord.type = HEX_RANGE;
    syncWord.minRangeValue = 0x01;
    syncWord.rangeValue = settings.loraSyncWord;
    syncWord.maxRangeValue = 0xFF;

    auto& loraBack = loraMenu.items[7];
    strcpy(loraBack.title, "BACK");
    loraBack.type = SUB_MENU;
    loraBack.index = 0;
}

void View::setMenuItemPosition(Menu& menu, uint8_t pos)
{
    const uint8_t itemPos = pos < menu.itemsCount ? pos : menu.itemsCount - 1;

    menu.uiStartPosition = itemPos > 2 ? itemPos - 2 : 0;
    menu.uiPosition = itemPos - menu.uiStartPosition;
}

void View::incMenuItemPosition(Menu& menu)
{
    if (getMenuItemPosition(menu) < menu.itemsCount - 1) {
        if (menu.uiPosition < 2) {
            menu.uiPosition++;
        } else {
            menu.uiStartPosition++;
        }
    } else {
        setMenuItemPosition(menu, 0);
    }
}

void View::decMenuItemPosition(Menu& menu)
{
    if (getMenuItemPosition(menu) > 0) {
        if (menu.uiPosition > 0) {
            menu.uiPosition--;
        } else {
            menu.uiStartPosition--;
        }
    } else {
        setMenuItemPosition(menu, menu.itemsCount - 1);
    }
}

void View::selectMenuItem(const MenuItem& item)
{
    switch (item.type) {
    case SUB_MENU:
        LOG_DEBUG_2("Select submenu Item: %s %u\n", item.title, item.index);
        m_menuIndex = item.index;
        showMenu();
        break;
    case STR_VALUE:
    case INT_VALUE:
    case INT_RANGE:
    case HEX_RANGE:
        showChangeValueScreen(item);
        break;
    case CALL_FUNCTION:
        if (item.handler != nullptr) {
            item.handler(this);
        }
        break;
    default:
        break;
    }
}

void View::showMenu()
{
    updateLocalSettings();
    m_screen = MENU_SCREEN;
    Menu& menu = m_menu[m_menuIndex];
    m_display->clear();
    m_display->print(menu.title, 0);
    for (uint8_t i = 0; i < MAX_MENU_ITEMS_ON_PAGE && i < menu.itemsCount - menu.uiStartPosition; i++) {
        MenuItem& item = menu.items[i + menu.uiStartPosition];
        const char first = i == menu.uiPosition ? '*' : ' ';
        char buff[LINE_BUFFER_SIZE + 1];
        if (item.type == STR_VALUE) {
            snprintf(buff, LINE_BUFFER_SIZE, "%c%s: %s", first, item.title, item.strValues[item.index]);
        } else if (item.type == INT_VALUE || item.type == INT_RANGE) {
            snprintf(buff,
                     LINE_BUFFER_SIZE,
                     "%c%s: %u",
                     first,
                     item.title,
                     item.type == INT_VALUE ? item.intValues[item.index] : item.rangeValue);
        } else if (item.type == HEX_RANGE) {
            const char* fmt = item.maxRangeValue > 0xFFFF ? "%c%s: 0x%08X" : item.maxRangeValue > 0xFF ? "%c%s: 0x%04X" : "%c%s: 0x%02X";
            snprintf(buff, LINE_BUFFER_SIZE, fmt, first, item.title, item.rangeValue);
        } else {
            snprintf(buff, LINE_BUFFER_SIZE, "%c%s", first, item.title);
        }
        m_display->print(buff, i + 1);
    }
}

void View::showChangeValueScreen(const MenuItem& item)
{
    m_screen = VALUE_CHANGE_SCREEN;
    char buff[LINE_BUFFER_SIZE + 1];
    switch (item.type) {
    case STR_VALUE:
        snprintf(buff, LINE_BUFFER_SIZE, " <%s>", item.strValues[item.index]);
        break;
    case INT_VALUE:
        snprintf(buff, LINE_BUFFER_SIZE, " <%u>", item.intValues[item.index]);
        break;
    case INT_RANGE:
        snprintf(buff, LINE_BUFFER_SIZE, " [%u]", item.rangeValue);
        break;
    case HEX_RANGE: {
        const char* fmt = item.maxRangeValue > 0xFFFF ? " [0x%08X]" : item.maxRangeValue > 0xFF ? " [0x%04X]" : " [0x%02X]";
        snprintf(buff, LINE_BUFFER_SIZE, fmt, item.rangeValue);
    } break;
    default:
        return;
    }
    m_display->clear();
    m_display->print(item.title, 0);
    m_display->print(buff, 1);
}

void View::updateLocalSettings()
{
    m_localSettings.workMode = static_cast<Mode>(m_menu[0].items[0].index);
    m_localSettings.checkNoisePeriod = m_menu[0].items[3].rangeValue;
    m_localSettings.rsBaudrate = BAUD_RATES[m_menu[1].items[0].index];
    m_localSettings.rsStopBits = m_menu[1].items[1].rangeValue;
    m_localSettings.loraFrequency = m_menu[2].items[0].rangeValue * 1000000;
    m_localSettings.loraTxPower = m_menu[2].items[1].rangeValue;
    m_localSettings.loraBandwidth = static_cast<LoraBW>(m_menu[2].items[2].index);
    m_localSettings.loraCodingRate = static_cast<LoraCR>(m_menu[2].items[3].index);
    m_localSettings.loraSpreadingFactor = static_cast<LoraSF>(m_menu[2].items[4].rangeValue);
    m_localSettings.loraPreambleLen = m_menu[2].items[5].rangeValue;
    m_localSettings.loraSyncWord = m_menu[2].items[6].rangeValue;
}

bool View::checkSettingsChanges()
{
    const auto& systemSettings = SystemSettings::getSettings();
    const uint8_t* localSettingsData = reinterpret_cast<const uint8_t*>(&m_localSettings);
    const uint8_t* systemSettingsData = reinterpret_cast<const uint8_t*>(&systemSettings);

    for (size_t i = 0; i < sizeof(systemSettings); i++) {
        if (localSettingsData[i] != systemSettingsData[i]) {
            return true;
        }
    }

    return false;
}

void View::updateMenu()
{
    m_menu[0].items[0].index = m_localSettings.workMode;
    m_menu[0].items[3].rangeValue = m_localSettings.checkNoisePeriod;
    for (size_t i = 0; i < get_array_size(BAUD_RATES); i++) {
        if (BAUD_RATES[i] == m_localSettings.rsBaudrate) {
            m_menu[1].items[0].index = i;
            break;
        }
    }
    m_menu[1].items[1].rangeValue = m_localSettings.rsStopBits;
    m_menu[2].items[0].rangeValue = m_localSettings.loraFrequency / 1000000;
    m_menu[2].items[1].rangeValue = m_localSettings.loraTxPower;
    m_menu[2].items[2].index = m_localSettings.loraBandwidth;
    m_menu[2].items[3].index = m_localSettings.loraCodingRate;
    m_menu[2].items[4].rangeValue = m_localSettings.loraSpreadingFactor;
    m_menu[2].items[5].rangeValue = m_localSettings.loraPreambleLen;
    m_menu[2].items[6].rangeValue = m_localSettings.loraSyncWord;
}

void View::updateFromSystemSettings()
{
    m_localSettings = SystemSettings::getSettings();
    updateMenu();
}

void View::saveChangesToFlash()
{
    showWaitScreen();
    updateLocalSettings();
    SystemSettings::updateSettings(m_localSettings, true);
    osDelay(2000);
    showMainScreen();
}

void View::showMainScreen()
{
    m_screen = MAIN_SCREEN;
    m_display->clear();
    char title[LINE_BUFFER_SIZE + 1];
    const int8_t extraSymbolsCount = (LINE_BUFFER_SIZE - strlen(MODES[m_localSettings.workMode])) / 2 - 2;
    char extraSymbols[extraSymbolsCount + 1] = "";
    for (int8_t i = 0; i < extraSymbolsCount; i++) {
        extraSymbols[i] = '-';
    }
    extraSymbols[extraSymbolsCount] = '\0';

    snprintf(title, LINE_BUFFER_SIZE, "%s %s %s", extraSymbols, MODES[m_localSettings.workMode], extraSymbols);
    m_display->print(title, 0, SSD1306::ALIGN_CENTER);
    updateMainScreen();
}

void View::updateMainScreen()
{
    if (m_screen != MAIN_SCREEN) {
        return;
    }
    switch (m_localSettings.workMode) {
    case BRIDGE_MODE:
        if (m_loraLinkData.isValid) {
            char strLine1[LINE_BUFFER_SIZE + 1];
            snprintf(strLine1, LINE_BUFFER_SIZE, "RSSI: %d dBm  ", m_loraLinkData.packetRSSI);
            char strLine2[LINE_BUFFER_SIZE + 1];
            snprintf(strLine2, LINE_BUFFER_SIZE, "SNR:    %d dB  ", m_loraLinkData.packetSNR);
            m_display->print(strLine1, 2, SSD1306::ALIGN_LEFT);
            m_display->print(strLine2, 3, SSD1306::ALIGN_LEFT);
        }
        break;
    case TEST_RS485_MODE:
        break;
    case TEST_RF_MODE:
        break;
    case NOICE_MEASUREMENT_MODE:
        char strLine1[LINE_BUFFER_SIZE + 1];
        snprintf(strLine1, LINE_BUFFER_SIZE, "Noice: %d dBm  ", m_currentRssi);
        char strLine2[LINE_BUFFER_SIZE + 1];
        snprintf(strLine2, LINE_BUFFER_SIZE, "-      %d MHz      +", m_localSettings.loraFrequency / 1000000);
        m_display->print(strLine1, 2, SSD1306::ALIGN_LEFT);
        m_display->print(strLine2, 3, SSD1306::ALIGN_LEFT);
        break;
    default:
        break;
    }
}

void View::showSaveChangesPopup()
{
    m_screen = SAVE_CHANGES_POPUP;
    m_display->clear();
    m_display->print("SAVE AND APPLY?", 0, SSD1306::ALIGN_CENTER);
    m_display->print("NO     FLASH     RAM", 3, SSD1306::ALIGN_CENTER);
}

void View::showSaveToFlashPopup()
{
    m_screen = SAVE_TO_FLASH_POPUP;
    m_display->clear();
    m_display->print("SAVE TO FLASH?", 0, SSD1306::ALIGN_CENTER);
    m_display->print("NO                YES", 3, SSD1306::ALIGN_CENTER);
}

void View::showWaitScreen()
{
    m_screen = WAIT_SCREEN;
    m_display->clear();
    m_display->print("... PLEASE WAIT ...", 1, SSD1306::ALIGN_CENTER);
}
