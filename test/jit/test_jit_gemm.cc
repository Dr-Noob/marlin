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
#include "gemm/gemm.h"
#include "gemm/gemm_f32.h"
#include "gtest/gtest.h"
#include "jit/jitter.h"

using namespace MARLIN;

TEST(JIT, GEMM) {
  index_t m = 3;
  index_t n = 5;
  index_t k = 2;

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

  // generate code
  std::shared_ptr<Jitter<float>> jitter = std::make_shared<Jitter<float>>();
  jitter->generate_code(B, m, k, n);

  memset(C, 0, m * n * sizeof(float));
  memset(C_REF, 0, m * n * sizeof(float));
#ifdef ENABLE_JIT
  gemm<float>('T', 'N', m, n, k, 1.0, A, k, B, n, 0, C_REF, n);
  sgemm('N', 'N', m, n, k, 1.0, A, k, B, n, 0, C, n, jitter);
#else
  gemm<float>('N', 'N', m, n, k, 1.0, A, k, B, n, 0, C_REF, n);
  sgemm('N', 'N', m, n, k, 1.0, A, k, B, n, 0, C, n);
#endif

#ifdef ENABLE_JIT
  // asm: col major & gemm: row major
  for (index_t i = 0; i < n; ++i) {
    for (index_t j = 0; j < m; ++j) {
      EXPECT_EQ(C_REF[j * n + i], C[i * m + j]);
    }
  }
#else
  for (index_t i = 0; i < m * n; ++i) {
    EXPECT_EQ(C_REF[i], C[i]);
  }
#endif

  std::free(A);
  std::free(B);
  std::free(C);
  std::free(C_REF);
}