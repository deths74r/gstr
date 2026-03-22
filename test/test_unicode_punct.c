/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_unicode_punct.c - Tests for gstr_is_unicode_punctuation and
 *                        gstr_is_whitespace_cp
 *
 * MC/DC test suite for Unicode punctuation/symbol classification
 * per Spec 11 (with review addendum changes).
 */

#include <ctype.h>
#include <gstr.h>
#include "test_macros.h"

/* ============================================================================
 * ASCII Punctuation (P* + S* categories, expected = 1)
 * ============================================================================ */

TEST(ascii_exclamation) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0021), 1); /* ! Po, first range start */
}

TEST(ascii_slash) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x002F), 1); /* / Po, first range end */
}

TEST(ascii_colon) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x003A), 1); /* : Po, second range start */
}

TEST(ascii_at_sign) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0040), 1); /* @ Po, second range end */
}

TEST(ascii_open_bracket) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x005B), 1); /* [ Ps, third range start */
}

TEST(ascii_backtick) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0060), 1); /* ` Sk, third range end */
}

TEST(ascii_open_brace) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x007B), 1); /* { Ps, fourth range start */
}

TEST(ascii_tilde) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x007E), 1); /* ~ Sm, fourth range end */
}

TEST(ascii_dollar) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0024), 1); /* $ Sc */
}

TEST(ascii_plus) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x002B), 1); /* + Sm */
}

TEST(ascii_less_than) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x003C), 1); /* < Sm */
}

TEST(ascii_equals) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x003D), 1); /* = Sm */
}

TEST(ascii_greater_than) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x003E), 1); /* > Sm */
}

TEST(ascii_caret) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x005E), 1); /* ^ Sk */
}

TEST(ascii_pipe) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x007C), 1); /* | Sm */
}

/* ============================================================================
 * ASCII Non-Punctuation (expected = 0)
 * ============================================================================ */

TEST(ascii_space) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0020), 0); /* Below first range */
}

TEST(ascii_nul) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0000), 0); /* Edge case */
}

TEST(ascii_digit_0) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0030), 0); /* Between range 1-2 */
}

TEST(ascii_letter_A) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0041), 0); /* Between range 2-3 */
}

TEST(ascii_letter_a) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0061), 0); /* Between range 3-4 */
}

TEST(ascii_del) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x007F), 0); /* Above last range */
}

/* ============================================================================
 * ASCII MC/DC Boundaries (just before each range start)
 * ============================================================================ */

TEST(ascii_before_range2) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0039), 0); /* '9', just before ':' */
}

TEST(ascii_before_range3) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x005A), 0); /* 'Z', just before '[' */
}

TEST(ascii_before_range4) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x007A), 0); /* 'z', just before '{' */
}

/* ============================================================================
 * ASCII-to-Binary-Search Transition
 * ============================================================================ */

TEST(first_non_ascii) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0080), 0); /* Cc, binary search path */
}

/* ============================================================================
 * Unicode Punctuation (non-ASCII, P* categories)
 * ============================================================================ */

TEST(unicode_inverted_exclamation) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A1), 1); /* ¡ Po */
}

TEST(unicode_section_sign) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A7), 1); /* § Po */
}

TEST(unicode_left_guillemet) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00AB), 1); /* « Pi */
}

TEST(unicode_right_guillemet) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00BB), 1); /* » Pf */
}

TEST(unicode_en_dash) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2013), 1); /* – Pd */
}

TEST(unicode_em_dash) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2014), 1); /* — Pd */
}

TEST(unicode_left_single_quote) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2018), 1); /* ' Pi */
}

TEST(unicode_right_single_quote) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2019), 1); /* ' Pf */
}

TEST(unicode_left_double_quote) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x201C), 1); /* " Pi */
}

TEST(unicode_right_double_quote) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x201D), 1); /* " Pf */
}

TEST(unicode_dagger) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2020), 1); /* † Po */
}

TEST(unicode_ellipsis) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2026), 1); /* … Po */
}

