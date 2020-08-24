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

#include "jxl/extras/codec_jpg.h"

#include <stddef.h>
#include <stdio.h>
// After stddef/stdio
#include <jpeglib.h>
#include <setjmp.h>
#include <stdint.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "jxl/base/compiler_specific.h"
#include "jxl/color_encoding.h"
#include "jxl/color_management.h"
#include "jxl/common.h"
#include "jxl/image.h"
#include "jxl/image_bundle.h"
#include "jxl/image_ops.h"
#include "jxl/luminance.h"
#if JPEGXL_ENABLE_SJPEG
#include "sjpeg.h"
#endif

#ifdef MEMORY_SANITIZER
#include "sanitizer/msan_interface.h"
#endif

namespace jxl {

namespace {

constexpr float kJPEGSampleMultiplier = 1 << (BITS_IN_JSAMPLE - 8);
constexpr unsigned char kICCSignature[12] = {
    0x49, 0x43, 0x43, 0x5F, 0x50, 0x52, 0x4F, 0x46, 0x49, 0x4C, 0x45, 0x00};
constexpr int kICCMarker = JPEG_APP0 + 2;
constexpr size_t kMaxBytesInMarker = 65533;

constexpr float kJPEGSampleMin = std::numeric_limits<JSAMPLE>::min();
constexpr float kJPEGSampleMax = std::numeric_limits<JSAMPLE>::max();

bool MarkerIsICC(const jpeg_saved_marker_ptr marker) {
  return marker->marker == kICCMarker &&
         marker->data_length >= sizeof kICCSignature + 2 &&
         std::equal(std::begin(kICCSignature), std::end(kICCSignature),
                    marker->data);
}

Status ReadICCProfile(jpeg_decompress_struct* const cinfo,
                      PaddedBytes* const icc) {
  // Markers are 1-indexed, and we keep them that way in this vector to get a
  // convenient 0 at the front for when we compute the offsets later.
  std::vector<size_t> marker_lengths;
  int num_markers = 0;
  bool has_num_markers = false;
  for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != nullptr;
       marker = marker->next) {
#ifdef MEMORY_SANITIZER
    // marker is initialized by libjpeg, which we are not instrumenting with
    // msan.
    __msan_unpoison(marker, sizeof(*marker));
    __msan_unpoison(marker->data, marker->data_length);
#endif
    if (!MarkerIsICC(marker)) continue;

    const int current_marker = marker->data[sizeof kICCSignature];
    const int current_num_markers = marker->data[sizeof kICCSignature + 1];
    if (has_num_markers) {
      if (current_num_markers != num_markers) {
        return JXL_FAILURE("inconsistent numbers of JPEG ICC markers");
      }
    } else {
      num_markers = current_num_markers;
      has_num_markers = true;
      marker_lengths.resize(num_markers + 1);
    }

    if (marker_lengths[current_marker] != 0) {
      return JXL_FAILURE("duplicate JPEG ICC marker number");
    }
    marker_lengths[current_marker] =
        marker->data_length - sizeof kICCSignature - 2;
  }

  if (marker_lengths.empty()) {
    // Not an error.
    return false;
  }

  std::vector<size_t> offsets = std::move(marker_lengths);
  std::partial_sum(offsets.begin(), offsets.end(), offsets.begin());
  icc->resize(offsets.back());

  for (jpeg_saved_marker_ptr marker = cinfo->marker_list; marker != nullptr;
       marker = marker->next) {
    if (!MarkerIsICC(marker)) continue;
    const uint8_t* first = marker->data + sizeof kICCSignature + 2;
    size_t count = marker->data_length - sizeof kICCSignature - 2;
    size_t offset = offsets[marker->data[sizeof kICCSignature] - 1];
    if (offset + count > icc->size()) {
      // TODO(lode): catch this issue earlier at the root cause of this.
      return JXL_FAILURE("ICC out of bounds");
    }
    std::copy_n(first, count, icc->data() + offset);
  }

