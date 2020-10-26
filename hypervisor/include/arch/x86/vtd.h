/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VTD_H
#define VTD_H
#include <types.h>
#include <x86/lib/spinlock.h>
#include <x86/io.h>
#include <x86/pgtable.h>
#include <platform_acpi_info.h>

#define INVALID_DRHD_INDEX 0xFFFFFFFFU

/*
 * Intel IOMMU register specification per version 1.0 public spec.
 */

#define DMAR_VER_REG    0x0U /* Arch version supported by this IOMMU */
#define DMAR_CAP_REG    0x8U /* Hardware supported capabilities */
#define DMAR_ECAP_REG   0x10U    /* Extended capabilities supported */
#define DMAR_GCMD_REG   0x18U    /* Global command register */
#define DMAR_GSTS_REG   0x1cU    /* Global status register */
#define DMAR_RTADDR_REG 0x20U    /* Root entry table */
#define DMAR_CCMD_REG   0x28U    /* Context command reg */
#define DMAR_FSTS_REG   0x34U    /* Fault Status register */
#define DMAR_FECTL_REG  0x38U    /* Fault control register */
#define DMAR_FEDATA_REG 0x3cU    /* Fault event interrupt data register */
#define DMAR_FEADDR_REG 0x40U    /* Fault event interrupt addr register */
#define DMAR_FEUADDR_REG 0x44U   /* Upper address register */
#define DMAR_AFLOG_REG  0x58U    /* Advanced Fault control */
#define DMAR_PMEN_REG   0x64U    /* Enable Protected Memory Region */
#define DMAR_PLMBASE_REG 0x68U   /* PMRR Low addr */
#define DMAR_PLMLIMIT_REG 0x6cU  /* PMRR low limit */
#define DMAR_PHMBASE_REG 0x70U   /* pmrr high base addr */
#define DMAR_PHMLIMIT_REG 0x78U  /* pmrr high limit */
#define DMAR_IQH_REG    0x80U    /* Invalidation queue head register */
#define DMAR_IQT_REG    0x88U    /* Invalidation queue tail register */
#define DMAR_IQ_SHIFT   4   /* Invalidation queue head/tail shift */
#define DMAR_IQA_REG    0x90U    /* Invalidation queue addr register */
#define DMAR_ICS_REG    0x9cU    /* Invalidation complete status register */
#define DMAR_IRTA_REG   0xb8U    /* Interrupt remapping table addr register */

/* 4 iommu fault register state */
#define	IOMMU_FAULT_REGISTER_STATE_NUM	4U
#define	IOMMU_FAULT_REGISTER_SIZE	4U

#define DBG_IOMMU 0

#if DBG_IOMMU
#define DBG_LEVEL_IOMMU LOG_INFO
#define DMAR_FAULT_LOOP_MAX 10
#else
#define DBG_LEVEL_IOMMU 6U
#endif
#define LEVEL_WIDTH 9U

/* Values for entry_type in ACPI_DMAR_DEVICE_SCOPE - device types */
enum acpi_dmar_scope_type {
	ACPI_DMAR_SCOPE_TYPE_NOT_USED       = 0,
	ACPI_DMAR_SCOPE_TYPE_ENDPOINT       = 1,
	ACPI_DMAR_SCOPE_TYPE_BRIDGE         = 2,
	ACPI_DMAR_SCOPE_TYPE_IOAPIC         = 3,
	ACPI_DMAR_SCOPE_TYPE_HPET           = 4,
	ACPI_DMAR_SCOPE_TYPE_NAMESPACE      = 5,
	ACPI_DMAR_SCOPE_TYPE_RESERVED       = 6 /* 6 and greater are reserved */
};

struct dmar_drhd {
	uint32_t dev_cnt;
	uint16_t segment;
	uint8_t flags;
	bool ignore;
	uint64_t reg_base_addr;
	/* assume no pci device hotplug support */
	struct dmar_dev_scope *devices;
};

/* dmar unit runtime data */
struct dmar_drhd_rt {
	uint32_t index;
	spinlock_t lock;

	struct dmar_drhd *drhd;

	uint64_t root_table_addr;
	uint64_t ir_table_addr;
	uint64_t qi_queue;
	uint16_t qi_tail;

	uint64_t cap;
	uint64_t ecap;
	uint32_t gcmd;  /* sw cache value of global cmd register */

	uint32_t dmar_irq;

	bool cap_pw_coherency;  /* page-walk coherency */
	uint8_t cap_msagaw;
	uint16_t cap_num_fault_regs;
	uint16_t cap_fault_reg_offset;
	uint16_t ecap_iotlb_offset;
	uint32_t fault_state[IOMMU_FAULT_REGISTER_STATE_NUM]; /* 32bit registers */
};

