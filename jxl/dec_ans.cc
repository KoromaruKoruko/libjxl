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

#include "jxl/dec_ans.h"

#include <stdint.h>

#include <vector>

#include "c/common/constants.h"
#include "c/dec/bit_reader.h"
#include "jxl/ans_common.h"
#include "jxl/ans_params.h"
#include "jxl/base/bits.h"
#include "jxl/base/profiler.h"
#include "jxl/base/status.h"
#include "jxl/common.h"
#include "jxl/dec_context_map.h"
#include "jxl/fields.h"

namespace jxl {
namespace {

// Decodes a number in the range [0..255], by reading 1 - 11 bits.
inline int DecodeVarLenUint8(BitReader* input) {
  if (input->ReadFixedBits<1>()) {
    int nbits = static_cast<int>(input->ReadFixedBits<3>());
    if (nbits == 0) {
      return 1;
    } else {
      return static_cast<int>(input->ReadBits(nbits)) + (1 << nbits);
    }
  }
  return 0;
}

// Decodes a number in the range [0..65535], by reading 1 - 21 bits.
inline int DecodeVarLenUint16(BitReader* input) {
  if (input->ReadFixedBits<1>()) {
    int nbits = static_cast<int>(input->ReadFixedBits<4>());
    if (nbits == 0) {
      return 1;
    } else {
      return static_cast<int>(input->ReadBits(nbits)) + (1 << nbits);
    }
  }
  return 0;
}

Status ReadHistogram(int precision_bits, std::vector<int>* counts,
                     BitReader* input) {
  int simple_code = input->ReadBits(1);
  if (simple_code == 1) {
    int i;
    int symbols[2] = {0};
    int max_symbol = 0;
    const int num_symbols = input->ReadBits(1) + 1;
    for (i = 0; i < num_symbols; ++i) {
      symbols[i] = DecodeVarLenUint8(input);
      if (symbols[i] > max_symbol) max_symbol = symbols[i];
    }
    counts->resize(max_symbol + 1);
    if (num_symbols == 1) {
      (*counts)[symbols[0]] = 1 << precision_bits;
    } else {
      if (symbols[0] == symbols[1]) {  // corrupt data
        return false;
      }
      (*counts)[symbols[0]] = input->ReadBits(precision_bits);
      (*counts)[symbols[1]] = (1 << precision_bits) - (*counts)[symbols[0]];
    }
  } else {
    int is_flat = input->ReadBits(1);
    if (is_flat == 1) {
      int alphabet_size = DecodeVarLenUint8(input) + 1;
      if (alphabet_size == 0) {
        return JXL_FAILURE("Invalid alphabet size for flat histogram.");
      }
      *counts = CreateFlatHistogram(alphabet_size, 1 << precision_bits);
      return true;
    }

    uint32_t shift;
    {
      // TODO(veluca): speed up reading with table lookups.
      int upper_bound_log = FloorLog2Nonzero(ANS_LOG_TAB_SIZE + 1);
      int log = 0;
      for (; log < upper_bound_log; log++) {
        if (input->ReadFixedBits<1>() == 0) break;
      }
      shift = (input->ReadBits(log) | (1 << log)) - 1;
      if (shift > ANS_LOG_TAB_SIZE + 1) {
        return JXL_FAILURE("Invalid shift value");
      }
    }

    int length = DecodeVarLenUint8(input) + 3;
    counts->resize(length);
    int total_count = 0;

    static const uint8_t huff[128][2] = {
        {3, 10}, {7, 12}, {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {5, 0},  {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {6, 11}, {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {5, 0},  {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {7, 13}, {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {5, 0},  {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {6, 11}, {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
        {3, 10}, {5, 0},  {3, 7}, {4, 3}, {3, 6}, {3, 8}, {3, 9}, {4, 5},
        {3, 10}, {4, 4},  {3, 7}, {4, 1}, {3, 6}, {3, 8}, {3, 9}, {4, 2},
    };

    std::vector<int> logcounts(counts->size());
    int omit_log = -1;
    int omit_pos = -1;
    // This array remembers which symbols have an RLE length.
    std::vector<int> same(counts->size(), 0);
    for (size_t i = 0; i < logcounts.size(); ++i) {
      input->Refill();  // for PeekFixedBits + Advance
      int idx = input->PeekFixedBits<7>();
      input->Consume(huff[idx][0]);
      logcounts[i] = huff[idx][1];
      // The RLE symbol.
      if (logcounts[i] == ANS_LOG_TAB_SIZE + 1) {
        int rle_length = DecodeVarLenUint8(input);
        same[i] = rle_length + 5;
        i += rle_length + 3;
        continue;
      }
      if (logcounts[i] > omit_log) {
        omit_log = logcounts[i];
        omit_pos = i;
      }
    }
    // Invalid input, e.g. due to invalid usage of RLE.
    if (omit_pos < 0) return JXL_FAILURE("Invalid histogram.");
    if (static_cast<size_t>(omit_pos) + 1 < logcounts.size() &&
        logcounts[omit_pos + 1] == ANS_TAB_SIZE + 1) {
      return JXL_FAILURE("Invalid histogram.");
    }
    int prev = 0;
    int numsame = 0;
    for (size_t i = 0; i < logcounts.size(); ++i) {
      if (same[i]) {
        // RLE sequence, let this loop output the same count for the next
        // iterations.
        numsame = same[i] - 1;
        prev = i > 0 ? (*counts)[i - 1] : 0;
      }
      if (numsame > 0) {
        (*counts)[i] = prev;
        numsame--;
      } else {
        int code = logcounts[i];
        // omit_pos may not be negative at this point (checked before).
        if (i == static_cast<size_t>(omit_pos)) {
          continue;
        } else if (code == 0) {
          continue;
        } else if (code == 1) {
          (*counts)[i] = 1;
        } else {
          int bitcount = GetPopulationCountPrecision(code - 1, shift);
          (*counts)[i] = (1 << (code - 1)) +
                         (input->ReadBits(bitcount) << (code - 1 - bitcount));
        }
      }
      total_count += (*counts)[i];
    }
    (*counts)[omit_pos] = (1 << precision_bits) - total_count;
    if ((*counts)[omit_pos] <= 0) {
      // The histogram we've read sums to more than total_count (including at
      // least 1 for the omitted value).
      return JXL_FAILURE("Invalid histogram count.");
    }
  }
  return true;
}

}  // namespace

Status DecodeANSCodes(const size_t num_histograms,
                      const size_t max_alphabet_size, BitReader* in,
                      ANSCode* result) {
  if (result->use_prefix_code) {
    JXL_ASSERT(max_alphabet_size <= 1 << brunsli::kMaxHuffmanBits);
    result->huffman_data.resize(num_histograms);
    std::vector<uint16_t> alphabet_sizes(num_histograms);
    for (size_t c = 0; c < num_histograms; c++) {
      alphabet_sizes[c] = DecodeVarLenUint16(in) + 1;
      if (alphabet_sizes[c] > max_alphabet_size) {
        return JXL_FAILURE("Alphabet size is too long: %u", alphabet_sizes[c]);
      }
    }
    size_t pos = in->TotalBitsConsumed();
    size_t size = in->TotalBytes();
    const uint8_t* data = in->FirstByte();
    if (pos > size * kBitsPerByte) return JXL_FAILURE("Truncated bitstream");
    data += pos / kBitsPerByte;
    size -= pos / kBitsPerByte;
    pos %= kBitsPerByte;
    size_t orig_size = size;
    brunsli::BrunsliBitReader br;
    brunsli::BrunsliBitReaderInit(&br);
    brunsli::BrunsliBitReaderResume(&br, data, size);
    // Skip already consumed bits.
    (void)brunsli::BrunsliBitReaderRead(&br, pos);
    for (size_t c = 0; c < num_histograms; c++) {
      if (alphabet_sizes[c] > 1) {
        if (!result->huffman_data[c].ReadFromBitStream(alphabet_sizes[c],
                                                       &br)) {
          return JXL_FAILURE(
              "Invalid huffman tree number %zu, alphabet size %u", c,
              alphabet_sizes[c]);
        }
      } else {
        // 0-bit codes does not requre extension tables.
        result->huffman_data[c].table_.resize(1u << brunsli::kHuffmanTableBits);
      }
    }
    if (!brunsli::BrunsliBitReaderIsHealthy(&br)) {
      return JXL_FAILURE("Invalid huffman code bitstream.");
    }
    size_t num_unused_bits = br.num_bits_;
    size_t unused_bytes = brunsli::BrunsliBitReaderSuspend(&br);
    brunsli::BrunsliBitReaderFinish(&br);
    size_t consumed_bytes = orig_size - unused_bytes;
    in->SkipBits(consumed_bytes * kBitsPerByte - num_unused_bits - pos);
  } else {
    JXL_ASSERT(max_alphabet_size <= ANS_MAX_ALPHABET_SIZE);
    result->alias_tables =
        AllocateArray(num_histograms * (1 << result->log_alpha_size) *
                      sizeof(AliasTable::Entry));
    AliasTable::Entry* alias_tables =
        reinterpret_cast<AliasTable::Entry*>(result->alias_tables.get());
    for (size_t c = 0; c < num_histograms; ++c) {
      std::vector<int> counts;
      if (!ReadHistogram(ANS_LOG_TAB_SIZE, &counts, in)) {
        return JXL_FAILURE("Invalid histogram bitstream.");
      }
      if (counts.size() > max_alphabet_size) {
        return JXL_FAILURE("Alphabet size is too long: %zu", counts.size());
      }
      InitAliasTable(counts, ANS_TAB_SIZE, result->log_alpha_size,
                     alias_tables + c * (1 << result->log_alpha_size));
    }
  }
  return true;
}
Status DecodeUintConfig(size_t log_alpha_size, HybridUintConfig* uint_config,
                        BitReader* br) {
  br->Refill();
  size_t split_exponent = br->ReadBits(CeilLog2Nonzero(log_alpha_size + 1));
  size_t msb_in_token = 0, lsb_in_token = 0;
  if (split_exponent != log_alpha_size) {
    // otherwise, msb/lsb don't matter.
    size_t nbits = CeilLog2Nonzero(split_exponent + 1);
    msb_in_token = br->ReadBits(nbits);
    if (msb_in_token > split_exponent) {
      // This could be invalid here already and we need to check this before
      // we use its value to read more bits.
      return JXL_FAILURE("Invalid HybridUintConfig");
    }
    nbits = CeilLog2Nonzero(split_exponent - msb_in_token + 1);
    lsb_in_token = br->ReadBits(nbits);
  }
  if (lsb_in_token + msb_in_token > split_exponent) {
    return JXL_FAILURE("Invalid HybridUintConfig");
  }
  *uint_config = HybridUintConfig(split_exponent, msb_in_token, lsb_in_token);
  return true;
}

Status DecodeUintConfigs(size_t log_alpha_size,
                         std::vector<HybridUintConfig>* uint_config,
                         BitReader* br) {
  // TODO(veluca): RLE?
  for (size_t i = 0; i < uint_config->size(); i++) {
    JXL_RETURN_IF_ERROR(
        DecodeUintConfig(log_alpha_size, &(*uint_config)[i], br));
  }
  return true;
}

LZ77Params::LZ77Params() { Bundle::Init(this); }

Status DecodeHistograms(BitReader* br, size_t num_contexts, ANSCode* code,
                        std::vector<uint8_t>* context_map, bool disallow_lz77) {
  PROFILER_FUNC;
  JXL_RETURN_IF_ERROR(Bundle::Read(br, &code->lz77));
  if (code->lz77.enabled) {
    num_contexts++;
    JXL_RETURN_IF_ERROR(DecodeUintConfig(/*log_alpha_size=*/8,
                                         &code->lz77.length_uint_config, br));
  }
  if (code->lz77.enabled && disallow_lz77) {
    return JXL_FAILURE("Using LZ77 when explicitly disallowed");
  }
  size_t num_histograms = 1;
  context_map->resize(num_contexts);
  if (num_contexts > 1) {
    JXL_RETURN_IF_ERROR(DecodeContextMap(context_map, &num_histograms, br));
  }
  code->lz77.nonserialized_distance_context = context_map->back();
  code->use_prefix_code = br->ReadFixedBits<1>();
  if (code->use_prefix_code) {
    code->log_alpha_size = brunsli::kMaxHuffmanBits;
  } else {
    code->log_alpha_size = br->ReadFixedBits<2>() + 5;
  }
  code->uint_config.resize(num_histograms);
  JXL_RETURN_IF_ERROR(
      DecodeUintConfigs(code->log_alpha_size, &code->uint_config, br));
  const size_t max_alphabet_size = 1 << code->log_alpha_size;
  if (!DecodeANSCodes(num_histograms, max_alphabet_size, br, code)) {
    return JXL_FAILURE("Histo DecodeANSCodes");
  }
  return true;
}

}  // namespace jxl