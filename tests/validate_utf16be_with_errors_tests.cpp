#include "simdutf.h"

#include <array>
#include <vector>

#include <tests/helpers/random_utf16.h>
#include <tests/helpers/test.h>
#include <tests/helpers/utf16.h>

constexpr size_t trials = 1000;

TEST_LOOP(trials, validate_utf16be_returns_true_for_valid_input_single_words) {
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  const auto utf16{generator.generate_be(512, seed)};

  simdutf::result res = implementation.validate_utf16be_with_errors(
      reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, utf16.size());
}

TEST_LOOP(trials,
          validate_utf16be_returns_true_for_valid_input_surrogate_pairs_short) {
  simdutf::tests::helpers::random_utf16 generator{seed, 0, 1};
  const auto utf16{generator.generate_be(8)};

  const simdutf::result res = implementation.validate_utf16be_with_errors(
      reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, utf16.size());
}

TEST_LOOP(trials,
          validate_utf16be_returns_true_for_valid_input_surrogate_pairs) {
  simdutf::tests::helpers::random_utf16 generator{seed, 0, 1};
  const auto utf16{generator.generate_be(512)};

  const simdutf::result res = implementation.validate_utf16be_with_errors(
      reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, utf16.size());
}

// mixed = either 16-bit or 32-bit codewords
TEST(validate_utf16be_returns_true_for_valid_input_mixed) {
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 1};
  const auto utf16{generator.generate_be(512)};
  const simdutf::result res = implementation.validate_utf16be_with_errors(
      reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, utf16.size());
}

TEST(validate_utf16be_returns_true_for_empty_string) {
  const char16_t *buf = (char16_t *)"";

  simdutf::result res = implementation.validate_utf16be_with_errors(
      reinterpret_cast<const char16_t *>(buf), 0);
  ASSERT_EQUAL(res.error, simdutf::error_code::SUCCESS);
  ASSERT_EQUAL(res.count, 0);
}

TEST(provoke_integer_wraparound_in_icelake) {
  // this is to prove signed integer wraparound in the icelake implementation
  unsigned char cleaned_crash[] = {
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
      0xff, 0xff, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20};
  unsigned int cleaned_crash_len = 62;

  ASSERT_EQUAL(
      reinterpret_cast<std::uintptr_t>(cleaned_crash) % alignof(char16_t), 0);

  const auto size = cleaned_crash_len / sizeof(char16_t);

  auto r = simdutf::validate_utf16be_with_errors(
      (const char16_t *)cleaned_crash, size);
  ASSERT_EQUAL(r.error, simdutf::error_code::SUCCESS);
}

// The first word must not be in range [0xDC00 .. 0xDFFF]
/*
2.2 Decoding UTF-16

   [...]

   1) If W1 < 0xD800 or W1 > 0xDFFF, the character value U is the value
      of W1. Terminate.

   2) Determine if W1 is between 0xD800 and 0xDBFF. If not, the sequence
      is in error [...]
*/
TEST_LOOP(
    10, validate_utf16be_returns_false_when_input_has_wrong_first_word_value) {
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  auto utf16{generator.generate_be(128)};
  const size_t len = utf16.size();

  for (char16_t wrong_value = 0xdc00; wrong_value <= 0xdfff; wrong_value++) {
    for (size_t i = 0; i < utf16.size(); i++) {
      const char16_t old = utf16[i];
      utf16[i] = to_utf16be(wrong_value);

      const simdutf::result res = implementation.validate_utf16be_with_errors(
          reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
      ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
      ASSERT_EQUAL(res.count, i);

      utf16[i] = old;
    }
  }
}

/*
 RFC-2781:

 3) [..] if W2 is not between 0xDC00 and 0xDFFF, the sequence is in error.
    Terminate.
*/
TEST(validate_utf16be_returns_false_when_input_has_wrong_second_word_value) {
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  auto utf16{generator.generate_be(128)};
  const size_t len = utf16.size();

  const std::array<char16_t, 5> sample_wrong_second_word{0x0000, 0x0010, 0xffdb,
                                                         0x00e0, 0xffff};

  const char16_t valid_surrogate_W1 = to_utf16be(0xd800);
  for (char16_t W2 : sample_wrong_second_word) {
    for (size_t i = 0; i < utf16.size() - 1; i++) {
      const char16_t old_W1 = utf16[i + 0];
      const char16_t old_W2 = utf16[i + 1];

      utf16[i + 0] = valid_surrogate_W1;
      utf16[i + 1] = to_utf16be(W2);

      simdutf::result res = implementation.validate_utf16be_with_errors(
          reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
      ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
      ASSERT_EQUAL(res.count, i);

      utf16[i + 0] = old_W1;
      utf16[i + 1] = old_W2;
    }
  }
}

/*
 RFC-2781:

 3) If there is no W2 (that is, the sequence ends with W1) [...]
    the sequence is in error. Terminate.
*/
TEST(validate_utf16be_returns_false_when_input_is_truncated) {
  const char16_t valid_surrogate_W1 = to_utf16be(0xd800);
  uint32_t seed{1234};
  simdutf::tests::helpers::random_utf16 generator{seed, 1, 0};
  for (size_t size = 1; size < 128; size++) {
    auto utf16{generator.generate_be(128)};
    const size_t len = utf16.size();

    utf16[size - 1] = valid_surrogate_W1;

    simdutf::result res = implementation.validate_utf16be_with_errors(
        reinterpret_cast<const char16_t *>(utf16.data()), utf16.size());
    ASSERT_EQUAL(res.error, simdutf::error_code::SURROGATE);
    ASSERT_EQUAL(res.count, size - 1);
  }
}

TEST_MAIN
