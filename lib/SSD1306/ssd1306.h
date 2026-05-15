/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "i2c.h"

class SSD1306
{
    static constexpr uint8_t DISPLAY_WIDTH = 128;
    static constexpr uint8_t DISPLAY_HEIGHT = 32;   
    static constexpr uint8_t MAX_SYMBOLS_IN_LINE = 22;
    static constexpr uint8_t LINES_ON_PAGE = 4;
    static constexpr uint8_t CMD_MAX_LENGTH = 4;
    
public:
    enum Align
    {
        ALIGN_LEFT,
        ALIGN_RIGHT,
        ALIGN_CENTER
    };

    static void onDataTransmitted(I2C_HandleTypeDef* hi2c, SSD1306* inst);

public:
    SSD1306(I2C_HandleTypeDef* i2cDevice, uint8_t i2cAddress);
    ~SSD1306();

    void init();
    void clear(uint8_t startLine = 0, uint8_t linesCount = 4);
    void print(const char* str, uint8_t line, Align align = ALIGN_LEFT);

private:
    HAL_StatusTypeDef send(const uint8_t* data, uint16_t size);
    HAL_StatusTypeDef sendDMA(const uint8_t* data, uint16_t size);

private:
    I2C_HandleTypeDef* m_i2cDevice;
    uint8_t m_i2cAddress;

    uint8_t m_cmdBuffer[CMD_MAX_LENGTH];
    uint8_t m_fb[LINES_ON_PAGE][DISPLAY_WIDTH + 1];
};