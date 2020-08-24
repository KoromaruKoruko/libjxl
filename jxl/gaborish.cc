// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "jxl/gaborish.h"

#include <stddef.h>

#include "jxl/base/status.h"
#include "jxl/convolve.h"
#include "jxl/image_ops.h"

namespace jxl {

Image3F GaborishInverse(const Image3F& in, float mul, ThreadPool* pool) {
  JXL_ASSERT(mul >= 0.0f);

  // Only an approximation. One or even two 3x3, and rank-1 (separable) 5x5
  // are insufficient.
  constexpr float kGaborish[5] = {
      -0.092359145662814029f,  -0.039253623634014627f, 0.016176494530216929f,
      0.00083458437774987476f, 0.004512465323949319f,
  };
  /*
    better would be:
      1.0 - mul * (4 * (kGaborish[0] + kGaborish[1] +
                        kGaborish[2] + kGaborish[4]) +
                   8 * (kGaborish[3]));
  */
  WeightsSymmetric5 weights = {{HWY_REP4(1.0f)},
                               {HWY_REP4(mul * kGaborish[0])},
                               {HWY_REP4(mul * kGaborish[2])},
                               {HWY_REP4(mul * kGaborish[1])},
                               {HWY_REP4(mul * kGaborish[4])},
                               {HWY_REP4(mul * kGaborish[3])}};
  double sum = weights.c[0];
  sum += 4 * weights.r[0];
  sum += 4 * weights.R[0];
  sum += 4 * weights.d[0];
  sum += 4 * weights.D[0];
  sum += 8 * weights.L[0];
  const float normalize = 1.0f / sum;
  for (size_t i = 0; i < 4; ++i) {
    weights.c[i] *= normalize;
    weights.r[i] *= normalize;
    weights.R[i] *= normalize;
    weights.d[i] *= normalize;
    weights.D[i] *= normalize;
    weights.L[i] *= normalize;
  }

  Image3F sharpened(in.xsize(), in.ysize());
  Symmetric5_3(in, Rect(in), weights, pool, &sharpened);
  return sharpened;
}

namespace {

// weight1,2 need not be normalized.
WeightsSymmetric3 GaborishKernel(float weight1, float weight2) {
  constexpr float weight0 = 1.0f;

  // Normalize
  const float mul = 1.0f / (weight0 + 4 * (weight1 + weight2));
  const float w0 = weight0 * mul;
  const float w1 = weight1 * mul;
  const float w2 = weight2 * mul;

  const WeightsSymmetric3 w = {{HWY_REP4(w0)}, {HWY_REP4(w1)}, {HWY_REP4(w2)}};
  return w;
}

}  // namespace

void ConvolveGaborish(const ImageF& in, float weight1, float weight2,
                      ThreadPool* pool, ImageF* JXL_RESTRICT out) {
  JXL_CHECK(SameSize(in, *out));
  Symmetric3(in, Rect(in), GaborishKernel(weight1, weight2), pool, out);
}

}  // namespace jxl