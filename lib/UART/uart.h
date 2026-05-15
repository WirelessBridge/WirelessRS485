/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "usart.h"
#include "link_controller.hpp"

using UartBase = LinkController<128, 128>;
class Uart : public UartBase
{
public:
    static void onDataTransmitted(UART_HandleTypeDef* huart, Uart* inst);
    static void onDataReceived(UART_HandleTypeDef* huart, uint16_t size, Uart* inst);
    
public:
    Uart(UART_HandleTypeDef* uartDevice);
    ~Uart() override;

    void startThread(Callback rxCallback, Callback txCallback) override;
    void send(const uint8_t* data, uint32_t length) override;

    void setBaudrate(const uint32_t baudrate);
    void setStopBits(const uint8_t stopBits);

private:
    void onDataRX();
    void onDataTX();

private:
    UART_HandleTypeDef* m_uartDevice;
};
