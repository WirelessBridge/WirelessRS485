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

SSD1306::SSD1306(I2C_HandleTypeDef* i2cDevice, uint8_t i2cAddress)
    : m_i2cDevice(i2cDevice)
    , m_i2cAddress(i2cAddress << 1)
    , m_fb{}
{}

SSD1306::~SSD1306() = default;

void SSD1306::init()
{
    uint8_t buf[4];
    buf[2] = 0;
    buf[3] = 0;

    //	delay after power on to stabilise voltage
    osDelay(100);

    //	RES# command (probably)
    buf[0] = 0x00;
    buf[1] = 0xAE;

    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 2, HAL_MAX_DELAY);

    //	keep RES# for controller to reset
    osDelay(100);

    //	Enable charge pump
    buf[1] = 0x8d;
    buf[2] = 0x14;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 3, HAL_MAX_DELAY);

    //	Power on, dismiss RES#
    buf[1] = 0xAF;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 2, HAL_MAX_DELAY);

    //	Wait for display to power on
    osDelay(100);

    //	Set normal display mode
    buf[1] = 0xA6;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 2, HAL_MAX_DELAY);

    // Set horizontal addressing
    buf[1] = 0x20;
    buf[2] = 0;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 3, HAL_MAX_DELAY);

    // COM Scan Direction
    buf[1] = 0xC8;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 2, HAL_MAX_DELAY);

    // Segment Remap
    buf[1] = 0xA1;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 2, HAL_MAX_DELAY);

    //	Set offset to 0
    buf[1] = 0xD3;
    buf[2] = 0x00;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 3, HAL_MAX_DELAY);

    //	set star/end column as 0/127

    buf[1] = 0x21;
    buf[2] = 0;
    buf[3] = 127;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 4, HAL_MAX_DELAY);

    //	Set page range to 0 : 3 (for 128 x 32 pixel display)
    buf[1] = 0x22;
    buf[2] = 0x00;
    buf[3] = 0x03;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 4, HAL_MAX_DELAY);

    //	Set Multiplex Ratio for 128 x 32 pixels
    buf[1] = 0xA8;
    buf[2] = 31;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 3, HAL_MAX_DELAY);

    //	set compins
    buf[1] = 0xDA;
    buf[2] = 0x02;
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, buf, 3, HAL_MAX_DELAY);

    osDelay(50);
    clear();
}

void SSD1306::clear(uint8_t startLine, uint8_t linesCount)
{
    const uint8_t firstLine = startLine < LINES_ON_PAGE ? startLine : LINES_ON_PAGE - 1;
    //const uint8_t count = linesCount != 0 && linesCount + firstLine <= LINES_ON_PAGE ? linesCount : LINES_ON_PAGE - firstLine;

    for (uint8_t i = 0; i < LINES_ON_PAGE; i++) {
        if (i >= firstLine && i < firstLine + linesCount) {
            m_fb[i][0] = 0x40;
        } else {
            m_fb[i][0] = 0x00;
        }
        for (uint8_t j = 1; j < 129; j++) {
            m_fb[i][j] = 0x00;
        }
        HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, m_fb[i], 129, HAL_MAX_DELAY);
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

    uint8_t data[len * SYMBOL_SIZE + len];
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

    uint8_t cmd[4];
    cmd[0] = 0x00;
    cmd[1] = (0xB0 | line);
    cmd[2] = (column & 0x0F);
    cmd[3] = (0x10 | (column >> 4));
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, cmd, 4, HAL_MAX_DELAY);
    HAL_I2C_Master_Transmit(m_i2cDevice, m_i2cAddress, data, sizeof(data), HAL_MAX_DELAY);
}
