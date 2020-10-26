/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef IOMMU_H
#define IOMMU_H

#include <types.h>
#include <pci.h>

/**
 * @file iommu.h
 *
 * @brief public APIs for IOMMU
 */

/**
 * @brief IOMMU
 *
 * @defgroup acrn_iommu ACRN IOMMU
 * @{
 */
struct dmar_entry {
        uint64_t lo_64;
        uint64_t hi_64;
};

union dmar_ir_entry {
    struct dmar_entry value;

    union {
	/* Remapped mode */
        struct {
            uint64_t present:1;
            uint64_t fpd:1;
            uint64_t dest_mode:1;
            uint64_t rh:1;
            uint64_t trigger_mode:1;
            uint64_t delivery_mode:3;
            uint64_t avail:4;
            uint64_t rsvd_1:3;
            uint64_t mode:1;
            uint64_t vector:8;
            uint64_t rsvd_2:8;
            uint64_t dest:32;

            uint64_t sid:16;
            uint64_t sq:2;
            uint64_t svt:2;
            uint64_t rsvd_3:44;
        } remap;

        /* Posted mode */
        struct {
            uint64_t present:1;
            uint64_t fpd:1;
            uint64_t rsvd_1:6;
            uint64_t avail:4;
            uint64_t rsvd_2:2;
            uint64_t urgent:1;
            uint64_t mode:1;
            uint64_t vector:8;
            uint64_t rsvd_3:14;
            uint64_t pda_l:26;

            uint64_t sid:16;
            uint64_t sq:2;
            uint64_t svt:2;
            uint64_t rsvd_4:12;
            uint64_t pda_h:32;
        } post;
    } bits __packed;
};

union source {
	uint16_t ioapic_id;
	union pci_bdf msi;
};

struct intr_source {
	bool is_msi;
	union source src;
	/*
	 * pid_paddr = 0: invalid address, indicate that remapped mode shall be used
	 *
	 * pid_paddr != 0: physical address of posted interrupt descriptor, indicate
	 * that posted mode shall be used
	 */
	uint64_t pid_paddr;
};

/**
 * @brief iommu domain.
 *
 * This struct declaration for iommu domain.
 *
 */
struct iommu_domain {
	uint16_t vm_id;
	uint32_t addr_width;   /* address width of the domain */
	uint64_t trans_table_ptr;
};

/**
 * @brief Assign a device specified by bus & devfun to a iommu domain.
 *
 * Remove the device from the from_domain (if non-NULL), and add it to the to_domain (if non-NULL).
 * API silently fails to add/remove devices to/from domains that are under "Ignored" DMAR units.
 *
 * @param[in]    from_domain iommu domain from which the device is removed from
 * @param[in]    to_domain iommu domain to which the device is assgined to
 * @param[in]    bus the 8-bit bus number of the device
 * @param[in]    devfun the 8-bit device(5-bit):function(3-bit) of the device
 *
 * @retval 0 on success.
 * @retval 1 fail to unassign the device
 *
 * @pre domain != NULL
 *
 */
int32_t move_pt_device(const struct iommu_domain *from_domain, const struct iommu_domain *to_domain, uint8_t bus, uint8_t devfun);

/**
 * @brief Create a iommu domain for a VM specified by vm_id.
 *
 * Create a iommu domain for a VM specified by vm_id, along with address translation table and address width.
 *
 * @param[in] vm_id vm_id of the VM the domain created for
 * @param[in] translation_table the physcial address of EPT table of the VM specified by the vm_id
 * @param[in] addr_width address width of the VM
 *
 * @return Pointer to the created iommu_domain
 *
 * @retval NULL when \p translation_table is 0
 * @retval !NULL when \p translation_table is not 0
 *
 * @pre vm_id is valid
 * @pre translation_table != 0
 *
 */
struct iommu_domain *create_iommu_domain(uint16_t vm_id, uint64_t translation_table, uint32_t addr_width);

/**
 * @brief Destroy the specific iommu domain.
 *
 * Destroy the specific iommu domain when a VM no longer needs it.
 *
 * @param[in] domain iommu domain to destroy
 *
 * @pre domain != NULL
 *
 */
void destroy_iommu_domain(struct iommu_domain *domain);

/**
 * @brief Enable translation of IOMMUs.
 *
 * Enable address translation of all IOMMUs, which are not ignored on the platform.
 *
 */
void enable_iommu(void);

/**
 * @brief Suspend IOMMUs.
 *
 * Suspend all IOMMUs, which are not ignored on the platform.
 *
 */
void suspend_iommu(void);

/**
 * @brief Resume IOMMUs.
 *
 * Resume all IOMMUs, which are not ignored on the platform.
 *
 */
void resume_iommu(void);

/**
 * @brief Init IOMMUs.
 *
 * Register DMAR units on the platform according to the pre-parsed information
 * or DMAR table. IOMMU is a must have feature, if init_iommu failed, the system
 * should not continue booting.
 *
 * @retval 0 on success
 * @retval <0 on failure
 *
 */
int32_t init_iommu(void);

/**
 * @brief Assign IRTE for Interrupt Remapping Table.
 *
 * @param[in] intr_src filled with type of interrupt source and the source
 * @param[in] irte filled with info about interrupt deliverymode, destination and destination mode
 * @param[in] index into Interrupt Remapping Table
 *
 * @retval -EINVAL if corresponding DMAR is not present
 * @retval 0 otherwise
 *
 */
int32_t iommu_ir_assign_irte(const struct intr_source *intr_src, union dmar_ir_entry *irte, uint16_t index);

/**
 * @brief Free IRTE for Interrupt Remapping Table.
 *
 * @param[in] intr_src filled with type of interrupt source and the source
 * @param[in] index into Interrupt Remapping Table
 *
 */
void iommu_ir_free_irte(const struct intr_source *intr_src, uint16_t index);

/**
 * @brief Flash cacheline(s) for a specific address with specific size.
 *
 * Flash cacheline(s) for a specific address with specific size,
 * if all IOMMUs active support page-walk coherency, cacheline(s) are not fluashed.
 *
 * @param[in] p the address of the buffer, whose cache need to be invalidated
 * @param[in] size the size of the buffer
 *
 */
void iommu_flush_cache(const void *p, uint32_t size);
/**
  * @}
  */
#endif