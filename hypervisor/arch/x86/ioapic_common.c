/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

//#include <types.h>
//#include <errno.h>
//#include <x86/lib/spinlock.h>
#include <x86/ioapic_common.h>
//#include <x86/irq.h>
#include <x86/pgtable.h>
//#include <x86/io.h>
//#include <x86/mmu.h>
#include <acpi.h>

void *map_ioapic(uint64_t ioapic_paddr)
{
	/* At some point we may need to translate this paddr to a vaddr.
	 * 1:1 mapping for now.
	 */
	return hpa2hva(ioapic_paddr);
}

void ioapic_get_rte_entry(void *ioapic_base, uint32_t pin, union ioapic_rte *rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	rte->u.lo_32 = ioapic_read_reg32(ioapic_base, rte_addr);
	rte->u.hi_32 = ioapic_read_reg32(ioapic_base, rte_addr + 1U);
}
