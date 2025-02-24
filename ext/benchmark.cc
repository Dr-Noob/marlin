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
#include "benchmark.h"

#include "onednn_matmul.h"

TEST(Benchmark, Jitters) {
  // lambda func defined to benchmark
  auto perf_bench = [=](const index_t mat_m_strt, const index_t mat_m_end,
                        const index_t mat_m_inc, const index_t mat_n_strt,
                        const index_t mat_n_end, const index_t mat_n_inc,
                        const index_t mat_k, const float sparsity,
                        const index_t iterations) {
    // define constants
    const float alpha = 1.0f;
    const float beta = 0.f;
    const index_t num_mat = ((mat_m_end - mat_m_strt) / mat_m_inc) *
                            ((mat_n_end - mat_n_strt) / mat_n_inc);
    // vars in line: mat_size, dyn, stat, sgemm, ..., asm-conv, ...
    const index_t width = 11;
    const index_t height = num_mat;
    mkl_set_num_threads(1);

    double *results =
        static_cast<double *>(aligned_alloc(height * width, sizeof(double)));
    memset(results, 0, sizeof(double) * height * width);

    index_t m, n, k;
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;
    std::chrono::nanoseconds duration;

    // warm up libraries
    m = 100;
    n = 100;
    k = 100;
    // define warm up matrices
    float *A = static_cast<float *>(aligned_alloc(m * k, sizeof(float)));
    float *B = static_cast<float *>(aligned_alloc(k * n, sizeof(float)));
    float *C = static_cast<float *>(aligned_alloc(m * n, sizeof(float)));
    // set warm up matrices to zero
    memset(A, 0, m * k * sizeof(float));
    memset(B, 0, k * n * sizeof(float));
    memset(C, 0, m * n * sizeof(float));
    void *jitter;
    mkl_jit_status_t status =
        mkl_jit_create_sgemm(&jitter, MKL_ROW_MAJOR, MKL_NOTRANS, MKL_NOTRANS,
                             m, n, k, alpha, k, n, beta, n);
    if (MKL_JIT_ERROR == status) {
      throw std::runtime_error(
          "insufficient memory to JIT and store the SGEMM kernel");
    }
    sgemm_jit_kernel_t mkl_sgemm = mkl_jit_get_sgemm_ptr(jitter);
    engine eng(engine::kind::cpu, 0);  // engine
    auto dynamic_matmul = dynamic_matmul_create(eng, beta);

    libxsmm_mmfunction<float> xsmm_wm_kernel(LIBXSMM_GEMM_FLAG_NONE, m, n, k,
                                             alpha, beta);

    Map<MatrixXf> eigen_A(A, m, k);
    Map<MatrixXf> eigen_B(B, k, n);
    Map<MatrixXf> eigen_C(C, m, n);

    // rounds of warming up
    for (index_t i = 0; i < 10; ++i) {
      mkl_sgemm(jitter, A, B, C);
      dynamic_matmul_execute(dynamic_matmul, eng, 'N', 'N', m, n, k, alpha, A,
                             k, B, n, beta, C, n);
      static_matmul_create_and_execute(eng, 'N', 'N', m, n, k, alpha, A, k, B,
                                       n, beta, C, n);
      xsmm_wm_kernel(A, B, C);
      cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1, A, k,
                  B, n, 0, C, n);
      eigen_C = eigen_A * eigen_B;
    }
    status = mkl_jit_destroy(jitter);
    if (MKL_JIT_ERROR == status) {
      throw std::runtime_error("failed releasing JIT memory");
    }
    aligned_free(A);
    aligned_free(B);
    aligned_free(C);
    // warming up libraries end here

    for (index_t mat_n = mat_n_strt; mat_n < mat_n_end; mat_n += mat_n_inc) {
      for (index_t mat_m = mat_m_strt; mat_m < mat_m_end; mat_m += mat_m_inc) {
        m = mat_m;
        n = mat_n;
        k = mat_k;
        float *A_ROW_MAJOR = static_cast<float *>(
            _mm_malloc(m * k * sizeof(float), ALIGN_BYTE_SIZE));
        float *A_COL_MAJOR = static_cast<float *>(
            _mm_malloc(m * k * sizeof(float), ALIGN_BYTE_SIZE));
        float *B_ROW_MAJOR = static_cast<float *>(
            _mm_malloc(k * n * sizeof(float), ALIGN_BYTE_SIZE));
        float *B_COL_MAJOR = static_cast<float *>(
            _mm_malloc(k * n * sizeof(float), ALIGN_BYTE_SIZE));
        float *C = static_cast<float *>(
            _mm_malloc(m * n * sizeof(float), ALIGN_BYTE_SIZE));
        for (index_t h = 0; h < m; ++h) {
          for (index_t w = 0; w < k; ++w) {
            index_t idx = h * k + w;
            A_COL_MAJOR[idx] = idx + 1;
            A_ROW_MAJOR[w * m + h] = idx + 1;
          }
        }
        for (index_t h = 0; h < k; ++h) {
          for (index_t w = 0; w < n; ++w) {
            index_t idx = h * n + w;
            B_ROW_MAJOR[idx] = sparsity_adjustment(sparsity);
            B_COL_MAJOR[w * k + h] = B_ROW_MAJOR[idx];
          }
        }

        const index_t offset = ((mat_n - mat_n_strt) / mat_n_inc) *
                                   ((mat_m_end - mat_m_strt) / mat_m_inc) *
                                   width +
                               ((mat_m - mat_m_strt) / mat_m_inc) * width;
        results[offset] = m;
        results[offset + 1] = n;
        results[offset + 2] = k;

        // ASM Jitter
        begin = std::chrono::steady_clock::now();
        std::shared_ptr<Jitter<float>> jit_ = std::make_shared<Jitter<float>>();
        jit_->generate_code(B_ROW_MAJOR, m, k, n);
        for (index_t i = 0; i < iterations; ++i) {
#ifdef ENABLE_JIT
          MARLIN::sgemm('N', 'N', m, n, k, 1.0, A_COL_MAJOR, k, B_ROW_MAJOR, n,
                        0, C, n, jit_);
#else
          MARLIN::sgemm('N', 'N', m, n, k, 1.0, A_COL_MAJOR, k, B_ROW_MAJOR, n,
                        0, C, n);
#endif
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 6] = duration.count() / (iterations * 1000.0f);

        // MKL JIT
        begin = std::chrono::steady_clock::now();
        mkl_jit_status_t status =
            mkl_jit_create_sgemm(&jitter, MKL_COL_MAJOR, MKL_NOTRANS,
                                 MKL_NOTRANS, m, n, k, alpha, m, k, beta, m);
        if (MKL_JIT_ERROR == status) {
          throw std::runtime_error(
              "insufficient memory to JIT and store the DGEMM kernel");
        }
        // retrieve the function pointer to the SGEMM kernel
        sgemm_jit_kernel_t mkl_sgemm = mkl_jit_get_sgemm_ptr(jitter);
        for (index_t i = 0; i < iterations; ++i) {
          mkl_sgemm(jitter, A_COL_MAJOR, B_COL_MAJOR, C);
        }
        status = mkl_jit_destroy(jitter);
        if (MKL_JIT_ERROR == status) {
          throw std::runtime_error("failed releasing JIT memory");
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 7] = duration.count() / (iterations * 1000.0f);

        // LIBXSMM
        begin = std::chrono::steady_clock::now();
        std::unique_ptr<libxsmm_mmfunction<float>> xsmm_kernel =
            std::make_unique<libxsmm_mmfunction<float>>(LIBXSMM_GEMM_FLAG_NONE,
                                                        m, n, k, alpha, beta);
        for (index_t i = 0; i < iterations; ++i) {
          xsmm_kernel->operator()(A_COL_MAJOR, B_COL_MAJOR, C);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 8] = duration.count() / (iterations * 1000.0f);

        // execute dynamic matmul
        begin = std::chrono::steady_clock::now();
        engine eng(engine::kind::cpu, 0);  // engine
        auto dynamic_matmul = dynamic_matmul_create(eng, beta);
        for (index_t i = 0; i < iterations; ++i) {
          dynamic_matmul_execute(dynamic_matmul, eng, 'N', 'N', m, n, k, alpha,
                                 A_ROW_MAJOR, k, B_ROW_MAJOR, n, beta, C, n);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 3] = duration.count() / (iterations * 1000.0f);

        // execute static matmul
        begin = std::chrono::steady_clock::now();
        engine eng_(engine::kind::cpu, 0);  // engine
        for (index_t i = 0; i < iterations; ++i) {
          static_matmul_create_and_execute(eng_, 'N', 'N', m, n, k, alpha,
                                           A_ROW_MAJOR, k, B_ROW_MAJOR, n, beta,
                                           C, n);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 4] = duration.count() / (iterations * 1000.0f);

        // dnnl sgemm
        begin = std::chrono::steady_clock::now();
        for (index_t i = 0; i < iterations; ++i) {
          dnnl_sgemm('N', 'N', m, n, k, alpha, A_ROW_MAJOR, k, B_ROW_MAJOR, n,
                     beta, C, n);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 5] = duration.count() / (iterations * 1000.0f);

        Map<MatrixXf> eigen_A(A_COL_MAJOR, m, k);
        Map<MatrixXf> eigen_B(B_COL_MAJOR, k, n);
        Map<MatrixXf> eigen_C(C, m, n);

        // eigen
        begin = std::chrono::steady_clock::now();
        for (index_t i = 0; i < iterations; ++i) {
          eigen_C = eigen_A * eigen_B;
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 9] = duration.count() / (iterations * 1000.0f);

        // OpenBLAS
        begin = std::chrono::steady_clock::now();
        for (index_t i = 0; i < iterations; ++i) {
          cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1,
                      A_ROW_MAJOR, k, B_ROW_MAJOR, n, 0, C, n);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 10] = duration.count() / (iterations * 1000.0f);

        _mm_free(A_ROW_MAJOR);
        _mm_free(A_COL_MAJOR);
        _mm_free(B_ROW_MAJOR);
        _mm_free(B_COL_MAJOR);
        _mm_free(C);
      }
    }

    std::stringstream stream;
    stream << mat_m_strt << "_" << (mat_m_end - mat_m_inc) << "_" << mat_m_inc
           << "_k" << k << "_" << std::fixed << std::setprecision(2) << sparsity
           << "_" << iterations << "_"
           << std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    std::string s = stream.str();
    std::string filename;
    filename = "build/results/benchmark_results_" + s + ".csv";
    std::string header =
        "IDX,M,N,K,ONEDNN_DYNAMIC,ONEDNN_STATIC,ONEDNN_SGEMM,"
        "MARLIN,MKL_JIT,LIBXSMM,EIGEN,MKL_BLAS";
    tofile(filename, results, height, width, header);
    aligned_free(results);
  };

  //
  // mat_m_strt
  // mat_m_end
  // mat_m_inc
  // mat_n_strt
  // mat_n_end
  // mat_n_inc
  // mat_k
  // sparsity
  // iterations
  perf_bench(1, 51, 1, 4, 32, 4, 2, 0, 100);
  perf_bench(1, 51, 1, 4, 32, 4, 4, 0, 100);
  perf_bench(1, 51, 1, 4, 32, 4, 10, 0, 100);
}

