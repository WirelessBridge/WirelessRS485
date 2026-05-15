/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "ssd1306.h"

#include "cmsis_os.h"

#include <string.h>
#include "printf.h"


#include "fonts.h"
#include "logger.h"

using namespace Font5x7;

extern "C" {
extern osSemaphoreId_t i2cTxDmaSemaphore;
}

void SSD1306::onDataTransmitted(I2C_HandleTypeDef* hi2c, SSD1306* inst)
{
    if (inst != nullptr && hi2c == inst->m_i2cDevice) {
        osSemaphoreRelease(i2cTxDmaSemaphore);
    }
}

SSD1306::SSD1306(I2C_HandleTypeDef* i2cDevice, uint8_t i2cAddress)
    : m_i2cDevice(i2cDevice)
    , m_i2cAddress(i2cAddress << 1)
    , m_cmdBuffer{0, 0, 0, 0}
    , m_fb{}
{}

SSD1306::~SSD1306() = default;

void SSD1306::init()
{
    //	delay after power on to stabilise voltage
    osDelay(100);

    m_cmdBuffer[0] = 0x00;
    m_cmdBuffer[1] = 0xAE;
    sendDMA(m_cmdBuffer, 2);

    osDelay(100);

    //	Enable charge pump
    m_cmdBuffer[1] = 0x8d;
    m_cmdBuffer[2] = 0x14;
    sendDMA(m_cmdBuffer, 3);

    //	Power on, dismiss RES#
    m_cmdBuffer[1] = 0xAF;
    sendDMA(m_cmdBuffer, 2);

    //	Wait for display to power on
    osDelay(100);

    //	Set normal display mode
    m_cmdBuffer[1] = 0xA6;
    sendDMA(m_cmdBuffer, 2);

    // Set horizontal addressing
    m_cmdBuffer[1] = 0x20;
    m_cmdBuffer[2] = 0;
    sendDMA(m_cmdBuffer, 3);

    // COM Scan Direction
    m_cmdBuffer[1] = 0xC8;
    sendDMA(m_cmdBuffer, 2);

    // Segment Remap
    m_cmdBuffer[1] = 0xA1;
    sendDMA(m_cmdBuffer, 2);

    //	Set offset to 0
    m_cmdBuffer[1] = 0xD3;
    m_cmdBuffer[2] = 0x00;
    sendDMA(m_cmdBuffer, 3);

    //	set star/end column as 0/127

    m_cmdBuffer[1] = 0x21;
    m_cmdBuffer[2] = 0;
    m_cmdBuffer[3] = 127;
    sendDMA(m_cmdBuffer, 4);

    //	Set page range to 0 : 3 (for 128 x 32 pixel display)
    m_cmdBuffer[1] = 0x22;
    m_cmdBuffer[2] = 0x00;
    m_cmdBuffer[3] = 0x03;
    sendDMA(m_cmdBuffer, 4);

    //	Set Multiplex Ratio for 128 x 32 pixels
    m_cmdBuffer[1] = 0xA8;
    m_cmdBuffer[2] = 31;
    sendDMA(m_cmdBuffer, 3);

    //	set compins
    m_cmdBuffer[1] = 0xDA;
    m_cmdBuffer[2] = 0x02;
    sendDMA(m_cmdBuffer, 3);

    osDelay(50);
    clear();
}

void SSD1306::clear(uint8_t startLine, uint8_t linesCount)
{
    const uint8_t firstLine = startLine < LINES_ON_PAGE ? startLine : LINES_ON_PAGE - 1;

    for (uint8_t i = 0; i < LINES_ON_PAGE; i++) {
        if (i >= firstLine && i < firstLine + linesCount) {
            m_fb[i][0] = 0x40;
        } else {
            m_fb[i][0] = 0x00;
        }
        for (uint8_t j = 1; j < 129; j++) {
            m_fb[i][j] = 0x00;
        }
        sendDMA(m_fb[i], 129);
    }
}

void SSD1306::print(const char* str, uint8_t line, Align align)
{
    uint8_t len = strlen(str);
    len = len > MAX_SYMBOLS_IN_LINE ? MAX_SYMBOLS_IN_LINE : len;

    if (line > 4) {
        LOG_WARNING("Out of display lines range: %d, will be set to 3\n", line);
        line = 3;
    }

    uint8_t column = 0;
    switch (align) {
    case ALIGN_RIGHT:
        column = (MAX_SYMBOLS_IN_LINE - len) * (SYMBOL_SIZE + 1);
        break;
    case ALIGN_CENTER:
        column = ((MAX_SYMBOLS_IN_LINE - len) * (SYMBOL_SIZE + 1)) / 2;
        break;
    case ALIGN_LEFT:
    default:
        break;
    }

    uint8_t* data = m_fb[line];
    data[0] = 0x40;
    uint8_t insertPos = 1;
    for (uint8_t i = 0; i < len; i++) {
        const uint8_t* symbolData = FONT[(uint8_t)str[i] - 0x20];
        for (uint8_t j = 0; j < SYMBOL_SIZE; j++) {
            data[insertPos++] = symbolData[j];
        }
        if (i < len - 1) {
            data[insertPos++] = 0x00;
        }
    }

    m_cmdBuffer[0] = 0x00;
    m_cmdBuffer[1] = (0xB0 | line);
    m_cmdBuffer[2] = (column & 0x0F);
    m_cmdBuffer[3] = (0x10 | (column >> 4));
    sendDMA(m_cmdBuffer, 4);
    sendDMA(data, insertPos);
}

HAL_StatusTypeDef SSD1306::send(const uint8_t* data, uint16_t size)
{
    return HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, const_cast<uint8_t*>(data), size, HAL_MAX_DELAY);
}

HAL_StatusTypeDef SSD1306::sendDMA(const uint8_t* data, uint16_t size)
{
    const auto result = HAL_I2C_Master_Transmit_DMA(m_i2cDevice, m_i2cAddress, const_cast<uint8_t*>(data), size);
    if (result == HAL_OK) {
        osSemaphoreAcquire(i2cTxDmaSemaphore, osWaitForever);
    } else {
        LOG_ERROR("Failed to start I2C DMA transmission: %d\n", result);
    }
    return result;
}
