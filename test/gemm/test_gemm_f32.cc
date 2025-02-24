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
#include <chrono>

#include "gemm/gemm.h"
#include "gemm/gemm_f32.h"
#include "gtest/gtest.h"

TEST(GEMM, f32) {
  index_t m = 5;
  index_t n = 5;
  index_t k = 5;

  float *A = static_cast<float *>(std::malloc(m * k * sizeof(float)));
  float *B = static_cast<float *>(std::malloc(n * k * sizeof(float)));
  float *C = static_cast<float *>(std::malloc(m * n * sizeof(float)));
  float *C_REF = static_cast<float *>(std::malloc(m * n * sizeof(float)));

  for (index_t i = 0; i < m; ++i) {
    for (index_t j = 0; j < k; ++j) {
      A[i * k + j] = i * k + j + 1;
    }
  }
  for (index_t i = 0; i < k; ++i) {
    for (index_t j = 0; j < n; ++j) {
      index_t idx = i * n + j;
      B[i * n + j] = idx + 1;
    }
  }

  memset(C, 0, m * n * sizeof(float));
  memset(C_REF, 0, m * n * sizeof(float));
  gemm<float>('N', 'N', m, n, k, 1.0, A, k, B, n, 0, C_REF, n);
  // sgemm('N', 'N', m, n, k, 1.0, A, k, B, n, 0, C, n);

  // for (index_t i = 0; i < m * n; ++i) {
  //   EXPECT_EQ(C_REF[i], C[i]);
  // }

  std::free(A);
  std::free(B);
  std::free(C);
  std::free(C_REF);
}