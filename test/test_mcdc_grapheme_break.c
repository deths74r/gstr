/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_mcdc_grapheme_break.c - MC/DC tests for is_grapheme_break
 *
 * Tests the grapheme break decision function directly with explicit
 * parameter values, covering all UAX #29 rules (GB3-GB999).
 */

#include <gstr.h>
#include "test_macros.h"

/* ============================================================================
 * GB3: prev==CR && curr==LF -> no break
 * ============================================================================ */

TEST(gb3_cr_lf_no_break) {
  /* C1=T, C2=T -> no break */
  ASSERT_EQ(is_grapheme_break(GCB_CR, GCB_LF, 0, 0, 0x0A, 0), 0);
}

TEST(gb3_c1_false) {
  /* C1=F(not CR), C2=T(LF) -> falls to GB5, break */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_LF, 0, 0, 0x0A, 0), 1);
}

TEST(gb3_c2_false) {
  /* C1=T(CR), C2=F(not LF) -> falls to GB4, break */
  ASSERT_EQ(is_grapheme_break(GCB_CR, GCB_EXTEND, 0, 0, 0x0300, 0), 1);
}

/* ============================================================================
 * GB4: prev==CONTROL || prev==CR || prev==LF -> break
 * ============================================================================ */

TEST(gb4_control_break) {
  ASSERT_EQ(is_grapheme_break(GCB_CONTROL, GCB_EXTEND, 0, 0, 0x0300, 0), 1);
}

TEST(gb4_cr_not_lf_break) {
  /* CR followed by non-LF -> GB3 false, GB4 triggers */
  ASSERT_EQ(is_grapheme_break(GCB_CR, GCB_EXTEND, 0, 0, 0x0300, 0), 1);
}

TEST(gb4_lf_break) {
  ASSERT_EQ(is_grapheme_break(GCB_LF, GCB_EXTEND, 0, 0, 0x0300, 0), 1);
}

TEST(gb4_other_no_break) {
  /* prev is not CONTROL/CR/LF -> falls through */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 0, 0x0300, 0), 0);
}

/* ============================================================================
 * GB5: curr==CONTROL || curr==CR || curr==LF -> break
 * ============================================================================ */

TEST(gb5_control_break) {
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_CONTROL, 0, 0, 0x00, 0), 1);
}

TEST(gb5_cr_break) {
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_CR, 0, 0, 0x0D, 0), 1);
}

TEST(gb5_lf_break) {
  /* Not preceded by CR, so GB3 is false */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_LF, 0, 0, 0x0A, 0), 1);
}

/* ============================================================================
 * GB6: prev==L && (curr==L || curr==V || curr==LV || curr==LVT) -> no break
 * ============================================================================ */

TEST(gb6_l_l_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_L, GCB_L, 0, 0, 0x1100, 0), 0);
}

TEST(gb6_l_v_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_L, GCB_V, 0, 0, 0x1160, 0), 0);
}

TEST(gb6_l_lv_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_L, GCB_LV, 0, 0, 0xAC00, 0), 0);
}

TEST(gb6_l_lvt_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_L, GCB_LVT, 0, 0, 0xAC01, 0), 0);
}

TEST(gb6_l_other_break) {
  /* prev=L, curr=EXTEND -> GB6 false, falls to GB9 */
  ASSERT_EQ(is_grapheme_break(GCB_L, GCB_EXTEND, 0, 0, 0x0300, 0), 0); /* GB9 catches it */
}

/* ============================================================================
 * GB7: (prev==LV || prev==V) && (curr==V || curr==T) -> no break
 * ============================================================================ */

TEST(gb7_lv_v_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_LV, GCB_V, 0, 0, 0x1160, 0), 0);
}

TEST(gb7_v_t_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_V, GCB_T, 0, 0, 0x11A8, 0), 0);
}

/* ============================================================================
 * GB8: (prev==LVT || prev==T) && curr==T -> no break
 * ============================================================================ */

TEST(gb8_lvt_t_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_LVT, GCB_T, 0, 0, 0x11A8, 0), 0);
}

TEST(gb8_t_t_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_T, GCB_T, 0, 0, 0x11A8, 0), 0);
}

