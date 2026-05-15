/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "gpio.h"

extern "C" {
void HAL_GPIO_EXTI_Callback(uint16_t pin);
}

class Gpio
{
public:
    using GpioCallback = void(*)(void*);

    Gpio(GPIO_TypeDef* port, uint16_t pin);
    ~Gpio();

    void set(GPIO_PinState state);
    GPIO_PinState get();

    void registerCallback(GpioCallback callback, void* userData = nullptr);

private:
    static constexpr uint8_t MAX_CALLBACKS = 20;
    struct CallbackData
    {
        GpioCallback callback;
        uint16_t pin;
        void* userData;
    };

    static CallbackData s_callbacks[MAX_CALLBACKS];
    static uint8_t s_callbacksCount;

private:
    friend void HAL_GPIO_EXTI_Callback(uint16_t pin);

private:
    GPIO_TypeDef* m_port;
    uint16_t m_pin;
};
