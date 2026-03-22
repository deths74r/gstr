/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * cursor_walk.c - Interactive grapheme cluster cursor walk tool
 *
 * Usage:
 *   ./tools/cursor_walk            # Interactive mode (arrow keys)
 *   ./tools/cursor_walk --verify   # Batch verification mode
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
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
    int         is_invalid;
};

/* ============================================================================
 * Curated Test Strings
 * ============================================================================ */

/* sizeof("...") includes the NUL terminator, so subtract 1 */
#define TS(label, data, invalid) {label, data, sizeof(data) - 1, invalid}

static const struct test_string TEST_STRINGS[] = {
    /* ZWJ Emoji Sequences */
    TS("ZWJ: family MWG",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7",
     0),
    TS("ZWJ: family MWGB",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7\xE2\x80\x8D\xF0\x9F\x91\xA6",
     0),
    TS("ZWJ: person + laptop",
     "\xF0\x9F\xA7\x91\xE2\x80\x8D\xF0\x9F\x92\xBB",
     0),

    /* Flag Sequences (Regional Indicators) */
    TS("Flags: US JP DE",
     "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
     "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"
     "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA",
     0),
    TS("Flags: odd RI count (5)",
     "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"
     "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"
     "\xF0\x9F\x87\xA9",
     0),
    TS("Flags: single RI",
     "\xF0\x9F\x87\xBA",
     0),

    /* Keycap Sequences */
    TS("Keycaps: #1*",
     "#\xEF\xB8\x8F\xE2\x83\xA3"
     "1\xEF\xB8\x8F\xE2\x83\xA3"
     "*\xEF\xB8\x8F\xE2\x83\xA3",
     0),

    /* Indic Conjunct (GB9c) */
    TS("Devanagari: KA+VIRAMA+KA",
     "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\x95",
     0),
    TS("Devanagari: triple conjunct",
     "\xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7\xE0\xA5\x8D\xE0\xA4\xA4",
     0),
    TS("Bengali: ksha",
     "\xE0\xA6\x95\xE0\xA7\x8D\xE0\xA6\xB7",
     0),

    /* Hangul Jamo */
    TS("Hangul L+V+T",
     "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8",
     0),
    TS("Hangul LV+T (precomposed)",
     "\xEA\xB0\x80\xE1\x86\xA8",
     0),

    /* Stacked Combining Marks */
    TS("Combining: e + 5 accents",
     "e\xCC\x81\xCC\x81\xCC\x81\xCC\x81\xCC\x81",
     0),
    TS("Combining: mixed marks",
     "e\xCC\x81\xCC\xA7\xCC\x88",
     0),

    /* CR/LF */
    TS("CRLF sequence", "\r\n", 0),
    TS("CR alone then a then LF", "\r" "a\n", 0),

    /* Mixed Scripts */
    TS("Mixed: combining + CJK + emoji",
     "e\xCC\x81\xE4\xB8\xAD\xF0\x9F\x98\x80",
     0),

    /* Variation Selectors */
    TS("VS15: text heart", "\xE2\x9D\xA4\xEF\xB8\x8E", 0),
    TS("VS16: emoji heart", "\xE2\x9D\xA4\xEF\xB8\x8F", 0),

    /* Edge Cases */
    TS("Single ASCII", "a", 0),
    TS("Single emoji", "\xF0\x9F\x98\x80", 0),
    TS("Prepend + base", "\xD8\x80\xD8\xA8", 0),
    TS("Emoji + skin tone", "\xF0\x9F\x91\x8B\xF0\x9F\x8F\xBD", 0),

    /* Invalid UTF-8 */
    TS("Invalid: lone continuation", "\x80", 1),
    TS("Invalid: truncated 2-byte", "\xC3", 1),
    TS("Invalid: overlong NUL", "\xC0\x80", 1),
    TS("Invalid: surrogate", "\xED\xA0\x80", 1),
    TS("Invalid: mixed valid+invalid", "Hi\xFEok", 1),

    /* Full sentence */
    TS("Full sentence",
     "Hello \xF0\x9F\x91\x8B\xF0\x9F\x8C\x8D world! \xE2\x9C\xA8",
     0),

    /* ====== Longer mixed strings ====== */

    /* Chat message with emoji reactions */
    TS("Chat: emoji + CJK + flags",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x92\xBB "  /* man+laptop ZWJ */
     "\xE4\xBB\x8A\xE6\x97\xA5\xE3\x81\xAF"            /* 今日は */
     " \xF0\x9F\x87\xAF\xF0\x9F\x87\xB5"                /* JP flag */
     " #\xEF\xB8\x8F\xE2\x83\xA3"                       /* keycap # */
     " \xE2\x9D\xA4\xEF\xB8\x8F"                        /* red heart */
     "!",
     0),

    /* Hindi + Devanagari conjuncts + emoji */
    TS("Hindi sentence with conjuncts",
     "\xE0\xA4\xA8\xE0\xA4\xAE"                          /* na ma */
     "\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87" /* s+virama+te (conjunct) */
     " \xF0\x9F\x99\x8F"                                  /* folded hands */
     " \xE0\xA4\xB6\xE0\xA5\x81\xE0\xA4\xAD"             /* shu bha */
     " \xE0\xA4\x95\xE0\xA5\x8D\xE0\xA4\xB7"             /* ksha conjunct */
     "\xE0\xA4\xBE",                                      /* aa vowel sign */
     0),

    /* Korean mixed Jamo + precomposed + Latin */
    TS("Korean: Jamo + precomposed + ASCII",
     "\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xA8"  /* Jamo L+V+T = 각 */
     "\xEA\xB0\x80"                            /* precomposed 가 (LV) */
     "\xEA\xB0\x81"                            /* precomposed 각 (LVT) */
     " = gak ga gag",
     0),

    /* Arabic RTL + Latin + emoji */
    TS("Arabic + Latin + emoji",
     "\xD9\x85\xD8\xB1\xD8\xAD\xD8\xA8\xD8\xA7"  /* مرحبا */
     " Hello "
     "\xF0\x9F\x91\x8B\xF0\x9F\x8F\xBD"            /* waving hand + skin tone */
     " \xF0\x9F\x87\xBA\xF0\x9F\x87\xB8"            /* US flag */
     "\xF0\x9F\x87\xB8\xF0\x9F\x87\xA6",            /* SA flag */
     0),

    /* Thai with combining marks (no word spaces) */
    TS("Thai: combining vowels + tone marks",
     "\xE0\xB8\xAA\xE0\xB8\xA7\xE0\xB8\xB1"  /* สวั */
     "\xE0\xB8\xAA\xE0\xB8\x94\xE0\xB8\xB5"  /* สดี */
     "\xE0\xB8\x84\xE0\xB8\xA3\xE0\xB8\xB1"  /* ครั */
     "\xE0\xB8\x9A",                           /* บ */
     0),

    /* Stress: lots of emoji types in one string */
    TS("Emoji soup: ZWJ + flag + keycap + skin + VS16",
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7"  /* family MWG */
     "\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7"                                              /* GB flag */
     "1\xEF\xB8\x8F\xE2\x83\xA3"                                                     /* keycap 1 */
     "\xF0\x9F\x91\x8D\xF0\x9F\x8F\xBF"                                              /* thumbs up dark */
     "\xE2\x98\x95\xEF\xB8\x8F"                                                      /* coffee VS16 */
     "\xE2\x9D\xA4\xEF\xB8\x8E"                                                      /* heart VS15 (text) */
     "\xF0\x9F\x98\x80",                                                              /* grinning face */
     0),

    /* Combining mark stress: multiple diacritics on Latin */
    TS("Zalgo-lite: stacked diacritics",
     "H\xCC\x81\xCC\xA7"                      /* H + acute + cedilla */
     "e\xCC\x88\xCC\x83"                      /* e + diaeresis + tilde */
     "l\xCC\xB2\xCC\xB3"                      /* l + underline + double underline */
     "l\xCC\x8A"                               /* l + ring above */
     "o\xCC\x81\xCC\x80\xCC\x82\xCC\x83",    /* o + acute + grave + circumflex + tilde */
     0),

    /* Realistic terminal line: path + status + emoji */
    TS("Terminal: path + status + indicator",
     "\xE2\x9C\x93 "                              /* checkmark + space */
     "src/\xE4\xB8\xBB\xE8\xA6\x81.c"            /* src/主要.c */
     " [200 OK] "
     "\xF0\x9F\x9F\xA2"                           /* green circle */
     " 42ms "
     "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8",         /* US flag */
     0),

    /* Everything at once: CRLF + control + wide + combining + emoji + Hangul */
    TS("Kitchen sink",
     "A"
     "\xCC\x81"                                    /* A + combining acute */
     "\xE4\xB8\xAD"                               /* CJK 中 */
     "\xEA\xB0\x80\xE1\x86\xA8"                   /* Hangul LV+T */
     "\r\n"                                        /* CRLF (1 grapheme) */
     "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x92\xBB" /* man+laptop ZWJ */
     " "
     "\xE2\x9D\xA4\xEF\xB8\x8F"                  /* red heart */
     "!"
     "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA",         /* DE flag */
     0),
};

