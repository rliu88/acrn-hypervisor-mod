/*
 * Copyright (C) 2019 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <x86/guest/vm.h>
#include <x86/per_cpu.h>
#include <vacpi.h>
#include <x86/pgtable.h>
#include <platform_acpi_info.h>

/* ACPI tables for pre-launched VM and SOS */
static struct acpi_table_info acpi_table_template[CONFIG_MAX_VM_NUM] = {
	[0U ... (CONFIG_MAX_VM_NUM - 1U)] = {
		.rsdp = {
			.signature = ACPI_SIG_RSDP,
			.oem_id = ACPI_OEM_ID,
			.revision = 0x2U,
			.length = ACPI_RSDP_XCHECKSUM_LENGTH,
			.xsdt_physical_address = ACPI_XSDT_ADDR,
		},
		.xsdt = {
			.header.revision = 0x1U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_XSDT,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNXSDT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,

			.table_offset_entry[0] = ACPI_MADT_ADDR,
		},
		.fadt = {
			.header.revision = 0x3U,
			.header.length = 0xF4U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_FADT,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNMADT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,

			.dsdt = ACPI_DSDT_ADDR,

			.pm1a_event_block = PM1A_EVT_ADDRESS,
			.pm1a_control_block = PM1A_CNT_ADDRESS,
			.pm1_event_length = 0x4U,
			.pm1_control_length = 0x02U,

			.flags = 0x00001125U,	/* HEADLESS | TMR_VAL_EXT | SLP_BUTTON | PROC_C1 | WBINVD */
		},
		.dsdt = {
			.revision = 0x3U,
			.length = sizeof(struct acpi_table_header),
			.oem_revision = 0x1U,
			.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.signature = ACPI_SIG_DSDT,
			.oem_id = ACPI_OEM_ID,
			.oem_table_id = "ACRNMADT",
			.asl_compiler_id = ACPI_ASL_COMPILER_ID,
		},
		.mcfg = {
			.header.revision = 0x3U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_MCFG,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNMADT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,
		},
		.mcfg_entry = {
			.address = VIRT_PCI_MMCFG_BASE,
			.pci_segment = 0U,
			.start_bus_number = 0x0U,
			.end_bus_number = 0xFFU,
		},
		.madt = {
			.header.revision = 0x3U,
			.header.oem_revision = 0x1U,
			.header.asl_compiler_revision = ACPI_ASL_COMPILER_VERSION,
			.header.signature = ACPI_SIG_MADT,
			.header.oem_id = ACPI_OEM_ID,
			.header.oem_table_id = "ACRNMADT",
			.header.asl_compiler_id = ACPI_ASL_COMPILER_ID,

			.address = 0xFEE00000U, /* Local APIC Address */
			.flags = 0x1U, /* PC-AT Compatibility=1 */
		},
		.ioapic_struct = {
			.header.type = ACPI_MADT_TYPE_IOAPIC,
			.header.length = sizeof(struct acpi_madt_ioapic),
			.id = 0x1U,
			.addr = VIOAPIC_BASE,
		},
		.lapic_nmi = {
			.header.type = ACPI_MADT_TYPE_LOCAL_APIC_NMI,
			.header.length = sizeof(struct acpi_madt_local_apic_nmi),
			.processor_id = 0xFFU,
			.flags = 0x5U,
			.lint = 0x1U,
		},
		.lapic_array = {
			[0U ... (MAX_PCPU_NUM - 1U)] = {
				.header.type = ACPI_MADT_TYPE_LOCAL_APIC,
				.header.length = sizeof(struct acpi_madt_local_apic),
				.lapic_flags = 0x1U, /* Processor Enabled=1, Runtime Online Capable=0 */
			}
		},
	}
};

/**
 * @pre vm != NULL
 * @pre vm->vm_id < CONFIG_MAX_VM_NUM
 * @pre (vm->min_mem_addr <= ACPI_XSDT_ADDR) && (ACPI_XSDT_ADDR < vm->max_mem_addr)
 */
