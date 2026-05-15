/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#include "app.h"

#include "printf.h"
#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "usart.h"

#include "fonts.h"
#include "ssd1306.h"

#include "UI/view.h"
#include "gpio_facade.h"

#include "logger.h"
#include "lora.h"
#include "system_settings.h"
#include "uart.h"

extern "C" {
void startLoraTask(void*)
{
    App::startLoraThread();
}

void startUartTask(void*)
{
    App::startUartThread();
}

void startUITask(void*)
{
    App::startUIThread();
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef* hspi)
{
    if (hspi == &hspi1) {
        Lora::onSpiTxRxDone();
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef* hspi)
{
    if (hspi == &hspi1) {
        Lora::onSpiTxRxDone();
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    Uart::onDataTransmitted(huart, App::s_uart);
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
{
    Uart::onDataReceived(huart, Size, App::s_uart);
}
}

View* App::s_view = nullptr;
Lora* App::s_lora = nullptr;
Uart* App::s_uart = nullptr;
Settings* App::s_settings = nullptr;
TimerHandle_t App::s_checkNoiseTimer = nullptr;

void App::startLoraThread()
{
    LOG_INFO("======= Start LoRA thread =======\n");
    Gpio cs(LORA_CS_GPIO_Port, LORA_CS_Pin);
    Gpio reset(LORA_RESET_GPIO_Port, LORA_RESET_Pin);
    Gpio irq(LORA_IRQ_GPIO_Port, LORA_IRQ_Pin);

    getSettings();

    Lora lora(&hspi1, &cs, &reset, &irq);
    s_lora = &lora;

    const auto checkNoisePeriod = s_settings->checkNoisePeriod > 0 ? s_settings->checkNoisePeriod : CHECK_NOISE_DEFAULT_PERIOD;
    s_checkNoiseTimer = xTimerCreate("CheckNoiseTimer", pdMS_TO_TICKS(checkNoisePeriod), pdTRUE, NULL, [](TimerHandle_t) {
        if (s_view != nullptr && s_lora != nullptr) {
            s_view->onLoraNoiseFloorUpdated(s_lora->getNoiceFloor());
        }
    });

    if (s_settings->workMode == NOICE_MEASUREMENT_MODE) {
        if (s_checkNoiseTimer != nullptr) {
            xTimerStart(s_checkNoiseTimer, 0);
        } else {
            LOG_ERROR("Failed to create noise measurement timer!\n");
        }
    }

    SystemSettings::registerCallback(
        [](void* userData) {
            if (s_checkNoiseTimer == nullptr) {
                return;
            }
            if (s_settings->workMode == NOICE_MEASUREMENT_MODE) {
                if (xTimerIsTimerActive(s_checkNoiseTimer) == pdFALSE) {
                    xTimerStart(s_checkNoiseTimer, 0);
                }
            } else if (xTimerIsTimerActive(s_checkNoiseTimer) == pdTRUE) {
                xTimerStop(s_checkNoiseTimer, 0);
            }

            if (s_settings->checkNoisePeriod != xTimerGetPeriod(s_checkNoiseTimer) / pdMS_TO_TICKS(1)) {
                xTimerChangePeriod(s_checkNoiseTimer, pdMS_TO_TICKS(s_settings->checkNoisePeriod), 0);
            }
        },
        nullptr);

    lora.startThread(
        [](const uint8_t* data, const uint16_t length) {
            switch (s_settings->workMode) {
            case BRIDGE_MODE:
                if (s_uart != nullptr) {
                    s_uart->send(data, length);
                }
                if (s_view != nullptr) {
                    s_view->onLoraPacketReceived(s_lora->getLinkData());
                }
                break;
            default:
                break;
            }
        },
        [](const uint8_t* data, const uint16_t length) {
            // switch (s_settings->workMode) {
            // case BRIDGE_MODE:
            //     if (s_view != nullptr) {
            //         s_view->onLoraPacketTransmitted();
            //     }
            //     break;
            // default:
            //     break;
            // }
        });
}

void App::startUartThread()
{
    LOG_INFO("======= Start UART thread =======\n");

    Uart uart(&huart1);
    s_uart = &uart;

    getSettings();
    uart.setBaudrate(SystemSettings::getSettings().rsBaudrate);
    uart.setStopBits(SystemSettings::getSettings().rsStopBits);

    uart.startThread(
        [](const uint8_t* data, const uint16_t length) {
            switch (s_settings->workMode) {
            case BRIDGE_MODE:
                if (s_lora != nullptr) {
                    s_lora->send(data, length);
                }
                break;
            default:
                break;
            }
        },
        nullptr);
}

void App::startUIThread()
{
    LOG_INFO("======= Start UI thread =======\n");

    Gpio btnMainGpio(BTN_MAIN_GPIO_Port, BTN_MAIN_Pin);
    Gpio btnDownGpio(BTN_DOWN_GPIO_Port, BTN_DOWN_Pin);
    Gpio btnUpGpio(BTN_UP_GPIO_Port, BTN_UP_Pin);

    SSD1306 display(&hi2c1, 0x3C);
    View view(&display, &btnMainGpio, &btnDownGpio, &btnUpGpio);
    s_view = &view;
    view.startThread();
}

void App::getSettings()
{
    if (s_settings == nullptr) {
        s_settings = const_cast<Settings*>(&SystemSettings::getSettings());
    }
}

#if SHOW_RUNTIME_STATS
extern "C" {
void startMainTask(void*)
{
    LOG_INFO("======= Start main thread =======\n");
    char buffer[256];

    for (;;) {
        vTaskGetRunTimeStats(buffer);
        printf("Counter: %lu %lu\n", getRunTimeCounterValue(), HAL_GetTick());

        printf("\n------------------------------------------------------\n");
        printf("\nName\t\t\t\t\t\tCPU Time\tCPU %%\n");
        printf("------------------------------------------------------\n");
        printf("%s", buffer);
        printf("------------------------------------------------------\n\n");
        printf("\nName\t\t\t\tStatus\tPrio\tStack\tID\n");
        printf("------------------------------------------------------\n");
        vTaskList(buffer);
        printf("%s", buffer);
        printf("------------------------------------------------------\n\n");

        uint32_t freeHeap = xPortGetFreeHeapSize();
        printf("Free Heap size:\t%lu bytes\n", freeHeap);

        uint32_t minEverFreeHeap = xPortGetMinimumEverFreeHeapSize();
        printf("Min free Heap size:\t%lu bytes\n", minEverFreeHeap);

        printf("\n===================================================\n\n");
        osDelay(RUNTIME_STATS_UPDATE_PERIOD_MS);
    }
}
}
#endif