#define NUM_STRINGS (sizeof(TEST_STRINGS) / sizeof(TEST_STRINGS[0]))
#define MAX_BOUNDARIES 1024

/* ============================================================================
 * Terminal handling
 * ============================================================================ */

static struct termios orig_termios;
static int raw_mode_active = 0;

static void disable_raw_mode(void) {
    if (raw_mode_active) {
        /* Leave alternate screen */
        write(STDOUT_FILENO, "\033[?1049l", 8);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode_active = 0;
    }
}

static void signal_handler(int sig) {
    disable_raw_mode();
    _exit(128 + sig);
}

static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode_active = 1;

    /* Enter alternate screen */
    write(STDOUT_FILENO, "\033[?1049h", 8);
}

enum key_code {
    KEY_NONE = 0,
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN,
    KEY_Q, KEY_B, KEY_P, KEY_V,
    KEY_HOME, KEY_END,
};

static enum key_code read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_NONE;

    if (c == 'q' || c == 'Q') return KEY_Q;
    if (c == 'b' || c == 'B') return KEY_B;
    if (c == 'p' || c == 'P') return KEY_P;
    if (c == 'v' || c == 'V') return KEY_V;
    if (c == '0') return KEY_HOME;
    if (c == '$') return KEY_END;

    if (c == '\033') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_NONE;
        if (seq[0] != '[') return KEY_NONE;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_NONE;
        switch (seq[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
        }
    }
    return KEY_NONE;
}