/* ============================================================================
 * GB9: curr==EXTEND || curr==ZWJ -> no break
 * ============================================================================ */

TEST(gb9_extend_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 0, 0x0300, 0), 0);
}

TEST(gb9_zwj_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_ZWJ, 0, 0, 0x200D, 0), 0);
}

/* ============================================================================
 * GB9a: curr==SPACING_MARK -> no break
 * ============================================================================ */

TEST(gb9a_spacing_mark_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_SPACING_MARK, 0, 0, 0x0903, 0), 0);
}

/* ============================================================================
 * GB9b: prev==PREPEND -> no break
 * ============================================================================ */

TEST(gb9b_prepend_no_break) {
  ASSERT_EQ(is_grapheme_break(GCB_PREPEND, GCB_EXTEND, 0, 0, 0x0300, 0), 0);
}

/* ============================================================================
 * GB9c: incb_state==2 && is_incb_consonant(curr_cp) -> no break
 * ============================================================================ */

TEST(gb9c_incb_conjunct_no_break) {
  /* incb_state=2 (saw consonant+linker), curr is a consonant */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 0, 0x0915, 2), 0);
}

TEST(gb9c_incb_state_not_2) {
  /* incb_state=1 (saw consonant but no linker) + consonant -> GB9 catches Extend */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 0, 0x0915, 1), 0);
}

TEST(gb9c_not_consonant) {
  /* incb_state=2 but curr is not a consonant -> GB9 still catches Extend */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 0, 0x0300, 2), 0);
}

/* ============================================================================
 * GB11: in_ext_pict && prev==ZWJ && is_extended_pictographic(curr_cp)
 * ============================================================================ */

TEST(gb11_extpict_zwj_extpict_no_break) {
  /* Full GB11 condition met */
  ASSERT_EQ(is_grapheme_break(GCB_ZWJ, GCB_EXTEND, 0, 1, 0x1F600, 0), 0);
}

TEST(gb11_no_ext_pict) {
  /* in_ext_pict=0 -> GB11 false */
  int r = is_grapheme_break(GCB_ZWJ, GCB_EXTEND, 0, 0, 0x1F600, 0);
  /* Falls to GB9 (curr=EXTEND), so no break */
  ASSERT_EQ(r, 0);
}

TEST(gb11_prev_not_zwj) {
  /* prev is not ZWJ -> GB11 false, but GB9 catches EXTEND */
  int r = is_grapheme_break(GCB_EXTEND, GCB_EXTEND, 0, 1, 0x1F600, 0);
  ASSERT_EQ(r, 0); /* GB9 */
}

/* ============================================================================
 * GB12/13: RI pairs - even count no break, odd count break
 * ============================================================================ */

TEST(gb12_ri_odd_no_break) {
  /* ri_count=1 (odd) -> (1%2)==0 is false -> return 0 (no break, pairing) */
  ASSERT_EQ(is_grapheme_break(GCB_REGIONAL_INDICATOR, GCB_REGIONAL_INDICATOR, 1, 0, 0x1F1FA, 0), 0);
}

TEST(gb13_ri_even_break) {
  /* ri_count=2 (even) -> (2%2)==0 is true -> return 1 (break, start new pair) */
  ASSERT_EQ(is_grapheme_break(GCB_REGIONAL_INDICATOR, GCB_REGIONAL_INDICATOR, 2, 0, 0x1F1FA, 0), 1);
}

TEST(gb12_ri_not_prev) {
  /* prev is not RI -> GB12/13 not reached, falls to GB999 */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_REGIONAL_INDICATOR, 0, 0, 0x1F1FA, 0), 1);
}

/* ============================================================================
 * GB999: default break
 * ============================================================================ */

TEST(gb999_default_break) {
  /* Two unrelated codepoints with no special properties -> break */
  ASSERT_EQ(is_grapheme_break(GCB_EXTEND, GCB_L, 0, 0, 0x1100, 0), 1);
}

/* ============================================================================
 * Integration tests: verify state machine via gstrlen
 * ============================================================================ */

TEST(integration_crlf) {
  ASSERT_EQ_SIZE(gstrlen("\r\n", 2), 1);
}