TEST(Benchmark, JitterOverhead) {
  // lambda func defined to benchmark
  auto perf_bench = [=](const index_t mat_m_strt, const index_t mat_m_end,
                        const index_t mat_m_inc, const index_t mat_n_strt,
                        const index_t mat_n_end, const index_t mat_n_inc,
                        const index_t mat_k, const float sparsity,
                        const index_t iterations = 1) {
    // define constants
    const float alpha = 1.0f;
    const float beta = 0.f;
    const index_t num_mat = ((mat_m_end - mat_m_strt) / mat_m_inc) *
                            ((mat_n_end - mat_n_strt) / mat_n_inc);
    // vars in line: mat_size, dyn, stat, sgemm, ..., asm-conv, ...
    const index_t width = 6;
    const index_t height = num_mat;
    mkl_set_num_threads(1);

    double *results =
        static_cast<double *>(aligned_alloc(height * width, sizeof(double)));
    memset(results, 0, sizeof(double) * height * width);

    index_t m, n, k;
    std::chrono::steady_clock::time_point begin;
    std::chrono::steady_clock::time_point end;
    std::chrono::nanoseconds duration;

    // warm up libraries
    m = 100;
    n = 100;
    k = 100;
    // define warm up matrices
    float *A = static_cast<float *>(aligned_alloc(m * k, sizeof(float)));
    float *B = static_cast<float *>(aligned_alloc(k * n, sizeof(float)));
    float *C = static_cast<float *>(aligned_alloc(m * n, sizeof(float)));
    // set warm up matrices to zero
    memset(A, 0, m * k * sizeof(float));
    memset(B, 0, k * n * sizeof(float));
    memset(C, 0, m * n * sizeof(float));
    void *jitter;
    mkl_jit_status_t status =
        mkl_jit_create_sgemm(&jitter, MKL_ROW_MAJOR, MKL_NOTRANS, MKL_NOTRANS,
                             m, n, k, alpha, k, n, beta, n);
    if (MKL_JIT_ERROR == status) {
      throw std::runtime_error(
          "insufficient memory to JIT and store the SGEMM kernel");
    }
    sgemm_jit_kernel_t mkl_sgemm = mkl_jit_get_sgemm_ptr(jitter);

    libxsmm_mmfunction<float> xsmm_wm_kernel(LIBXSMM_GEMM_FLAG_NONE, m, n, k,
                                             alpha, beta);

    // rounds of warming up
    for (index_t i = 0; i < 10; ++i) {
      mkl_sgemm(jitter, A, B, C);
      xsmm_wm_kernel(A, B, C);
    }
    status = mkl_jit_destroy(jitter);
    if (MKL_JIT_ERROR == status) {
      throw std::runtime_error("failed releasing JIT memory");
    }
    aligned_free(A);
    aligned_free(B);
    aligned_free(C);
    // warming up libraries end here

    for (index_t mat_n = mat_n_strt; mat_n < mat_n_end; mat_n += mat_n_inc) {
      for (index_t mat_m = mat_m_strt; mat_m < mat_m_end; mat_m += mat_m_inc) {
        m = mat_m;
        n = mat_n;
        k = mat_k;
        float *A_ROW_MAJOR = static_cast<float *>(
            _mm_malloc(m * k * sizeof(float), ALIGN_BYTE_SIZE));
        float *A_COL_MAJOR = static_cast<float *>(
            _mm_malloc(m * k * sizeof(float), ALIGN_BYTE_SIZE));
        float *B_ROW_MAJOR = static_cast<float *>(
            _mm_malloc(k * n * sizeof(float), ALIGN_BYTE_SIZE));
        float *B_COL_MAJOR = static_cast<float *>(
            _mm_malloc(k * n * sizeof(float), ALIGN_BYTE_SIZE));
        float *C = static_cast<float *>(
            _mm_malloc(m * n * sizeof(float), ALIGN_BYTE_SIZE));
        for (index_t h = 0; h < m; ++h) {
          for (index_t w = 0; w < k; ++w) {
            index_t idx = h * k + w;
            A_COL_MAJOR[idx] = idx + 1;
            A_ROW_MAJOR[w * m + h] = idx + 1;
          }
        }
        for (index_t h = 0; h < k; ++h) {
          for (index_t w = 0; w < n; ++w) {
            index_t idx = h * n + w;
            B_ROW_MAJOR[idx] = sparsity_adjustment(sparsity);
            B_COL_MAJOR[w * k + h] = B_ROW_MAJOR[idx];
          }
        }

        const index_t offset = ((mat_n - mat_n_strt) / mat_n_inc) *
                                   ((mat_m_end - mat_m_strt) / mat_m_inc) *
                                   width +
                               ((mat_m - mat_m_strt) / mat_m_inc) * width;
        results[offset] = m;
        results[offset + 1] = n;
        results[offset + 2] = k;

        // ASM Jitter
        begin = std::chrono::steady_clock::now();
        std::shared_ptr<Jitter<float>> jit_ = std::make_shared<Jitter<float>>();
        jit_->generate_code(B_ROW_MAJOR, m, k, n);
        end = std::chrono::steady_clock::now();
        for (index_t i = 0; i < iterations; ++i) {
#ifdef ENABLE_JIT
          MARLIN::sgemm('N', 'N', m, n, k, 1.0, A_COL_MAJOR, k, B_ROW_MAJOR, n,
                        0, C, n, jit_);
#else
          MARLIN::sgemm('N', 'N', m, n, k, 1.0, A_COL_MAJOR, k, B_ROW_MAJOR, n,
                        0, C, n);
#endif
        }
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 3] = duration.count() / (iterations * 1000.0f);

        // MKL JIT
        begin = std::chrono::steady_clock::now();
        mkl_jit_status_t status =
            mkl_jit_create_sgemm(&jitter, MKL_COL_MAJOR, MKL_NOTRANS,
                                 MKL_NOTRANS, m, n, k, alpha, m, k, beta, m);
        if (MKL_JIT_ERROR == status) {
          throw std::runtime_error(
              "insufficient memory to JIT and store the DGEMM kernel");
        }
        // retrieve the function pointer to the SGEMM kernel
        sgemm_jit_kernel_t mkl_sgemm = mkl_jit_get_sgemm_ptr(jitter);
        for (index_t i = 0; i < iterations; ++i) {
          mkl_sgemm(jitter, A_COL_MAJOR, B_COL_MAJOR, C);
        }
        end = std::chrono::steady_clock::now();
        status = mkl_jit_destroy(jitter);
        if (MKL_JIT_ERROR == status) {
          throw std::runtime_error("failed releasing JIT memory");
        }
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 4] = duration.count() / (iterations * 1000.0f);

        // LIBXSMM
        begin = std::chrono::steady_clock::now();
        std::unique_ptr<libxsmm_mmfunction<float>> xsmm_kernel =
            std::make_unique<libxsmm_mmfunction<float>>(LIBXSMM_GEMM_FLAG_NONE,
                                                        m, n, k, alpha, beta);
        for (index_t i = 0; i < iterations; ++i) {
          xsmm_kernel->operator()(A_COL_MAJOR, B_COL_MAJOR, C);
        }
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - begin);
        results[offset + 5] = duration.count() / (iterations * 1000.0f);

        _mm_free(A_ROW_MAJOR);
        _mm_free(A_COL_MAJOR);
        _mm_free(B_ROW_MAJOR);
        _mm_free(B_COL_MAJOR);
        _mm_free(C);
      }
    }

    std::stringstream stream;
    stream << mat_m_strt << "_" << (mat_m_end - mat_m_inc) << "_" << mat_m_inc
           << "_k" << k << "_" << std::fixed << std::setprecision(2) << sparsity
           << "_" << iterations << "_"
           << std::chrono::duration_cast<std::chrono::seconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    std::string s = stream.str();
    std::string filename;
    filename = "build/results/jitter_overhead_results_" + s + ".csv";
    std::string header = "IDX,M,N,K,MARLIN,MKL_JIT,LIBXSMM";
    tofile(filename, results, height, width, header);
    aligned_free(results);
  };

  //
  // mat_m_strt
  // mat_m_end
  // mat_m_inc
  // mat_n_strt
  // mat_n_end
  // mat_n_inc
  // mat_k
  // sparsity
  // iterations
  perf_bench(1, 51, 1, 4, 32, 4, 2, 0, 1);
  perf_bench(1, 51, 1, 4, 32, 4, 4, 0, 1);
  perf_bench(1, 51, 1, 4, 32, 4, 10, 0, 1);
}

