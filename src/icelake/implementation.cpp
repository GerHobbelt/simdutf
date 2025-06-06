#include <tuple>
#include <utility>
#include "simdutf/icelake/intrinsics.h"

#include "simdutf/icelake/begin.h"
namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {
namespace {
#ifndef SIMDUTF_ICELAKE_H
  #error "icelake.h must be included"
#endif
using namespace simd;

#include "icelake/icelake_macros.inl.cpp"
#include "icelake/icelake_common.inl.cpp"
#if SIMDUTF_FEATURE_UTF8
  #include "icelake/icelake_utf8_common.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING
  #include "icelake/icelake_utf8_validation.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8 &&                                                    \
    (SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_LATIN1)
  #include "icelake/icelake_from_valid_utf8.inl.cpp"
  #include "icelake/icelake_from_utf8.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8 && (SIMDUTF_FEATURE_UTF16 ||
       // SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_LATIN1)

#if SIMDUTF_FEATURE_UTF16
  #include "icelake/icelake_utf16fix.cpp"
#endif // SIMDUTF_FEATURE_UTF16
#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
  #include "icelake/icelake_convert_utf8_to_latin1.inl.cpp"
  #include "icelake/icelake_convert_valid_utf8_to_latin1.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16
  #include "icelake/icelake_convert_utf16_to_latin1.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
  #include "icelake/icelake_convert_utf16_to_utf8.inl.cpp"
  #include "icelake/icelake_convert_utf8_to_utf16.inl.cpp"
  #include "icelake/icelake_utf8_length_from_utf16.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  #include "icelake/icelake_convert_utf16_to_utf32.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF32
  #include "icelake/icelake_convert_utf32_to_latin1.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  #include "icelake/icelake_convert_utf32_to_utf8.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
  #include "icelake/icelake_convert_utf32_to_utf16.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_ASCII
  #include "icelake/icelake_ascii_validation.inl.cpp"
#endif // SIMDUTF_FEATURE_ASCII
#if SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING
  #include "icelake/icelake_utf32_validation.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING
#if SIMDUTF_FEATURE_UTF8
  #include "icelake/icelake_convert_latin1_to_utf8.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF8
#if SIMDUTF_FEATURE_UTF16
  #include "icelake/icelake_convert_latin1_to_utf16.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF16
#if SIMDUTF_FEATURE_UTF32
  #include "icelake/icelake_convert_latin1_to_utf32.inl.cpp"
#endif // SIMDUTF_FEATURE_UTF32
#if SIMDUTF_FEATURE_BASE64
  #include "icelake/icelake_base64.inl.cpp"
#endif // SIMDUTF_FEATURE_BASE64

#include <cstdint>

} // namespace
} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
  #include "generic/utf32.h"
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

namespace simdutf {
namespace SIMDUTF_IMPLEMENTATION {

#if SIMDUTF_FEATURE_DETECT_ENCODING
simdutf_warn_unused int
implementation::detect_encodings(const char *input,
                                 size_t length) const noexcept {
  // If there is a BOM, then we trust it.
  auto bom_encoding = simdutf::BOM::check_bom(input, length);
  if (bom_encoding != encoding_type::unspecified) {
    return bom_encoding;
  }

  int out = 0;
  uint32_t utf16_err = (length % 2);
  uint32_t utf32_err = (length % 4);
  uint32_t ends_with_high = 0;
  avx512_utf8_checker checker{};
  const __m512i offset = _mm512_set1_epi32((uint32_t)0xffff2000);
  __m512i currentmax = _mm512_setzero_si512();
  __m512i currentoffsetmax = _mm512_setzero_si512();
  const char *ptr = input;
  const char *end = ptr + length;
  for (; end - ptr >= 64; ptr += 64) {
    // utf8 checks
    const __m512i data = _mm512_loadu_si512((const __m512i *)ptr);
    checker.check_next_input(data);

    // utf16le_checks
    __m512i diff = _mm512_sub_epi16(data, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    __mmask32 highsurrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
    __mmask32 lowsurrogates = surrogates ^ highsurrogates;
    utf16_err |= (((highsurrogates << 1) | ends_with_high) != lowsurrogates);
    ends_with_high = ((highsurrogates & 0x80000000) != 0);

    // utf32le checks
    currentoffsetmax =
        _mm512_max_epu32(_mm512_add_epi32(data, offset), currentoffsetmax);
    currentmax = _mm512_max_epu32(data, currentmax);
  }

  // last block with 0 <= len < 64
  __mmask64 read_mask = (__mmask64(1) << (end - ptr)) - 1;
  const __m512i data = _mm512_maskz_loadu_epi8(read_mask, (const __m512i *)ptr);
  checker.check_next_input(data);

  __m512i diff = _mm512_sub_epi16(data, _mm512_set1_epi16(uint16_t(0xD800)));
  __mmask32 surrogates =
      _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
  __mmask32 highsurrogates =
      _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
  __mmask32 lowsurrogates = surrogates ^ highsurrogates;
  utf16_err |= (((highsurrogates << 1) | ends_with_high) != lowsurrogates);

  currentoffsetmax =
      _mm512_max_epu32(_mm512_add_epi32(data, offset), currentoffsetmax);
  currentmax = _mm512_max_epu32(data, currentmax);

  const __m512i standardmax = _mm512_set1_epi32((uint32_t)0x10ffff);
  const __m512i standardoffsetmax = _mm512_set1_epi32((uint32_t)0xfffff7ff);
  __m512i is_zero =
      _mm512_xor_si512(_mm512_max_epu32(currentmax, standardmax), standardmax);
  utf32_err |= (_mm512_test_epi8_mask(is_zero, is_zero) != 0);
  is_zero = _mm512_xor_si512(
      _mm512_max_epu32(currentoffsetmax, standardoffsetmax), standardoffsetmax);
  utf32_err |= (_mm512_test_epi8_mask(is_zero, is_zero) != 0);
  checker.check_eof();
  bool is_valid_utf8 = !checker.errors();
  if (is_valid_utf8) {
    out |= encoding_type::UTF8;
  }
  if (utf16_err == 0) {
    out |= encoding_type::UTF16_LE;
  }
  if (utf32_err == 0) {
    out |= encoding_type::UTF32_LE;
  }
  return out;
}
#endif // SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING
simdutf_warn_unused bool
implementation::validate_utf8(const char *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return true;
  }
  avx512_utf8_checker checker{};
  const char *ptr = buf;
  const char *end = ptr + len;
  for (; end - ptr >= 64; ptr += 64) {
    const __m512i utf8 = _mm512_loadu_si512((const __m512i *)ptr);
    checker.check_next_input(utf8);
  }
  if (end != ptr) {
    const __m512i utf8 = _mm512_maskz_loadu_epi8(
        ~UINT64_C(0) >> (64 - (end - ptr)), (const __m512i *)ptr);
    checker.check_next_input(utf8);
  }
  checker.check_eof();
  return !checker.errors();
}
#endif // SIMDUTF_FEATURE_UTF8 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF8
simdutf_warn_unused result implementation::validate_utf8_with_errors(
    const char *buf, size_t len) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return result(error_code::SUCCESS, len);
  }
  avx512_utf8_checker checker{};
  const char *ptr = buf;
  const char *end = ptr + len;
  size_t count{0};
  for (; end - ptr >= 64; ptr += 64) {
    const __m512i utf8 = _mm512_loadu_si512((const __m512i *)ptr);
    checker.check_next_input(utf8);
    if (checker.errors()) {
      if (count != 0) {
        count--;
      } // Sometimes the error is only detected in the next chunk
      result res = scalar::utf8::rewind_and_validate_with_errors(
          reinterpret_cast<const char *>(buf),
          reinterpret_cast<const char *>(buf + count), len - count);
      res.count += count;
      return res;
    }
    count += 64;
  }
  if (end != ptr) {
    const __m512i utf8 = _mm512_maskz_loadu_epi8(
        ~UINT64_C(0) >> (64 - (end - ptr)), (const __m512i *)ptr);
    checker.check_next_input(utf8);
  }
  checker.check_eof();
  if (checker.errors()) {
    if (count != 0) {
      count--;
    } // Sometimes the error is only detected in the next chunk
    result res = scalar::utf8::rewind_and_validate_with_errors(
        reinterpret_cast<const char *>(buf),
        reinterpret_cast<const char *>(buf + count), len - count);
    res.count += count;
    return res;
  }
  return result(error_code::SUCCESS, len);
}
#endif // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_ASCII
simdutf_warn_unused bool
implementation::validate_ascii(const char *buf, size_t len) const noexcept {
  return icelake::validate_ascii(buf, len);
}