  return true;
}

void WriteICCProfile(jpeg_compress_struct* const cinfo,
                     const PaddedBytes& icc) {
  constexpr size_t kMaxIccBytesInMarker =
      kMaxBytesInMarker - sizeof kICCSignature - 2;
  const int num_markers =
      static_cast<int>(DivCeil(icc.size(), kMaxIccBytesInMarker));
  size_t begin = 0;
  for (int current_marker = 0; current_marker < num_markers; ++current_marker) {
    const auto length = std::min(kMaxIccBytesInMarker, icc.size() - begin);
    jpeg_write_m_header(
        cinfo, kICCMarker,
        static_cast<unsigned int>(length + sizeof kICCSignature + 2));
    for (const unsigned char c : kICCSignature) {
      jpeg_write_m_byte(cinfo, c);
    }
    jpeg_write_m_byte(cinfo, current_marker + 1);
    jpeg_write_m_byte(cinfo, num_markers);
    for (int i = 0; i < length; ++i) {
      jpeg_write_m_byte(cinfo, icc[begin]);
      ++begin;
    }
  }
}

Status SetChromaSubsampling(const YCbCrChromaSubsampling chroma_subsampling,
                            jpeg_compress_struct* const cinfo) {
  cinfo->comp_info[1].h_samp_factor = 1;
  cinfo->comp_info[1].v_samp_factor = 1;
  cinfo->comp_info[2].h_samp_factor = 1;
  cinfo->comp_info[2].v_samp_factor = 1;
  switch (chroma_subsampling) {
    case YCbCrChromaSubsampling::kAuto:
      return JXL_FAILURE(
          "no rule for setting chroma subsampling automatically with libjpeg");

    case YCbCrChromaSubsampling::k444:
      cinfo->comp_info[0].h_samp_factor = 1;
      cinfo->comp_info[0].v_samp_factor = 1;
      return true;

    case YCbCrChromaSubsampling::k422:
      cinfo->comp_info[0].h_samp_factor = 2;
      cinfo->comp_info[0].v_samp_factor = 1;
      return true;

    case YCbCrChromaSubsampling::k420:
      cinfo->comp_info[0].h_samp_factor = 2;
      cinfo->comp_info[0].v_samp_factor = 2;
      return true;

    case YCbCrChromaSubsampling::k440:
      cinfo->comp_info[0].h_samp_factor = 1;
      cinfo->comp_info[0].v_samp_factor = 2;
      return true;

    default:
      return JXL_FAILURE("invalid chroma subsampling");
  }
}

void MyErrorExit(j_common_ptr cinfo) {
  jmp_buf* env = static_cast<jmp_buf*>(cinfo->client_data);
  (*cinfo->err->output_message)(cinfo);
  jpeg_destroy_decompress(reinterpret_cast<j_decompress_ptr>(cinfo));
  longjmp(*env, 1);
}

void MyOutputMessage(j_common_ptr cinfo) {
#if JXL_DEBUG_WARNING == 1
  char buf[JMSG_LENGTH_MAX];
  (*cinfo->err->format_message)(cinfo, buf);
  JXL_WARNING("%s", buf);
#endif
}

constexpr int kPlaneOrder[] = {1, 0, 2};
constexpr int kInvPlaneOrder[] = {1, 0, 2};

}  // namespace

