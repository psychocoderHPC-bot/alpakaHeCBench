/*
 * SPDX-FileCopyrightText: 2026 René Widera
 * SPDX-License-Identifier: ISC
 *
 * Numeric self-test for the HPL BLAS shim. Compares dpcpp_dgemm with
 * cblas_dgemm for all four transpose combinations on deliberately padded
 * leading dimensions.
 */

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

#include <cblas.h>

extern "C" void dpcpp_dgemm(int order, int transa, int transb, int M, int N,
                            int K, double alpha, double const *A, int lda,
                            double const *B, int ldb, double beta, double *C,
                            int ldc);

namespace {
struct Case {
  int M;
  int N;
  int K;
  CBLAS_TRANSPOSE transa;
  CBLAS_TRANSPOSE transb;
};

double maxRelativeError(std::vector<double> const &got,
                        std::vector<double> const &ref) {
  double maxError = 0.0;
  for (std::size_t i = 0; i < got.size(); ++i) {
    double const scale = std::max({1.0, std::abs(got[i]), std::abs(ref[i])});
    maxError = std::max(maxError, std::abs(got[i] - ref[i]) / scale);
  }
  return maxError;
}

bool runCase(Case const &c, std::mt19937 &rng) {
  int const aRows = c.transa == CblasTrans ? c.K : c.M;
  int const aCols = c.transa == CblasTrans ? c.M : c.K;
  int const bRows = c.transb == CblasTrans ? c.N : c.K;
  int const bCols = c.transb == CblasTrans ? c.K : c.N;

  int const lda = std::max(aRows, aCols) + 5;
  int const ldb = std::max(bRows, bCols) + 7;
  int const ldc = c.M + 3;

  std::vector<double> A(static_cast<std::size_t>(aCols) * lda);
  std::vector<double> B(static_cast<std::size_t>(bCols) * ldb);
  std::vector<double> C0(static_cast<std::size_t>(c.N) * ldc);
  std::uniform_real_distribution<double> dist(-1.0, 1.0);
  std::generate(A.begin(), A.end(), [&] { return dist(rng); });
  std::generate(B.begin(), B.end(), [&] { return dist(rng); });
  std::generate(C0.begin(), C0.end(), [&] { return dist(rng); });

  auto Cref = C0;
  auto Cgot = C0;
  double const alpha = 1.3;
  double const beta = -0.7;

  cblas_dgemm(CblasColMajor, c.transa, c.transb, c.M, c.N, c.K, alpha, A.data(),
              lda, B.data(), ldb, beta, Cref.data(), ldc);

  dpcpp_dgemm(CblasColMajor, c.transa, c.transb, c.M, c.N, c.K, alpha, A.data(),
              lda, B.data(), ldb, beta, Cgot.data(), ldc);

  double const error = maxRelativeError(Cgot, Cref);
  char const *ta = c.transa == CblasTrans ? "T" : "N";
  char const *tb = c.transb == CblasTrans ? "T" : "N";
  std::printf(
      "dgemm %s/%s M=%d N=%d K=%d lda=%d ldb=%d ldc=%d max_rel_err=%.3e %s\n",
      ta, tb, c.M, c.N, c.K, lda, ldb, ldc, error,
      error <= 1.0e-10 ? "PASS" : "FAIL");
  return error <= 1.0e-10;
}
} // namespace

int main() {
  // Keep diagnostic output even if the runtime aborts during teardown.
  std::setvbuf(stdout, nullptr, _IONBF, 0);
  std::mt19937 rng{1234567u};
  Case const cases[] = {
      {37, 41, 29, CblasNoTrans, CblasNoTrans},
      {37, 41, 29, CblasNoTrans, CblasTrans},
      {37, 41, 29, CblasTrans, CblasNoTrans},
      {37, 41, 29, CblasTrans, CblasTrans},
      {128, 96, 160, CblasNoTrans, CblasNoTrans},
      {128, 96, 160, CblasTrans, CblasTrans},
  };

  bool ok = true;
  for (auto const &c : cases)
    ok = runCase(c, rng) && ok;
  std::printf("hla_selftest: %s\n", ok ? "PASSED" : "FAILED");
  return ok ? 0 : 1;
}