TEST(unicode_fullwidth_exclamation) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0xFF01), 1); /* ！ Po */
}

TEST(unicode_fullwidth_period) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0xFF0E), 1); /* ．Po */
}

TEST(unicode_cjk_comma) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x3001), 1); /* 、 Po */
}

TEST(unicode_cjk_period) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x3002), 1); /* 。 Po */
}

TEST(unicode_left_corner_bracket) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x300C), 1); /* 「 Ps */
}

/* ============================================================================
 * Unicode Symbols (non-ASCII, S* categories)
 * ============================================================================ */

TEST(unicode_cent) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A2), 1); /* ¢ Sc */
}

TEST(unicode_yen) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A5), 1); /* ¥ Sc */
}

TEST(unicode_copyright) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A9), 1); /* © So */
}

TEST(unicode_not_sign) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00AC), 1); /* ¬ Sm */
}

TEST(unicode_registered) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00AE), 1); /* ® So */
}

TEST(unicode_degree) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00B0), 1); /* ° So */
}

TEST(unicode_multiply) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00D7), 1); /* × Sm */
}

TEST(unicode_divide) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00F7), 1); /* ÷ Sm */
}

TEST(unicode_euro) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x20AC), 1); /* € Sc */
}

TEST(unicode_not_equal) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2260), 1); /* ≠ Sm */
}

/* ============================================================================
 * Unicode Non-Punctuation, Non-Symbol (expected = 0)
 * ============================================================================ */

TEST(unicode_latin_a_umlaut) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00E4), 0); /* ä Ll */
}

TEST(unicode_cyrillic_A) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0410), 0); /* А Lu */
}

TEST(unicode_cjk_ideograph) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x4E2D), 0); /* 中 Lo */
}

TEST(unicode_arabic_alef) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0627), 0); /* ا Lo */
}

TEST(unicode_combining_acute) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0301), 0); /* combining acute Mn */
}

TEST(unicode_devanagari_ka) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x0915), 0); /* क Lo */
}

TEST(unicode_nb_space) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00A0), 0); /* NBSP Zs */
}

TEST(unicode_em_space) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2003), 0); /* EM SPACE Zs */
}

TEST(unicode_line_separator) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x2028), 0); /* LINE SEPARATOR Zl */
}

/* ============================================================================
 * Boundary Cases
 * ============================================================================ */

TEST(boundary_max_bmp) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0xFFFF), 0); /* Cn */
}

TEST(boundary_replacement_char) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0xFFFD), 1); /* REPLACEMENT CHARACTER So */
}

TEST(boundary_surrogate_start) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0xD800), 0); /* Cs */
}

TEST(boundary_first_supplementary) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x10000), 0); /* LINEAR B SYLLABLE Lo */
}

TEST(boundary_max_codepoint) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x10FFFF), 0);
}

TEST(boundary_above_max) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x110000), 0);
}

/* ============================================================================
 * Supplementary Plane Punctuation/Symbols
 * ============================================================================ */

TEST(supplementary_aegean_separator) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x10100), 1); /* AEGEAN WORD SEPARATOR Po */
}

TEST(supplementary_musical_symbol) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x1D100), 1); /* MUSICAL SYMBOL So */
}

/* ============================================================================
 * Range-Gap Verification (between P and S ranges, expected = 0)
 * ============================================================================ */

TEST(gap_feminine_ordinal) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00AA), 0); /* ª Lo */
}

TEST(gap_soft_hyphen) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00AD), 0); /* Cf (soft hyphen) */
}

TEST(gap_micro_sign) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x00B5), 0); /* µ Ll */
}

/* ============================================================================
 * Out-of-Range Defensive Tests
 * ============================================================================ */

TEST(out_of_range) {
  ASSERT_EQ(gstr_is_unicode_punctuation(0x110000), 0);
  ASSERT_EQ(gstr_is_unicode_punctuation(0xFFFFFFFF), 0);
}

/* ============================================================================
 * Exhaustive ASCII Verification
 * ============================================================================ */

