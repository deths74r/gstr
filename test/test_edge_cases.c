/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_edge_cases.c - Cross-cutting edge case tests for gstr.h
 */

#include <gstr.h>
#include "test_macros.h"

/* ============================================================================
 * Invalid UTF-8 across gstr functions
 * ============================================================================ */

TEST(invalid_utf8_gstrlen) {
  /* Bare continuation bytes should each be 1 grapheme */
  ASSERT_EQ_SIZE(gstrlen("\x80\x80\x80", 3), 3);
}

TEST(invalid_utf8_gstrwidth) {
  /* Invalid bytes should still produce some width, not crash */
  size_t w = gstrwidth("\xFF\x80\x41", 3);
  ASSERT(w >= 1); /* At least the 'A' contributes */
}

TEST(invalid_utf8_gstrcmp) {
  /* Comparing invalid UTF-8 should not crash */
  int r = gstrcmp("\xFF", 1, "\xFF", 1);
  ASSERT_EQ(r, 0); /* Same invalid byte sequences are equal */
}

/* ============================================================================
 * CRLF grapheme cluster
 * ============================================================================ */

TEST(crlf_is_one_grapheme) {
  ASSERT_EQ_SIZE(gstrlen("\r\n", 2), 1);
}

TEST(crlf_gstrwidth) {
  /* CR+LF: the combined grapheme has control-char width */
  size_t w = gstrwidth("\r\n", 2);
  ASSERT_EQ_SIZE(w, 0);
}

TEST(crlf_trim) {
  char buf[32];
  size_t n = gstrtrim(buf, sizeof(buf), "\r\nHello\r\n", 9);
  ASSERT_EQ_SIZE(n, 5);
  ASSERT_STR_EQ(buf, "Hello");
}

/* ============================================================================
 * Regional Indicator edge cases
 * ============================================================================ */

TEST(ri_odd_count) {
  /* 3 regional indicators: should form 1 flag pair + 1 singleton */
  const char *three_ri = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xAC";
  ASSERT_EQ_SIZE(gstrlen(three_ri, 12), 2); /* US flag + lone G */
}

TEST(ri_single) {
  /* Single regional indicator: 1 grapheme */
  ASSERT_EQ_SIZE(gstrlen("\xF0\x9F\x87\xBA", 4), 1);
}

/* ============================================================================
 * Deeply stacked combining marks
 * ============================================================================ */

TEST(many_combining_marks) {
  /* Base char + 50 combining acute accents (U+0301 = CC 81) */
  char buf[102];
  buf[0] = 'A'; /* base */
  for (int i = 0; i < 50; i++) {
    buf[1 + i * 2] = (char)0xCC;
    buf[2 + i * 2] = (char)0x81;
  }
  /* Total: 1 + 100 = 101 bytes, should be 1 grapheme */
  ASSERT_EQ_SIZE(gstrlen(buf, 101), 1);
  /* Width should be 1 (base + combining = 1 col) */
  ASSERT_EQ_SIZE(gstrwidth(buf, 101), 1);
}

/* ============================================================================
 * Variation selectors
 * ============================================================================ */

TEST(vs16_heart) {
  /* U+2764 + U+FE0F = emoji presentation, width 2 */
  ASSERT_EQ_SIZE(gstrwidth("\xE2\x9D\xA4\xEF\xB8\x8F", 6), 2);
}

TEST(vs15_heart) {
  /* U+2764 + U+FE0E = text presentation, width 1 */
  ASSERT_EQ_SIZE(gstrwidth("\xE2\x9D\xA4\xEF\xB8\x8E", 6), 1);
}

/* ============================================================================
 * Empty string edge cases
 * ============================================================================ */

TEST(empty_gstrlen)    { ASSERT_EQ_SIZE(gstrlen("", 0), 0); }
TEST(empty_gstrwidth)  { ASSERT_EQ_SIZE(gstrwidth("", 0), 0); }
TEST(empty_gstrcmp)    { ASSERT_EQ(gstrcmp("", 0, "", 0), 0); }
TEST(empty_gstrstr)    { ASSERT_NOT_NULL(gstrstr("Hello", 5, "", 0)); }
TEST(empty_gstrrstr)   { ASSERT_NOT_NULL(gstrrstr("Hello", 5, "", 0)); }

/* ============================================================================
 * NULL input edge cases
 * ============================================================================ */

