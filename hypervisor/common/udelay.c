/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <timecount.h>

void udelay(uint32_t us)
{
	uint64_t dest_timecnt, delta_timecnt;

	/* Calculate number of ticks to wait */
	delta_timecnt = us_to_ticks(us);
	dest_timecnt = get_timecount() + delta_timecnt;

	/* Loop until time expired */
	while (get_timecount() < dest_timecnt) {
	}
}