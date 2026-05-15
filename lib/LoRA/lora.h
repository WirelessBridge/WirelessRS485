/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include "spi.h"

#include "gpio_facade.h"
#include "lora_types.h"
#include "link_controller.hpp"

using LoraBase = LinkController<256, 256>;

class Lora : public LoraBase
{
public:
    static void onSpiTxRxDone();

    Lora(SPI_HandleTypeDef* spiDevice, Gpio* cs, Gpio* reset, Gpio* irq);
    ~Lora() override;

    void startThread(Callback rxCallback, Callback txCallback) override;
    void send(const uint8_t* data, uint32_t length) override;

    const LoraConfig& getConfig() const;
    void setConfig(const LoraConfig&& config);

    uint32_t getFrequency() const;
    void setFrequency(const uint32_t frequency);

    LoraBW getBandwidth() const;
    void setBandwidth(const LoraBW bandwidth);

    LoraSF getSpreadingFactor() const;
    void setSpreadingFactor(const LoraSF spreadingFactor);

    LoraCR getCodingRate() const;
    void setCodingRate(const LoraCR codingRate);

    uint16_t getPreambleLength() const;
    void setPreambleLength(const uint16_t preambleLength);

    uint8_t getTxPower() const;
    void setTxPower(const uint8_t txPower);

    uint8_t getSyncWord() const;
    void setSyncWord(const uint8_t syncWord);

    const LoraLinkData& getLinkData();

    int16_t getNoiceFloor();

private:
    void writeReg(const uint8_t reg, const uint8_t value);
    void writeRegDma(const uint8_t reg, const uint8_t value);
    uint8_t readReg(const uint8_t reg);
    void setMode(const LoraMode mode);
    void setFrequency();
    void setTxPower();
    void setPreambleLength();
    void setInvertIQ();
    void setSyncWord();
    void setModemConfig1();
    void setModemConfig2();
    void setModemConfig3();
    void clearIrqFlags();
    void updateLinkData();

    void init();
    void updateConfigFromSettings();
    void applyConfig();
    void onDataRX();
    void onDataTX();


private:
    SPI_HandleTypeDef* m_spiDevice;
    Gpio* m_cs;
    Gpio* m_reset;
    Gpio* m_irq;

    LoraConfig m_config;
    LoraLinkData m_linkData;
};