TEST(null_gstrlen)     { ASSERT_EQ_SIZE(gstrlen(NULL, 0), 0); }
TEST(null_gstrwidth)   { ASSERT_EQ_SIZE(gstrwidth(NULL, 0), 0); }
TEST(null_gstrchr)     { ASSERT_NULL(gstrchr(NULL, 0, "A", 1)); }
TEST(null_gstrstr)     { ASSERT_NULL(gstrstr(NULL, 0, "A", 1)); }
TEST(null_gstrdup)     { ASSERT_NULL(gstrdup(NULL, 0)); }

/* ============================================================================
 * Hangul Jamo
 * ============================================================================ */

TEST(hangul_precomposed) {
  /* U+AC00 (GA) = 3 bytes, 1 grapheme */
  ASSERT_EQ_SIZE(gstrlen("\xEA\xB0\x80", 3), 1);
}

TEST(hangul_width) {
  /* Hangul syllables are wide (2 columns) */
  ASSERT_EQ_SIZE(gstrwidth("\xEA\xB0\x80", 3), 2);
}

/* ============================================================================
 * Indic Conjunct (InCB) edge cases
 * ============================================================================ */

TEST(devanagari_conjunct) {
  /* KA + VIRAMA(094D) + KA = 1 grapheme (conjunct via GB9c) */
  const char *kka = "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x95";
  ASSERT_EQ_SIZE(gstrlen(kka, 9), 1);
}

TEST(non_linker_no_conjunct) {
  /* BRAHMI VIRAMA (U+11046) is NOT InCB=Linker, so no conjunct joining */
  /* This is verify-only: Brahmi consonant + Brahmi virama + Brahmi consonant
   * should be 2 graphemes (virama extends first, doesn't link) */
  /* U+11013(BRAHMI KA) + U+11046(BRAHMI VIRAMA) + U+11013(BRAHMI KA) */
  const char *s = "\xF0\x91\x80\x93\xF0\x91\x81\x86\xF0\x91\x80\x93";
  ASSERT_EQ_SIZE(gstrlen(s, 12), 2);
}

/* ============================================================================
 * GRAPHEME_MAX_BACKTRACK stress
 * ============================================================================ */

TEST(max_backtrack_stress) {
  /* 130 combining marks + 1 base = should not hang */
  char buf[262];
  buf[0] = 'A';
  for (int i = 0; i < 130; i++) {
    buf[1 + i * 2] = (char)0xCC;
    buf[2 + i * 2] = (char)0x81;
  }
  size_t len = 261;
  /* Just verify it doesn't hang and returns something */
  size_t prev = utf8_prev_grapheme(buf, len, len);
  ASSERT(prev <= len);
  (void)prev;
}

int main(void) {
  printf("\nEdge Case Tests\n");
  printf("================\n");

  printf("\nInvalid UTF-8:\n");
  RUN(invalid_utf8_gstrlen);
  RUN(invalid_utf8_gstrwidth);
  RUN(invalid_utf8_gstrcmp);

  printf("\nCRLF:\n");
  RUN(crlf_is_one_grapheme);
  RUN(crlf_gstrwidth);
  RUN(crlf_trim);

  printf("\nRegional Indicators:\n");
  RUN(ri_odd_count);
  RUN(ri_single);

  printf("\nCombining marks:\n");
  RUN(many_combining_marks);

  printf("\nVariation selectors:\n");
  RUN(vs16_heart);
  RUN(vs15_heart);

  printf("\nEmpty strings:\n");
  RUN(empty_gstrlen);
  RUN(empty_gstrwidth);
  RUN(empty_gstrcmp);
  RUN(empty_gstrstr);
  RUN(empty_gstrrstr);

  printf("\nNULL inputs:\n");
  RUN(null_gstrlen);
  RUN(null_gstrwidth);
  RUN(null_gstrchr);
  RUN(null_gstrstr);
  RUN(null_gstrdup);

  printf("\nHangul:\n");
  RUN(hangul_precomposed);
  RUN(hangul_width);

  printf("\nIndic Conjunct:\n");
  RUN(devanagari_conjunct);
  RUN(non_linker_no_conjunct);

  printf("\nBacktrack stress:\n");
  RUN(max_backtrack_stress);

  printf("\n----------------------------------------\n");
  printf("Tests passed: %d\n", tests_passed);
  printf("Tests failed: %d\n", tests_failed);
  printf("----------------------------------------\n");

  return tests_failed > 0 ? 1 : 0;
}