TEST(ascii_exhaustive) {
  for (uint32_t cp = 0; cp < 0x80; cp++) {
    int expected = (cp >= 0x21 && cp <= 0x2F) ||
                   (cp >= 0x3A && cp <= 0x40) ||
                   (cp >= 0x5B && cp <= 0x60) ||
                   (cp >= 0x7B && cp <= 0x7E);
    ASSERT_EQ(gstr_is_unicode_punctuation(cp), expected);
  }
}

/* ============================================================================
 * ispunct() Compatibility
 * ============================================================================ */

TEST(ispunct_compatibility) {
  for (int cp = 1; cp < 128; cp++) {
    int c_result = ispunct(cp) ? 1 : 0;
    ASSERT_EQ(gstr_is_unicode_punctuation((uint32_t)cp), c_result);
  }
}

/* ============================================================================
 * Fast-Path vs Table Agreement
 * ============================================================================ */

TEST(fast_path_matches_table) {
  for (uint32_t cp = 0; cp < 0x80; cp++) {
    int fast = gstr_is_unicode_punctuation(cp);
    int table = unicode_range_contains(cp, UNICODE_PUNCT_SYMBOL_RANGES,
                                       UNICODE_PUNCT_SYMBOL_COUNT);
    ASSERT_EQ(fast, table);
  }
}

/* ============================================================================
 * Whitespace / Punctuation Mutual Exclusion
 * ============================================================================ */

TEST(whitespace_punct_mutual_exclusion) {
  uint32_t ws[] = {0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020,
                   0x0085, 0x00A0, 0x1680, 0x2000, 0x2003, 0x2028,
                   0x2029, 0x202F, 0x205F, 0x3000};
  for (size_t i = 0; i < sizeof(ws) / sizeof(ws[0]); i++) {
    ASSERT_EQ(gstr_is_unicode_punctuation(ws[i]), 0);
  }
}

/* ============================================================================
 * gstr_is_whitespace_cp Tests
 * ============================================================================ */

TEST(whitespace_cp_space) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0020), 1);
}

TEST(whitespace_cp_tab) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0009), 1);
}

TEST(whitespace_cp_lf) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x000A), 1);
}

TEST(whitespace_cp_nel) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0085), 1);
}

TEST(whitespace_cp_nbsp) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x00A0), 1);
}

TEST(whitespace_cp_ideographic_space) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x3000), 1);
}

TEST(whitespace_cp_non_whitespace_letter) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0041), 0); /* 'A' */
}

TEST(whitespace_cp_non_whitespace_punct) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0021), 0); /* '!' */
}

TEST(whitespace_cp_non_whitespace_nul) {
  ASSERT_EQ(gstr_is_whitespace_cp(0x0000), 0);
}