TEST(integration_cr_a) {
  ASSERT_EQ_SIZE(gstrlen("\r" "a", 2), 2);
}

TEST(integration_family) {
  /* Man+ZWJ+Woman+ZWJ+Girl */
  const char *s = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
  ASSERT_EQ_SIZE(gstrlen(s, 18), 1);
}

TEST(integration_flags) {
  /* US + GB = 2 flags (4 RIs) */
  const char *s = "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7";
  ASSERT_EQ_SIZE(gstrlen(s, 16), 2);
}

TEST(integration_devanagari_conjunct) {
  /* KA + VIRAMA + KA = 1 grapheme via GB9c */
  const char *s = "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x95";
  ASSERT_EQ_SIZE(gstrlen(s, 9), 1);
}

TEST(integration_combining) {
  /* e + combining acute = 1 grapheme via GB9 */
  ASSERT_EQ_SIZE(gstrlen("e\xCC\x81", 3), 1);
}

TEST(integration_hangul_lv_t) {
  /* Precomposed Hangul + trailing T = 1 grapheme (GB8) */
  /* U+AC00 (LV) + U+11A8 (T) */
  ASSERT_EQ_SIZE(gstrlen("\xEA\xB0\x80\xE1\x86\xA8", 6), 1);
}

int main(void) {
  printf("\nMC/DC Grapheme Break Tests\n");
  printf("===========================\n");

  printf("\nGB3 (CR+LF):\n");
  RUN(gb3_cr_lf_no_break);
  RUN(gb3_c1_false);
  RUN(gb3_c2_false);

  printf("\nGB4 (CONTROL/CR/LF break after):\n");
  RUN(gb4_control_break);
  RUN(gb4_cr_not_lf_break);
  RUN(gb4_lf_break);
  RUN(gb4_other_no_break);

  printf("\nGB5 (CONTROL/CR/LF break before):\n");
  RUN(gb5_control_break);
  RUN(gb5_cr_break);
  RUN(gb5_lf_break);

  printf("\nGB6 (Hangul L):\n");
  RUN(gb6_l_l_no_break);
  RUN(gb6_l_v_no_break);
  RUN(gb6_l_lv_no_break);
  RUN(gb6_l_lvt_no_break);
  RUN(gb6_l_other_break);

  printf("\nGB7 (Hangul LV/V):\n");
  RUN(gb7_lv_v_no_break);
  RUN(gb7_v_t_no_break);

  printf("\nGB8 (Hangul LVT/T):\n");
  RUN(gb8_lvt_t_no_break);
  RUN(gb8_t_t_no_break);

  printf("\nGB9 (Extend/ZWJ):\n");
  RUN(gb9_extend_no_break);
  RUN(gb9_zwj_no_break);

  printf("\nGB9a (SpacingMark):\n");
  RUN(gb9a_spacing_mark_no_break);

  printf("\nGB9b (Prepend):\n");
  RUN(gb9b_prepend_no_break);

  printf("\nGB9c (InCB Conjunct):\n");
  RUN(gb9c_incb_conjunct_no_break);
  RUN(gb9c_incb_state_not_2);
  RUN(gb9c_not_consonant);

  printf("\nGB11 (ZWJ Emoji):\n");
  RUN(gb11_extpict_zwj_extpict_no_break);
  RUN(gb11_no_ext_pict);
  RUN(gb11_prev_not_zwj);

  printf("\nGB12/13 (Regional Indicators):\n");
  RUN(gb12_ri_odd_no_break);
  RUN(gb13_ri_even_break);
  RUN(gb12_ri_not_prev);

  printf("\nGB999 (Default):\n");
  RUN(gb999_default_break);

  printf("\nIntegration:\n");
  RUN(integration_crlf);
  RUN(integration_cr_a);
  RUN(integration_family);
  RUN(integration_flags);
  RUN(integration_devanagari_conjunct);
  RUN(integration_combining);
  RUN(integration_hangul_lv_t);

  printf("\n----------------------------------------\n");
  printf("Tests passed: %d\n", tests_passed);
  printf("Tests failed: %d\n", tests_failed);
  printf("----------------------------------------\n");

  return tests_failed > 0 ? 1 : 0;
}