TEST(Benchmark, WinogradJIT) {
  std::vector<index_t> vbatches = {1};
  std::vector<index_t> vin_channels = {3};
  //{3, 4, 8, 16, 32, 64, 128, 256, 512};
  std::vector<index_t> vout_channels = {
      1};  //{1,  3,   4,   8,   16,  32,64, 128, 256, 512, 1024};

  const index_t iterations = 1000;
  const index_t im_height = 32;
  const index_t im_width = 32;

#ifdef ENABLE_JIT
  const index_t width = 5;
#else
  const index_t width = 8;
#endif
  const index_t height =
      vbatches.size() * vin_channels.size() * vout_channels.size() * iterations;
  double *results =
      static_cast<double *>(malloc(height * width * sizeof(double)));
  memset(results, 0, sizeof(double) * height * width);

  std::chrono::steady_clock::time_point begin;
  std::chrono::steady_clock::time_point end;
  std::chrono::microseconds duration;
  std::shared_ptr<Tensor<float>> input;
  std::shared_ptr<Tensor<float>> filter;
  std::shared_ptr<Tensor<float>> output;

  Winograd<float, WINO_K_3x3, WINO_O_2x2> winograd;

  for (index_t b = 0; b < vbatches.size(); ++b) {
    for (index_t ch = 0; ch < vin_channels.size(); ++ch) {
      for (index_t f = 0; f < vout_channels.size(); ++f) {
        const index_t batch = vbatches[b];
        const index_t in_channels = vin_channels[ch];
        const index_t out_channels = vout_channels[f];
        const index_t b_offset = b * vin_channels.size() + ch;
        const index_t ch_offset = b_offset * vout_channels.size() + f;

        input = std::make_shared<Tensor<float>>();
        filter = std::make_shared<Tensor<float>>(true);
        output = std::make_shared<Tensor<float>>();

        input->resize({batch, in_channels, im_height, im_width});
        filter->resize({out_channels, in_channels, 3, 3});
        initialize_tensor(input);
        initialize_tensor(filter);

#ifdef ENABLE_JIT
        const index_t in_tile_area = 16;
        std::shared_ptr<Tensor<float>> transformed_filter =
            std::make_shared<Tensor<float>>();
        transformed_filter->resize({out_channels, in_channels, in_tile_area});
        float *transformed_filter_data = transformed_filter->mutable_data();
        const float *filter_data = filter->data();

        begin = std::chrono::steady_clock::now();
        winograd.transform_kernel(filter_data, out_channels, in_channels,
                                  transformed_filter_data);

        std::shared_ptr<WinoJitter<float>> jitter =
            std::make_shared<WinoJitter<float>>();
        jitter->generate_code(transformed_filter_data, in_tile_area,
                              in_channels, out_channels);
        end = std::chrono::steady_clock::now();
        duration =
            std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
        const double overhead = duration.count();
        results[ch_offset * iterations * width] += overhead;
#endif
        for (index_t i = 0; i < iterations; ++i) {
          const index_t f_offset = ch_offset * iterations + i;
          index_t idx = f_offset * width;

          results[idx++] = batch;
          results[idx++] = in_channels;
          results[idx++] = out_channels;
          results[idx++] = i;

          begin = std::chrono::steady_clock::now();
#ifdef ENABLE_JIT
          winograd.run(input, filter, output, jitter);
#else
          winograd.set_switch(LIBMARLIN);
          winograd.run(input, filter, output);
#endif
          end = std::chrono::steady_clock::now();
          duration = std::chrono::duration_cast<std::chrono::microseconds>(
              end - begin);
          results[idx++] += duration.count();
#ifndef ENABLE_JIT
          begin = std::chrono::steady_clock::now();
          winograd.set_switch(LIBMKL);
          winograd.run(input, filter, output);
          end = std::chrono::steady_clock::now();
          duration = std::chrono::duration_cast<std::chrono::microseconds>(
              end - begin);
          results[idx++] += duration.count();

          begin = std::chrono::steady_clock::now();
          winograd.set_switch(JITMKL);
          winograd.run(input, filter, output);
          end = std::chrono::steady_clock::now();
          duration = std::chrono::duration_cast<std::chrono::microseconds>(
              end - begin);
          results[idx++] += duration.count();

          begin = std::chrono::steady_clock::now();
          winograd.set_switch(LIBONEDNN);
          winograd.run(input, filter, output);
          end = std::chrono::steady_clock::now();
          duration = std::chrono::duration_cast<std::chrono::microseconds>(
              end - begin);
          results[idx++] += duration.count();

#endif
        }
      }
    }
  }
  std::stringstream stream;
  stream << "winograd"
         << "_"
         << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count();
  std::string s_ = stream.str();
  std::string filename;
#ifdef ENABLE_JIT
  std::string header = "IDX,BATCH,IN_CHANNELS,OUT_CHANNELS,ITERATION,WINOGRAD";
  filename = "build/results/jit_" + s_ + ".csv";
#else
  std::string header =
      "IDX,BATCH,IN_CHANNELS,OUT_CHANNELS,ITERATION,WINOGRAD,MKL,JITMKL"
      ",ONEDNN";
  filename = "build/results/" + s_ + ".csv";
#endif
  tofile(filename, results, height, width, header);
  free(results);
}