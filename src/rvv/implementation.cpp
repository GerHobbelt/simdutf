#include "simdutf/rvv/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_RVV_H
  #error "rvv.h must be included"
#endif

} // unnamed namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

//
// Implementation-specific overrides
//
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
#include "rvv/rvv_helpers.inl.cpp"

#include "rvv/rvv_length_from.inl.cpp"
#include "rvv/rvv_validate.inl.cpp"

#include "rvv/rvv_latin1_to.inl.cpp"
#include "rvv/rvv_utf16_to.inl.cpp"

#include "rvv/rvv_utf32_to.inl.cpp"
#include "rvv/rvv_utf8_to.inl.cpp"

#if SIMDUTF_FEATURE_DETECT_ENCODING
simdutf_warn_unused int
implementation::detect_encodings(const char *input,
                                 size_t length) const noexcept {
  // If there is a BOM, then we trust it.
  auto bom_encoding = simdutf::BOM::check_bom(input, length);
  if (bom_encoding != encoding_type::unspecified)
    return bom_encoding;
  // todo: reimplement as a one-pass algorithm.
  int out = 0;
  if (validate_utf8(input, length))
    out |= encoding_type::UTF8;
  if (length % 2 == 0) {
    if (validate_utf16le(reinterpret_cast<const char16_t *>(input), length / 2))
      out |= encoding_type::UTF16_LE;
  }
  if (length % 4 == 0) {
    if (validate_utf32(reinterpret_cast<const char32_t *>(input), length / 4))
      out |= encoding_type::UTF32_LE;
  }

  return out;
}
#endif // SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF16

void implementation::to_well_formed_utf16le(const char16_t *input, size_t len,
                                            char16_t *output) const noexcept {
  return scalar::utf16::to_well_formed_utf16<endianness::LITTLE>(input, len,
                                                                 output);
}

void implementation::to_well_formed_utf16be(const char16_t *input, size_t len,
                                            char16_t *output) const noexcept {
  return scalar::utf16::to_well_formed_utf16<endianness::BIG>(input, len,
                                                              output);
}

template <simdutf_ByteFlip bflip>
simdutf_really_inline static void
rvv_change_endianness_utf16(const char16_t *src, size_t len, char16_t *dst) {
  for (size_t vl; len > 0; len -= vl, src += vl, dst += vl) {
    vl = __riscv_vsetvl_e16m8(len);
    vuint16m8_t v = __riscv_vle16_v_u16m8((uint16_t *)src, vl);
    __riscv_vse16_v_u16m8((uint16_t *)dst, simdutf_byteflip<bflip>(v, vl), vl);
  }
}

