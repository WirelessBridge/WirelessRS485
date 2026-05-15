/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once
#include <stdint.h>

#define LORA_REG_OP_MODE                0x01
#define LORA_REG_FRF_MSB                0x06
#define LORA_REG_FRF_MID                0x07
#define LORA_REG_FRF_LSB                0x08
#define LORA_REG_PA_CONFIG              0x09
#define LORA_REG_OCP                    0x0B
#define LORA_REG_LNA                    0x0C
#define LORA_REG_FIFO_ADDR_PTR          0x0D
#define LORA_REG_FIFO_TX_BASE_ADDR      0x0E
#define LORA_REG_FIFO_RX_BASE_ADDR      0x0F
#define LORA_REG_FIFO_RX_CURRENT_ADDR   0x10
#define LORA_REG_IRQ_FLAGS_MASK         0x11
#define LORA_REG_IRQ_FLAGS              0x12
#define LORA_REG_RX_NB_BYTES            0x13
#define LORA_REG_MODEM_STATUS           0x18
#define LORA_REG_PACKET_SNR             0x19
#define LORA_REG_PACKET_RSSI            0x1A
#define LORA_REG_RSSI                   0x1B
#define LORA_REG_MODEM_CONFIG1          0x1D
#define LORA_REG_MODEM_CONFIG2          0x1E
#define LORA_REG_SYMB_TIMEOUT_LSB       0x1F
#define LORA_REG_PREAMBLEW_MSB          0x20
#define LORA_REG_PREAMBLEW_LSB          0x21
#define LORA_REG_PAYLOAD_LENGTH         0x22
#define LORA_REG_MAX_PAYLOAD_LENGTH     0x23
#define LORA_REG_FIFO_DATA_ADDR         0x25
#define LORA_REG_MODEM_CONFIG3          0x26
#define LORA_REG_DETECT_OPTIMIZE        0x31
#define LORA_REG_INVERT_IQ              0x33
#define LORA_REG_DETECTION_THLD         0x37
#define LORA_REG_SYNC_WORD              0x39
#define LORA_REG_INVERT_IQ_2            0x3B
#define LORA_REG_DIO_MAPPING_1          0x40
#define LORA_REG_VERSION                0x42

#define LORA_MODE_LONG_RANGE_MODE       0x80

#define LORA_OSC_FREQUENCY              32000000
#define LORA_RSSI_CONSTANT              -157

enum LoraMode
{
    LORA_MODE_SLEEP,
    LORA_MODE_STDBY,
    LORA_MODE_FSTX,
    LORA_MODE_TX,
    LORA_MODE_FSRX,
    LORA_MODE_RX_CONTINUOUS,
    LORA_MODE_RX_SINGLE,
    LORA_MODE_CAD
};

enum LoraBW : uint8_t
{
    BW_7800_HZ = 0x00,
    BW_10400_HZ,
    BW_15600_HZ,
    BW_20800_HZ,
    BW_31250_HZ,
    BW_41700_HZ,
    BW_62500_HZ,
	BW_125_KHZ,
	BW_250_KHZ,
	BW_500_KHZ,
};

enum LoraSF : uint8_t
{
	SF_6 = 0x06,
	SF_7,
	SF_8,
	SF_9,
	SF_10,
	SF_11,
	SF_12
};

enum LoraCR : uint8_t
{
	CR_4_5 = 0x01,
	CR_4_6,
	CR_4_7,
	CR_4_8 
};

struct LoraConfig
{
    uint32_t frequency = 434000000;
    LoraBW bandwidth = BW_125_KHZ;
    LoraSF sf = SF_7;
    LoraCR cr = CR_4_5;
    uint16_t preambleLength = 8;
    uint8_t txPower = 12;
    uint8_t syncWord = 0x12;
    bool isImplicitHeader = false;
    bool isCrcEnabled = true;
    bool isAgcEnabled = true;
    bool isInvertIQ = false;
    bool isLowDataRateOptimize = false;
};

struct LoraLinkData
{
    int8_t packetSNR;
    int8_t packetRSSI;
    bool isValid = false;
};

using LoraCallback = void(*)(const uint8_t* data, const uint16_t length);