/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_type_boundary.c - Type-boundary tests from spec 03 §8.2.
 *
 * Verifies the UTF-8/grapheme layer handles byte_len/offset values past
 * INT_MAX without truncating through int. The fast tests pass a byte_len
 * far larger than the real buffer, which is safe because the functions
 * under test stop reading at the first grapheme boundary; they run in
 * milliseconds and are all spec 03 §8.2 requires.
 *
 * The huge tests allocate a real >2 GB buffer and exercise the
 * grapheme-string layer with offsets past INT_MAX. Every one of them
 * must grapheme-walk on the order of 2^31 positions to reach its
 * regression (measured 1-5 minutes each at -O3), so they all require
 * GSTR_FULL_BOUNDARY=1 — run 'make test-boundary-full'. They SKIP (not
 * fail) if the >2 GB allocation isn't possible.
 */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gstr.h>
#include "test_macros.h"

_Static_assert(sizeof(size_t) >= sizeof(int),
               "size_t must be at least as wide as int");

/* ============================================================================
 * Fast tests: byte_len past INT_MAX on small buffers
 * ============================================================================
 */

TEST(next_grapheme_huge_bytelen_ascii) {
  /* Spec 03 §8.2: byte_len = INT_MAX+1 on a short ASCII string must
   * still find the correct boundary. Only bytes up to the first break
   * are read, so the lie is safe. */
  const char *s = "ab";
  ASSERT_EQ_SIZE(utf8_next_grapheme(s, (size_t)INT_MAX + 1, 0), 1);
}

TEST(next_grapheme_huge_bytelen_multibyte) {
  /* e + combining acute + x: first cluster is 3 bytes */
  const char *s = "e\xCC\x81x";
  ASSERT_EQ_SIZE(utf8_next_grapheme(s, (size_t)INT_MAX + 1, 0), 3);
}

TEST(prev_at_zero_no_underflow) {
  ASSERT_EQ_SIZE(utf8_prev("abc", 3, 0), 0);
}

TEST(range_contains_zero_count) {
  ASSERT_EQ(unicode_range_contains(0x41, INCB_CONSONANTS, 0), 0);
}

/* ============================================================================
 * Huge tests: real buffer with content past INT_MAX
 * ============================================================================
 */

#define HUGE_EXTRA 128
static char *huge_buf = NULL;
static size_t huge_len = (size_t)INT_MAX + HUGE_EXTRA;

TEST(endswith_suffix_past_int_max) {
  /* Regression: gstrendswith truncated the comparison start offset
   * through int and lost genuinely matching suffixes past INT_MAX. */
  memcpy(huge_buf + huge_len - 4, "WXYZ", 4);
  int r = gstrendswith(huge_buf, huge_len, "WXYZ", 4);
  /* Restore before asserting so a failure can't cascade into the
   * later tests that share huge_buf. */
  memcpy(huge_buf + huge_len - 4, "aaaa", 4);
  ASSERT_EQ(r, 1);
}

TEST(sub_start_past_int_max) {
  /* Regression: gstrsub's dst-too-small path truncated start_offset
   * through int and returned an empty string. */
  char dst[16];
  size_t n = gstrsub(dst, sizeof(dst), huge_buf, huge_len,
                     (size_t)INT_MAX + 1, 50);
  ASSERT_EQ_SIZE(n, 15);
  ASSERT_STR_EQ(dst, "aaaaaaaaaaaaaaa");
}

TEST(strstr_needle_past_int_max) {
  /* Regression: the search loop truncated grapheme offsets through int,
   * so needles located past INT_MAX were never found. */
  char *site = huge_buf + huge_len - 3;
  memcpy(site, "NDL", 3);
  const char *p = gstrstr(huge_buf, huge_len, "NDL", 3);
  memcpy(site, "aaa", 3);
  ASSERT_NOT_NULL(p);
  ASSERT(p == site);
}

TEST(rev_truncated_from_huge_src) {
  /* Not a regression test for the old code (whose truncation measured
   * forward from 0 and never saw offsets past INT_MAX for a small dst);
   * it guards the current forward cut-search, whose offsets do cross
   * INT_MAX on this input. */
  char dst[8];
  size_t n = gstrrev(dst, sizeof(dst), huge_buf, huge_len);
  ASSERT_EQ_SIZE(n, 7);
  ASSERT_STR_EQ(dst, "aaaaaaa");
}

TEST(replace_truncation_with_huge_remaining) {
  /* Regression: gstrreplace passed (int)remaining as a size_t length;
   * past INT_MAX the negative int sign-extended to a near-SIZE_MAX
   * length (out-of-bounds read potential). */
  char dst[8];
  size_t n = gstrreplace(dst, sizeof(dst), huge_buf, huge_len,
                         "ZZ", 2, "Y", 1);
  ASSERT_EQ_SIZE(n, 7);
  ASSERT_STR_EQ(dst, "aaaaaaa");
}

int main(void) {
  printf("\nType Boundary Tests (spec 03 \xC2\xA7" "8.2)\n");
  printf("==================================\n\n");

  RUN(next_grapheme_huge_bytelen_ascii);
  RUN(next_grapheme_huge_bytelen_multibyte);
  RUN(prev_at_zero_no_underflow);
  RUN(range_contains_zero_count);

  if (!getenv("GSTR_FULL_BOUNDARY")) {
    printf("\n  [SKIP] huge (>2 GB, offsets past INT_MAX) tests: each\n"
           "         grapheme-walks ~2^31 positions (minutes at -O3).\n"
           "         Run 'make test-boundary-full' to include them.\n");
  } else {
    huge_buf = (char *)malloc(huge_len + 1);
    if (!huge_buf) {
      printf("\n  [SKIP] huge (>2 GB) tests: allocation failed\n");
    } else {
      memset(huge_buf, 'a', huge_len);
      huge_buf[huge_len] = '\0';

      RUN(endswith_suffix_past_int_max);
      RUN(sub_start_past_int_max);
      RUN(rev_truncated_from_huge_src);
      RUN(strstr_needle_past_int_max);
      RUN(replace_truncation_with_huge_remaining);

      free(huge_buf);
    }
  }

  printf("\n----------------------------------------\n");
  printf("Tests passed: %d\n", tests_passed);
  printf("Tests failed: %d\n", tests_failed);
  printf("----------------------------------------\n");

  return tests_failed > 0 ? 1 : 0;
}
