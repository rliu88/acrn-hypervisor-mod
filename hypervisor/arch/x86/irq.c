/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <x86/per_cpu.h>
#include <x86/io.h>
#include <x86/irq.h>
#include <x86/idt.h>
#include <x86/ioapic.h>
#include <x86/lapic.h>
#include <dump.h>
#include <logmsg.h>
#include <x86/vmx.h>

static spinlock_t x86_exception_spinlock = { .head = 0U, .tail = 0U, };
static spinlock_t x86_irq_spinlock = { .head = 0U, .tail = 0U, };

spurious_handler_t spurious_handler;

static uint32_t vector_to_irq[NR_MAX_VECTOR + 1];

static struct x86_irq_data irq_data[NR_IRQS];

static struct {
	uint32_t irq;
	uint32_t vector;
} irq_static_mappings[NR_STATIC_MAPPINGS] = {
	{ TIMER_IRQ, TIMER_VECTOR },
	{ NOTIFY_VCPU_IRQ, NOTIFY_VCPU_VECTOR },
	{ PMI_IRQ, PMI_VECTOR },

	/* To be initialized at runtime in init_irq_descs() */
	[ NR_STATIC_MAPPINGS_1 ... (NR_STATIC_MAPPINGS - 1U) ] = {},
};

/*
 * alloc an vectror and bind it to irq
 * for legacy_irq (irq num < 16) and static mapped ones, do nothing
 * if mapping is correct.
 * retval: valid vector num on susccess, VECTOR_INVALID on failure.
 */
uint32_t alloc_irq_vector(uint32_t irq)
{
	struct x86_irq_data *irqd;
	uint64_t rflags;
	uint32_t vr = VECTOR_INVALID;
	uint32_t ret = VECTOR_INVALID;

	if (irq < NR_IRQS) {
		irqd = &irq_data[irq];
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);

		if (irqd->vector <= NR_MAX_VECTOR) {
			if (vector_to_irq[irqd->vector] == irq) {
				/* statically bound */
				vr = irqd->vector;
			} else {
				pr_err("[%s] irq[%u]:vector[%u] mismatch",
					__func__, irq, irqd->vector);
			}
		} else {
			/*
			 * alloc a vector between:
			 *   VECTOR_DYNAMIC_START ~ VECTOR_DYNAMC_END
			 */

			for (vr = VECTOR_DYNAMIC_START;
				vr <= VECTOR_DYNAMIC_END; vr++) {
				if (vector_to_irq[vr] == IRQ_INVALID) {
					irqd->vector = vr;
					vector_to_irq[vr] = irq;
					break;
				}
			}

			vr = (vr > VECTOR_DYNAMIC_END) ? VECTOR_INVALID : vr;
		}
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
		ret = vr;
	} else {
		pr_err("invalid irq[%u] to alloc vector", irq);
	}
	return ret;
}

uint32_t irq_to_vector(uint32_t irq)
{
	uint64_t rflags;
	uint32_t ret = VECTOR_INVALID;

	if (irq < NR_IRQS) {
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);
		ret = irq_data[irq].vector;
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
	}

	return ret;
}

bool request_irq_arch(uint32_t irq)
{
	return (alloc_irq_vector(irq) != VECTOR_INVALID);
}

/* free the vector allocated via alloc_irq_vector() */
static void free_irq_vector(uint32_t irq)
{
	struct x86_irq_data *irqd;
	uint32_t vr;
	uint64_t rflags;

	if ((irq >= NR_LEGACY_IRQ) && (irq < NR_IRQS)) {
		irqd = &irq_data[irq];
		spinlock_irqsave_obtain(&x86_irq_spinlock, &rflags);

		if (irqd->vector < VECTOR_FIXED_START) {
			/* do nothing for LEGACY_IRQ and statically allocated ones */
			vr = irqd->vector;
			irqd->vector = VECTOR_INVALID;

			if (vr <= NR_MAX_VECTOR && vector_to_irq[vr] == irq) {
				vector_to_irq[vr] = IRQ_INVALID;
			}
		}
		spinlock_irqrestore_release(&x86_irq_spinlock, rflags);
	}
}

