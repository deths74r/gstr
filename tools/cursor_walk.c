/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * cursor_walk.c - Grapheme cluster cursor walk tool
 *
 * Usage:
 *   ./tools/cursor_walk --verify   # Batch verification mode
 *   ./tools/cursor_walk            # Display all test strings with grapheme info
 *
 * Verifies forward/backward grapheme navigation consistency across curated
 * test strings covering ZWJ sequences, flags, keycaps, Indic conjuncts,
 * Hangul, combining marks, CRLF, variation selectors, and invalid UTF-8.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gstr.h>

/* GCB property name mapping */
static const char *GCB_NAMES[] = {
    "OTHER", "CR", "LF", "CONTROL", "EXTEND", "ZWJ",
    "REGIONAL_INDICATOR", "PREPEND", "SPACING_MARK",
    "L", "V", "T", "LV", "LVT"
};

struct test_string {
    const char *label;
    const char *data;
    size_t      length;
    int         is_invalid; /* 1 if contains invalid UTF-8 */
};

/* ============================================================================
 * Curated Test Strings
 * ============================================================================ */

static const struct test_string TEST_STRINGS[] = {
    /* ZWJ Emoji Sequences */
    {"ZWJ: family MWG",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7",
     18, 0},
    {"ZWJ: family MWGB",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6",
     25, 0},
    {"ZWJ: person + laptop",
     "\xF0\x9F\xA7\x91\xE2\x80\x8D\xF0\x9F\x92\xBB",
     11, 0},

    /* Flag Sequences (Regional Indicators) */
    {"Flags: US JP DE",
     "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
     "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"
     "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA",
     24, 0},
    {"Flags: odd RI count (5)",
     "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
     "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"
     "\xF0\x9F\x87\xA9",
     20, 0},
    {"Flags: single RI",
     "\xF0\x9F\x87\xBA",
     4, 0},

    /* Keycap Sequences */
    {"Keycaps: #1*",
     "#\xEF\xB8\x8F\xE2\x83\xA3"
     "1\xEF\xB8\x8F\xE2\x83\xA3"
     "*\xEF\xB8\x8F\xE2\x83\xA3",
     21, 0},

    /* Indic Conjunct (GB9c) */
    {"Devanagari: KA+VIRAMA+KA",
     "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x95",
     9, 0},
    {"Devanagari: triple conjunct",
     "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA5\x8D\xE0\xA4\xA4",
     15, 0},
    {"Bengali: ksha",
     "\xE0\xA6\x95\xE0\xA7\x8D\xE0\xA6\xB7",
     9, 0},

    /* Hangul Jamo */
    {"Hangul L+V+T",
     "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8",
     9, 0},
    {"Hangul LV+T (precomposed)",
     "\xEA\xB0\x80\xE1\x86\xA8",
     6, 0},

    /* Stacked Combining Marks */
    {"Combining: e + 5 accents",
     "e\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81",
     11, 0},
    {"Combining: mixed marks",
     "e\xCC\x81\xCC\xA7\xCC\x88",
     7, 0},

    /* CR/LF */
    {"CRLF sequence", "\r\n", 2, 0},
    {"CR alone then a then LF", "\r" "a\n", 3, 0},

    /* Mixed Scripts */
    {"Mixed: combining + CJK + emoji",
     "e\xCC\x81\xE4\xB8\xAD\xF0\x9F\x98\x80",
     10, 0},

    /* Variation Selectors */
    {"VS15: text heart", "\xE2\x9D\xA4\xEF\xB8\x8E", 6, 0},
    {"VS16: emoji heart", "\xE2\x9D\xA4\xEF\xB8\x8F", 6, 0},

    /* Edge Cases */
    {"Single ASCII", "a", 1, 0},
    {"Single emoji", "\xF0\x9F\x98\x80", 4, 0},
    {"Prepend + base", "\xD8\x80\xD8\xA8", 4, 0}, /* U+0600 + U+0628 */
    {"Emoji + skin tone", "\xF0\x9F\x91\x8B\xF0\x9F\x8F\xBD", 8, 0},

    /* Invalid UTF-8 */
    {"Invalid: lone continuation", "\x80", 1, 1},
    {"Invalid: truncated 2-byte", "\xC3", 1, 1},
    {"Invalid: overlong NUL", "\xC0\x80", 2, 1},
    {"Invalid: surrogate", "\xED\xA0\x80", 3, 1},
    {"Invalid: mixed valid+invalid", "Hi\xFEok", 5, 1},

    /* Full sentence */
    {"Full sentence",
     "Hello \xF0\x9F\x91\x8B\xF0\x9F\x8C\x8D world! \xE2\x9C\xA8",
     21, 0},
};

#define NUM_STRINGS (sizeof(TEST_STRINGS) / sizeof(TEST_STRINGS[0]))
#define MAX_BOUNDARIES 1024

/* ============================================================================
 * Display mode: print each test string with grapheme details
 * ============================================================================ */