Status DecodeImageJPG(const Span<const uint8_t> bytes, ThreadPool* pool,
                      CodecInOut* io) {
  // Don't do anything for non-JPEG files (no need to report an error)
  if (!IsJPG(bytes)) return false;

  // We need to declare all the non-trivial destructor local variables before
  // the call to setjmp().
  ColorEncoding color_encoding;
  PaddedBytes icc;
  Image3F coeffs;
  Image3F image;
  std::unique_ptr<JSAMPLE[]> row;
  ImageBundle bundle(&io->metadata);
  const DecodeTarget target = io->dec_target;

  jpeg_decompress_struct cinfo;
#ifdef MEMORY_SANITIZER
  // cinfo is initialized by libjpeg, which we are not instrumenting with
  // msan, therefore we need to initialize cinfo here.
  memset(&cinfo, 0, sizeof(cinfo));
#endif
  // Setup error handling in jpeg library so we can deal with broken jpegs in
  // the fuzzer.
  jpeg_error_mgr jerr;
  jmp_buf env;
  cinfo.err = jpeg_std_error(&jerr);
  jerr.error_exit = &MyErrorExit;
  jerr.output_message = &MyOutputMessage;
  if (setjmp(env)) {
    return false;
  }
  cinfo.client_data = static_cast<void*>(&env);

  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, reinterpret_cast<const unsigned char*>(bytes.data()),
               bytes.size());
  jpeg_save_markers(&cinfo, kICCMarker, 0xFFFF);
  jpeg_read_header(&cinfo, TRUE);
  if (ReadICCProfile(&cinfo, &icc)) {
    if (!color_encoding.SetICC(std::move(icc))) {
      jpeg_abort_decompress(&cinfo);
      jpeg_destroy_decompress(&cinfo);
      return JXL_FAILURE("read an invalid ICC profile");
    }
  } else {
    color_encoding = ColorEncoding::SRGB(cinfo.output_components == 1);
  }
  io->metadata.SetUintSamples(BITS_IN_JSAMPLE);
  io->metadata.color_encoding = color_encoding;
  io->enc_size = bytes.size();
  int nbcomp = cinfo.num_components;
  if (nbcomp != 1 && nbcomp != 3) {
    jpeg_abort_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return JXL_FAILURE("unsupported number of components (%d) in JPEG",
                       cinfo.output_components);
  }
  (void)io->dec_hints.Foreach(
      [](const std::string& key, const std::string& /*value*/) {
        JXL_WARNING("JPEG decoder ignoring %s hint", key.c_str());
        return true;
      });

  if (target == DecodeTarget::kPixels) {
    jpeg_start_decompress(&cinfo);
    JXL_ASSERT(cinfo.output_components == nbcomp);
    image = Image3F(cinfo.image_width, cinfo.image_height);
    row.reset(new JSAMPLE[cinfo.output_components * cinfo.image_width]);
    for (size_t y = 0; y < image.ysize(); ++y) {
      JSAMPROW rows[] = {row.get()};
      jpeg_read_scanlines(&cinfo, rows, 1);
#ifdef MEMORY_SANITIZER
      __msan_unpoison(row.get(), sizeof(JSAMPLE) * cinfo.output_components *
                                     cinfo.image_width);
#endif
      float* const JXL_RESTRICT output_row[] = {
          image.PlaneRow(0, y), image.PlaneRow(1, y), image.PlaneRow(2, y)};
      if (cinfo.output_components == 1) {
        for (size_t x = 0; x < image.xsize(); ++x) {
          output_row[0][x] = output_row[1][x] = output_row[2][x] =
              row[x] * (1.f / kJPEGSampleMultiplier);
        }
      } else {  // 3 components
        for (size_t x = 0; x < image.xsize(); ++x) {
          for (size_t c = 0; c < 3; ++c) {
            output_row[c][x] = row[3 * x + c] * (1.f / kJPEGSampleMultiplier);
          }
        }
      }
    }
    io->SetFromImage(std::move(image), color_encoding);
    JXL_RETURN_IF_ERROR(Map255ToTargetNits(io, pool));
  } else {  // (target == DecodeTarget::kQuantizedCoeffs)
    jvirt_barray_ptr* coeffs_array = jpeg_read_coefficients(&cinfo);

#ifdef MEMORY_SANITIZER
    if (cinfo.comp_info) {
      __msan_unpoison(cinfo.comp_info,
                      sizeof(*cinfo.comp_info) * cinfo.num_components);
    }
#endif

    std::vector<int> normalized_h_samp_factor(nbcomp),
        normalized_v_samp_factor(nbcomp);
    for (size_t i = 0; i < nbcomp; i++) {
      normalized_h_samp_factor[i] = cinfo.comp_info[i].h_samp_factor;
      normalized_v_samp_factor[i] = cinfo.comp_info[i].v_samp_factor;
    }
    int div_h = *std::min_element(normalized_h_samp_factor.begin(),
                                  normalized_h_samp_factor.end());
    int div_v = *std::min_element(normalized_v_samp_factor.begin(),
                                  normalized_v_samp_factor.end());
    bool normalize = true;
    for (size_t i = 0; i < nbcomp; i++) {
      if (normalized_h_samp_factor[i] % div_h != 0 ||
          normalized_v_samp_factor[i] % div_v != 0) {
        normalize = false;
      }
    }
    if (normalize) {
      for (size_t i = 0; i < nbcomp; i++) {
        normalized_h_samp_factor[i] /= div_h;
        normalized_v_samp_factor[i] /= div_v;
      }
    }

    YCbCrChromaSubsampling cs = YCbCrChromaSubsampling::k444;
    std::vector<int> sf211 = {2, 1, 1};
    std::vector<int> sf111 = {1, 1, 1};
    if (nbcomp == 1 || (normalized_h_samp_factor == sf111 &&
                        normalized_v_samp_factor == sf111)) {
      cs = YCbCrChromaSubsampling::k444;
    } else if (normalized_h_samp_factor == sf211 &&
               normalized_v_samp_factor == sf211) {
      cs = YCbCrChromaSubsampling::k420;
    } else if (normalized_h_samp_factor == sf211 &&
               normalized_v_samp_factor == sf111) {
      cs = YCbCrChromaSubsampling::k422;
    } else if (normalized_h_samp_factor == sf111 &&
               normalized_v_samp_factor == sf211) {
      cs = YCbCrChromaSubsampling::k440;
    } else {
      for (int ci = 0; ci < nbcomp; ci++) {
        jpeg_component_info* compptr = cinfo.comp_info + ci;
        int srh = compptr->h_samp_factor;
        int srv = compptr->v_samp_factor;
        if (srh > 1 || srv > 1) {
          return JXL_FAILURE("Cannot handle this chroma subsampling mode");
        }
      }
    }

    io->frames.clear();
    io->frames.reserve(1);

    bundle.is_jpeg = true;
    bundle.jpeg_xsize = cinfo.image_width;
    bundle.jpeg_ysize = cinfo.image_height;
    bundle.chroma_subsampling = cs;
    bundle.color_transform =
        (cinfo.jpeg_color_space == JCS_YCbCr || nbcomp == 1)
            ? ColorTransform::kYCbCr
            : ColorTransform::kNone;

    coeffs = Image3F(cinfo.comp_info->width_in_blocks * 8,
                     cinfo.comp_info->height_in_blocks * 8);

    ZeroFillImage(&coeffs);
    for (int ci : kPlaneOrder) {
      if (ci >= nbcomp) {
        for (size_t i = 0; i < kDCTBlockSize; i++) {
          bundle.jpeg_quant_table.push_back(1);
        }
        continue;
      }
      jpeg_component_info* compptr = cinfo.comp_info + ci;
#ifdef MEMORY_SANITIZER
      if (compptr->quant_table) {
        __msan_unpoison(compptr->quant_table, sizeof(*compptr->quant_table));
      }
#endif
      int hib = compptr->height_in_blocks;
      int wib = compptr->width_in_blocks;
      for (size_t i = 0; i < kDCTBlockSize; i++) {
        bundle.jpeg_quant_table.push_back(compptr->quant_table->quantval[i]);
      }
      const intptr_t onerow = coeffs.PixelsPerRow();
      for (int by = 0; by < hib; by++) {
#ifdef MEMORY_SANITIZER
        if (cinfo.mem) {
          __msan_unpoison(cinfo.mem, sizeof(*cinfo.mem));
        }
#endif
        const JDIMENSION kNumRows = 1;
        const boolean kWriteable = FALSE;
        JBLOCKARRAY buffer = (cinfo.mem->access_virt_barray)(
            (j_common_ptr)&cinfo, coeffs_array[ci], by, kNumRows, kWriteable);
#ifdef MEMORY_SANITIZER
        if (buffer) {
          __msan_unpoison(buffer, sizeof(buffer));
        }
#endif

        float* JXL_RESTRICT coeff =
            const_cast<float*>(coeffs.PlaneRow((ci < 2 ? 1 - ci : 2), by * 8));
        for (int bx = 0; bx < wib; bx++) {
          JCOEFPTR blockptr = buffer[0][bx];
          for (int i = 0; i < 64; i++) {
            coeff[8 * bx + (i % 8) + onerow * (i / 8)] = blockptr[i];
          }
        }
      }
    }

    bundle.SetFromImage(std::move(coeffs), color_encoding);
    io->frames.push_back(std::move(bundle));
    io->metadata.SetIntensityTarget(
        io->target_nits != 0 ? io->target_nits : kDefaultIntensityTarget);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  io->dec_pixels = io->xsize() * io->ysize();
  return true;
}

