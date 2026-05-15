/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "lora.h"

#include "cmsis_os.h"
#include "logger.h"
#include "system_settings.h"

extern "C" {
extern osSemaphoreId_t loraDmaSemaphore;
extern osSemaphoreId_t loraIrqSemaphore;
}

void Lora::onSpiTxRxDone()
{
    osSemaphoreRelease(loraDmaSemaphore);
}

Lora::Lora(SPI_HandleTypeDef* spiDevice, Gpio* cs, Gpio* reset, Gpio* irq)
    : LoraBase()
    , m_spiDevice(spiDevice)
    , m_cs(cs)
    , m_reset(reset)
    , m_irq(irq)
    , m_config{}
    , m_linkData{}
{}

Lora::~Lora() {}

void Lora::startThread(Callback rxCallback, Callback txCallback)
{
    LoraBase::startThread(rxCallback, txCallback);

    m_irq->registerCallback([](void*) { osSemaphoreRelease(loraIrqSemaphore); });

    updateConfigFromSettings();
    init();
    setMode(LORA_MODE_RX_CONTINUOUS);

    SystemSettings::registerCallback(
        [](void* userData) {
            Lora* inst = static_cast<Lora*>(userData);
            if (inst != nullptr) {
                inst->updateConfigFromSettings();
            }
        },
        this);

    for (;;) {
        osSemaphoreAcquire(loraIrqSemaphore, osWaitForever);
        uint8_t irqFlags = readReg(LORA_REG_IRQ_FLAGS);
        clearIrqFlags();
        if (irqFlags & 0x40) {
            if (irqFlags & 0x20) {
                LOG_WARNING("Payload CRC error! Received packet ignored!\n");
            } else {
                onDataRX();
            }
        } else if (irqFlags & 0x08) {
            if (m_txCallback != nullptr) {
                m_txCallback(m_txBuffer, m_txDataLength);
            }
            writeRegDma(LORA_REG_DIO_MAPPING_1, 0x00);
            setMode(LORA_MODE_RX_CONTINUOUS);
        } else {
            onDataTX();
        }
        osDelay(1);
    }
}

void Lora::send(const uint8_t* data, uint32_t length)
{
    m_txDataLength = length;
    memcpy(&m_txBuffer[1], data, length);
    osSemaphoreRelease(loraIrqSemaphore);
}

const LoraConfig& Lora::getConfig() const
{
    return m_config;
}

void Lora::setConfig(const LoraConfig&& config)
{
    m_config = config;
    applyConfig();
}

uint32_t Lora::getFrequency() const
{
    return m_config.frequency;
}