void implementation::change_endianness_utf16(const char16_t *src, size_t len,
                                             char16_t *dst) const noexcept {
  if (supports_zvbb())
    return rvv_change_endianness_utf16<simdutf_ByteFlip::ZVBB>(src, len, dst);
  else
    return rvv_change_endianness_utf16<simdutf_ByteFlip::V>(src, len, dst);
}
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_BASE64
simdutf_warn_unused result implementation::base64_to_binary(
    const char *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage);
  while (length > 0 &&
         scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation =
      length; // location of the first padding character if any
  size_t equalsigns = 0;
  if (length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while (length > 0 &&
           scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if (length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      if (last_chunk_options == last_chunk_handling_options::strict) {
        return {BASE64_INPUT_REMAINDER, 0};
      } else if (last_chunk_options ==
                 last_chunk_handling_options::stop_before_partial) {
        return {SUCCESS, 0};
      }
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
    return {SUCCESS, 0};
  }
  result r = scalar::base64::base64_tail_decode(
      output, input, length, equalsigns, options, last_chunk_options);
  if (last_chunk_options != stop_before_partial &&
      r.error == error_code::SUCCESS && equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.count % 3 == 0) || ((r.count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
  }
  return r;
}

simdutf_warn_unused full_result implementation::base64_to_binary_details(
    const char *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage);
  while (length > 0 &&
         scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation =
      length; // location of the first padding character if any
  size_t equalsigns = 0;
  if (length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while (length > 0 &&
           scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if (length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      if (last_chunk_options == last_chunk_handling_options::strict) {
        return {BASE64_INPUT_REMAINDER, 0, 0};
      } else if (last_chunk_options ==
                 last_chunk_handling_options::stop_before_partial) {
        return {SUCCESS, 0, 0};
      }
      return {INVALID_BASE64_CHARACTER, equallocation, 0};
    }
    return {SUCCESS, 0, 0};
  }
  full_result r = scalar::base64::base64_tail_decode(
      output, input, length, equalsigns, options, last_chunk_options);
  if (last_chunk_options != stop_before_partial &&
      r.error == error_code::SUCCESS && equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.output_count % 3 == 0) ||
        ((r.output_count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation, r.output_count};
    }
  }
  return r;
}

simdutf_warn_unused result implementation::base64_to_binary(
    const char16_t *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage);
  while (length > 0 &&
         scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation =
      length; // location of the first padding character if any
  auto equalsigns = 0;
  if (length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while (length > 0 &&
           scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if (length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      if (last_chunk_options == last_chunk_handling_options::strict) {
        return {BASE64_INPUT_REMAINDER, 0};
      } else if (last_chunk_options ==
                 last_chunk_handling_options::stop_before_partial) {
        return {SUCCESS, 0};
      }
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
    return {SUCCESS, 0};
  }
  result r = scalar::base64::base64_tail_decode(
      output, input, length, equalsigns, options, last_chunk_options);
  if (last_chunk_options != stop_before_partial &&
      r.error == error_code::SUCCESS && equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.count % 3 == 0) || ((r.count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation};
    }
  }
  return r;
}

simdutf_warn_unused full_result implementation::base64_to_binary_details(
    const char16_t *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  const bool ignore_garbage =
      (options == base64_options::base64_url_accept_garbage) ||
      (options == base64_options::base64_default_accept_garbage);
  while (length > 0 &&
         scalar::base64::is_ascii_white_space(input[length - 1])) {
    length--;
  }
  size_t equallocation =
      length; // location of the first padding character if any
  size_t equalsigns = 0;
  if (length > 0 && input[length - 1] == '=') {
    equallocation = length - 1;
    length -= 1;
    equalsigns++;
    while (length > 0 &&
           scalar::base64::is_ascii_white_space(input[length - 1])) {
      length--;
    }
    if (length > 0 && input[length - 1] == '=') {
      equallocation = length - 1;
      equalsigns++;
      length -= 1;
    }
  }
  if (length == 0) {
    if (!ignore_garbage && equalsigns > 0) {
      if (last_chunk_options == last_chunk_handling_options::strict) {
        return {BASE64_INPUT_REMAINDER, 0, 0};
      } else if (last_chunk_options ==
                 last_chunk_handling_options::stop_before_partial) {
        return {SUCCESS, 0, 0};
      }
      return {INVALID_BASE64_CHARACTER, equallocation, 0};
    }
    return {SUCCESS, 0, 0};
  }
  full_result r = scalar::base64::base64_tail_decode(
      output, input, length, equalsigns, options, last_chunk_options);
  if (last_chunk_options != stop_before_partial &&
      r.error == error_code::SUCCESS && equalsigns > 0 && !ignore_garbage) {
    // additional checks
    if ((r.output_count % 3 == 0) ||
        ((r.output_count % 3) + 1 + equalsigns != 4)) {
      return {INVALID_BASE64_CHARACTER, equallocation, r.output_count};
    }
  }
  return r;
}

size_t implementation::binary_to_base64(const char *input, size_t length,
                                        char *output,
                                        base64_options options) const noexcept {
  return scalar::base64::tail_encode_base64(output, input, length, options);
}
#endif // SIMDUTF_FEATURE_BASE64

} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/rvv/end.h"
