/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <timecount.h>

uint64_t us_to_ticks(uint32_t us)
{
	return (((uint64_t)us * (uint64_t)get_frequency_khz()) / 1000UL);
}

uint64_t ticks_to_us(uint64_t ticks)
{
	uint64_t us = 0UL;

	if (get_frequency_khz() != 0U ) {
		us = (ticks * 1000UL) / (uint64_t)get_frequency_khz();
	}

	return us;
}

uint64_t ticks_to_ms(uint64_t ticks)
{
	return ticks / (uint64_t)get_frequency_khz();
}