static inline uint32_t iommu_read32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	return mmio_read32(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static inline uint64_t iommu_read64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset)
{
	return mmio_read64(hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static inline void iommu_write32(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint32_t value)
{
	mmio_write32(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static inline void iommu_write64(const struct dmar_drhd_rt *dmar_unit, uint32_t offset, uint64_t value)
{
	mmio_write64(value, hpa2hva(dmar_unit->drhd->reg_base_addr + offset));
}

static inline uint8_t dmar_ver_major(uint64_t version)
{
	return (((uint8_t)version & 0xf0U) >> 4U);
}

static inline uint8_t dmar_ver_minor(uint64_t version)
{
	return ((uint8_t)version & 0x0fU);
}

/*
 * Decoding Capability Register
 */
static inline uint8_t iommu_cap_pi(uint64_t cap)
{
	return ((uint8_t)(cap >> 59U) & 1U);
}

static inline uint8_t iommu_cap_read_drain(uint64_t cap)
{
	return ((uint8_t)(cap >> 55U) & 1U);
}

static inline uint8_t iommu_cap_write_drain(uint64_t cap)
{
	return ((uint8_t)(cap >> 54U) & 1U);
}

static inline uint8_t iommu_cap_max_amask_val(uint64_t cap)
{
	return ((uint8_t)(cap >> 48U) & 0x3fU);
}

static inline uint16_t iommu_cap_num_fault_regs(uint64_t cap)
{
	return (((uint16_t)(cap >> 40U) & 0xffU) + 1U);
}

static inline uint8_t iommu_cap_pgsel_inv(uint64_t cap)
{
	return ((uint8_t)(cap >> 39U) & 1U);
}

static inline uint8_t iommu_cap_super_page_val(uint64_t cap)
{
	return ((uint8_t)(cap >> 34U) & 0xfU);
}

static inline uint16_t iommu_cap_fault_reg_offset(uint64_t cap)
{
	return (((uint16_t)(cap >> 24U) & 0x3ffU) * 16U);
}

static inline uint16_t iommu_cap_max_fault_reg_offset(uint64_t cap)
{
	return (iommu_cap_fault_reg_offset(cap) +
		(iommu_cap_num_fault_regs(cap) * 16U));
}

static inline uint8_t iommu_cap_zlr(uint64_t cap)
{
	return ((uint8_t)(cap >> 22U) & 1U);
}

static inline uint8_t iommu_cap_isoch(uint64_t cap)
{
	return ((uint8_t)(cap >> 23U) & 1U);
}

static inline uint8_t iommu_cap_mgaw(uint64_t cap)
{
	return (((uint8_t)(cap >> 16U) & 0x3fU) + 1U);
}

static inline uint8_t iommu_cap_sagaw(uint64_t cap)
{
	return ((uint8_t)(cap >> 8U) & 0x1fU);
}

static inline uint8_t iommu_cap_caching_mode(uint64_t cap)
{
	return ((uint8_t)(cap >> 7U) & 1U);
}

static inline uint8_t iommu_cap_phmr(uint64_t cap)
{
	return ((uint8_t)(cap >> 6U) & 1U);
}

static inline uint8_t iommu_cap_plmr(uint64_t cap)
{
	return ((uint8_t)(cap >> 5U) & 1U);
}

static inline uint8_t iommu_cap_afl(uint64_t cap)
{
	return ((uint8_t)(cap >> 3U) & 1U);
}

static inline uint32_t iommu_cap_ndoms(uint64_t cap)
{
	return ((1U) << (4U + (2U * ((uint8_t)cap & 0x7U))));
}

/*
 * Decoding Extended Capability Register
 */
static inline uint8_t iommu_ecap_c(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 0U) & 1U);
}

static inline uint8_t iommu_ecap_qi(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 1U) & 1U);
}

static inline uint8_t iommu_ecap_dt(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 2U) & 1U);
}

static inline uint8_t iommu_ecap_ir(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 3U) & 1U);
}

static inline uint8_t iommu_ecap_eim(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 4U) & 1U);
}

static inline uint8_t iommu_ecap_pt(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 6U) & 1U);
}

static inline uint16_t iommu_ecap_iro(uint64_t ecap)
{
	return ((uint16_t)(ecap >> 8U) & 0x3ffU);
}

static inline uint8_t iommu_ecap_mhmv(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 20U) & 0xfU);
}

