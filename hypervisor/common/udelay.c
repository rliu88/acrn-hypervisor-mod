/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <cycles.h>

void udelay(uint32_t us)
{
	uint64_t dest_timecnt, delta_timecnt;

	/* Calculate number of ticks to wait */
	delta_timecnt = us_to_cycles(us);
	dest_timecnt = get_cpu_cycles() + delta_timecnt;

	/* Loop until time expired */
	while (get_cpu_cycles() < dest_timecnt) {
	}
}