void free_irq_arch(uint32_t irq)
{
	if (irq < NR_IRQS) {
		dev_dbg(DBG_LEVEL_IRQ, "[%s] irq%d vr:0x%x",
			__func__, irq, irq_to_vector(irq));
		free_irq_vector(irq);
	}
}

static inline bool irq_need_mask(const struct irq_desc *desc)
{
	/* level triggered gsi should be masked */
	return (((desc->flags & IRQF_LEVEL) != 0U)
		&& is_ioapic_irq(desc->irq));
}

static inline bool irq_need_unmask(const struct irq_desc *desc)
{
	/* level triggered gsi for non-ptdev should be unmasked */
	return (((desc->flags & IRQF_LEVEL) != 0U)
		&& ((desc->flags & IRQF_PT) == 0U)
		&& is_ioapic_irq(desc->irq));
}

void pre_irq_arch(const struct irq_desc *desc)
{
	if (irq_need_mask(desc))  {
		ioapic_gsi_mask_irq(desc->irq);
	}
}

void eoi_irq_arch(__unused const struct irq_desc *desc)
{
	/* Send EOI to LAPIC/IOAPIC IRR */
	send_lapic_eoi();
}

void post_irq_arch(const struct irq_desc *desc)
{
	if (irq_need_unmask(desc)) {
		ioapic_gsi_unmask_irq(desc->irq);
	}
}

static void handle_spurious_interrupt(uint32_t vector)
{
	send_lapic_eoi();

	get_cpu_var(spurious)++;

	pr_warn("Spurious vector: 0x%x.", vector);

	if (spurious_handler != NULL) {
		spurious_handler(vector);
	}
}

void dispatch_interrupt(const struct intr_excp_ctx *ctx)
{
	uint32_t vr = ctx->vector;
	uint32_t irq = vector_to_irq[vr];
	struct x86_irq_data *irqd;

	/*
	 * The value from vector_to_irq[] must be:
	 *
	 * IRQ_INVALID, which means the vector is not allocated;
	 * or
	 * < NR_IRQS, which is the irq number it's bound to;
	 * Any other value means there is something wrong.
	 */
	if (irq < NR_IRQS) {
		irqd = &irq_data[irq];

		if (vr == irqd->vector) {
#ifdef PROFILING_ON
			/* Save ctx info into irq_data */
			irqd->ctx_rip = ctx->rip;
			irqd->ctx_rflags = ctx->rflags;
			irqd->ctx_cs = ctx->cs;
#endif
			do_irq(irq);
		}
	} else {
		handle_spurious_interrupt(vr);
	}
}

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

void handle_nmi(__unused struct intr_excp_ctx *ctx)
{
	uint32_t value32;

	/*
	 * There is a window where we may miss the current request in this
	 * notification period when the work flow is as the following:
	 *
	 *       CPUx +                   + CPUr
	 *            |                   |
	 *            |                   +--+
	 *            |                   |  | Handle pending req
	 *            |                   <--+
	 *            +--+                |
	 *            |  | Set req flag   |
	 *            <--+                |
	 *            +------------------>---+
	 *            |     Send NMI      |  | Handle NMI
	 *            |                   <--+
	 *            |                   |
	 *            |                   |
	 *            |                   +--> vCPU enter
	 *            |                   |
	 *            +                   +
	 *
	 * So, here we enable the NMI-window exiting to trigger the next vmexit
	 * once there is no "virtual-NMI blocking" after vCPU enter into VMX non-root
	 * mode. Then we can process the pending request on time.
	 */
	value32 = exec_vmread32(VMX_PROC_VM_EXEC_CONTROLS);
	value32 |= VMX_PROCBASED_CTLS_NMI_WINEXIT;
	exec_vmwrite32(VMX_PROC_VM_EXEC_CONTROLS, value32);
}

