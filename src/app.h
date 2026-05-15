/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "FreeRTOS.h"
#include "usart.h"
#include "timers.h"

class Lora;
class View;
class Uart;
struct Settings;

extern "C" {
extern void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);
extern void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);  
}

class App
{
    static const constexpr auto CHECK_NOISE_DEFAULT_PERIOD = 500; // ms
public:
    static void startLoraThread();
    static void startUartThread();
    static void startUIThread();

private:
    static void getSettings();
    friend void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart);
    friend void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);

private:
    static View* s_view;
    static Lora* s_lora;
    static Uart* s_uart;
    static Settings* s_settings;
    static TimerHandle_t s_checkNoiseTimer;
};