TEST(whitespace_cp_matches_grapheme_variant) {
  /* Verify codepoint-level and grapheme-level agree for all 25 values */
  uint32_t ws[] = {0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x0020,
                   0x0085, 0x00A0, 0x1680,
                   0x2000, 0x2001, 0x2002, 0x2003,
                   0x2004, 0x2005, 0x2006, 0x2007,
                   0x2008, 0x2009, 0x200A,
                   0x2028, 0x2029, 0x202F, 0x205F, 0x3000};
  for (size_t i = 0; i < sizeof(ws) / sizeof(ws[0]); i++) {
    ASSERT_EQ(gstr_is_whitespace_cp(ws[i]), 1);
  }
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void) {
  printf("Unicode Punctuation Classification Tests\n");
  printf("========================================\n");

  printf("\nASCII Punctuation:\n");
  RUN(ascii_exclamation);
  RUN(ascii_slash);
  RUN(ascii_colon);
  RUN(ascii_at_sign);
  RUN(ascii_open_bracket);
  RUN(ascii_backtick);
  RUN(ascii_open_brace);
  RUN(ascii_tilde);
  RUN(ascii_dollar);
  RUN(ascii_plus);
  RUN(ascii_less_than);
  RUN(ascii_equals);
  RUN(ascii_greater_than);
  RUN(ascii_caret);
  RUN(ascii_pipe);

  printf("\nASCII Non-Punctuation:\n");
  RUN(ascii_space);
  RUN(ascii_nul);
  RUN(ascii_digit_0);
  RUN(ascii_letter_A);
  RUN(ascii_letter_a);
  RUN(ascii_del);

  printf("\nASCII MC/DC Boundaries:\n");
  RUN(ascii_before_range2);
  RUN(ascii_before_range3);
  RUN(ascii_before_range4);

  printf("\nASCII-to-Binary-Search Transition:\n");
  RUN(first_non_ascii);

  printf("\nUnicode Punctuation (P*):\n");
  RUN(unicode_inverted_exclamation);
  RUN(unicode_section_sign);
  RUN(unicode_left_guillemet);
  RUN(unicode_right_guillemet);
  RUN(unicode_en_dash);
  RUN(unicode_em_dash);
  RUN(unicode_left_single_quote);
  RUN(unicode_right_single_quote);
  RUN(unicode_left_double_quote);
  RUN(unicode_right_double_quote);
  RUN(unicode_dagger);
  RUN(unicode_ellipsis);
  RUN(unicode_fullwidth_exclamation);
  RUN(unicode_fullwidth_period);
  RUN(unicode_cjk_comma);
  RUN(unicode_cjk_period);
  RUN(unicode_left_corner_bracket);

  printf("\nUnicode Symbols (S*):\n");
  RUN(unicode_cent);
  RUN(unicode_yen);
  RUN(unicode_copyright);
  RUN(unicode_not_sign);
  RUN(unicode_registered);
  RUN(unicode_degree);
  RUN(unicode_multiply);
  RUN(unicode_divide);
  RUN(unicode_euro);
  RUN(unicode_not_equal);

  printf("\nUnicode Non-Punctuation:\n");
  RUN(unicode_latin_a_umlaut);
  RUN(unicode_cyrillic_A);
  RUN(unicode_cjk_ideograph);
  RUN(unicode_arabic_alef);
  RUN(unicode_combining_acute);
  RUN(unicode_devanagari_ka);
  RUN(unicode_nb_space);
  RUN(unicode_em_space);
  RUN(unicode_line_separator);

  printf("\nBoundary Cases:\n");
  RUN(boundary_max_bmp);
  RUN(boundary_replacement_char);
  RUN(boundary_surrogate_start);
  RUN(boundary_first_supplementary);
  RUN(boundary_max_codepoint);
  RUN(boundary_above_max);

  printf("\nSupplementary Plane:\n");
  RUN(supplementary_aegean_separator);
  RUN(supplementary_musical_symbol);

  printf("\nRange-Gap Verification:\n");
  RUN(gap_feminine_ordinal);
  RUN(gap_soft_hyphen);
  RUN(gap_micro_sign);

  printf("\nOut-of-Range:\n");
  RUN(out_of_range);

  printf("\nExhaustive / Compatibility:\n");
  RUN(ascii_exhaustive);
  RUN(ispunct_compatibility);
  RUN(fast_path_matches_table);
  RUN(whitespace_punct_mutual_exclusion);

  printf("\ngstr_is_whitespace_cp:\n");
  RUN(whitespace_cp_space);
  RUN(whitespace_cp_tab);
  RUN(whitespace_cp_lf);
  RUN(whitespace_cp_nel);
  RUN(whitespace_cp_nbsp);
  RUN(whitespace_cp_ideographic_space);
  RUN(whitespace_cp_non_whitespace_letter);
  RUN(whitespace_cp_non_whitespace_punct);
  RUN(whitespace_cp_non_whitespace_nul);
  RUN(whitespace_cp_matches_grapheme_variant);

  printf("\n----------------------------------------\n");
  printf("Tests passed: %d\n", tests_passed);
  printf("Tests failed: %d\n", tests_failed);
  printf("----------------------------------------\n");

  return tests_failed > 0 ? 1 : 0;
}
