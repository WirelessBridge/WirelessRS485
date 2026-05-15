/* 
 *  SPDX-License-Identifier: MIT
 *  
 *  Copyright (c) 2026 Vasyl Dykyi <wasyl.dykyi@gmail.com>
 */

#pragma once

#include <ctype.h>
#include <string.h>
#include "printf.h"

#define LOG_LEVEL_NO_LOGS 0
#define LOG_LEVEL_ERROR 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_INFO 3
#define LOG_LEVEL_DEBUG 4
#define LOG_LEVEL_DEBUG_2 5

#ifndef CURRENT_LOG_LEVEL
#define CURRENT_LOG_LEVEL LOG_LEVEL_NO_LOGS
#endif

#define LOG_IMPL(level, func, fmt, ...)                                 \
    {                                                                   \
        uint8_t startIndex = 0;                                         \
        char* firstBracket = strchr(func, '(');                         \
        if (firstBracket == nullptr) {                                  \
            printf(fmt __VA_OPT__(, ) __VA_ARGS__);                     \
        } else {                                                        \
            const size_t bracketIndex = firstBracket - func;            \
            for (size_t i = bracketIndex; i > 0; i--) {                 \
                if (func[i] == ' ') {                                   \
                    startIndex = i + 1;                                 \
                    break;                                              \
                }                                                       \
            }                                                           \
            char buff[strlen(func)];                                    \
            memcpy(buff, &func[startIndex], bracketIndex - startIndex); \
            buff[bracketIndex - startIndex] = '\0';                     \
            printf("[%s][%s] ", level, buff);                           \
            printf(fmt __VA_OPT__(, ) __VA_ARGS__);                     \
        }                                                               \
    }

#define LOG_ERROR(fmt, ...)                   \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR) \
    LOG_IMPL("ERR", __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

#define LOG_WARNING(fmt, ...)                   \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_WARNING) \
    LOG_IMPL("WRN", __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

#define LOG_INFO(fmt, ...)                   \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO) \
    LOG_IMPL("INF", __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

#define LOG_INFO_PURE(fmt, ...)              \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO) \
    printf(fmt __VA_OPT__(, ) __VA_ARGS__)

#define LOG_DEBUG(fmt, ...)                   \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) \
    LOG_IMPL("DBG", __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

#define LOG_DEBUG_PURE(fmt, ...)              \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG) \
    printf(fmt __VA_OPT__(, ) __VA_ARGS__)

#define LOG_DEBUG_2(fmt, ...)                   \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG_2) \
    LOG_IMPL("DBG_2", __PRETTY_FUNCTION__, fmt, __VA_ARGS__)

#define LOG_DEBUG_2_PURE(fmt, ...)              \
    if (CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG_2) \
    printf(fmt __VA_OPT__(, ) __VA_ARGS__)
