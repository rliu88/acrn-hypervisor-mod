/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <msr.h>
#include <softirq.h>
#include <irq.h>
#include <apicreg.h>
#include <cpu.h>

/* run in interrupt context */
void timer_expired_handler(__unused uint32_t irq, __unused void *data)
{
	fire_softirq(SOFTIRQ_TIMER);
}

void set_timeout(uint64_t timeout)
{
	msr_write(MSR_IA32_TSC_DEADLINE, timeout);
}

void init_hw_timer(void)
{
	uint32_t val;

	val = TIMER_VECTOR;
	val |= APIC_LVTT_TM_TSCDLT; /* TSC deadline and unmask */
	msr_write(MSR_IA32_EXT_APIC_LVT_TIMER, val);
	cpu_memory_barrier();

	/* disarm timer */
	msr_write(MSR_IA32_TSC_DEADLINE, 0UL);
}