static inline uint8_t iommu_ecap_ecs(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 24U) & 1U);
}

static inline uint8_t iommu_ecap_mts(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 25U) & 1U);
}

static inline uint8_t iommu_ecap_nest(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 26U) & 1U);
}

static inline uint8_t iommu_ecap_dis(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 27U) & 1U);
}

static inline uint8_t iommu_ecap_prs(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 29U) & 1U);
}

static inline uint8_t iommu_ecap_ers(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 30U) & 1U);
}

static inline uint8_t iommu_ecap_srs(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 31U) & 1U);
}

static inline uint8_t iommu_ecap_nwfs(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 33U) & 1U);
}

static inline uint8_t iommu_ecap_eafs(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 34U) & 1U);
}

static inline uint8_t iommu_ecap_pss(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 35U) & 0x1fU);
}

static inline uint8_t iommu_ecap_pasid(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 40U) & 1U);
}

static inline uint8_t iommu_ecap_dit(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 41U) & 1U);
}

static inline uint8_t iommu_ecap_pds(uint64_t ecap)
{
	return ((uint8_t)(ecap >> 42U) & 1U);
}

/* PMEN_REG */
#define DMA_PMEN_EPM (1U << 31U)
#define DMA_PMEN_PRS (1U << 0U)

/* GCMD_REG */
#define DMA_GCMD_TE (1U << 31U)
#define DMA_GCMD_SRTP (1U << 30U)
#define DMA_GCMD_SFL (1U << 29U)
#define DMA_GCMD_EAFL (1U << 28U)
#define DMA_GCMD_WBF (1U << 27U)
#define DMA_GCMD_QIE (1U << 26U)
#define DMA_GCMD_SIRTP (1U << 24U)
#define DMA_GCMD_IRE (1U << 25U)
#define DMA_GCMD_CFI (1U << 23U)

/* GSTS_REG */
#define DMA_GSTS_TES (1U << 31U)
#define DMA_GSTS_RTPS (1U << 30U)
#define DMA_GSTS_FLS (1U << 29U)
#define DMA_GSTS_AFLS (1U << 28U)
#define DMA_GSTS_WBFS (1U << 27U)
#define DMA_GSTS_QIES (1U << 26U)
#define DMA_GSTS_IRTPS (1U << 24U)
#define DMA_GSTS_IRES (1U << 25U)
#define DMA_GSTS_CFIS (1U << 23U)

/* CCMD_REG */
#define DMA_CONTEXT_GLOBAL_INVL (1UL << 4U)
#define DMA_CONTEXT_DOMAIN_INVL (2UL << 4U)
#define DMA_CONTEXT_DEVICE_INVL (3UL << 4U)
static inline uint64_t dma_ccmd_fm(uint8_t fm)
{
	return (((uint64_t)fm & 0x3UL) << 48UL);
}

#define DMA_CCMD_MASK_NOBIT 0UL
#define DMA_CCMD_MASK_1BIT 1UL
#define DMA_CCMD_MASK_2BIT 2UL
#define DMA_CCMD_MASK_3BIT 3UL
static inline uint64_t dma_ccmd_sid(uint16_t sid)
{
	return (((uint64_t)sid & 0xffffUL) << 32UL);
}