static void get_terminal_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
    } else {
        *rows = 24;
        *cols = 80;
    }
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

struct cursor_state {
    size_t string_idx;
    size_t byte_offset;    /* start of current grapheme */
    size_t grapheme_idx;
    int show_bytes;
    int show_props;
    int show_verify;
};

static void move_to(int row, int col) {
    printf("\033[%d;%dH", row, col);
}

static void clear_screen(void) {
    printf("\033[2J\033[H");
}

static void draw_screen(struct cursor_state *st) {
    int rows, cols;
    get_terminal_size(&rows, &cols);

    const struct test_string *ts = &TEST_STRINGS[st->string_idx];
    size_t cur_start = st->byte_offset;
    size_t cur_end = (ts->length > 0)
        ? utf8_next_grapheme(ts->data, ts->length, cur_start)
        : 0;

    clear_screen();

    /* Header */
    move_to(1, 1);
    printf("\033[1mgstr-cursor-walk\033[0m | Unicode %s | String %zu/%zu: \033[1m%s\033[0m",
           GSTR_UNICODE_VERSION, st->string_idx + 1, (size_t)NUM_STRINGS, ts->label);

    /* Key help */
    move_to(2, 1);
    printf("\033[2m\xe2\x86\x90/\xe2\x86\x92:grapheme  \xe2\x86\x91/\xe2\x86\x93:string  "
           "[b]ytes [p]rops [v]erify  [q]uit  [0]start [$]end\033[0m");

    if (ts->length == 0) {
        move_to(4, 1);
        printf("\033[2m(empty string)\033[0m");
        goto status_bar;
    }

    /* Rendered string with highlighted current grapheme */
    move_to(4, 1);
    printf("  ");
    size_t off = 0;
    while (off < ts->length) {
        size_t next = utf8_next_grapheme(ts->data, ts->length, off);
        size_t glen = next - off;

        if (off == cur_start) {
            printf("\033[7m"); /* reverse video */
        }

        /* Check if it's a control/invisible character */
        uint32_t first_cp;
        utf8_decode(ts->data + off, glen, &first_cp);
        if (first_cp < 0x20 || first_cp == 0x7F ||
            (first_cp >= 0x80 && first_cp <= 0x9F) ||
            first_cp == 0xFFFD) {
            /* Show as placeholder */
            printf("\033[2m<U+%04X>\033[22m", first_cp);
        } else {
            fwrite(ts->data + off, 1, glen, stdout);
        }

        if (off == cur_start) {
            printf("\033[0m"); /* reset */
        }

        off = next;
    }

    /* Cluster detail panel */
    size_t g_byte_len = cur_end - cur_start;
    move_to(6, 1);
    printf("  Grapheme index:  %zu", st->grapheme_idx);
    move_to(7, 1);
    printf("  Byte offset:     %zu..%zu (%zu bytes)", cur_start, cur_end, g_byte_len);
    move_to(8, 1);
    printf("  Display width:   %zu col%s",
           gstr_grapheme_width(ts->data, ts->length, cur_start, cur_end),
           gstr_grapheme_width(ts->data, ts->length, cur_start, cur_end) == 1 ? "" : "s");

    /* Count codepoints in this cluster */
    size_t cp_count = 0;
    off = cur_start;
    while (off < cur_end) {
        uint32_t cp;
        size_t cb = utf8_decode(ts->data + off, cur_end - off, &cp);
        if (cb == 0) break;
        cp_count++;
        off += cb;
    }
    move_to(9, 1);
    printf("  Codepoint count: %zu", cp_count);

    /* Codepoint breakdown */
    move_to(11, 1);
    printf("  Codepoints:");
    off = cur_start;
    size_t cp_idx = 0;
    int detail_row = 12;
    while (off < cur_end && detail_row < rows - 4) {
        uint32_t cp;
        size_t cb = utf8_decode(ts->data + off, cur_end - off, &cp);
        if (cb == 0) break;
        enum gcb_property prop = get_gcb(cp);
        const char *prop_name = ((size_t)prop < sizeof(GCB_NAMES)/sizeof(GCB_NAMES[0]))
            ? GCB_NAMES[prop] : "?";

        move_to(detail_row, 1);
        printf("    [%zu] U+%04X  %-18s  width=%d", cp_idx, cp, prop_name,
               utf8_cpwidth(cp));

        if (st->show_bytes) {
            printf("  bytes:");
            for (size_t b = 0; b < cb; b++)
                printf(" %02X", (unsigned char)ts->data[off + b]);
        }

        off += cb;
        cp_idx++;
        detail_row++;
    }

    /* Verify overlay */
    if (st->show_verify && cur_end <= ts->length) {
        detail_row++;
        move_to(detail_row, 1);
        size_t roundtrip = utf8_prev_grapheme(ts->data, ts->length,
            utf8_next_grapheme(ts->data, ts->length, cur_start));
        if (roundtrip == cur_start) {
            printf("  \033[32m\xe2\x9c\x93 fwd->bwd roundtrip OK (offset %zu)\033[0m", roundtrip);
        } else {
            printf("  \033[31m\xe2\x9c\x97 fwd->bwd MISMATCH: expected %zu, got %zu\033[0m",
                   cur_start, roundtrip);
        }
    }

status_bar:
    /* Status bar */
    move_to(rows, 1);
    printf("\033[7m");
    size_t total_graphemes = gstrlen(ts->data, ts->length);
    char status[256];
    snprintf(status, sizeof(status),
             " Grapheme %zu/%zu | %zu bytes | %s",
             ts->length > 0 ? st->grapheme_idx + 1 : (size_t)0,
             total_graphemes, ts->length,
             ts->is_invalid ? "INVALID UTF-8" : "valid UTF-8");
    printf("%-*s", cols, status);
    printf("\033[0m");

    fflush(stdout);
}

