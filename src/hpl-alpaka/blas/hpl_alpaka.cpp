/*
 * SPDX-FileCopyrightText: 2026 René Widera
 * SPDX-License-Identifier: ISC
 *
 * HPL BLAS shim: HPL_dgemm/HPL_dtrsm (via the dpcpp_* macros in hpl_blas.h)
 * implemented on top of alpakaVendor. This translation unit is compiled in
 * the language of the selected backend (C++, CUDA, HIP or SYCL).
 */

#include <alpaka/alpaka.hpp>
#include <alpaka/blas.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace {
// HPL/CBLAS enum values used by hpl_blas.h (kept numeric so this file does
// not need HPL's headers).
constexpr int kColMajor = 102;
constexpr int kNoTrans = 111;
constexpr int kTrans = 112;
constexpr int kConjTrans = 113;
constexpr int kUpper = 121;
constexpr int kLower = 122;
constexpr int kUnit = 132;
constexpr int kLeft = 141;
constexpr int kRight = 142;

bool isTranspose(int op) { return op == kTrans || op == kConjTrans; }

std::string getEnvString(char const *name, char const *defaultValue) {
  char const *value = std::getenv(name);
  return std::string{value == nullptr || *value == '\0' ? defaultValue : value};
}

[[noreturn]] void fail(std::string const &message) {
  throw std::runtime_error("HPL_ALPAKA: " + message);
}

using Extents1 = alpaka::Vec<std::size_t, 1u>;
using Extents2 = alpaka::Vec<std::size_t, 2u>;

// Call alpakaVendor GEMM on a queue. A and B are the column-major HPL
// operands, C is column-major C. The row-major views represent X^T, and the
// operands are swapped so C^T = op(B)^T * op(A)^T.
template <typename T_Queue>
void callGemm(
    T_Queue &queue,
    int transa,
    int transb,
    int M,
    int N,
    int K,
    double alpha,
    double const *A,
    int lda,
    double const *B,
    int ldb,
    double beta,
    double *C,
    int ldc) {
  int const aRows = isTranspose(transa) ? K : M;
  int const aCols = isTranspose(transa) ? M : K;
  int const bRows = isTranspose(transb) ? N : K;
  int const bCols = isTranspose(transb) ? K : N;

  auto vA = alpaka::makeMdSpan(
      const_cast<double *>(A),
      Extents2{static_cast<std::size_t>(aCols),
               static_cast<std::size_t>(aRows)},
      Extents2{static_cast<std::size_t>(lda) * sizeof(double), sizeof(double)});
  auto vB = alpaka::makeMdSpan(
      const_cast<double *>(B),
      Extents2{static_cast<std::size_t>(bCols),
               static_cast<std::size_t>(bRows)},
      Extents2{static_cast<std::size_t>(ldb) * sizeof(double), sizeof(double)});
  auto vC = alpaka::makeMdSpan(
      C, Extents2{static_cast<std::size_t>(N), static_cast<std::size_t>(M)},
      Extents2{static_cast<std::size_t>(ldc) * sizeof(double), sizeof(double)});

  alpaka::blas::AnnotatedView aView{
      vA, isTranspose(transa) ? alpaka::blas::Transpose::transposed
                              : alpaka::blas::Transpose::none};
  alpaka::blas::AnnotatedView bView{
      vB, isTranspose(transb) ? alpaka::blas::Transpose::transposed
                              : alpaka::blas::Transpose::none};

  alpaka::blas::onHost::gemm(queue, alpha, bView, aView, beta, vC);
}

// HPL keeps DTRSM on the host. Use alpakaVendor's host backend with a host
// device and queue instead of calling CBLAS directly.
template <typename T_Queue>
void callHostTrsm(
    T_Queue &queue,
    int side,
    int uplo,
    int trans,
    int diag,
    int M,
    int N,
    double alpha,
    double const *A,
    int lda,
    double *B,
    int ldb) {
  int const dim = side == kLeft ? M : N;
  auto vA = alpaka::makeMdSpan(
      const_cast<double *>(A),
      Extents2{static_cast<std::size_t>(dim), static_cast<std::size_t>(dim)},
      Extents2{static_cast<std::size_t>(lda) * sizeof(double), sizeof(double)});
  auto vB = alpaka::makeMdSpan(
      B, Extents2{static_cast<std::size_t>(N), static_cast<std::size_t>(M)},
      Extents2{static_cast<std::size_t>(ldb) * sizeof(double), sizeof(double)});

  // Transpose the column-major equation. For HPL's side=left, the row-major
  // system becomes X^T * op(A)^T = alpha B^T, which is side=right.
  auto aView = isTranspose(trans)
                   ? alpaka::blas::transposed(vA)
                   : alpaka::blas::AnnotatedView{vA};
  // A row-major view of A is A^T, so the stored triangle flips.
  aView = uplo == kUpper ? alpaka::blas::lower(aView)
                         : alpaka::blas::upper(aView);
  aView = diag == kUnit ? alpaka::blas::unitDiag(aView)
                        : alpaka::blas::nonUnitDiag(aView);

  auto const rowMajorSide
      = side == kLeft ? alpaka::blas::Side::right : alpaka::blas::Side::left;
  alpaka::blas::onHost::trsm(queue, rowMajorSide, alpha, aView, vB);
}