static inline uint64_t dma_ccmd_did(uint16_t did)
{
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

static inline uint8_t dma_ccmd_get_caig_32(uint32_t gaig)
{
	return ((uint8_t)(gaig >> 27U) & 0x3U);
}


/* IOTLB_REG */
#define DMA_IOTLB_IVT				(((uint64_t)1UL) << 63U)
#define DMA_IOTLB_IVT_32			(((uint32_t)1U)  << 31U)
#define DMA_IOTLB_GLOBAL_INVL			(((uint64_t)1UL) << 4U)
#define DMA_IOTLB_DOMAIN_INVL			(((uint64_t)2UL) << 4U)
#define DMA_IOTLB_PAGE_INVL			(((uint64_t)3UL) << 4U)
#define DMA_IOTLB_DR				(((uint64_t)1UL) << 7U)
#define DMA_IOTLB_DW				(((uint64_t)1UL) << 6U)
static inline uint64_t dma_iotlb_did(uint16_t did)
{
	return (((uint64_t)did & 0xffffUL) << 16UL);
}

static inline uint8_t dma_iotlb_get_iaig_32(uint32_t iai)
{
	return ((uint8_t)(iai >> 25U) & 0x3U);
}

/* INVALIDATE_ADDRESS_REG */
static inline uint8_t dma_iotlb_invl_addr_am(uint8_t am)
{
	return (am & 0x3fU);
}

/* IEC_REG */
#define DMAR_IECI_INDEXED		(((uint64_t)1UL) << 4U)
#define DMAR_IEC_GLOBAL_INVL		(((uint64_t)0UL) << 4U)
static inline uint64_t dma_iec_index(uint16_t index, uint8_t index_mask)
{
	return ((((uint64_t)index & 0xFFFFU) << 32U) | (((uint64_t)index_mask & 0x1FU) << 27U));
}

#define DMA_IOTLB_INVL_ADDR_IH_UNMODIFIED	(((uint64_t)1UL) << 6U)

/* FECTL_REG */
#define DMA_FECTL_IM				(((uint32_t)1U) << 31U)

/* FSTS_REG */
static inline bool dma_fsts_pfo(uint32_t pfo)
{
	return (((pfo >> 0U) & 1U) == 1U);
}

static inline bool dma_fsts_ppf(uint32_t ppf)
{
	return (((ppf >> 1U) & 1U) == 1U);
}

static inline bool dma_fsts_afo(uint32_t afo)
{
	return (((afo >> 2U) & 1U) == 1U);
}

static inline bool dma_fsts_apf(uint32_t apf)
{
	return (((apf >> 3U) & 1U) == 1U);
}

static inline bool dma_fsts_iqe(uint32_t iqe)
{
	return (((iqe >> 4U) & 1U) == 1U);
}

static inline bool dma_fsts_ice(uint32_t ice)
{
	return (((ice >> 5U) & 1U) == 1U);
}

static inline bool dma_fsts_ite(uint32_t ite)
{
	return (((ite >> 6U) & 1U) == 1U);
}

static inline bool dma_fsts_pro(uint32_t pro)
{
	return (((pro >> 7U) & 1U) == 1U);
}

static inline uint8_t dma_fsts_fri(uint32_t fri)
{
	return ((uint8_t)(fri >> 8U) & 0xFFU);
}

/* FRCD_REGs: upper 64 bits*/
static inline bool dma_frcd_up_f(uint64_t up_f)
{
	return (((up_f >> 63U) & 1UL) == 1UL);
}

static inline uint8_t dma_frcd_up_t(uint64_t up_t)
{
	return ((uint8_t)(up_t >> 62U) & 1U);
}

static inline uint8_t dma_frcd_up_at(uint64_t up_at)
{
	return ((uint8_t)(up_at >> 60U) & 3U);
}

static inline uint32_t dma_frcd_up_pasid(uint64_t up_pasid)
{
	return ((uint32_t)(up_pasid >> 40U) & 0xfffffU);
}

static inline uint8_t dma_frcd_up_fr(uint64_t up_fr)
{
	return ((uint8_t)(up_fr >> 32U) & 0xffU);
}

static inline bool dma_frcd_up_pp(uint64_t up_pp)
{
	return (((up_pp >> 31U) & 1UL) == 1UL);
}

static inline bool dma_frcd_up_exe(uint64_t up_exe)
{
	return (((up_exe >> 30U) & 1UL) == 1UL);
}

static inline bool dma_frcd_up_priv(uint64_t up_priv)
{
	return (((up_priv >> 29U) & 1UL) == 1UL);
}

static inline uint16_t dma_frcd_up_sid(uint64_t up_sid)
{
	return ((uint16_t)up_sid & 0xffffU);
}

#define MAX_DRHDS		DRHD_COUNT
#define MAX_DRHD_DEVSCOPES	16U

#define DMAR_CONTEXT_TRANSLATION_TYPE_TRANSLATED 0x00U
#define DMAR_CONTEXT_TRANSLATION_TYPE_RESERVED 0x01U
#define DMAR_CONTEXT_TRANSLATION_TYPE_PASSED_THROUGH 0x02U

#define DRHD_FLAG_INCLUDE_PCI_ALL_MASK      (1U)

#define DEVFUN(dev, fun)            (((dev & 0x1FU) << 3U) | ((fun & 0x7U)))

struct dmar_dev_scope {
	enum acpi_dmar_scope_type type;
	uint8_t id;
	uint8_t bus;
	uint8_t devfun;
};

struct dmar_info {
	uint32_t drhd_count;
	struct dmar_drhd *drhd_units;
};

void dmar_fault_event_mask(struct dmar_drhd_rt *dmar_unit);
void dmar_fault_event_unmask(struct dmar_drhd_rt *dmar_unit);

#ifdef CONFIG_ACPI_PARSE_ENABLED
int32_t parse_dmar_table(struct dmar_info *plat_dmar_info);
#endif
#endif