Status EncodeWithLibJpeg(const ImageBundle* ib, size_t quality,
                         YCbCrChromaSubsampling chroma_subsampling,
                         PaddedBytes* bytes, const DecodeTarget target) {
  jpeg_compress_struct cinfo;
  jvirt_barray_ptr coeffs_array[3] = {};
#ifdef MEMORY_SANITIZER
  // cinfo is initialized by libjpeg, which we are not instrumenting with
  // msan.
  __msan_unpoison(&cinfo, sizeof(cinfo));
#endif
  jpeg_error_mgr jerr;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  unsigned char* buffer = nullptr;
  unsigned long size = 0;
  jpeg_mem_dest(&cinfo, &buffer, &size);
  cinfo.image_width = ib->xsize();
  cinfo.image_height = ib->ysize();
  if (ib->IsGray()) {
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
  } else {
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
  }
  jpeg_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;
  if (target == DecodeTarget::kPixels) {
    if (cinfo.input_components == 3) {
      JXL_RETURN_IF_ERROR(SetChromaSubsampling(chroma_subsampling, &cinfo));
    }
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    if (!ib->IsSRGB()) {
      WriteICCProfile(&cinfo, ib->c_current().ICC());
    }
    if (cinfo.input_components > 3)
      return JXL_FAILURE("invalid numbers of components");

    std::unique_ptr<JSAMPLE[]> row(
        new JSAMPLE[cinfo.input_components * cinfo.image_width]);
    for (size_t y = 0; y < ib->ysize(); ++y) {
      const float* const JXL_RESTRICT input_row[3] = {
          ib->color().ConstPlaneRow(0, y), ib->color().ConstPlaneRow(1, y),
          ib->color().ConstPlaneRow(2, y)};
      for (size_t x = 0; x < ib->xsize(); ++x) {
        for (size_t c = 0; c < cinfo.input_components; ++c) {
          JXL_RETURN_IF_ERROR(c < 3);
          row[cinfo.input_components * x + c] = static_cast<JSAMPLE>(
              std::max(std::min(kJPEGSampleMultiplier * input_row[c][x] + .5f,
                                kJPEGSampleMax),
                       kJPEGSampleMin));
        }
      }
      JSAMPROW rows[] = {row.get()};
      jpeg_write_scanlines(&cinfo, rows, 1);
    }
  } else {
    cinfo.image_width = ib->xsize();
    cinfo.image_height = ib->ysize();
#if JPEG_LIB_VERSION >= 70
    cinfo.jpeg_width = ib->xsize();
    cinfo.jpeg_height = ib->ysize();
    cinfo.scale_num = 1;
    cinfo.scale_denom = 1;
    cinfo.min_DCT_h_scaled_size = DCTSIZE;
    cinfo.min_DCT_v_scaled_size = DCTSIZE;
#endif  // JPEG_LIB_VERSION >= 70

    cinfo.jpeg_color_space = JCS_RGB;
    if (ib->color_transform == ColorTransform::kYCbCr)
      cinfo.jpeg_color_space = JCS_YCbCr;
    if (cinfo.input_components == 3) {
      JXL_RETURN_IF_ERROR(SetChromaSubsampling(ib->chroma_subsampling, &cinfo));
    }

    // reconstruct the height-in-blocks (hib) and width-in-blocks (wib)
    int hib = ib->color().ysize() / 8;
    int wib = ib->color().xsize() / 8;
    int chroma_hib = hib;
    int chroma_wib = wib;
    int luma_alloc_hib = hib;
    int luma_alloc_wib = wib;
    if (ib->chroma_subsampling == YCbCrChromaSubsampling::k420 ||
        ib->chroma_subsampling == YCbCrChromaSubsampling::k440) {
      chroma_hib = ((ib->ysize() + 1) / 2 + 7) / 8;
      luma_alloc_hib = 2 * chroma_hib;
    }
    if (ib->chroma_subsampling == YCbCrChromaSubsampling::k420 ||
        ib->chroma_subsampling == YCbCrChromaSubsampling::k422) {
      chroma_wib = ((ib->xsize() + 1) / 2 + 7) / 8;
      luma_alloc_wib = 2 * chroma_wib;
    }

    for (int ci = 0; ci < 3; ci++) {
      jpeg_component_info* compptr = cinfo.comp_info + ci;
      if (ci == 0) {
        compptr->height_in_blocks = hib;
        compptr->width_in_blocks = wib;
        coeffs_array[ci] = cinfo.mem->request_virt_barray(
            (j_common_ptr)&cinfo, JPOOL_IMAGE, TRUE, luma_alloc_wib,
            luma_alloc_hib, luma_alloc_hib);
      } else {
        compptr->height_in_blocks = chroma_hib;
        compptr->width_in_blocks = chroma_wib;
        coeffs_array[ci] = cinfo.mem->request_virt_barray(
            (j_common_ptr)&cinfo, JPOOL_IMAGE, TRUE, compptr->width_in_blocks,
            compptr->height_in_blocks, compptr->height_in_blocks);
      }
      cinfo.quant_tbl_ptrs[ci] = jpeg_alloc_quant_table((j_common_ptr)&cinfo);
      cinfo.quant_tbl_ptrs[ci]->sent_table = FALSE;
      compptr->quant_tbl_no = ci;
    }
    cinfo.mem->realize_virt_arrays((j_common_ptr)&cinfo);
    for (int ci = 0; ci < 3; ci++) {
      int src_q = kInvPlaneOrder[ci] * 64;
      for (int i = 0; i < 64; i++) {
        cinfo.quant_tbl_ptrs[ci]->quantval[i] = ib->jpeg_quant_table[i + src_q];
      }
    }
    jpeg_write_coefficients(&cinfo, coeffs_array);
    intptr_t onerow = ib->color().PixelsPerRow();
    for (int ci = 0; ci < 3; ci++) {
      for (int by = 0; by < (ci == 0 ? hib : chroma_hib); by++) {
        JBLOCKARRAY buffer = (cinfo.mem->access_virt_barray)(
            (j_common_ptr)&cinfo, coeffs_array[ci], by, (JDIMENSION)1, TRUE);
#ifdef MEMORY_SANITIZER
        if (buffer) {
          __msan_unpoison(buffer, sizeof(buffer));
        }
#endif
        const float* JXL_RESTRICT coeff =
            ib->color().ConstPlaneRow((ci < 2 ? 1 - ci : ci), by * 8);
        for (int bx = 0; bx < (ci == 0 ? wib : chroma_wib); bx++) {
          JCOEFPTR blockptr = buffer[0][bx];
          for (int i = 0; i < 64; i++) {
            blockptr[i] = coeff[8 * bx + (i % 8) + onerow * (i / 8)];
          }
        }
      }
    }
    if (!ib->IsSRGB()) {
      WriteICCProfile(&cinfo, ib->c_current().ICC());
    }
  }
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  bytes->resize(size);
#ifdef MEMORY_SANITIZER
  // Compressed image data is initialized by libjpeg, which we are not
  // instrumenting with msan.
  __msan_unpoison(buffer, size);
#endif
  std::copy_n(buffer, size, bytes->data());
  std::free(buffer);
  return true;
}