auto &hostQueue() {
  static auto device = alpaka::onHost::makeHostDevice();
  static auto queue = device.makeQueue();
  return queue;
}

template <typename T_Device, typename T_Queue>
void runDeviceGemm(T_Device &device, T_Queue &queue, int transa, int transb,
                   int M, int N, int K, double alpha, double const *A, int lda,
                   double const *B, int ldb, double beta, double *C, int ldc);

struct Context {
  using GemmFn = std::function<void(int,
                                    int,
                                    int,
                                    int,
                                    int,
                                    double,
                                    double const *,
                                    int,
                                    double const *,
                                    int,
                                    double,
                                    double *,
                                    int)>;
  GemmFn gemm;
};

template <typename T_DeviceSpec> Context makeContext(T_DeviceSpec spec) {
  auto selector = alpaka::onHost::makeDeviceSelector(spec);
  if (!selector.isAvailable())
    fail("requested backend has no available device: " + spec.getName());

  auto device =
      std::make_shared<decltype(selector.makeDevice(0))>(selector.makeDevice(0));
  auto queue =
      std::make_shared<decltype(device->makeQueue())>(device->makeQueue());

  return Context{[device, queue](int transa, int transb, int M, int N, int K,
                                 double alpha, double const *A, int lda,
                                 double const *B, int ldb, double beta,
                                 double *C, int ldc) {
    runDeviceGemm(*device, *queue, transa, transb, M, N, K, alpha, A, lda, B,
                  ldb, beta, C, ldc);
  }};
}

bool matchesRequestedBackend(
    std::string const &requested,
    alpaka::concepts::DeviceSpec auto const &spec) {
  if (requested == "cpu")
    return spec.getApi() == alpaka::api::host
        && spec.getDeviceKind() == alpaka::deviceKind::cpu;
  if (requested == "cuda")
    return spec.getApi() == alpaka::api::cuda
        && spec.getDeviceKind() == alpaka::deviceKind::nvidiaGpu;
  if (requested == "hip")
    return spec.getApi() == alpaka::api::hip
        && spec.getDeviceKind() == alpaka::deviceKind::amdGpu;
  if (requested == "sycl")
    return spec.getApi() == alpaka::api::oneApi
        && spec.getDeviceKind() == alpaka::deviceKind::intelGpu;
  return false;
}

Context &getContext() {
  static Context *context = [] {
    std::string const requested = getEnvString("HPL_ALPAKA_DEVICE", "");
    if (requested.empty())
      fail("HPL_ALPAKA_DEVICE is not set; choose cpu, cuda, hip, or sycl");
    if (requested != "cpu" && requested != "cuda" && requested != "hip"
        && requested != "sycl")
      fail("unknown HPL_ALPAKA_DEVICE value '" + requested
           + "'; choose cpu, cuda, hip, or sycl");

    Context *result = nullptr;
    std::string available;
    auto trySpec = [&](auto const &spec) {
      if (result)
        return;
      if (!available.empty())
        available += ", ";
      available += spec.getName();
      if (!matchesRequestedBackend(requested, spec))
        return;
      auto selector = alpaka::onHost::makeDeviceSelector(spec);
      if (!selector.isAvailable())
        fail("HPL_ALPAKA_DEVICE=" + requested
             + " requested, but no matching device is available at runtime");
      result = new Context(makeContext(spec));
    };
    std::apply([&](auto const &...specs) { (trySpec(specs), ...); },
               alpaka::onHost::enabledDeviceSpecs);

    if (!result)
      fail("HPL_ALPAKA_DEVICE=" + requested
           + " is not enabled in this alpaka build; available: " + available);
    return result;
  }();
  return *context;
}

