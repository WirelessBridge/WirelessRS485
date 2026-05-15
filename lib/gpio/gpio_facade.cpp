
/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "gpio_facade.h"
#include "logger.h"

extern "C" {
void HAL_GPIO_EXTI_Callback(uint16_t pin)
{
    for (uint8_t i = 0; i < Gpio::s_callbacksCount; i++) {
        if (Gpio::s_callbacks[i].pin == pin) {
            Gpio::s_callbacks[i].callback(Gpio::s_callbacks[i].userData);
        }
    }
}
}

Gpio::CallbackData Gpio::s_callbacks[MAX_CALLBACKS] = {};
uint8_t Gpio::s_callbacksCount = 0;

Gpio::Gpio(GPIO_TypeDef* port, uint16_t pin)
    : m_port(port)
    , m_pin(pin)
{}

Gpio::~Gpio() = default;

void Gpio::set(GPIO_PinState state)
{
    HAL_GPIO_WritePin(m_port, m_pin, state);
}

GPIO_PinState Gpio::get()
{
    return HAL_GPIO_ReadPin(m_port, m_pin);
}

void Gpio::registerCallback(GpioCallback callback, void* userData)
{
    s_callbacks[s_callbacksCount++] = {callback, m_pin, userData};
}