Status EncodeWithSJpeg(const ImageBundle* ib, size_t quality,
                       YCbCrChromaSubsampling chroma_subsampling,
                       PaddedBytes* bytes) {
#if !JPEGXL_ENABLE_SJPEG
  return JXL_FAILURE("JPEG XL was built without sjpeg support");
#else
  sjpeg::EncoderParam param(quality);
  if (!ib->IsSRGB()) {
    param.iccp.assign(ib->metadata()->color_encoding.ICC().begin(),
                      ib->metadata()->color_encoding.ICC().end());
  }
  switch (chroma_subsampling) {
    case YCbCrChromaSubsampling::kAuto:
      param.yuv_mode = SJPEG_YUV_AUTO;
      break;
    case YCbCrChromaSubsampling::k444:
      param.yuv_mode = SJPEG_YUV_444;
      break;
    case YCbCrChromaSubsampling::k422:
      return JXL_FAILURE("sjpeg does not support 4:2:2 chroma subsampling");
    case YCbCrChromaSubsampling::k420:
      param.yuv_mode = SJPEG_YUV_SHARP;
      break;
    case YCbCrChromaSubsampling::k440:
      return JXL_FAILURE("sjpeg does not support 4:4:0 chroma subsampling");
  }
  std::vector<uint8_t> rgb;
  rgb.reserve(ib->xsize() * ib->ysize() * 3);
  for (size_t y = 0; y < ib->ysize(); ++y) {
    const float* const rows[] = {
        ib->color().ConstPlaneRow(0, y),
        ib->color().ConstPlaneRow(1, y),
        ib->color().ConstPlaneRow(2, y),
    };
    for (size_t x = 0; x < ib->xsize(); ++x) {
      for (const float* const row : rows) {
        rgb.push_back(static_cast<uint8_t>(
            std::max(0.f, std::min(255.f, std::round(row[x])))));
      }
    }
  }
  std::string output;
  JXL_RETURN_IF_ERROR(sjpeg::Encode(rgb.data(), ib->xsize(), ib->ysize(),
                                    ib->xsize() * 3, param, &output));
  bytes->assign(
      reinterpret_cast<const uint8_t*>(output.data()),
      reinterpret_cast<const uint8_t*>(output.data() + output.size()));
  return true;
#endif
}