void Lora::setFrequency(const uint32_t frequency)
{
    if (frequency < 137000000 || frequency > 525000000) {
        LOG_ERROR("Invalid frequency: %u\n", frequency);
        return;
    }

    if (frequency != m_config.frequency) {
        m_config.frequency = frequency;
        setMode(LORA_MODE_SLEEP);
        setFrequency();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

LoraBW Lora::getBandwidth() const
{
    return m_config.bandwidth;
}

void Lora::setBandwidth(const LoraBW bandwidth)
{
    if (bandwidth != m_config.bandwidth) {
        m_config.bandwidth = bandwidth;
        setMode(LORA_MODE_SLEEP);
        setModemConfig1();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

LoraSF Lora::getSpreadingFactor() const
{
    return m_config.sf;
}

void Lora::setSpreadingFactor(const LoraSF spreadingFactor)
{
    if (spreadingFactor < SF_6 || spreadingFactor > SF_12) {
        LOG_ERROR("Invalid spreading factor: %u\n", spreadingFactor);
        return;
    }
    if (spreadingFactor != m_config.sf) {
        m_config.sf = spreadingFactor;
        setMode(LORA_MODE_SLEEP);
        setModemConfig2();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

LoraCR Lora::getCodingRate() const
{
    return m_config.cr;
}

void Lora::setCodingRate(const LoraCR codingRate)
{
    if (codingRate < CR_4_5 || codingRate > CR_4_8) {
        LOG_ERROR("Invalid coding rate: %u\n", codingRate);
        return;
    }
    if (codingRate != m_config.cr) {
        m_config.cr = codingRate;
        setMode(LORA_MODE_SLEEP);
        setModemConfig1();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

uint16_t Lora::getPreambleLength() const
{
    return m_config.preambleLength;
}

void Lora::setPreambleLength(const uint16_t preambleLength)
{
    if (preambleLength < 1 || preambleLength > 65535) {
        LOG_ERROR("Invalid preamble length: %u\n", preambleLength);
        return;
    }
    if (preambleLength != m_config.preambleLength) {
        m_config.preambleLength = preambleLength;
        setMode(LORA_MODE_SLEEP);
        setPreambleLength();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

uint8_t Lora::getTxPower() const
{
    return m_config.txPower;
}

void Lora::setTxPower(const uint8_t txPower)
{
    const auto clampedTxPower = txPower < 11 ? 11 : txPower > 20 ? 20 : txPower;
    if (clampedTxPower != m_config.txPower) {
        m_config.txPower = clampedTxPower;
        setMode(LORA_MODE_SLEEP);
        setTxPower();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

uint8_t Lora::getSyncWord() const
{
    return m_config.syncWord;
}

void Lora::setSyncWord(const uint8_t syncWord)
{
    if (syncWord != m_config.syncWord) {
        m_config.syncWord = syncWord;
        setMode(LORA_MODE_SLEEP);
        setSyncWord();
        setMode(LORA_MODE_RX_CONTINUOUS);
    }
}

void Lora::updateLinkData()
{
    const int8_t snrRegValue = static_cast<int8_t>(readReg(LORA_REG_PACKET_SNR));
    m_linkData.packetSNR = snrRegValue / 4;

    const uint8_t packetRssiRegValue = readReg(LORA_REG_PACKET_RSSI);
    m_linkData.packetRSSI = m_linkData.packetSNR >= 0 ? packetRssiRegValue + LORA_RSSI_CONSTANT
                                                      : packetRssiRegValue + m_linkData.packetSNR + LORA_RSSI_CONSTANT;

    m_linkData.isValid = true;
}

const LoraLinkData& Lora::getLinkData()
{
    updateLinkData();
    return m_linkData;
}

int16_t Lora::getNoiceFloor()
{
    const uint8_t rssiRegValue = readReg(LORA_REG_RSSI);
    return rssiRegValue + LORA_RSSI_CONSTANT;
}

void Lora::writeReg(const uint8_t reg, const uint8_t value)
{
    m_cs->set(GPIO_PIN_RESET);
    uint8_t buff[2] = {(uint8_t)(reg | 0x80), value};
    if (const auto res = HAL_SPI_Transmit(m_spiDevice, buff, 2, 2000); res != 0) {
        LOG_ERROR("Error write reg: 0x%02X, with value: 0x%02X, error code: %d\n", reg, value, res);
    }
    m_cs->set(GPIO_PIN_SET);
}

void Lora::writeRegDma(const uint8_t reg, const uint8_t value)
{
    uint8_t txData[2] = {(uint8_t)(reg | 0x80), value};
    uint8_t rxData[2];
    m_cs->set(GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive_DMA(m_spiDevice, txData, rxData, 2);
    if (const auto res = osSemaphoreAcquire(loraDmaSemaphore, osWaitForever); res != 0) {
        LOG_ERROR("Error: %d\n", res);
    }
    m_cs->set(GPIO_PIN_SET);
}

uint8_t Lora::readReg(const uint8_t reg)
{
    uint8_t regValue[2];
    regValue[0] = reg & 0x7F;
    regValue[1] = 0x00;

    uint8_t value[2] = {0x00, 0x00};
    m_cs->set(GPIO_PIN_RESET);
    if (const auto res = HAL_SPI_TransmitReceive(m_spiDevice, regValue, value, 2, 2000); res != 0) {
        LOG_ERROR("Error read reg: 0x%02X, error code: %d\n", res);
    }
    m_cs->set(GPIO_PIN_SET);
    return value[1];
}

void Lora::setMode(const LoraMode mode)
{
    writeRegDma(LORA_REG_OP_MODE, LORA_MODE_LONG_RANGE_MODE | mode);
}

void Lora::setFrequency()
{
    const uint32_t freqRegValue = (m_config.frequency / 1000000 * 524288) >> 5;
    writeReg(LORA_REG_FRF_MSB, ((freqRegValue >> 16) & 0xFF));
    writeReg(LORA_REG_FRF_MID, ((freqRegValue >> 8) & 0xFF));
    writeReg(LORA_REG_FRF_LSB, (freqRegValue & 0xFF));
}

void Lora::setTxPower()
{
    const uint8_t regValue = (m_config.txPower | 0xEB);
    writeReg(LORA_REG_PA_CONFIG, regValue);
    osDelay(5);
}

void Lora::setPreambleLength()
{
    uint8_t regValue = ((m_config.preambleLength >> 8) & 0xFF);
    writeReg(LORA_REG_PREAMBLEW_MSB, regValue);
    osDelay(2);
    regValue = (m_config.preambleLength & 0xFF);
    writeReg(LORA_REG_PREAMBLEW_LSB, regValue);
    osDelay(2);
}

void Lora::setInvertIQ()
{
    uint8_t regValue = ((m_config.isInvertIQ << 6) & 0x40);
    regValue |= 0x27;
    LOG_DEBUG_2("Value: 0x%02X\n", regValue);
    writeReg(LORA_REG_INVERT_IQ, regValue);
    osDelay(2);
}

void Lora::setSyncWord()
{
    writeReg(LORA_REG_SYNC_WORD, m_config.syncWord);
    osDelay(2);
}

void Lora::setModemConfig1()
{
    uint8_t regValue = ((m_config.bandwidth << 4) & 0xF0);
    regValue |= ((m_config.cr << 1) & 0x0E);
    regValue |= (m_config.isImplicitHeader & 0x01);
    LOG_DEBUG_2("Value: 0x%02X\n", regValue);
    writeReg(LORA_REG_MODEM_CONFIG1, regValue);
    osDelay(2);
}

void Lora::setModemConfig2()
{
    uint8_t regValue = ((m_config.sf << 4) & 0xF0);
    regValue |= ((m_config.isCrcEnabled << 2) & 0x04);
    // regValue |= 0x03;
    LOG_DEBUG_2("Value: 0x%02X\n", regValue);
    writeReg(LORA_REG_MODEM_CONFIG2, regValue);
    osDelay(2);
}

void Lora::setModemConfig3()
{
    uint8_t regValue = ((m_config.isLowDataRateOptimize << 3) & 0x08);
    regValue |= ((m_config.isAgcEnabled << 2) & 0x04);
    LOG_DEBUG_2("Value: 0x%02X\n", regValue);
    writeReg(LORA_REG_MODEM_CONFIG3, regValue);
    osDelay(2);
}

void Lora::clearIrqFlags()
{
    writeRegDma(LORA_REG_IRQ_FLAGS, 0xFF);
}

void Lora::init()
{
    m_reset->set(GPIO_PIN_RESET);
    osDelay(10);
    m_reset->set(GPIO_PIN_SET);
    osDelay(10);

    const uint8_t version = readReg(LORA_REG_VERSION);
    if (version != 0x12) {
        LOG_ERROR("Wrong LoRA version: 0x%02X!!!\n", version);
        return;
    }

    setMode(LORA_MODE_SLEEP);
    osDelay(10);
    setFrequency();
    setTxPower();
    setModemConfig1();
    setModemConfig2();
    setPreambleLength();
    setModemConfig3();
    setInvertIQ();
    setSyncWord();

    writeReg(LORA_REG_SYMB_TIMEOUT_LSB, 0x08);
    writeReg(LORA_REG_DIO_MAPPING_1, 0x00);
    writeReg(LORA_REG_INVERT_IQ_2, 0x1D);
    writeReg(LORA_REG_DETECT_OPTIMIZE, 0x03);
    writeReg(LORA_REG_DETECTION_THLD, 0x0A);
    writeReg(LORA_REG_FIFO_TX_BASE_ADDR, 0x00);
    writeReg(LORA_REG_FIFO_RX_BASE_ADDR, 0x00);

    writeReg(LORA_REG_OCP, 0x32);
}

void Lora::updateConfigFromSettings()
{
    const auto& settings = SystemSettings::getSettings();
    bool isConfigChanged = m_config.bandwidth != settings.loraBandwidth;
    m_config.bandwidth = settings.loraBandwidth;
    isConfigChanged = isConfigChanged || m_config.cr != settings.loraCodingRate;
    m_config.cr = settings.loraCodingRate;
    isConfigChanged = isConfigChanged || m_config.sf != settings.loraSpreadingFactor;
    m_config.sf = settings.loraSpreadingFactor;
    isConfigChanged = isConfigChanged || m_config.preambleLength != settings.loraPreambleLen;
    m_config.preambleLength = settings.loraPreambleLen;
    isConfigChanged = isConfigChanged || m_config.txPower != settings.loraTxPower;
    m_config.txPower = settings.loraTxPower;
    isConfigChanged = isConfigChanged || m_config.frequency != settings.loraFrequency;
    m_config.frequency = settings.loraFrequency;
    isConfigChanged = isConfigChanged || m_config.syncWord != settings.loraSyncWord;
    m_config.syncWord = settings.loraSyncWord;
    if (isConfigChanged) {
        applyConfig();
    }
}

void Lora::applyConfig()
{
    setMode(LORA_MODE_SLEEP);
    osDelay(1);
    setFrequency();
    setTxPower();
    setPreambleLength();
    setInvertIQ();
    setSyncWord();
    setModemConfig1();
    setModemConfig2();
    setModemConfig3();
    setMode(LORA_MODE_RX_CONTINUOUS);
}

void Lora::onDataRX()
{
    m_rxDataLength = readReg(LORA_REG_RX_NB_BYTES);
    const uint8_t currentAddr = readReg(LORA_REG_FIFO_RX_CURRENT_ADDR);
    writeRegDma(LORA_REG_FIFO_ADDR_PTR, currentAddr);
    m_txBuffer[0] = 0x00;
    m_txBuffer[1] = 0x7F;
    m_cs->set(GPIO_PIN_RESET);
    HAL_SPI_TransmitReceive_DMA(m_spiDevice, m_txBuffer, m_rxBuffer, m_rxDataLength + 1);
    const auto res = osSemaphoreAcquire(loraDmaSemaphore, osWaitForever);
    m_cs->set(GPIO_PIN_SET);

    if (res == osOK) {
        LOG_DEBUG("<<=== Lora received %d bytes: [", m_rxDataLength);
        for (uint8_t i = 1; i <= m_rxDataLength; i++) {
            LOG_DEBUG_PURE(i < m_rxDataLength ? "%02X:" : "%02X", m_rxBuffer[i]);
        }
        LOG_DEBUG_PURE("]\n");
        if (m_rxCallback != nullptr) {
            m_rxCallback(&m_rxBuffer[1], m_rxDataLength);
        }
    } else {
        LOG_ERROR("Error: %d\n", res);
    }
}

void Lora::onDataTX()
{
    LOG_DEBUG("===>> Lora transmit %d bytes.", m_txDataLength);
    LOG_DEBUG_2_PURE(" Data: [");
    for (uint8_t i = 1; i <= m_txDataLength; i++) {
        LOG_DEBUG_2_PURE(i < m_txDataLength ? "%02X:" : "%02X", m_txBuffer[i]);
    }
    LOG_DEBUG_2_PURE("]");
    LOG_DEBUG_PURE("\n");
    setMode(LORA_MODE_SLEEP);
    writeRegDma(LORA_REG_FIFO_ADDR_PTR, 0x00);
    writeRegDma(LORA_REG_PAYLOAD_LENGTH, m_txDataLength);
    m_txBuffer[0] = 0x00 | 0x80;
    m_cs->set(GPIO_PIN_RESET);
    HAL_SPI_Transmit_DMA(m_spiDevice, m_txBuffer, m_txDataLength + 1);
    osSemaphoreAcquire(loraDmaSemaphore, osWaitForever);
    m_cs->set(GPIO_PIN_SET);
    writeRegDma(LORA_REG_DIO_MAPPING_1, 0x40);
    setMode(LORA_MODE_TX);
}
