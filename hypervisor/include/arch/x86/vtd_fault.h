/*
 * Copyright (C) 2020 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef VTD_FAULT_H
#define VTD_FAULT_H

struct dmar_drhd_rt;

#ifdef CONFIG_SOFTIRQ
void dmar_setup_interrupt(struct dmar_drhd_rt *dmar_unit);
#else
#define dmar_setup_interrupt(x) do { } while(0)
#endif
#endif