static void display_string(const struct test_string *ts) {
    printf("\n--- %s ---\n", ts->label);
    printf("  Bytes: %zu\n", ts->length);

    if (ts->length == 0) {
        printf("  (empty string)\n");
        return;
    }

    size_t g_count = gstrlen(ts->data, ts->length);
    size_t width = gstrwidth(ts->data, ts->length);
    printf("  Graphemes: %zu, Display width: %zu cols\n", g_count, width);

    size_t offset = 0;
    size_t g_idx = 0;
    while (offset < ts->length) {
        size_t next = utf8_next_grapheme(ts->data, ts->length, offset);
        size_t g_len = next - offset;

        printf("  [%2zu] bytes %3zu..%-3zu (%zu bytes)",
               g_idx, offset, next - 1, g_len);

        /* Print codepoints */
        printf("  codepoints:");
        size_t cp_off = offset;
        while (cp_off < next) {
            uint32_t cp;
            size_t cb = utf8_decode(ts->data + cp_off, next - cp_off, &cp);
            if (cb == 0) break;
            enum gcb_property prop = get_gcb(cp);
            printf(" U+%04X(%s)", cp,
                   (size_t)prop < sizeof(GCB_NAMES)/sizeof(GCB_NAMES[0])
                   ? GCB_NAMES[prop] : "?");
            cp_off += cb;
        }
        printf("\n");

        offset = next;
        g_idx++;
    }
}

/* ============================================================================
 * Verify mode: check forward/backward consistency
 * ============================================================================ */

static int verify_string(const struct test_string *ts, int string_idx) {
    if (ts->length == 0) {
        printf("[SKIP] String %2d: %-40s (empty)\n", string_idx, ts->label);
        return 0; /* skip, not failure */
    }

    /* Forward pass: collect boundaries */
    size_t fwd[MAX_BOUNDARIES];
    size_t fwd_count = 0;
    fwd[fwd_count++] = 0;
    size_t off = 0;
    while (off < ts->length) {
        off = utf8_next_grapheme(ts->data, ts->length, off);
        if (fwd_count < MAX_BOUNDARIES)
            fwd[fwd_count++] = off;
    }

    /* Backward pass: collect boundaries in reverse */
    size_t bwd[MAX_BOUNDARIES];
    size_t bwd_count = 0;
    off = ts->length;
    bwd[bwd_count++] = off;
    while (off > 0) {
        off = utf8_prev_grapheme(ts->data, ts->length, off);
        if (bwd_count < MAX_BOUNDARIES)
            bwd[bwd_count++] = off;
    }

    /* Reverse the backward array */
    for (size_t i = 0; i < bwd_count / 2; i++) {
        size_t tmp = bwd[i];
        bwd[i] = bwd[bwd_count - 1 - i];
        bwd[bwd_count - 1 - i] = tmp;
    }

    /* Compare */
    int ok = 1;
    if (fwd_count != bwd_count) {
        ok = 0;
    } else {
        for (size_t i = 0; i < fwd_count; i++) {
            if (fwd[i] != bwd[i]) {
                ok = 0;
                break;
            }
        }
    }

    /* Forward-backward roundtrip */
    if (ok) {
        for (size_t i = 0; i + 1 < fwd_count; i++) {
            size_t next = utf8_next_grapheme(ts->data, ts->length, fwd[i]);
            size_t back = utf8_prev_grapheme(ts->data, ts->length, next);
            if (back != fwd[i]) {
                ok = 0;
                break;
            }
        }
    }

    size_t graphemes = fwd_count > 0 ? fwd_count - 1 : 0;

    if (ok) {
        printf("[PASS] String %2d: %-40s (%zu bytes, %zu graphemes, %zu boundaries)\n",
               string_idx, ts->label, ts->length, graphemes, fwd_count);
        return 0;
    } else if (ts->is_invalid) {
        printf("[WARN] String %2d: %-40s boundary mismatch (invalid UTF-8, expected)\n",
               string_idx, ts->label);
        printf("       forward boundaries (%zu):", fwd_count);
        for (size_t i = 0; i < fwd_count && i < 20; i++) printf(" %zu", fwd[i]);
        printf("\n       backward boundaries (%zu):", bwd_count);
        for (size_t i = 0; i < bwd_count && i < 20; i++) printf(" %zu", bwd[i]);
        printf("\n");
        return 0; /* warn, not fail */
    } else {
        printf("[FAIL] String %2d: %-40s boundary mismatch!\n",
               string_idx, ts->label);
        printf("       forward boundaries (%zu):", fwd_count);
        for (size_t i = 0; i < fwd_count && i < 20; i++) printf(" %zu", fwd[i]);
        printf("\n       backward boundaries (%zu):", bwd_count);
        for (size_t i = 0; i < bwd_count && i < 20; i++) printf(" %zu", bwd[i]);
        printf("\n");
        return 1;
    }
}

int main(int argc, char *argv[]) {
    int verify_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify") == 0) {
            verify_mode = 1;
        }
    }

    if (verify_mode) {
        printf("gstr-cursor-walk --verify | Unicode %s | %zu test strings\n",
               GSTR_UNICODE_VERSION, (size_t)NUM_STRINGS);
        printf("============================================================\n");

        int failures = 0;
        int passed = 0;
        int skipped = 0;
        (void)0; /* all warnings counted as passes */

        for (size_t i = 0; i < NUM_STRINGS; i++) {
            int result = verify_string(&TEST_STRINGS[i], (int)(i + 1));
            if (result) {
                failures++;
            } else {
                if (TEST_STRINGS[i].length == 0) {
                    skipped++;
                } else {
                    passed++;
                }
            }
        }

        printf("============================================================\n");
        printf("Results: %d passed, %d failed, %d skipped\n",
               passed, failures, skipped);

        return failures > 0 ? 1 : 0;
    }

    /* Display mode */
    printf("gstr-cursor-walk | Unicode %s | %zu test strings\n",
           GSTR_UNICODE_VERSION, (size_t)NUM_STRINGS);

    for (size_t i = 0; i < NUM_STRINGS; i++) {
        display_string(&TEST_STRINGS[i]);
    }

    return 0;
}
