/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "uart.h"

#include "cmsis_os.h"
#include "string.h"
#include "logger.h"

extern "C" {
extern osSemaphoreId_t uartTxDmaSemaphore;
extern osSemaphoreId_t uartRxDmaSemaphore;
}

void Uart::onDataTransmitted(UART_HandleTypeDef* huart, Uart* inst)
{
    if (inst != nullptr && huart == inst->m_uartDevice) {
        osSemaphoreRelease(uartTxDmaSemaphore);
    }
}

void Uart::onDataReceived(UART_HandleTypeDef* huart, uint16_t size, Uart* inst)
{
    if (inst != nullptr && huart == inst->m_uartDevice) {
        inst->m_rxDataLength = size;
        osSemaphoreRelease(uartRxDmaSemaphore);
    }
}

Uart::Uart(UART_HandleTypeDef* uartDevice)
    : UartBase()
    , m_uartDevice(uartDevice)
{}

Uart::~Uart() = default;

void Uart::startThread(Callback rxCallback, Callback txCallback)
{
    UartBase::startThread(rxCallback, txCallback);

    HAL_UARTEx_ReceiveToIdle_DMA(m_uartDevice, m_rxBuffer, RX_BUFFER_SIZE);

    for (;;) {
        osSemaphoreAcquire(uartRxDmaSemaphore, osWaitForever);
        onDataRX();
        osDelay(1);
    }
}

void Uart::send(const uint8_t* data, uint32_t length)
{
    m_txDataLength = length;
    memcpy(m_txBuffer, data, length);

    HAL_UART_Transmit_DMA(m_uartDevice, m_txBuffer, m_txDataLength);
    osSemaphoreAcquire(uartTxDmaSemaphore, osWaitForever);
    onDataTX();
}

void Uart::setBaudrate(const uint32_t baudrate)
{
    if (m_uartDevice->Init.BaudRate == baudrate) {
        return;
    }
    HAL_UART_DeInit(m_uartDevice);
    m_uartDevice->Init.BaudRate = baudrate;
    if (const auto res = HAL_UART_Init(m_uartDevice); res != HAL_OK) {
        LOG_ERROR("Failed to initialize uart! Error code: %d\n", res);
    }
}

void Uart::setStopBits(const uint8_t stopBits)
{
    if (m_uartDevice->Init.StopBits == stopBits) {
        return;
    }
    HAL_UART_DeInit(m_uartDevice);
    m_uartDevice->Init.StopBits = stopBits;
    if (const auto res = HAL_UART_Init(m_uartDevice); res != HAL_OK) {
        LOG_ERROR("Failed to initialize uart! Error code: %d\n", res);
    }
}

void Uart::onDataRX()
{
    LOG_DEBUG("<<--- Received: %d bytes: [", m_rxDataLength);
    for (uint8_t i = 0; i < m_rxDataLength; i++) {
        LOG_DEBUG_PURE(i < m_rxDataLength - 1 ? "%02X:" : "%02X", m_rxBuffer[i]);
    }
    LOG_DEBUG_PURE("]\n");
    if (m_rxCallback != nullptr) {
        m_rxCallback(m_rxBuffer, m_rxDataLength);
    }
    HAL_UARTEx_ReceiveToIdle_DMA(m_uartDevice, m_rxBuffer, RX_BUFFER_SIZE);
}

void Uart::onDataTX()
{
    if (m_txCallback != nullptr) {
        m_txCallback(m_txBuffer, m_txDataLength);
    }
    LOG_DEBUG("--->> Sent %d bytes\n", m_txDataLength);
}
