/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "i2c.h"

class SSD1306
{
    
public:
    static constexpr uint8_t MAX_SYMBOLS_IN_LINE = 22;
    static constexpr uint8_t LINES_ON_PAGE = 4;

    enum Align
    {
        ALIGN_LEFT,
        ALIGN_RIGHT,
        ALIGN_CENTER
    };

    SSD1306(I2C_HandleTypeDef* i2cDevice, uint8_t i2cAddress);
    ~SSD1306();

    void init();
    void clear(uint8_t startLine = 0, uint8_t linesCount = 4);
    void print(const char* str, uint8_t line, Align align = ALIGN_LEFT);

private:
    I2C_HandleTypeDef* m_i2cDevice;
    uint8_t m_i2cAddress;

    uint8_t m_fb[LINES_ON_PAGE][129];
};