/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TIMECOUNT_H
#define TIMECOUNT_H

#include <types.h>

#define MIN_TIMER_PERIOD_US	500U

#define CYCLES_PER_MS	us_to_ticks(1000U)

/**
 * @brief  Get frequency.
 *
 * @return frequency(KHz)
 */
uint32_t get_frequency_khz(void);

/**
 * @brief Get timecount.
 *
 * @return timecount in ticks
 */
uint64_t get_timecount(void);

/**
 * @brief convert us to timeticks.
 *
 * @return ticks
 */
uint64_t us_to_ticks(uint32_t us);

/**
 * @brief convert ticks to us.
 *
 * @return microsecond
 */
uint64_t ticks_to_us(uint64_t ticks);

/**
 * @brief convert ticks to ms.
 *
 * @return millisecond
 */
uint64_t ticks_to_ms(uint64_t ticks);
#endif