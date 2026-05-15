/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

// #include <usbd_cdc_if.h>
// #include "stm32f1xx_hal.h"
// #include "stm32f1xx_it.h"
#include "SEGGER_RTT.h"
#include "printf.h"

// ---------- RTT ----------
int _write(int file, char *ptr, int len) {
    SEGGER_RTT_Write(0, ptr, len);
    return len;
}

// ---------- Tiny printf ----------
void _putchar(char character)
{
    SEGGER_RTT_Write(0, &character, 1);
}

// ---------- SWO ----------
// int _write(int file, char *ptr, int len) {
//     for (int i = 0; i < len; i++) {
//         ITM_SendChar((*(ptr + 1)));
//     }
// }


// ---------- UART ----------
// extern UART_HandleTypeDef huart1;
// int _write(int file, char *ptr, int len) {
//     if (file == STDOUT_FILENO || file == STDERR_FILENO) {
//         HAL_UART_Transmit(&huart1, (uint8_t*)ptr, len, HAL_MAX_DELAY);
//     }
//     return len;
// }


// ---------- USB CDC -------
// int _write(int file, char *ptr, int len) {
//     extern USBD_HandleTypeDef hUsbDeviceFS;
//     if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
//         return len; 
//     }

//     USBD_CDC_HandleTypeDef *hcdc = (USBD_CDC_HandleTypeDef*)hUsbDeviceFS.pClassData;
    
//     uint8_t result = CDC_Transmit_FS((uint8_t*)ptr, len);
    
//     uint32_t timeout = 0xFFFF; 
//     while (hcdc->TxState != 0 && timeout--) {
//     }

//     if (result == USBD_OK) return len;
//     return -1;
// }