simdutf_warn_unused result implementation::validate_ascii_with_errors(
    const char *buf, size_t len) const noexcept {
  const char *buf_orig = buf;
  const char *end = buf + len;
  const __m512i ascii = _mm512_set1_epi8((uint8_t)0x80);
  for (; end - buf >= 64; buf += 64) {
    const __m512i input = _mm512_loadu_si512((const __m512i *)buf);
    __mmask64 notascii = _mm512_cmp_epu8_mask(input, ascii, _MM_CMPINT_NLT);
    if (notascii) {
      return result(error_code::TOO_LARGE,
                    buf - buf_orig + _tzcnt_u64(notascii));
    }
  }
  if (end != buf) {
    const __m512i input = _mm512_maskz_loadu_epi8(
        ~UINT64_C(0) >> (64 - (end - buf)), (const __m512i *)buf);
    __mmask64 notascii = _mm512_cmp_epu8_mask(input, ascii, _MM_CMPINT_NLT);
    if (notascii) {
      return result(error_code::TOO_LARGE,
                    buf - buf_orig + _tzcnt_u64(notascii));
    }
  }
  return result(error_code::SUCCESS, len);
}
#endif // SIMDUTF_FEATURE_ASCII

#if SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING
simdutf_warn_unused bool
implementation::validate_utf16le(const char16_t *buf,
                                 size_t len) const noexcept {
  const char16_t *end = buf + len;

  for (; end - buf >= 32;) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        return false;
      }
      bool ends_with_high = ((highsurrogates & 0x80000000) != 0);
      if (ends_with_high) {
        buf += 31; // advance only by 31 code units so that we start with the
                   // high surrogate on the next round.
      } else {
        buf += 32;
      }
    } else {
      buf += 32;
    }
  }
  if (buf < end) {
    __m512i in =
        _mm512_maskz_loadu_epi16((1U << (end - buf)) - 1, (__m512i *)buf);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        return false;
      }
    }
  }
  return true;
}
#endif // SIMDUTF_FEATURE_UTF16 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF16
simdutf_warn_unused bool
implementation::validate_utf16be(const char16_t *buf,
                                 size_t len) const noexcept {
  const char16_t *end = buf + len;

  for (; end - buf >= 32;) {
    __m512i in = _mm512_slli_epi32(_mm512_loadu_si512((__m512i *)buf), 8);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        return false;
      }
      bool ends_with_high = ((highsurrogates & 0x80000000) != 0);
      if (ends_with_high) {
        buf += 31; // advance only by 31 code units so that we start with the
                   // high surrogate on the next round.
      } else {
        buf += 32;
      }
    } else {
      buf += 32;
    }
  }
  if (buf < end) {
    __m512i in = _mm512_slli_epi16(
        _mm512_maskz_loadu_epi16((1U << (end - buf)) - 1, (__m512i *)buf), 8);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        return false;
      }
    }
  }
  return true;
}

simdutf_warn_unused result implementation::validate_utf16le_with_errors(
    const char16_t *buf, size_t len) const noexcept {
  const char16_t *start_buf = buf;
  const char16_t *end = buf + len;
  for (; end - buf >= 32;) {
    __m512i in = _mm512_loadu_si512((__m512i *)buf);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        uint32_t extra_low = _tzcnt_u32(lowsurrogates & ~(highsurrogates << 1));
        uint32_t extra_high =
            _tzcnt_u32(highsurrogates & ~(lowsurrogates >> 1));
        return result(error_code::SURROGATE,
                      (buf - start_buf) +
                          (extra_low < extra_high ? extra_low : extra_high));
      }
      bool ends_with_high = ((highsurrogates & 0x80000000) != 0);
      if (ends_with_high) {
        buf += 31; // advance only by 31 code units so that we start with the
                   // high surrogate on the next round.
      } else {
        buf += 32;
      }
    } else {
      buf += 32;
    }
  }
  if (buf < end) {
    __m512i in =
        _mm512_maskz_loadu_epi16((1U << (end - buf)) - 1, (__m512i *)buf);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        uint32_t extra_low = _tzcnt_u32(lowsurrogates & ~(highsurrogates << 1));
        uint32_t extra_high =
            _tzcnt_u32(highsurrogates & ~(lowsurrogates >> 1));
        return result(error_code::SURROGATE,
                      (buf - start_buf) +
                          (extra_low < extra_high ? extra_low : extra_high));
      }
    }
  }
  return result(error_code::SUCCESS, len);
}

simdutf_warn_unused result implementation::validate_utf16be_with_errors(
    const char16_t *buf, size_t len) const noexcept {
  const char16_t *start_buf = buf;
  const char16_t *end = buf + len;

  for (; end - buf >= 32;) {
    __m512i in = _mm512_slli_epi16(_mm512_loadu_si512((__m512i *)buf), 8);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        uint32_t extra_low = _tzcnt_u32(lowsurrogates & ~(highsurrogates << 1));
        uint32_t extra_high =
            _tzcnt_u32(highsurrogates & ~(lowsurrogates >> 1));
        return result(error_code::SURROGATE,
                      (buf - start_buf) +
                          (extra_low < extra_high ? extra_low : extra_high));
      }
      bool ends_with_high = ((highsurrogates & 0x80000000) != 0);
      if (ends_with_high) {
        buf += 31; // advance only by 31 code units so that we start with the
                   // high surrogate on the next round.
      } else {
        buf += 32;
      }
    } else {
      buf += 32;
    }
  }
  if (buf < end) {
    __m512i in = _mm512_slli_epi16(
        _mm512_maskz_loadu_epi16((1U << (end - buf)) - 1, (__m512i *)buf), 8);
    __m512i diff = _mm512_sub_epi16(in, _mm512_set1_epi16(uint16_t(0xD800)));
    __mmask32 surrogates =
        _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0800)));
    if (surrogates) {
      __mmask32 highsurrogates =
          _mm512_cmplt_epu16_mask(diff, _mm512_set1_epi16(uint16_t(0x0400)));
      __mmask32 lowsurrogates = surrogates ^ highsurrogates;
      // high must be followed by low
      if ((highsurrogates << 1) != lowsurrogates) {
        uint32_t extra_low = _tzcnt_u32(lowsurrogates & ~(highsurrogates << 1));
        uint32_t extra_high =
            _tzcnt_u32(highsurrogates & ~(lowsurrogates >> 1));
        return result(error_code::SURROGATE,
                      (buf - start_buf) +
                          (extra_low < extra_high ? extra_low : extra_high));
      }
    }
  }
  return result(error_code::SUCCESS, len);
}

