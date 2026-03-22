/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_utf8_layer.c - Tests for the UTF-8 layer functions in gstr.h
 */

#include <gstr.h>
#include "test_macros.h"

/* ============================================================================
 * utf8_decode tests
 * ============================================================================ */

TEST(decode_ascii)        { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("A", 1, &cp), 1); ASSERT_EQ_U32(cp, 0x41); }
TEST(decode_2byte_start)  { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xC2\x80", 2, &cp), 2); ASSERT_EQ_U32(cp, 0x80); }
TEST(decode_2byte_end)    { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xDF\xBF", 2, &cp), 2); ASSERT_EQ_U32(cp, 0x7FF); }
TEST(decode_3byte_start)  { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xE0\xA0\x80", 3, &cp), 3); ASSERT_EQ_U32(cp, 0x800); }
TEST(decode_3byte_end)    { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xEF\xBF\xBF", 3, &cp), 3); ASSERT_EQ_U32(cp, 0xFFFF); }
TEST(decode_4byte_start)  { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xF0\x90\x80\x80", 4, &cp), 4); ASSERT_EQ_U32(cp, 0x10000); }
TEST(decode_4byte_max)    { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("\xF4\x8F\xBF\xBF", 4, &cp), 4); ASSERT_EQ_U32(cp, 0x10FFFF); }
TEST(decode_overlong_2)   { uint32_t cp; utf8_decode("\xC0\x80", 2, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_overlong_3)   { uint32_t cp; utf8_decode("\xE0\x80\x80", 3, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_overlong_4)   { uint32_t cp; utf8_decode("\xF0\x80\x80\x80", 4, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_surrogate)    { uint32_t cp; utf8_decode("\xED\xA0\x80", 3, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_above_max)    { uint32_t cp; utf8_decode("\xF4\x90\x80\x80", 4, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_truncated_2)  { uint32_t cp; utf8_decode("\xC3", 1, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_truncated_3)  { uint32_t cp; utf8_decode("\xE4\xB8", 2, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_truncated_4)  { uint32_t cp; utf8_decode("\xF0\x9F\x98", 3, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_bare_cont)    { uint32_t cp; utf8_decode("\x80", 1, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_0xff)         { uint32_t cp; utf8_decode("\xFF", 1, &cp); ASSERT_EQ_U32(cp, 0xFFFD); }
TEST(decode_zero_len)     { uint32_t cp; ASSERT_EQ_SIZE(utf8_decode("A", 0, &cp), 0); }

/* ============================================================================
 * utf8_encode tests
 * ============================================================================ */

TEST(encode_ascii)     { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x41, b), 1); ASSERT_EQ(b[0], 'A'); }
TEST(encode_2byte)     { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x80, b), 2); ASSERT_EQ((unsigned char)b[0], 0xC2); ASSERT_EQ((unsigned char)b[1], 0x80); }
TEST(encode_3byte)     { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x800, b), 3); ASSERT_EQ((unsigned char)b[0], 0xE0); }
TEST(encode_4byte)     { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x10000, b), 4); ASSERT_EQ((unsigned char)b[0], 0xF0); }
TEST(encode_max)       { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x10FFFF, b), 4); }
TEST(encode_surrogate) { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0xD800, b), 0); }
TEST(encode_too_large) { char b[5]; ASSERT_EQ_SIZE(utf8_encode(0x110000, b), 0); }

/* Roundtrip: encode then decode */
TEST(encode_decode_roundtrip) {
  uint32_t test_cps[] = {0, 0x7F, 0x80, 0x7FF, 0x800, 0xFFFF, 0x10000, 0x10FFFF};
  for (size_t i = 0; i < sizeof(test_cps)/sizeof(test_cps[0]); i++) {
    char buf[5];
    size_t n = utf8_encode(test_cps[i], buf);
    ASSERT(n > 0);
    uint32_t out;
    size_t n2 = utf8_decode(buf, n, &out);
    ASSERT_EQ_SIZE(n2, n);
    ASSERT_EQ_U32(out, test_cps[i]);
  }
}

/* ============================================================================
 * utf8_valid tests
 * ============================================================================ */

TEST(valid_ascii)      { ASSERT(utf8_valid("Hello", 5, NULL)); }
TEST(valid_multibyte)  { ASSERT(utf8_valid("\xC3\xA9", 2, NULL)); }
TEST(valid_empty)      { ASSERT(utf8_valid("", 0, NULL)); }
TEST(invalid_bare_cont){ ASSERT(!utf8_valid("\x80", 1, NULL)); }
TEST(invalid_overlong) { ASSERT(!utf8_valid("\xC0\x80", 2, NULL)); }
TEST(invalid_surrogate){ ASSERT(!utf8_valid("\xED\xA0\x80", 3, NULL)); }
TEST(invalid_0xff)     { size_t e; ASSERT(!utf8_valid("\xFF", 1, &e)); ASSERT_EQ_SIZE(e, 0); }
TEST(valid_null_text)  { ASSERT(!utf8_valid(NULL, 5, NULL)); }
TEST(valid_error_offset) {
  size_t e;
  ASSERT(!utf8_valid("A\xFF" "B", 3, &e));
  ASSERT_EQ_SIZE(e, 1);
}

/* ============================================================================
 * utf8_next / utf8_prev tests
 * ============================================================================ */

TEST(next_ascii)  { ASSERT_EQ_SIZE(utf8_next("AB", 2, 0), 1); }
TEST(next_2byte)  { ASSERT_EQ_SIZE(utf8_next("\xC3\xA9X", 3, 0), 2); }
TEST(next_3byte)  { ASSERT_EQ_SIZE(utf8_next("\xE4\xB8\xADX", 4, 0), 3); }
TEST(next_4byte)  { ASSERT_EQ_SIZE(utf8_next("\xF0\x9F\x98\x80X", 5, 0), 4); }
TEST(next_at_end) { ASSERT_EQ_SIZE(utf8_next("A", 1, 0), 1); }
TEST(next_past)   { ASSERT_EQ_SIZE(utf8_next("A", 1, 1), 1); }

TEST(prev_ascii)  { ASSERT_EQ_SIZE(utf8_prev("AB", 2, 2), 1); }
TEST(prev_2byte)  { ASSERT_EQ_SIZE(utf8_prev("A\xC3\xA9", 3, 3), 1); }
TEST(prev_3byte)  { ASSERT_EQ_SIZE(utf8_prev("A\xE4\xB8\xAD", 4, 4), 1); }
TEST(prev_4byte)  { ASSERT_EQ_SIZE(utf8_prev("A\xF0\x9F\x98\x80", 5, 5), 1); }
TEST(prev_zero)   { ASSERT_EQ_SIZE(utf8_prev("A", 1, 0), 0); }
TEST(prev_null)   { ASSERT_EQ_SIZE(utf8_prev(NULL, 0, 0), 0); }

/* ============================================================================
 * utf8_is_zerowidth / utf8_is_wide tests
 * ============================================================================ */

TEST(zw_combining_start)  { ASSERT(utf8_is_zerowidth(0x0300)); }
TEST(zw_combining_end)    { ASSERT(utf8_is_zerowidth(0x036F)); }
TEST(zw_before_range)     { ASSERT(!utf8_is_zerowidth(0x02FF)); }
TEST(zw_after_range)      { ASSERT(!utf8_is_zerowidth(0x0370)); }
TEST(zw_skin_tone)        { ASSERT(utf8_is_zerowidth(0x1F3FB)); }
TEST(zw_ascii)            { ASSERT(!utf8_is_zerowidth(0x41)); }

TEST(wide_hangul_start)   { ASSERT(utf8_is_wide(0x1100)); }
TEST(wide_hangul_end)     { ASSERT(utf8_is_wide(0x115F)); }
TEST(wide_before_hangul)  { ASSERT(!utf8_is_wide(0x10FF)); }
TEST(wide_cjk)            { ASSERT(utf8_is_wide(0x4E00)); }
TEST(wide_emoji)          { ASSERT(utf8_is_wide(0x1F600)); }
TEST(wide_ascii)          { ASSERT(!utf8_is_wide(0x5A)); }

/* ============================================================================
 * utf8_charwidth tests
 * ============================================================================ */

TEST(charwidth_ascii)     { ASSERT_EQ(utf8_charwidth("A", 1, 0), 1); }
TEST(charwidth_cjk)       { ASSERT_EQ(utf8_charwidth("\xE4\xB8\xAD", 3, 0), 2); }
TEST(charwidth_combining) { ASSERT_EQ(utf8_charwidth("\xCC\x81", 2, 0), 0); }
TEST(charwidth_emoji)     { ASSERT_EQ(utf8_charwidth("\xF0\x9F\x98\x80", 4, 0), 2); }
TEST(charwidth_zwj)       { ASSERT_EQ(utf8_charwidth("\xE2\x80\x8D", 3, 0), 0); }
TEST(charwidth_fullwidth) { ASSERT_EQ(utf8_charwidth("\xEF\xBC\xA8", 3, 0), 2); }

/* ============================================================================
 * utf8_prev_grapheme tests
 * ============================================================================ */

TEST(prev_grapheme_ascii) {
  ASSERT_EQ_SIZE(utf8_prev_grapheme("AB", 2, 2), 1);
  ASSERT_EQ_SIZE(utf8_prev_grapheme("AB", 2, 1), 0);
  ASSERT_EQ_SIZE(utf8_prev_grapheme("A", 1, 1), 0);
}

TEST(prev_grapheme_emoji) {
  /* Family ZWJ: one grapheme of 18 bytes */
  const char *family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
  ASSERT_EQ_SIZE(utf8_prev_grapheme(family, 18, 18), 0);
}

TEST(prev_grapheme_flag) {
  /* Two regional indicators: 8 bytes, one grapheme */
  const char *flag = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
  ASSERT_EQ_SIZE(utf8_prev_grapheme(flag, 8, 8), 0);
}

TEST(prev_grapheme_null)  { ASSERT_EQ_SIZE(utf8_prev_grapheme(NULL, 0, 0), 0); }
TEST(prev_grapheme_zero)  { ASSERT_EQ_SIZE(utf8_prev_grapheme("A", 1, 0), 0); }

TEST(prev_grapheme_fwd_bwd_consistency) {
  /* Walk forward collecting boundaries, then walk backward and compare */
  const char *s = "Caf\xC3\xA9 \xF0\x9F\x87\xBA\xF0\x9F\x87\xB8!";
  size_t len = strlen(s);
  size_t boundaries[20];
  size_t n = 0;
  boundaries[n++] = 0;
  size_t off = 0;
  while (off < len) {
    off = utf8_next_grapheme(s, len, off);
    boundaries[n++] = off;
  }
  /* Walk backward */
  for (size_t i = n - 1; i > 0; i--) {
    size_t prev = utf8_prev_grapheme(s, len, boundaries[i]);
    ASSERT_EQ_SIZE(prev, boundaries[i - 1]);
  }
}

/* ============================================================================
 * utf8_cpcount tests
 * ============================================================================ */

TEST(cpcount_ascii)  { ASSERT_EQ_SIZE(utf8_cpcount("Hello", 5), 5); }
TEST(cpcount_multi)  { ASSERT_EQ_SIZE(utf8_cpcount("Caf\xC3\xA9", 5), 4); }
TEST(cpcount_empty)  { ASSERT_EQ_SIZE(utf8_cpcount("", 0), 0); }
TEST(cpcount_null)   { ASSERT_EQ_SIZE(utf8_cpcount(NULL, 0), 0); }

/* ============================================================================
 * utf8_truncate tests
 * ============================================================================ */

TEST(truncate_ascii) { ASSERT_EQ_SIZE(utf8_truncate("Hello", 5, 3), 3); }
TEST(truncate_wide)  { ASSERT_EQ_SIZE(utf8_truncate("\xE4\xB8\xAD\xE6\x96\x87", 6, 3), 3); }
TEST(truncate_zero)  { ASSERT_EQ_SIZE(utf8_truncate("Hello", 5, 0), 0); }

int main(void) {
  printf("\nUTF-8 Layer Tests\n");
  printf("==================\n");

  printf("\nutf8_decode:\n");
  RUN(decode_ascii);
  RUN(decode_2byte_start);
  RUN(decode_2byte_end);
  RUN(decode_3byte_start);
  RUN(decode_3byte_end);
  RUN(decode_4byte_start);
  RUN(decode_4byte_max);
  RUN(decode_overlong_2);
  RUN(decode_overlong_3);
  RUN(decode_overlong_4);
  RUN(decode_surrogate);
  RUN(decode_above_max);
  RUN(decode_truncated_2);
  RUN(decode_truncated_3);
  RUN(decode_truncated_4);
  RUN(decode_bare_cont);
  RUN(decode_0xff);
  RUN(decode_zero_len);

  printf("\nutf8_encode:\n");
  RUN(encode_ascii);
  RUN(encode_2byte);
  RUN(encode_3byte);
  RUN(encode_4byte);
  RUN(encode_max);
  RUN(encode_surrogate);
  RUN(encode_too_large);
  RUN(encode_decode_roundtrip);

  printf("\nutf8_valid:\n");
  RUN(valid_ascii);
  RUN(valid_multibyte);
  RUN(valid_empty);
  RUN(invalid_bare_cont);
  RUN(invalid_overlong);
  RUN(invalid_surrogate);
  RUN(invalid_0xff);
  RUN(valid_null_text);
  RUN(valid_error_offset);

  printf("\nutf8_next/prev:\n");
  RUN(next_ascii);
  RUN(next_2byte);
  RUN(next_3byte);
  RUN(next_4byte);
  RUN(next_at_end);
  RUN(next_past);
  RUN(prev_ascii);
  RUN(prev_2byte);
  RUN(prev_3byte);
  RUN(prev_4byte);
  RUN(prev_zero);
  RUN(prev_null);

  printf("\nutf8_is_zerowidth/is_wide:\n");
  RUN(zw_combining_start);
  RUN(zw_combining_end);
  RUN(zw_before_range);
  RUN(zw_after_range);
  RUN(zw_skin_tone);
  RUN(zw_ascii);
  RUN(wide_hangul_start);
  RUN(wide_hangul_end);
  RUN(wide_before_hangul);
  RUN(wide_cjk);
  RUN(wide_emoji);
  RUN(wide_ascii);

  printf("\nutf8_charwidth:\n");
  RUN(charwidth_ascii);
  RUN(charwidth_cjk);
  RUN(charwidth_combining);
  RUN(charwidth_emoji);
  RUN(charwidth_zwj);
  RUN(charwidth_fullwidth);

  printf("\nutf8_prev_grapheme:\n");
  RUN(prev_grapheme_ascii);
  RUN(prev_grapheme_emoji);
  RUN(prev_grapheme_flag);
  RUN(prev_grapheme_null);
  RUN(prev_grapheme_zero);
  RUN(prev_grapheme_fwd_bwd_consistency);

  printf("\nutf8_cpcount:\n");
  RUN(cpcount_ascii);
  RUN(cpcount_multi);
  RUN(cpcount_empty);
  RUN(cpcount_null);

  printf("\nutf8_truncate:\n");
  RUN(truncate_ascii);
  RUN(truncate_wide);
  RUN(truncate_zero);

  printf("\n----------------------------------------\n");
  printf("Tests passed: %d\n", tests_passed);
  printf("Tests failed: %d\n", tests_failed);
  printf("----------------------------------------\n");

  return tests_failed > 0 ? 1 : 0;
}