/* ============================================================================
 * Navigation
 * ============================================================================ */

static void advance_grapheme(struct cursor_state *st) {
    const struct test_string *ts = &TEST_STRINGS[st->string_idx];
    if (ts->length == 0) return;
    size_t next = utf8_next_grapheme(ts->data, ts->length, st->byte_offset);
    if (next < ts->length) {
        st->byte_offset = next;
        st->grapheme_idx++;
    }
}

static void retreat_grapheme(struct cursor_state *st) {
    const struct test_string *ts = &TEST_STRINGS[st->string_idx];
    if (ts->length == 0 || st->byte_offset == 0) return;
    st->byte_offset = utf8_prev_grapheme(ts->data, ts->length, st->byte_offset);
    if (st->grapheme_idx > 0) st->grapheme_idx--;
}

static void next_string(struct cursor_state *st) {
    st->string_idx = (st->string_idx + 1) % NUM_STRINGS;
    st->byte_offset = 0;
    st->grapheme_idx = 0;
}

static void prev_string(struct cursor_state *st) {
    st->string_idx = (st->string_idx + NUM_STRINGS - 1) % NUM_STRINGS;
    st->byte_offset = 0;
    st->grapheme_idx = 0;
}

static void jump_start(struct cursor_state *st) {
    st->byte_offset = 0;
    st->grapheme_idx = 0;
}

