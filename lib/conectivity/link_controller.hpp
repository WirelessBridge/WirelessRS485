/*
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

template<size_t txBufferSize, size_t rxBufferSize>
class LinkController
{
public:
    using Callback = void (*)(const uint8_t* txData, uint16_t length);

    LinkController()
        : m_txCallback(nullptr)
        , m_rxCallback(nullptr)
        , m_txBuffer{}
        , m_rxBuffer{}
        , m_txDataLength(0)
        , m_rxDataLength(0)
    {}
    virtual ~LinkController() = default;

    virtual void startThread(Callback rxCallback, Callback txCallback)
    {
        m_rxCallback = rxCallback;
        m_txCallback = txCallback;
    }

    virtual void send(const uint8_t* data, uint32_t length) = 0;

protected:
    static constexpr size_t TX_BUFFER_SIZE = txBufferSize;
    static constexpr size_t RX_BUFFER_SIZE = rxBufferSize;

protected:
    Callback m_txCallback;
    Callback m_rxCallback;

    uint8_t m_txBuffer[txBufferSize];
    uint8_t m_rxBuffer[rxBufferSize];

    size_t m_txDataLength;
    size_t m_rxDataLength;
};