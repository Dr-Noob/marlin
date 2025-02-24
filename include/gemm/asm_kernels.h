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
#ifndef __ASM_KERNELS_H_
#define __ASM_KERNELS_H_

#include "../types/types.h"

extern "C" index_t asm_gemm_f32_j1(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j2(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j3(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j4(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j5(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j6(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j7(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j8(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j9(index_t m, index_t k, index_t n, float *a,
                                   float *c, void *p_addr, index_t *offset_data,
                                   uint16_t mask, index_t idx);

extern "C" index_t asm_gemm_f32_j10(index_t m, index_t k, index_t n, float *a,
                                    float *c, void *p_addr,
                                    index_t *offset_data, uint16_t mask,
                                    index_t idx);

extern "C" index_t asm_gemm_f32_j11(index_t m, index_t k, index_t n, float *a,
                                    float *c, void *p_addr,
                                    index_t *offset_data, uint16_t mask,
                                    index_t idx);

extern "C" index_t asm_gemm_f32_j12(index_t m, index_t k, index_t n, float *a,
                                    float *c, void *p_addr,
                                    index_t *offset_data, uint16_t mask,
                                    index_t idx);

extern "C" index_t asm_gemm_f32_j13(index_t m, index_t k, index_t n, float *a,
                                    float *c, void *p_addr,
                                    index_t *offset_data, uint16_t mask,
                                    index_t idx);

extern "C" index_t asm_gemm_f32_j14(index_t m, index_t k, index_t n, float *a,
                                    float *c, void *p_addr,
                                    index_t *offset_data, uint16_t mask,
                                    index_t idx);

extern "C" index_t asm_gemm(index_t m, index_t k, index_t n, float *a, float *c,
                            void *p_addr, index_t *offset_data, uint16_t mask,
                            index_t idx);

#endif