static void jump_end(struct cursor_state *st) {
    const struct test_string *ts = &TEST_STRINGS[st->string_idx];
    if (ts->length == 0) return;
    /* Walk forward to find last grapheme */
    size_t off = 0, prev_off = 0, idx = 0;
    while (off < ts->length) {
        prev_off = off;
        off = utf8_next_grapheme(ts->data, ts->length, off);
        if (off < ts->length) idx++;
    }
    st->byte_offset = prev_off;
    st->grapheme_idx = idx;
}

/* ============================================================================
 * Interactive mode
 * ============================================================================ */

static void run_interactive(void) {
    if (!isatty(STDIN_FILENO)) {
        fprintf(stderr, "cursor_walk: interactive mode requires a terminal\n");
        exit(1);
    }

    enable_raw_mode();

    struct cursor_state st = {0, 0, 0, 0, 0, 0};

    for (;;) {
        draw_screen(&st);
        enum key_code key = read_key();
        switch (key) {
            case KEY_RIGHT: advance_grapheme(&st); break;
            case KEY_LEFT:  retreat_grapheme(&st); break;
            case KEY_DOWN:  next_string(&st); break;
            case KEY_UP:    prev_string(&st); break;
            case KEY_Q:     return;
            case KEY_B:     st.show_bytes = !st.show_bytes; break;
            case KEY_P:     st.show_props = !st.show_props; break;
            case KEY_V:     st.show_verify = !st.show_verify; break;
            case KEY_HOME:  jump_start(&st); break;
            case KEY_END:   jump_end(&st); break;
            default: break;
        }
    }
}

/* ============================================================================
 * Verify mode: check forward/backward consistency
 * ============================================================================ */

static int verify_string(const struct test_string *ts, int string_idx) {
    if (ts->length == 0) {
        printf("[SKIP] String %2d: %-40s (empty)\n", string_idx, ts->label);
        return 0;
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

    /* Backward pass */
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
            if (fwd[i] != bwd[i]) { ok = 0; break; }
        }
    }

    /* Forward-backward roundtrip */
    if (ok) {
        for (size_t i = 0; i + 1 < fwd_count; i++) {
            size_t next = utf8_next_grapheme(ts->data, ts->length, fwd[i]);
            size_t back = utf8_prev_grapheme(ts->data, ts->length, next);
            if (back != fwd[i]) { ok = 0; break; }
        }
    }

    size_t graphemes = fwd_count > 0 ? fwd_count - 1 : 0;

    if (ok) {
        printf("[PASS] String %2d: %-40s (%zu bytes, %zu graphemes)\n",
               string_idx, ts->label, ts->length, graphemes);
        return 0;
    } else if (ts->is_invalid) {
        printf("[WARN] String %2d: %-40s boundary mismatch (invalid UTF-8)\n",
               string_idx, ts->label);
        return 0;
    } else {
        printf("[FAIL] String %2d: %-40s boundary mismatch!\n",
               string_idx, ts->label);
        printf("       fwd(%zu):", fwd_count);
        for (size_t i = 0; i < fwd_count && i < 20; i++) printf(" %zu", fwd[i]);
        printf("\n       bwd(%zu):", bwd_count);
        for (size_t i = 0; i < bwd_count && i < 20; i++) printf(" %zu", bwd[i]);
        printf("\n");
        return 1;
    }
}

static int run_verify(void) {
    printf("gstr-cursor-walk --verify | Unicode %s | %zu test strings\n",
           GSTR_UNICODE_VERSION, (size_t)NUM_STRINGS);
    printf("============================================================\n");

    int failures = 0, passed = 0;
    for (size_t i = 0; i < NUM_STRINGS; i++) {
        int result = verify_string(&TEST_STRINGS[i], (int)(i + 1));
        if (result) failures++;
        else passed++;
    }

    printf("============================================================\n");
    printf("Results: %d passed, %d failed\n", passed, failures);
    return failures > 0 ? 1 : 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verify") == 0)
            return run_verify();
    }
    run_interactive();
    return 0;
}