Status EncodeImageJPG(const CodecInOut* io, JpegEncoder encoder, size_t quality,
                      YCbCrChromaSubsampling chroma_subsampling,
                      ThreadPool* pool, PaddedBytes* bytes,
                      const DecodeTarget target) {
  if (io->Main().HasAlpha()) {
    return JXL_FAILURE("alpha is not supported");
  }
  if (quality < 0 || quality > 100) {
    return JXL_FAILURE("please specify a 0-100 JPEG quality");
  }

  ImageBundle ib_0_255 = io->Main().Copy();
  if (target == DecodeTarget::kPixels) {
    JXL_RETURN_IF_ERROR(MapTargetNitsTo255(&ib_0_255, pool));
  }
  const ImageBundle* ib;
  ImageMetadata metadata = io->metadata;
  ImageBundle ib_store(&metadata);
  JXL_RETURN_IF_ERROR(TransformIfNeeded(ib_0_255, io->metadata.color_encoding,
                                        pool, &ib_store, &ib));

  switch (encoder) {
    case JpegEncoder::kLibJpeg:
      JXL_RETURN_IF_ERROR(
          EncodeWithLibJpeg(ib, quality, chroma_subsampling, bytes, target));
      break;
    case JpegEncoder::kSJpeg:
      if (target != DecodeTarget::kPixels)
        return JXL_FAILURE("Not implemented: SJpeg encode from DCT");
      JXL_RETURN_IF_ERROR(
          EncodeWithSJpeg(ib, quality, chroma_subsampling, bytes));
      break;
    default:
      return JXL_FAILURE("tried to use an unknown JPEG encoder");
  }

  io->enc_size = bytes->size();
  return true;
}

}  // namespace jxl