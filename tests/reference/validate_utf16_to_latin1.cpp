#include "validate_utf16.h"

#ifndef SIMDUTF_IS_BIG_ENDIAN
  #error "SIMDUTF_IS_BIG_ENDIAN should be defined."
#endif

namespace simdutf {
namespace tests {
namespace reference {

simdutf_warn_unused bool
validate_utf16_to_latin1(simdutf::endianness utf16_endianness,
                         const char16_t *buf, size_t len) noexcept {
  const char16_t *curr = buf;
  const char16_t *end = buf + len;

  while (curr != end) {
    uint16_t W1;
    if (!match_system(utf16_endianness)) {
      W1 = (uint16_t(*curr) << 8) | (uint16_t(*curr) >> 8);
    } else {
      W1 = *curr;
    }

    curr += 1;
    if (0xff < W1) {
      return false;
    }
  }
  return true;
}

} // namespace reference
} // namespace tests
} // namespace simdutf
