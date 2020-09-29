/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOAPIC_COMMON_H
#define IOAPIC_COMMON_H

#include <types.h>
#include <x86/lib/spinlock.h>
#include <x86/ioapic.h>
#include <x86/io.h>
#include <logmsg.h>

extern spinlock_t ioapic_lock;

void *map_ioapic(uint64_t ioapic_paddr);
void ioapic_get_rte_entry(void *ioapic_base, uint32_t pin, union ioapic_rte *rte);

static inline uint32_t
ioapic_read_reg32(void *ioapic_base, const uint32_t offset)
{
	uint32_t v;
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Read  IOWIN */
	v = mmio_read32(ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
	return v;
}

static inline void
ioapic_write_reg32(void *ioapic_base, const uint32_t offset, const uint32_t value)
{
	uint64_t rflags;

	spinlock_irqsave_obtain(&ioapic_lock, &rflags);

	/* Write IOREGSEL */
	mmio_write32(offset, ioapic_base + IOAPIC_REGSEL);
	/* Write IOWIN */
	mmio_write32(value, ioapic_base + IOAPIC_WINDOW);

	spinlock_irqrestore_release(&ioapic_lock, rflags);
}

static inline void
ioapic_set_rte_entry(void *ioapic_base,
		uint32_t pin, union ioapic_rte rte)
{
	uint32_t rte_addr = (pin * 2U) + 0x10U;
	ioapic_write_reg32(ioapic_base, rte_addr, rte.u.lo_32);
	ioapic_write_reg32(ioapic_base, rte_addr + 1U, rte.u.hi_32);
}
#endif /* IOAPIC_COMMON_H */
