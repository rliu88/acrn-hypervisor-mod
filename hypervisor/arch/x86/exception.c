/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/irq.h>
#include <dump.h>
#include <logmsg.h>

static spinlock_t x86_exception_spinlock = { .head = 0U, .tail = 0U, };

void dispatch_exception(struct intr_excp_ctx *ctx)
{
	uint16_t pcpu_id = get_pcpu_id();

	/* Obtain lock to ensure exception dump doesn't get corrupted */
	spinlock_obtain(&x86_exception_spinlock);

	/* Dump exception context */
	dump_exception(ctx, pcpu_id);

	/* Release lock to let other CPUs handle exception */
	spinlock_release(&x86_exception_spinlock);

	/* Halt the CPU */
	cpu_dead();
}