/* XXX lockless operation */
bool irq_allocated_arch(struct irq_desc *desc)
{
	struct x86_irq_data *irqd = NULL;
	uint32_t irq = IRQ_INVALID;
	uint32_t vr = VECTOR_INVALID;

	if (desc != NULL) {
		irq = desc->irq;
		irqd = desc->arch_data;
	}

	if (irqd != NULL) {
		vr = irqd->vector;
	}

	return ((irq < NR_IRQS) && (vr <= NR_MAX_VECTOR) &&
		(vector_to_irq[vr] == irq));
}

/*
 * descs[] must have NR_IRQS entries
 */
void init_irq_descs_arch(struct irq_desc descs[])
{
	uint32_t i;

	/*
	 * Fill in #CONFIG_MAX_VM_NUM posted interrupt specific irq and vector pairs
	 * at runtime
	 */
	for (i = 0U; i < CONFIG_MAX_VM_NUM; i++) {
		uint32_t idx = i + NR_STATIC_MAPPINGS_1;

		ASSERT(irq_static_mappings[idx].irq == 0U, "");
		ASSERT(irq_static_mappings[idx].vector == 0U, "");

		irq_static_mappings[idx].irq = POSTED_INTR_IRQ + i;
		irq_static_mappings[idx].vector = POSTED_INTR_VECTOR + i;
	}

	for (i = 0U; i < NR_IRQS; i++) {
		irq_data[i].vector = VECTOR_INVALID;
		descs[i].arch_data = &irq_data[i];
	}

	for (i = 0U; i < ARRAY_SIZE(vector_to_irq); i++) {
		vector_to_irq[i] = IRQ_INVALID;
	}

	/* init fixed mapping for specific irq and vector */
	for (i = 0U; i < ARRAY_SIZE(irq_static_mappings); i++) {
		uint32_t irq = irq_static_mappings[i].irq;
		uint32_t vr = irq_static_mappings[i].vector;

		/* XXX irq0 -> vec0 ? */
		irq_data[irq].vector = vr;
		vector_to_irq[vr] = irq;
	}
}

static void disable_pic_irqs(void)
{
	pio_write8(0xffU, 0xA1U);
	pio_write8(0xffU, 0x21U);
}

static inline void fixup_idt(const struct host_idt_descriptor *idtd)
{
	uint32_t i;
	struct idt_64_descriptor *idt_desc = idtd->idt->host_idt_descriptors;
	uint32_t entry_hi_32, entry_lo_32;

	for (i = 0U; i < HOST_IDT_ENTRIES; i++) {
		entry_lo_32 = idt_desc[i].offset_63_32;
		entry_hi_32 = idt_desc[i].rsvd;
		idt_desc[i].rsvd = 0U;
		idt_desc[i].offset_63_32 = entry_hi_32;
		idt_desc[i].high32.bits.offset_31_16 = entry_lo_32 >> 16U;
		idt_desc[i].low32.bits.offset_15_0 = entry_lo_32 & 0xffffUL;
	}
}

static inline void set_idt(struct host_idt_descriptor *idtd)
{
	asm volatile ("   lidtq %[idtd]\n" :	/* no output parameters */
		      :		/* input parameters */
		      [idtd] "m"(*idtd));
}

void init_interrupt_arch(uint16_t pcpu_id)
{
	struct host_idt_descriptor *idtd = &HOST_IDTR;

	if (pcpu_id == BSP_CPU_ID) {
		fixup_idt(idtd);
	}
	set_idt(idtd);
	init_lapic(pcpu_id);

	if (pcpu_id == BSP_CPU_ID) {
		/* we use ioapic only, disable legacy PIC */
		disable_pic_irqs();
		ioapic_setup_irqs();
	}
}