void build_vacpi(struct acrn_vm *vm)
{
	struct acpi_table_rsdp *rsdp;
	struct acpi_table_xsdt *xsdt;
	struct acpi_table_fadt *fadp;
	struct acpi_table_header *dsdt;
	struct acpi_table_mcfg *mcfg;
	struct acpi_table_madt *madt;
	struct acpi_madt_local_apic *lapic;
	uint16_t i;

	rsdp = &acpi_table_template[vm->vm_id].rsdp;
	rsdp->checksum = calculate_checksum8(rsdp, ACPI_RSDP_CHECKSUM_LENGTH);
	rsdp->extended_checksum = calculate_checksum8(rsdp, ACPI_RSDP_XCHECKSUM_LENGTH);
	/* Copy RSDP table to guest physical memory */
	(void)copy_to_gpa(vm, rsdp, ACPI_RSDP_ADDR, ACPI_RSDP_XCHECKSUM_LENGTH);

	xsdt = &acpi_table_template[vm->vm_id].xsdt;
	/* Copy XSDT table to guest physical memory */
	(void)copy_to_gpa(vm, xsdt, ACPI_XSDT_ADDR, sizeof(struct acpi_table_header));
	xsdt = (struct acpi_table_xsdt *)gpa2hva(vm, ACPI_XSDT_ADDR);
	stac();
	xsdt->table_offset_entry[0] = ACPI_FADT_ADDR;
	xsdt->table_offset_entry[1] = ACPI_MCFG_ADDR;
	xsdt->table_offset_entry[2] = ACPI_MADT_ADDR;
	/* Currently XSDT table only pointers to 3 ACPI table entry (FADT/MCFG/MADT) */
	xsdt->header.length = sizeof(struct acpi_table_header) + (3U * sizeof(uint64_t));
	xsdt->header.checksum = calculate_checksum8(xsdt, xsdt->header.length);
	clac();

	fadp = &acpi_table_template[vm->vm_id].fadt;
	fadp->header.checksum = calculate_checksum8(fadp, fadp->header.length);

	/* Copy FADT table to guest physical memory */
	(void)copy_to_gpa(vm, fadp, ACPI_FADT_ADDR, fadp->header.length);

	dsdt = &acpi_table_template[vm->vm_id].dsdt;
	dsdt->checksum = calculate_checksum8(dsdt, dsdt->length);

	/* Copy DSDT table and its subtables to guest physical memory */
	(void)copy_to_gpa(vm, dsdt, ACPI_DSDT_ADDR, dsdt->length);

       mcfg = &acpi_table_template[vm->vm_id].mcfg;
       mcfg->header.length = sizeof(struct acpi_table_mcfg)
               + (1U * sizeof(struct acpi_mcfg_allocation));   /* We only support one mcfg allocation structure */
       mcfg->header.checksum = calculate_checksum8(mcfg, mcfg->header.length);

       /* Copy MCFG table and its subtables to guest physical memory */
       (void)copy_to_gpa(vm, mcfg, ACPI_MCFG_ADDR, mcfg->header.length);

	/* Fix up MADT LAPIC subtables */
	for (i = 0U; i < vm->hw.created_vcpus; i++) {
		lapic = &acpi_table_template[vm->vm_id].lapic_array[i];
		lapic->processor_id = (uint8_t)i;
		lapic->id = (uint8_t)i;
	}

	madt = &acpi_table_template[vm->vm_id].madt;
	madt->header.length = sizeof(struct acpi_table_madt) + sizeof(struct acpi_madt_ioapic)
		+ sizeof(struct acpi_madt_local_apic_nmi)
		+ (sizeof(struct acpi_madt_local_apic) * (size_t)vm->hw.created_vcpus);
	madt->header.checksum = calculate_checksum8(madt, madt->header.length);

	/* Copy MADT table and its subtables to guest physical memory */
	(void)copy_to_gpa(vm, madt, ACPI_MADT_ADDR, madt->header.length);
}
