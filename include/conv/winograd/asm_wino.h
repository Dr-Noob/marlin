/*******************************************************************************
 * Copyright (c) Malith Jayaweera - All rights reserved.                       *
 * This file is part of the MARLIN library.                                    *
 *                                                                             *
 * For information on the license, see the LICENSE file.                       *
 * Further information: https://github.com/malithj/marlin/                     *
 * SPDX-License-Identifier: BSD-3-Clause                                       *
 ******************************************************************************/
/* Malith Jayaweera
*******************************************************************************/
#ifndef __ASM_WINO_KERNELS_H_
#define __ASM_WINO_KERNELS_H_

#include "types/types.h"

#ifndef ENABLE_JIT
extern "C" index_t asm_wino_multiply(index_t a_stride, index_t b_stride,
                                     index_t c_stride, index_t tile_count,
                                     index_t in_channels, uint16_t mask,
                                     const float *a, float *c, const float *b);
#else
extern "C" index_t asm_wino_multiply(index_t a_stride, index_t c_stride,
                                     index_t tile_count, index_t in_channels,
                                     uint16_t mask, const float *a, float *c,
                                     void *p_addr, index_t *offset_data,
                                     index_t idx);

#endif

#endif