template <typename T_Device, typename T_Queue>
void runDeviceGemm(T_Device &device, T_Queue &queue, int transa, int transb,
                   int M, int N, int K, double alpha, double const *A, int lda,
                   double const *B, int ldb, double beta, double *C, int ldc) {
  int const aRows = isTranspose(transa) ? K : M;
  int const aCols = isTranspose(transa) ? M : K;
  int const bRows = isTranspose(transb) ? N : K;
  int const bCols = isTranspose(transb) ? K : N;
  std::size_t const aCount = static_cast<std::size_t>(aCols - 1) * lda + aRows;
  std::size_t const bCount = static_cast<std::size_t>(bCols - 1) * ldb + bRows;
  std::size_t const cCount = static_cast<std::size_t>(N - 1) * ldc + M;

  // Mirror ORNL's cuda_dgemm.cpp: same operand sizes, all three copies,
  // one synchronization after the upload, vendor GEMM, C download, and free.
  std::size_t const aElems = std::max(
      static_cast<std::size_t>(K) * static_cast<std::size_t>(lda), aCount);
  std::size_t const bElems = std::max(
      static_cast<std::size_t>(N) * static_cast<std::size_t>(ldb), bCount);
  std::size_t const cElems = std::max(
      static_cast<std::size_t>(N) * static_cast<std::size_t>(ldc), cCount);

  static auto hostDevice = alpaka::onHost::makeHostDevice();
  auto hostViewA = alpaka::onHost::SharedBuffer(
      hostDevice, const_cast<double *>(A), Extents1{aElems}, Extents1{1u},
      [] {});
  auto hostViewB = alpaka::onHost::SharedBuffer(
      hostDevice, const_cast<double *>(B), Extents1{bElems}, Extents1{1u},
      [] {});
  auto hostViewC = alpaka::onHost::SharedBuffer(
      hostDevice, C, Extents1{cElems}, Extents1{1u}, [] {});

  auto deviceA = alpaka::onHost::alloc<double>(device, Extents1{aElems});
  auto deviceB = alpaka::onHost::alloc<double>(device, Extents1{bElems});
  auto deviceC = alpaka::onHost::alloc<double>(device, Extents1{cElems});

  alpaka::onHost::memcpy(queue, deviceA, hostViewA);
  alpaka::onHost::memcpy(queue, deviceB, hostViewB);
  alpaka::onHost::memcpy(queue, deviceC, hostViewC);
  alpaka::onHost::wait(queue);

  callGemm(queue, transa, transb, M, N, K, alpha,
           alpaka::onHost::data(deviceA), lda,
           alpaka::onHost::data(deviceB), ldb, beta,
           alpaka::onHost::data(deviceC), ldc);
  alpaka::onHost::wait(queue);

  alpaka::onHost::memcpy(queue, hostViewC, deviceC);
  alpaka::onHost::wait(queue);
}

void runHostGemm(int transa, int transb, int M, int N, int K, double alpha,
                 double const *A, int lda, double const *B, int ldb,
                 double beta, double *C, int ldc) {
  auto &queue = hostQueue();
  callGemm(queue, transa, transb, M, N, K, alpha, A, lda, B, ldb, beta, C, ldc);
  alpaka::onHost::wait(queue);
}
} // namespace

extern "C" void dpcpp_dgemm(int const order, int const transa, int const transb,
                            int const M, int const N, int const K,
                            double const alpha, double const *A, int const lda,
                            double const *B, int const ldb, double const beta,
                            double *C, int const ldc) {
  try {
    if (M == 0 || N == 0 || K == 0)
      return;
    if (order != kColMajor)
      fail("dgemm requires column-major order");
    if (N < 64 || M < 128 || K < 128) {
      runHostGemm(transa, transb, M, N, K, alpha, A, lda, B, ldb, beta, C,
                  ldc);
      return;
    }
    getContext().gemm(transa, transb, M, N, K, alpha, A, lda, B, ldb, beta, C,
                      ldc);
  } catch (std::exception const &e) {
    std::fprintf(stderr, "%s\n", e.what());
    std::exit(EXIT_FAILURE);
  }
}

extern "C" void dpcpp_dtrsm(int const order, int const side, int const uplo,
                            int const trans, int const diag, int const M,
                            int const N, double const alpha, double const *A,
                            int const lda, double *B, int const ldb) {
  try {
    if (M == 0 || N == 0)
      return;
    if (order != kColMajor)
      fail("dtrsm requires column-major order");
    callHostTrsm(hostQueue(), side, uplo, trans, diag, M, N, alpha, A, lda, B,
                 ldb);
    alpaka::onHost::wait(hostQueue());
  } catch (std::exception const &e) {
    std::fprintf(stderr, "%s\n", e.what());
    std::exit(EXIT_FAILURE);
  }
}