void implementation::to_well_formed_utf16le(const char16_t *input, size_t len,
                                            char16_t *output) const noexcept {
  return utf16fix_avx512<endianness::LITTLE>(input, len, output);
}

void implementation::to_well_formed_utf16be(const char16_t *input, size_t len,
                                            char16_t *output) const noexcept {
  return utf16fix_avx512<endianness::BIG>(input, len, output);
}
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING
simdutf_warn_unused bool
implementation::validate_utf32(const char32_t *buf, size_t len) const noexcept {
  return icelake::validate_utf32(buf, len);
}
#endif // SIMDUTF_FEATURE_UTF32 || SIMDUTF_FEATURE_DETECT_ENCODING

#if SIMDUTF_FEATURE_UTF32
simdutf_warn_unused result implementation::validate_utf32_with_errors(
    const char32_t *buf, size_t len) const noexcept {
  const char32_t *buf_orig = buf;
  if (len >= 16) {
    const char32_t *end = buf + len - 16;
    while (buf <= end) {
      __m512i utf32 = _mm512_loadu_si512((const __m512i *)buf);
      __mmask16 outside_range = _mm512_cmp_epu32_mask(
          utf32, _mm512_set1_epi32(0x10ffff), _MM_CMPINT_GT);

      __m512i utf32_off =
          _mm512_add_epi32(utf32, _mm512_set1_epi32(0xffff2000));

      __mmask16 surrogate_range = _mm512_cmp_epu32_mask(
          utf32_off, _mm512_set1_epi32(0xfffff7ff), _MM_CMPINT_GT);
      if ((outside_range | surrogate_range)) {
        auto outside_idx = _tzcnt_u32(outside_range);
        auto surrogate_idx = _tzcnt_u32(surrogate_range);

        if (outside_idx < surrogate_idx) {
          return result(error_code::TOO_LARGE, buf - buf_orig + outside_idx);
        }

        return result(error_code::SURROGATE, buf - buf_orig + surrogate_idx);
      }

      buf += 16;
    }
  }
  if (len > 0) {
    __m512i utf32 = _mm512_maskz_loadu_epi32(
        __mmask16((1U << (buf_orig + len - buf)) - 1), (const __m512i *)buf);
    __mmask16 outside_range = _mm512_cmp_epu32_mask(
        utf32, _mm512_set1_epi32(0x10ffff), _MM_CMPINT_GT);
    __m512i utf32_off = _mm512_add_epi32(utf32, _mm512_set1_epi32(0xffff2000));

    __mmask16 surrogate_range = _mm512_cmp_epu32_mask(
        utf32_off, _mm512_set1_epi32(0xfffff7ff), _MM_CMPINT_GT);
    if ((outside_range | surrogate_range)) {
      auto outside_idx = _tzcnt_u32(outside_range);
      auto surrogate_idx = _tzcnt_u32(surrogate_range);

      if (outside_idx < surrogate_idx) {
        return result(error_code::TOO_LARGE, buf - buf_orig + outside_idx);
      }

      return result(error_code::SURROGATE, buf - buf_orig + surrogate_idx);
    }
  }

  return result(error_code::SUCCESS, len);
}
#endif // SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_latin1_to_utf8(
    const char *buf, size_t len, char *utf8_output) const noexcept {
  return icelake::latin1_to_utf8_avx512_start(buf, len, utf8_output);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_latin1_to_utf16le(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  return icelake_convert_latin1_to_utf16<endianness::LITTLE>(buf, len,
                                                             utf16_output);
}

simdutf_warn_unused size_t implementation::convert_latin1_to_utf16be(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  return icelake_convert_latin1_to_utf16<endianness::BIG>(buf, len,
                                                          utf16_output);
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_latin1_to_utf32(
    const char *buf, size_t len, char32_t *utf32_output) const noexcept {
  avx512_convert_latin1_to_utf32(buf, len, utf32_output);
  return len;
}
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_utf8_to_latin1(
    const char *buf, size_t len, char *latin1_output) const noexcept {
  return icelake::utf8_to_latin1_avx512(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_latin1_with_errors(
    const char *buf, size_t len, char *latin1_output) const noexcept {
  // First, try to convert as much as possible using the SIMD implementation.
  const char *obuf = buf;
  char *olatin1_output = latin1_output;
  size_t written = icelake::utf8_to_latin1_avx512(obuf, len, olatin1_output);

  // If we have completely converted the string
  if (obuf == buf + len) {
    return {simdutf::SUCCESS, written};
  }
  size_t pos = obuf - buf;
  result res = scalar::utf8_to_latin1::rewind_and_convert_with_errors(
      pos, buf + pos, len - pos, latin1_output);
  res.count += pos;
  return res;
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_latin1(
    const char *buf, size_t len, char *latin1_output) const noexcept {
  return icelake::valid_utf8_to_latin1_avx512(buf, len, latin1_output);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
simdutf_warn_unused size_t implementation::convert_utf8_to_utf16le(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  utf8_to_utf16_result ret =
      fast_avx512_convert_utf8_to_utf16<endianness::LITTLE>(buf, len,
                                                            utf16_output);
  if (ret.second == nullptr) {
    return 0;
  }
  return ret.second - utf16_output;
}

simdutf_warn_unused size_t implementation::convert_utf8_to_utf16be(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  utf8_to_utf16_result ret = fast_avx512_convert_utf8_to_utf16<endianness::BIG>(
      buf, len, utf16_output);
  if (ret.second == nullptr) {
    return 0;
  }
  return ret.second - utf16_output;
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16le_with_errors(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  return fast_avx512_convert_utf8_to_utf16_with_errors<endianness::LITTLE>(
      buf, len, utf16_output);
}

simdutf_warn_unused result implementation::convert_utf8_to_utf16be_with_errors(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  return fast_avx512_convert_utf8_to_utf16_with_errors<endianness::BIG>(
      buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16le(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  utf8_to_utf16_result ret =
      icelake::valid_utf8_to_fixed_length<endianness::LITTLE, char16_t>(
          buf, len, utf16_output);
  size_t saved_bytes = ret.second - utf16_output;
  const char *end = buf + len;
  if (ret.first == end) {
    return saved_bytes;
  }

  // Note: AVX512 procedure looks up 4 bytes forward, and
  //       correctly converts multi-byte chars even if their
  //       continuation bytes lie outsiede 16-byte window.
  //       It meas, we have to skip continuation bytes from
  //       the beginning ret.first, as they were already consumed.
  while (ret.first != end && ((uint8_t(*ret.first) & 0xc0) == 0x80)) {
    ret.first += 1;
  }

  if (ret.first != end) {
    const size_t scalar_saved_bytes =
        scalar::utf8_to_utf16::convert_valid<endianness::LITTLE>(
            ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }

  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf16be(
    const char *buf, size_t len, char16_t *utf16_output) const noexcept {
  utf8_to_utf16_result ret =
      icelake::valid_utf8_to_fixed_length<endianness::BIG, char16_t>(
          buf, len, utf16_output);
  size_t saved_bytes = ret.second - utf16_output;
  const char *end = buf + len;
  if (ret.first == end) {
    return saved_bytes;
  }

  // Note: AVX512 procedure looks up 4 bytes forward, and
  //       correctly converts multi-byte chars even if their
  //       continuation bytes lie outsiede 16-byte window.
  //       It meas, we have to skip continuation bytes from
  //       the beginning ret.first, as they were already consumed.
  while (ret.first != end && ((uint8_t(*ret.first) & 0xc0) == 0x80)) {
    ret.first += 1;
  }

  if (ret.first != end) {
    const size_t scalar_saved_bytes =
        scalar::utf8_to_utf16::convert_valid<endianness::BIG>(
            ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }

  return saved_bytes;
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::convert_utf8_to_utf32(
    const char *buf, size_t len, char32_t *utf32_out) const noexcept {
  uint32_t *utf32_output = reinterpret_cast<uint32_t *>(utf32_out);
  utf8_to_utf32_result ret =
      icelake::validating_utf8_to_fixed_length<endianness::LITTLE, uint32_t>(
          buf, len, utf32_output);
  if (ret.second == nullptr)
    return 0;

  size_t saved_bytes = ret.second - utf32_output;
  const char *end = buf + len;
  if (ret.first == end) {
    return saved_bytes;
  }

  // Note: the AVX512 procedure looks up 4 bytes forward, and
  //       correctly converts multi-byte chars even if their
  //       continuation bytes lie outside 16-byte window.
  //       It means, we have to skip continuation bytes from
  //       the beginning ret.first, as they were already consumed.
  while (ret.first != end && ((uint8_t(*ret.first) & 0xc0) == 0x80)) {
    ret.first += 1;
  }
  if (ret.first != end) {
    const size_t scalar_saved_bytes = scalar::utf8_to_utf32::convert(
        ret.first, len - (ret.first - buf), utf32_out + saved_bytes);
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }

  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf8_to_utf32_with_errors(
    const char *buf, size_t len, char32_t *utf32) const noexcept {
  if (simdutf_unlikely(len == 0)) {
    return {error_code::SUCCESS, 0};
  }
  uint32_t *utf32_output = reinterpret_cast<uint32_t *>(utf32);
  auto ret = icelake::validating_utf8_to_fixed_length_with_constant_checks<
      endianness::LITTLE, uint32_t>(buf, len, utf32_output);

  if (!std::get<2>(ret)) {
    size_t pos = std::get<0>(ret) - buf;
    // We might have an error that occurs right before  pos.
    // This is only a concern if buf[pos] is not a continuation byte.
    if ((buf[pos] & 0xc0) != 0x80 && pos >= 64) {
      pos -= 1;
    } else if ((buf[pos] & 0xc0) == 0x80 && pos >= 64) {
      // We must check whether we are the fourth continuation byte
      bool c1 = (buf[pos - 1] & 0xc0) == 0x80;
      bool c2 = (buf[pos - 2] & 0xc0) == 0x80;
      bool c3 = (buf[pos - 3] & 0xc0) == 0x80;
      if (c1 && c2 && c3) {
        return {simdutf::TOO_LONG, pos};
      }
    }
    // todo: we reset the output to utf32 instead of using std::get<2.(ret) as
    // you'd expect. that is because
    // validating_utf8_to_fixed_length_with_constant_checks may have processed
    // data beyond the error.
    result res = scalar::utf8_to_utf32::rewind_and_convert_with_errors(
        pos, buf + pos, len - pos, utf32);
    res.count += pos;
    return res;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  const char *end = buf + len;
  if (std::get<0>(ret) == end) {
    return {simdutf::SUCCESS, saved_bytes};
  }

  // Note: the AVX512 procedure looks up 4 bytes forward, and
  //       correctly converts multi-byte chars even if their
  //       continuation bytes lie outside 16-byte window.
  //       It means, we have to skip continuation bytes from
  //       the beginning ret.first, as they were already consumed.
  while (std::get<0>(ret) != end and
         ((uint8_t(*std::get<0>(ret)) & 0xc0) == 0x80)) {
    std::get<0>(ret) += 1;
  }

  if (std::get<0>(ret) != end) {
    auto scalar_result = scalar::utf8_to_utf32::convert_with_errors(
        std::get<0>(ret), len - (std::get<0>(ret) - buf),
        reinterpret_cast<char32_t *>(utf32_output) + saved_bytes);
    if (scalar_result.error != simdutf::SUCCESS) {
      scalar_result.count += (std::get<0>(ret) - buf);
    } else {
      scalar_result.count += saved_bytes;
    }
    return scalar_result;
  }

  return {simdutf::SUCCESS, size_t(std::get<1>(ret) - utf32_output)};
}

simdutf_warn_unused size_t implementation::convert_valid_utf8_to_utf32(
    const char *buf, size_t len, char32_t *utf32_out) const noexcept {
  uint32_t *utf32_output = reinterpret_cast<uint32_t *>(utf32_out);
  utf8_to_utf32_result ret =
      icelake::valid_utf8_to_fixed_length<endianness::LITTLE, uint32_t>(
          buf, len, utf32_output);
  size_t saved_bytes = ret.second - utf32_output;
  const char *end = buf + len;
  if (ret.first == end) {
    return saved_bytes;
  }

  // Note: AVX512 procedure looks up 4 bytes forward, and
  //       correctly converts multi-byte chars even if their
  //       continuation bytes lie outsiede 16-byte window.
  //       It meas, we have to skip continuation bytes from
  //       the beginning ret.first, as they were already consumed.
  while (ret.first != end && ((uint8_t(*ret.first) & 0xc0) == 0x80)) {
    ret.first += 1;
  }

  if (ret.first != end) {
    const size_t scalar_saved_bytes = scalar::utf8_to_utf32::convert_valid(
        ret.first, len - (ret.first - buf), utf32_out + saved_bytes);
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }

  return saved_bytes;
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_utf16le_to_latin1(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf16_to_latin1<endianness::LITTLE>(buf, len,
                                                             latin1_output);
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_latin1(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf16_to_latin1<endianness::BIG>(buf, len,
                                                          latin1_output);
}

simdutf_warn_unused result
implementation::convert_utf16le_to_latin1_with_errors(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf16_to_latin1_with_errors<endianness::LITTLE>(
             buf, len, latin1_output)
      .first;
}

simdutf_warn_unused result
implementation::convert_utf16be_to_latin1_with_errors(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf16_to_latin1_with_errors<endianness::BIG>(
             buf, len, latin1_output)
      .first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_latin1(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  // optimization opportunity: implement custom function
  return convert_utf16be_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_latin1(
    const char16_t *buf, size_t len, char *latin1_output) const noexcept {
  // optimization opportunity: implement custom function
  return convert_utf16le_to_latin1(buf, len, latin1_output);
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
simdutf_warn_unused size_t implementation::convert_utf16le_to_utf8(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  size_t outlen;
  size_t inlen = utf16_to_utf8_avx512i<endianness::LITTLE>(
      buf, len, (unsigned char *)utf8_output, &outlen);
  if (inlen != len) {
    return 0;
  }
  return outlen;
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf8(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  size_t outlen;
  size_t inlen = utf16_to_utf8_avx512i<endianness::BIG>(
      buf, len, (unsigned char *)utf8_output, &outlen);
  if (inlen != len) {
    return 0;
  }
  return outlen;
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf8_with_errors(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  size_t outlen;
  size_t inlen = utf16_to_utf8_avx512i<endianness::LITTLE>(
      buf, len, (unsigned char *)utf8_output, &outlen);
  if (inlen != len) {
    result res = scalar::utf16_to_utf8::convert_with_errors<endianness::LITTLE>(
        buf + inlen, len - inlen, utf8_output + outlen);
    res.count += inlen;
    return res;
  }
  return {simdutf::SUCCESS, outlen};
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf8_with_errors(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  size_t outlen;
  size_t inlen = utf16_to_utf8_avx512i<endianness::BIG>(
      buf, len, (unsigned char *)utf8_output, &outlen);
  if (inlen != len) {
    result res = scalar::utf16_to_utf8::convert_with_errors<endianness::BIG>(
        buf + inlen, len - inlen, utf8_output + outlen);
    res.count += inlen;
    return res;
  }
  return {simdutf::SUCCESS, outlen};
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf8(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  return convert_utf16le_to_utf8(buf, len, utf8_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf8(
    const char16_t *buf, size_t len, char *utf8_output) const noexcept {
  return convert_utf16be_to_utf8(buf, len, utf8_output);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::convert_utf32_to_latin1(
    const char32_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf32_to_latin1(buf, len, latin1_output);
}

simdutf_warn_unused result implementation::convert_utf32_to_latin1_with_errors(
    const char32_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf32_to_latin1_with_errors(buf, len, latin1_output)
      .first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_latin1(
    const char32_t *buf, size_t len, char *latin1_output) const noexcept {
  return icelake_convert_utf32_to_latin1(buf, len, latin1_output);
}
#endif // SIMDUTF_FEATURE_UTF32 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::convert_utf32_to_utf8(
    const char32_t *buf, size_t len, char *utf8_output) const noexcept {
  std::pair<const char32_t *, char *> ret =
      avx512_convert_utf32_to_utf8(buf, len, utf8_output);
  if (ret.first == nullptr) {
    return 0;
  }
  size_t saved_bytes = ret.second - utf8_output;
  if (ret.first != buf + len) {
    const size_t scalar_saved_bytes = scalar::utf32_to_utf8::convert(
        ret.first, len - (ret.first - buf), ret.second);
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf8_with_errors(
    const char32_t *buf, size_t len, char *utf8_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of
  // code units written even if finished
  std::pair<result, char *> ret =
      icelake::avx512_convert_utf32_to_utf8_with_errors(buf, len, utf8_output);
  if (ret.first.count != len) {
    result scalar_res = scalar::utf32_to_utf8::convert_with_errors(
        buf + ret.first.count, len - ret.first.count, ret.second);
    if (scalar_res.error) {
      scalar_res.count += ret.first.count;
      return scalar_res;
    } else {
      ret.second += scalar_res.count;
    }
  }
  ret.first.count =
      ret.second -
      utf8_output; // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf8(
    const char32_t *buf, size_t len, char *utf8_output) const noexcept {
  return convert_utf32_to_utf8(buf, len, utf8_output);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::convert_utf32_to_utf16le(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  std::pair<const char32_t *, char16_t *> ret =
      avx512_convert_utf32_to_utf16<endianness::LITTLE>(buf, len, utf16_output);
  if (ret.first == nullptr) {
    return 0;
  }
  size_t saved_bytes = ret.second - utf16_output;
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf32_to_utf16be(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  std::pair<const char32_t *, char16_t *> ret =
      avx512_convert_utf32_to_utf16<endianness::BIG>(buf, len, utf16_output);
  if (ret.first == nullptr) {
    return 0;
  }
  size_t saved_bytes = ret.second - utf16_output;
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16le_with_errors(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of
  // code units written even if finished
  std::pair<result, char16_t *> ret =
      avx512_convert_utf32_to_utf16_with_errors<endianness::LITTLE>(
          buf, len, utf16_output);
  if (ret.first.error) {
    return ret.first;
  }
  ret.first.count =
      ret.second -
      utf16_output; // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused result implementation::convert_utf32_to_utf16be_with_errors(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  // ret.first.count is always the position in the buffer, not the number of
  // code units written even if finished
  std::pair<result, char16_t *> ret =
      avx512_convert_utf32_to_utf16_with_errors<endianness::BIG>(buf, len,
                                                                 utf16_output);
  if (ret.first.error) {
    return ret.first;
  }
  ret.first.count =
      ret.second -
      utf16_output; // Set count to the number of 8-bit code units written
  return ret.first;
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16le(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  return convert_utf32_to_utf16le(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_valid_utf32_to_utf16be(
    const char32_t *buf, size_t len, char16_t *utf16_output) const noexcept {
  return convert_utf32_to_utf16be(buf, len, utf16_output);
}

simdutf_warn_unused size_t implementation::convert_utf16le_to_utf32(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::LITTLE>(buf, len,
                                                          utf32_output);
  if (!std::get<2>(ret)) {
    return 0;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    const size_t scalar_saved_bytes =
        scalar::utf16_to_utf32::convert<endianness::LITTLE>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_utf16be_to_utf32(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::BIG>(buf, len, utf32_output);
  if (!std::get<2>(ret)) {
    return 0;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    const size_t scalar_saved_bytes =
        scalar::utf16_to_utf32::convert<endianness::BIG>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused result implementation::convert_utf16le_to_utf32_with_errors(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::LITTLE>(buf, len,
                                                          utf32_output);
  if (!std::get<2>(ret)) {
    result scalar_res =
        scalar::utf16_to_utf32::convert_with_errors<endianness::LITTLE>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    scalar_res.count += (std::get<0>(ret) - buf);
    return scalar_res;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    result scalar_res =
        scalar::utf16_to_utf32::convert_with_errors<endianness::LITTLE>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_res.error) {
      scalar_res.count += (std::get<0>(ret) - buf);
      return scalar_res;
    } else {
      scalar_res.count += saved_bytes;
      return scalar_res;
    }
  }
  return simdutf::result(simdutf::SUCCESS, saved_bytes);
}

simdutf_warn_unused result implementation::convert_utf16be_to_utf32_with_errors(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::BIG>(buf, len, utf32_output);
  if (!std::get<2>(ret)) {
    result scalar_res =
        scalar::utf16_to_utf32::convert_with_errors<endianness::BIG>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    scalar_res.count += (std::get<0>(ret) - buf);
    return scalar_res;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    result scalar_res =
        scalar::utf16_to_utf32::convert_with_errors<endianness::BIG>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_res.error) {
      scalar_res.count += (std::get<0>(ret) - buf);
      return scalar_res;
    } else {
      scalar_res.count += saved_bytes;
      return scalar_res;
    }
  }
  return simdutf::result(simdutf::SUCCESS, saved_bytes);
}

simdutf_warn_unused size_t implementation::convert_valid_utf16le_to_utf32(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::LITTLE>(buf, len,
                                                          utf32_output);
  if (!std::get<2>(ret)) {
    return 0;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    const size_t scalar_saved_bytes =
        scalar::utf16_to_utf32::convert<endianness::LITTLE>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}

simdutf_warn_unused size_t implementation::convert_valid_utf16be_to_utf32(
    const char16_t *buf, size_t len, char32_t *utf32_output) const noexcept {
  std::tuple<const char16_t *, char32_t *, bool> ret =
      icelake::convert_utf16_to_utf32<endianness::BIG>(buf, len, utf32_output);
  if (!std::get<2>(ret)) {
    return 0;
  }
  size_t saved_bytes = std::get<1>(ret) - utf32_output;
  if (std::get<0>(ret) != buf + len) {
    const size_t scalar_saved_bytes =
        scalar::utf16_to_utf32::convert<endianness::BIG>(
            std::get<0>(ret), len - (std::get<0>(ret) - buf), std::get<1>(ret));
    if (scalar_saved_bytes == 0) {
      return 0;
    }
    saved_bytes += scalar_saved_bytes;
  }
  return saved_bytes;
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16
void implementation::change_endianness_utf16(const char16_t *input,
                                             size_t length,
                                             char16_t *output) const noexcept {
  size_t pos = 0;
  const __m512i byteflip = _mm512_setr_epi64(
      0x0607040502030001, 0x0e0f0c0d0a0b0809, 0x0607040502030001,
      0x0e0f0c0d0a0b0809, 0x0607040502030001, 0x0e0f0c0d0a0b0809,
      0x0607040502030001, 0x0e0f0c0d0a0b0809);
  while (pos + 32 <= length) {
    __m512i utf16 = _mm512_loadu_si512((const __m512i *)(input + pos));
    utf16 = _mm512_shuffle_epi8(utf16, byteflip);
    _mm512_storeu_si512(output + pos, utf16);
    pos += 32;
  }
  if (pos < length) {
    __mmask32 m((1U << (length - pos)) - 1);
    __m512i utf16 = _mm512_maskz_loadu_epi16(m, (const __m512i *)(input + pos));
    utf16 = _mm512_shuffle_epi8(utf16, byteflip);
    _mm512_mask_storeu_epi16(output + pos, m, utf16);
  }
}

simdutf_warn_unused size_t implementation::count_utf16le(
    const char16_t *input, size_t length) const noexcept {
  const char16_t *ptr = input;
  size_t count{0};

  if (length >= 32) {
    const char16_t *end = input + length - 32;

    const __m512i low = _mm512_set1_epi16((uint16_t)0xdc00);
    const __m512i high = _mm512_set1_epi16((uint16_t)0xdfff);

    while (ptr <= end) {
      __m512i utf16 = _mm512_loadu_si512((const __m512i *)ptr);
      ptr += 32;
      uint64_t not_high_surrogate =
          static_cast<uint64_t>(_mm512_cmpgt_epu16_mask(utf16, high) |
                                _mm512_cmplt_epu16_mask(utf16, low));
      count += count_ones(not_high_surrogate);
    }
  }

  return count + scalar::utf16::count_code_points<endianness::LITTLE>(
                     ptr, length - (ptr - input));
}

simdutf_warn_unused size_t implementation::count_utf16be(
    const char16_t *input, size_t length) const noexcept {
  const char16_t *ptr = input;
  size_t count{0};
  if (length >= 32) {

    const char16_t *end = input + length - 32;

    const __m512i low = _mm512_set1_epi16((uint16_t)0xdc00);
    const __m512i high = _mm512_set1_epi16((uint16_t)0xdfff);

    const __m512i byteflip = _mm512_setr_epi64(
        0x0607040502030001, 0x0e0f0c0d0a0b0809, 0x0607040502030001,
        0x0e0f0c0d0a0b0809, 0x0607040502030001, 0x0e0f0c0d0a0b0809,
        0x0607040502030001, 0x0e0f0c0d0a0b0809);
    while (ptr <= end) {
      __m512i utf16 =
          _mm512_shuffle_epi8(_mm512_loadu_si512((__m512i *)ptr), byteflip);
      ptr += 32;
      uint64_t not_high_surrogate =
          static_cast<uint64_t>(_mm512_cmpgt_epu16_mask(utf16, high) |
                                _mm512_cmplt_epu16_mask(utf16, low));
      count += count_ones(not_high_surrogate);
    }
  }

  return count + scalar::utf16::count_code_points<endianness::BIG>(
                     ptr, length - (ptr - input));
}
#endif // SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8
simdutf_warn_unused size_t
implementation::count_utf8(const char *input, size_t length) const noexcept {
  const uint8_t *str = reinterpret_cast<const uint8_t *>(input);
  size_t answer =
      length / sizeof(__m512i) *
      sizeof(__m512i); // Number of 512-bit chunks that fits into the length.
  size_t i = 0;
  __m512i unrolled_popcount{0};

  const __m512i continuation = _mm512_set1_epi8(char(0b10111111));

  while (i + sizeof(__m512i) <= length) {
    size_t iterations = (length - i) / sizeof(__m512i);

    size_t max_i = i + iterations * sizeof(__m512i) - sizeof(__m512i);
    for (; i + 8 * sizeof(__m512i) <= max_i; i += 8 * sizeof(__m512i)) {
      __m512i input1 = _mm512_loadu_si512((const __m512i *)(str + i));
      __m512i input2 =
          _mm512_loadu_si512((const __m512i *)(str + i + sizeof(__m512i)));
      __m512i input3 =
          _mm512_loadu_si512((const __m512i *)(str + i + 2 * sizeof(__m512i)));
      __m512i input4 =
          _mm512_loadu_si512((const __m512i *)(str + i + 3 * sizeof(__m512i)));
      __m512i input5 =
          _mm512_loadu_si512((const __m512i *)(str + i + 4 * sizeof(__m512i)));
      __m512i input6 =
          _mm512_loadu_si512((const __m512i *)(str + i + 5 * sizeof(__m512i)));
      __m512i input7 =
          _mm512_loadu_si512((const __m512i *)(str + i + 6 * sizeof(__m512i)));
      __m512i input8 =
          _mm512_loadu_si512((const __m512i *)(str + i + 7 * sizeof(__m512i)));

      __mmask64 mask1 = _mm512_cmple_epi8_mask(input1, continuation);
      __mmask64 mask2 = _mm512_cmple_epi8_mask(input2, continuation);
      __mmask64 mask3 = _mm512_cmple_epi8_mask(input3, continuation);
      __mmask64 mask4 = _mm512_cmple_epi8_mask(input4, continuation);
      __mmask64 mask5 = _mm512_cmple_epi8_mask(input5, continuation);
      __mmask64 mask6 = _mm512_cmple_epi8_mask(input6, continuation);
      __mmask64 mask7 = _mm512_cmple_epi8_mask(input7, continuation);
      __mmask64 mask8 = _mm512_cmple_epi8_mask(input8, continuation);

      __m512i mask_register = _mm512_set_epi64(mask8, mask7, mask6, mask5,
                                               mask4, mask3, mask2, mask1);

      unrolled_popcount = _mm512_add_epi64(unrolled_popcount,
                                           _mm512_popcnt_epi64(mask_register));
    }

    for (; i <= max_i; i += sizeof(__m512i)) {
      __m512i more_input = _mm512_loadu_si512((const __m512i *)(str + i));
      uint64_t continuation_bitmask = static_cast<uint64_t>(
          _mm512_cmple_epi8_mask(more_input, continuation));
      answer -= count_ones(continuation_bitmask);
    }
  }

  answer -= _mm512_reduce_add_epi64(unrolled_popcount);

  return answer + scalar::utf8::count_code_points(
                      reinterpret_cast<const char *>(str + i), length - i);
}
#endif // SIMDUTF_FEATURE_UTF8

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::latin1_length_from_utf8(
    const char *buf, size_t len) const noexcept {
  return count_utf8(buf, len);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
simdutf_warn_unused size_t implementation::utf8_length_from_utf16le(
    const char16_t *input, size_t length) const noexcept {
  return icelake_utf8_length_from_utf16<endianness::LITTLE>(input, length);
}

simdutf_warn_unused size_t implementation::utf8_length_from_utf16be(
    const char16_t *input, size_t length) const noexcept {
  return icelake_utf8_length_from_utf16<endianness::BIG>(input, length);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::utf32_length_from_utf16le(
    const char16_t *input, size_t length) const noexcept {
  return implementation::count_utf16le(input, length);
}

simdutf_warn_unused size_t implementation::utf32_length_from_utf16be(
    const char16_t *input, size_t length) const noexcept {
  return implementation::count_utf16be(input, length);
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1
simdutf_warn_unused size_t implementation::utf8_length_from_latin1(
    const char *input, size_t length) const noexcept {
  const uint8_t *str = reinterpret_cast<const uint8_t *>(input);
  size_t answer = length / sizeof(__m512i) * sizeof(__m512i);
  size_t i = 0;
  if (answer >= 2048) { // long strings optimization
    unsigned char v_0xFF = 0xff;
    __m512i eight_64bits = _mm512_setzero_si512();
    while (i + sizeof(__m512i) <= length) {
      __m512i runner = _mm512_setzero_si512();
      size_t iterations = (length - i) / sizeof(__m512i);
      if (iterations > 255) {
        iterations = 255;
      }
      size_t max_i = i + iterations * sizeof(__m512i) - sizeof(__m512i);
      for (; i + 4 * sizeof(__m512i) <= max_i; i += 4 * sizeof(__m512i)) {
        // Load four __m512i vectors
        __m512i input1 = _mm512_loadu_si512((const __m512i *)(str + i));
        __m512i input2 =
            _mm512_loadu_si512((const __m512i *)(str + i + sizeof(__m512i)));
        __m512i input3 = _mm512_loadu_si512(
            (const __m512i *)(str + i + 2 * sizeof(__m512i)));
        __m512i input4 = _mm512_loadu_si512(
            (const __m512i *)(str + i + 3 * sizeof(__m512i)));

        // Generate four masks
        __mmask64 mask1 =
            _mm512_cmpgt_epi8_mask(_mm512_setzero_si512(), input1);
        __mmask64 mask2 =
            _mm512_cmpgt_epi8_mask(_mm512_setzero_si512(), input2);
        __mmask64 mask3 =
            _mm512_cmpgt_epi8_mask(_mm512_setzero_si512(), input3);
        __mmask64 mask4 =
            _mm512_cmpgt_epi8_mask(_mm512_setzero_si512(), input4);
        // Apply the masks and subtract from the runner
        __m512i not_ascii1 =
            _mm512_mask_set1_epi8(_mm512_setzero_si512(), mask1, v_0xFF);
        __m512i not_ascii2 =
            _mm512_mask_set1_epi8(_mm512_setzero_si512(), mask2, v_0xFF);
        __m512i not_ascii3 =
            _mm512_mask_set1_epi8(_mm512_setzero_si512(), mask3, v_0xFF);
        __m512i not_ascii4 =
            _mm512_mask_set1_epi8(_mm512_setzero_si512(), mask4, v_0xFF);

        runner = _mm512_sub_epi8(runner, not_ascii1);
        runner = _mm512_sub_epi8(runner, not_ascii2);
        runner = _mm512_sub_epi8(runner, not_ascii3);
        runner = _mm512_sub_epi8(runner, not_ascii4);
      }

      for (; i <= max_i; i += sizeof(__m512i)) {
        __m512i more_input = _mm512_loadu_si512((const __m512i *)(str + i));

        __mmask64 mask =
            _mm512_cmpgt_epi8_mask(_mm512_setzero_si512(), more_input);
        __m512i not_ascii =
            _mm512_mask_set1_epi8(_mm512_setzero_si512(), mask, v_0xFF);
        runner = _mm512_sub_epi8(runner, not_ascii);
      }

      eight_64bits = _mm512_add_epi64(
          eight_64bits, _mm512_sad_epu8(runner, _mm512_setzero_si512()));
    }

    answer += _mm512_reduce_add_epi64(eight_64bits);
  } else if (answer > 0) {
    for (; i + sizeof(__m512i) <= length; i += sizeof(__m512i)) {
      __m512i latin = _mm512_loadu_si512((const __m512i *)(str + i));
      uint64_t non_ascii = _mm512_movepi8_mask(latin);
      answer += count_ones(non_ascii);
    }
  }
  return answer + scalar::latin1::utf8_length_from_latin1(
                      reinterpret_cast<const char *>(str + i), length - i);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_LATIN1

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16
simdutf_warn_unused size_t implementation::utf16_length_from_utf8(
    const char *input, size_t length) const noexcept {
  size_t pos = 0;

  // UTF-16 char length based on the four most significant bits of UTF-8 bytes
  const __m128i utf8_length_128 = _mm_setr_epi8(
      // ASCII chars
      /* 0000 */ 1,
      /* 0001 */ 1,
      /* 0010 */ 1,
      /* 0011 */ 1,
      /* 0100 */ 1,
      /* 0101 */ 1,
      /* 0110 */ 1,
      /* 0111 */ 1,

      // continutation bytes
      /* 1000 */ 0,
      /* 1001 */ 0,
      /* 1010 */ 0,
      /* 1011 */ 0,

      // leading bytes
      /* 1100 */ 1, // 2-byte UTF-8 char => 1 UTF-16 word
      /* 1101 */ 1, // 2-byte UTF-8 char => 1 UTF-16 word
      /* 1110 */ 1, // 3-byte UTF-8 char => 1 UTF-16 word
      /* 1111 */ 2  // 4-byte UTF-8 char => 2 UTF-16 words (surrogate pair)
  );

  const __m512i char_length = broadcast_128bit_lane(utf8_length_128);

  constexpr size_t max_iterations = 255 / 2;

  size_t iterations = 0;
  const auto zero = _mm512_setzero_si512();
  __m512i local = _mm512_setzero_si512();    // byte-wise counters
  __m512i counters = _mm512_setzero_si512(); // 64-bit counters
  for (; pos + 64 <= length; pos += 64) {
    __m512i utf8 = _mm512_loadu_si512((const __m512i *)(input + pos));
    const auto t0 = _mm512_srli_epi32(utf8, 4);
    const auto t1 = _mm512_and_si512(t0, _mm512_set1_epi8(0xf));
    const auto t2 = _mm512_shuffle_epi8(char_length, t1);
    local = _mm512_add_epi8(local, t2);

    iterations += 1;
    if (iterations == max_iterations) {
      counters = _mm512_add_epi64(counters, _mm512_sad_epu8(local, zero));
      local = zero;
      iterations = 0;
    }
  }

  size_t count = 0;

  if (pos > 0) {
    // don't waste time for short strings
    if (iterations > 0) {
      counters = _mm512_add_epi64(counters, _mm512_sad_epu8(local, zero));
    }

    const auto l0 = _mm512_extracti32x4_epi32(counters, 0);
    const auto l1 = _mm512_extracti32x4_epi32(counters, 1);
    const auto l2 = _mm512_extracti32x4_epi32(counters, 2);
    const auto l3 = _mm512_extracti32x4_epi32(counters, 3);

    const auto sum =
        _mm_add_epi64(_mm_add_epi64(l0, l1), _mm_add_epi64(l2, l3));

    count = uint64_t(_mm_extract_epi64(sum, 0)) +
            uint64_t(_mm_extract_epi64(sum, 1));
  }

  return count +
         scalar::utf8::utf16_length_from_utf8(input + pos, length - pos);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF16

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::utf8_length_from_utf32(
    const char32_t *input, size_t length) const noexcept {
  return utf32::utf8_length_from_utf32(input, length);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::utf16_length_from_utf32(
    const char32_t *input, size_t length) const noexcept {
  const char32_t *ptr = input;
  size_t count{0};

  if (length >= 16) {
    const char32_t *end = input + length - 16;

    const __m512i v_0000_ffff = _mm512_set1_epi32((uint32_t)0x0000ffff);

    while (ptr <= end) {
      __m512i utf32 = _mm512_loadu_si512((const __m512i *)ptr);
      ptr += 16;
      __mmask16 surrogates_bitmask =
          _mm512_cmpgt_epu32_mask(utf32, v_0000_ffff);

      count += 16 + count_ones(surrogates_bitmask);
    }
  }

  return count +
         scalar::utf32::utf16_length_from_utf32(ptr, length - (ptr - input));
}
#endif // SIMDUTF_FEATURE_UTF16 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32
simdutf_warn_unused size_t implementation::utf32_length_from_utf8(
    const char *input, size_t length) const noexcept {
  return implementation::count_utf8(input, length);
}
#endif // SIMDUTF_FEATURE_UTF8 && SIMDUTF_FEATURE_UTF32

#if SIMDUTF_FEATURE_BASE64
simdutf_warn_unused result implementation::base64_to_binary(
    const char *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  if (options & base64_default_or_url) {
    if (options == base64_options::base64_default_or_url_accept_garbage) {
      return compress_decode_base64<false, true, true>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, true>(
          output, input, length, options, last_chunk_options);
    }
  } else if (options & base64_url) {
    if (options == base64_options::base64_url_accept_garbage) {
      return compress_decode_base64<true, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<true, false, false>(
          output, input, length, options, last_chunk_options);
    }
  } else {
    if (options == base64_options::base64_default_accept_garbage) {
      return compress_decode_base64<false, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, false>(
          output, input, length, options, last_chunk_options);
    }
  }
}

simdutf_warn_unused full_result implementation::base64_to_binary_details(
    const char *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  if (options & base64_default_or_url) {
    if (options == base64_options::base64_default_or_url_accept_garbage) {
      return compress_decode_base64<false, true, true>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, true>(
          output, input, length, options, last_chunk_options);
    }
  } else if (options & base64_url) {
    if (options == base64_options::base64_url_accept_garbage) {
      return compress_decode_base64<true, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<true, false, false>(
          output, input, length, options, last_chunk_options);
    }
  } else {
    if (options == base64_options::base64_default_accept_garbage) {
      return compress_decode_base64<false, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, false>(
          output, input, length, options, last_chunk_options);
    }
  }
}

simdutf_warn_unused result implementation::base64_to_binary(
    const char16_t *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  if (options & base64_default_or_url) {
    if (options == base64_options::base64_default_or_url_accept_garbage) {
      return compress_decode_base64<false, true, true>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, true>(
          output, input, length, options, last_chunk_options);
    }
  } else if (options & base64_url) {
    if (options == base64_options::base64_url_accept_garbage) {
      return compress_decode_base64<true, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<true, false, false>(
          output, input, length, options, last_chunk_options);
    }
  } else {
    if (options == base64_options::base64_default_accept_garbage) {
      return compress_decode_base64<false, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, false>(
          output, input, length, options, last_chunk_options);
    }
  }
}

simdutf_warn_unused full_result implementation::base64_to_binary_details(
    const char16_t *input, size_t length, char *output, base64_options options,
    last_chunk_handling_options last_chunk_options) const noexcept {
  if (options & base64_default_or_url) {
    if (options == base64_options::base64_default_or_url_accept_garbage) {
      return compress_decode_base64<false, true, true>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, true>(
          output, input, length, options, last_chunk_options);
    }
  } else if (options & base64_url) {
    if (options == base64_options::base64_url_accept_garbage) {
      return compress_decode_base64<true, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<true, false, false>(
          output, input, length, options, last_chunk_options);
    }
  } else {
    if (options == base64_options::base64_default_accept_garbage) {
      return compress_decode_base64<false, true, false>(
          output, input, length, options, last_chunk_options);
    } else {
      return compress_decode_base64<false, false, false>(
          output, input, length, options, last_chunk_options);
    }
  }
}

size_t implementation::binary_to_base64(const char *input, size_t length,
                                        char *output,
                                        base64_options options) const noexcept {
  if (options & base64_url) {
    return encode_base64<true>(output, input, length, options);
  } else {
    return encode_base64<false>(output, input, length, options);
  }
}
#endif // SIMDUTF_FEATURE_BASE64

} // namespace SIMDUTF_IMPLEMENTATION
} // namespace simdutf

#include "simdutf/icelake/end.h"
