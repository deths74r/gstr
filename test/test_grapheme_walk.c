/*
 * test_grapheme_walk.c - Reproduce grapheme cursor skipping bug
 *
 * Walks through a mixed-script string grapheme by grapheme using
 * utf8_next_grapheme() and prints each grapheme's byte range,
 * decoded codepoints, and the raw bytes.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <gstr.h>

/* Print a single codepoint as U+XXXX */
static void print_cp(uint32_t cp) {
  if (cp < 0x10000)
    printf("U+%04X", cp);
  else
    printf("U+%05X", cp);
}

/* Walk a string grapheme by grapheme, printing everything */
static void walk_graphemes(const char *label, const char *text) {
  int len = (int)strlen(text);
  int offset = 0;
  int grapheme = 0;

  printf("=== %s ===\n", label);
  printf("Total bytes: %d\n", len);
  printf("gstrlen reports: %zu graphemes\n\n", gstrlen(text, len));

  while (offset < len) {
    int next = utf8_next_grapheme(text, len, offset);
    int span = next - offset;

    /* Print grapheme index and byte range */
    printf("  [%2d] bytes %3d..%-3d (%2d bytes)  \"",
           grapheme, offset, next - 1, span);

    /* Print the raw grapheme text */
    fwrite(text + offset, 1, span, stdout);
    printf("\"");

    /* Decode and print each codepoint in this grapheme */
    printf("  codepoints:");
    int cp_offset = offset;
    while (cp_offset < next) {
      uint32_t cp;
      int cp_bytes = utf8_decode(text + cp_offset, len - cp_offset, &cp);
      printf(" ");
      print_cp(cp);
      if (cp == 0xFFFD) {
        printf("(REPLACEMENT!)");
      }
      if (cp_bytes == 0) {
        printf("(ZERO-ADVANCE!)");
        break;  /* prevent infinite loop */
      }
      cp_offset += cp_bytes;
    }
    printf("\n");

    /* Sanity checks */
    if (next <= offset) {
      printf("  *** BUG: utf8_next_grapheme returned %d (not advancing from %d)!\n",
             next, offset);
      break;
    }
    if (span > 50) {
      printf("  *** BUG: grapheme spans %d bytes — likely skipping multiple graphemes\n",
             span);
    }

    offset = next;
    grapheme++;
  }
  printf("  --- walked %d graphemes ---\n\n", grapheme);
}

/* Verify round-trip: walk forward, then verify gstroff/gstrat agree */
static void verify_indexing(const char *label, const char *text) {
  int len = (int)strlen(text);
  size_t count = gstrlen(text, len);

  printf("=== Verify indexing: %s ===\n", label);
  printf("gstrlen = %zu\n", count);

  /* Walk with utf8_next_grapheme and collect offsets */
  int walk_offsets[256];
  int walk_count = 0;
  int offset = 0;
  while (offset < len && walk_count < 256) {
    walk_offsets[walk_count++] = offset;
    int next = utf8_next_grapheme(text, len, offset);
    if (next <= offset) break;
    offset = next;
  }

  printf("utf8_next_grapheme walk: %d graphemes\n", walk_count);

  if ((size_t)walk_count != count) {
    printf("  *** MISMATCH: gstrlen=%zu vs walk=%d\n", count, walk_count);
  }

  /* Check gstroff for each index */
  int mismatches = 0;
  for (int i = 0; i < walk_count && (size_t)i < count; i++) {
    size_t goff = gstroff(text, len, i);
    if ((int)goff != walk_offsets[i]) {
      printf("  gstroff(%d) = %zu, expected %d  *** MISMATCH\n",
             i, goff, walk_offsets[i]);
      mismatches++;
    }
  }

  if (mismatches == 0) {
    printf("  All %d gstroff indices match walk offsets.\n", walk_count);
  } else {
    printf("  *** %d mismatches found!\n", mismatches);
  }
  printf("\n");
}

/* Walk every line of a file */
static void walk_file(const char *path) {
FILE *f = fopen(path, "r");
if (!f) {
printf("*** Cannot open %s\n", path);
return;
}

printf("=== Walking file: %s ===\n\n", path);

char line[4096];
int lineno = 0;
int total_bugs = 0;

while (fgets(line, sizeof(line), f)) {
lineno++;
/* Strip trailing newline */
size_t slen = strlen(line);
if (slen > 0 && line[slen - 1] == '\n') {
line[--slen] = '\0';
}
if (slen > 0 && line[slen - 1] == '\r') {
line[--slen] = '\0';
}

int len = (int)slen;
size_t glen = gstrlen(line, len);

/* Walk with utf8_next_grapheme */
int walk_count = 0;
int offset = 0;
int has_replacement = 0;
int has_skip = 0;

while (offset < len) {
int next = utf8_next_grapheme(line, len, offset);
int span = next - offset;

/* Check for replacement characters in this grapheme */
int cp_off = offset;
while (cp_off < next) {
uint32_t cp;
int cb = utf8_decode(line + cp_off, len - cp_off, &cp);
if (cp == 0xFFFD) has_replacement = 1;
if (cb == 0) break;
cp_off += cb;
}

/* Check for suspiciously large graphemes (>30 bytes) */
if (span > 30) has_skip = 1;

if (next <= offset) {
printf(" L%d: *** STUCK at offset %d\n", lineno, offset);
total_bugs++;
break;
}

walk_count++;
offset = next;
}

/* Check gstrlen vs walk agreement */
int mismatch = ((size_t)walk_count != glen);

/* Only print lines with problems, or every line in verbose mode */
if (mismatch || has_replacement || has_skip) {
printf(" L%-3d graphemes: walk=%d gstrlen=%zu", lineno, walk_count, glen);
if (mismatch) printf(" *** COUNT MISMATCH");
if (has_replacement) printf(" [has U+FFFD]");
if (has_skip) printf(" [suspect skip]");
printf("\n");

/* Print the detailed walk for problem lines */
walk_graphemes("", line);
total_bugs++;
} else {
printf(" L%-3d OK %3d graphemes \"%.*s\"\n",
lineno, walk_count, len > 72 ? 72 : len, line);
}
}

fclose(f);
printf("\n--- %d lines, %d problems ---\n\n", lineno, total_bugs);
}

int main(int argc, char **argv) {
/* If a file argument is given, walk that file */
if (argc > 1) {
for (int i = 1; i < argc; i++) {
walk_file(argv[i]);
}
return 0;
}
  printf("Grapheme Walk Test\n");
  printf("==================\n\n");

  /* The user's test string */
  const char *mixed = "café résumé naïve über straße → ★ ♠ ♣ ♥ ♦ 🂡 🂢";
  walk_graphemes("Mixed script string", mixed);
  verify_indexing("Mixed script string", mixed);

  /* Decomposed forms (e + combining acute accent) */
  const char *decomposed = "cafe\xCC\x81 re\xCC\x81sume\xCC\x81 nai\xCC\x88ve";
  walk_graphemes("Decomposed accents", decomposed);
  verify_indexing("Decomposed accents", decomposed);

  /* Emoji that exercise is_extended_pictographic + ZWJ (GB11) */
  const char *family = "👨\u200D👩\u200D👧";  /* family emoji ZWJ sequence */
  walk_graphemes("Family emoji (ZWJ)", family);

  /* Should be 1 grapheme if GB11 works, 3+ if broken */
  size_t flen = gstrlen(family, strlen(family));
  if (flen != 1) {
    printf("  *** BUG: Family emoji = %zu graphemes (expected 1)\n\n", flen);
  }

  /* Emoji followed by non-emoji — is_extended_pictographic pollution test */
  const char *emoji_then_text = "😀abc";
  walk_graphemes("Emoji then ASCII", emoji_then_text);

  /* Regional indicators (flags) */
  const char *flags = "🇺🇸🇬🇧";  /* US + GB flags */
  walk_graphemes("Flags (regional indicators)", flags);

  size_t flag_count = gstrlen(flags, strlen(flags));
  if (flag_count != 2) {
    printf("  *** BUG: Two flags = %zu graphemes (expected 2)\n\n", flag_count);
  }

  /* Card characters (SMP, 4-byte UTF-8) */
  const char *cards = "🂡🂢🂣🂤🂥";
  walk_graphemes("Playing cards (SMP)", cards);

  size_t card_count = gstrlen(cards, strlen(cards));
  if (card_count != 5) {
    printf("  *** BUG: 5 cards = %zu graphemes (expected 5)\n\n", card_count);
  }

  /* Symbols that border EAW_WIDE_RANGES boundaries */
  const char *boundary = "★♠♣♥♦";  /* U+2605, U+2660, U+2663, U+2665, U+2666 */
  walk_graphemes("Card suit symbols (boundary)", boundary);

  size_t sym_count = gstrlen(boundary, strlen(boundary));
  if (sym_count != 5) {
    printf("  *** BUG: 5 symbols = %zu graphemes (expected 5)\n\n", sym_count);
  }

 /* ================================================================
* Extended_Pictographic / ZWJ / width tests.
* These verify the split tables: EAW_WIDE_RANGES for width,
* EXTENDED_PICTOGRAPHIC_RANGES for GB11 (ZWJ sequences).
* ================================================================ */

printf("=== Extended_Pictographic + ZWJ tests ===\n\n");

/* Test 1: ★ (U+2605) is NOT Extended_Pictographic per Unicode 17.0.
* GB11 does not apply, so ★+ZWJ+★ = 2 graphemes: [★+ZWJ] [★]. */
{
const char *star_zwj = "\xE2\x98\x85\xE2\x80\x8D\xE2\x98\x85";
walk_graphemes("★+ZWJ+★ (U+2605 not ExtPict)", star_zwj);
size_t n = gstrlen(star_zwj, strlen(star_zwj));
if (n != 2) {
printf(" *** BUG: ★‍★ = %zu graphemes (expected 2, ★ is not ExtPict)\n\n", n);
} else {
printf(" OK: ★‍★ = 2 graphemes (★ is not Extended_Pictographic)\n\n");
}
}

/* Test 2: ☀ (U+2600) IS Extended_Pictographic.
* GB11 applies: ☀+ZWJ+☀ = 1 grapheme. */
{
const char *sun_zwj = "\xE2\x98\x80\xE2\x80\x8D\xE2\x98\x80";
walk_graphemes("☀+ZWJ+☀ (U+2600 IS ExtPict)", sun_zwj);
size_t n = gstrlen(sun_zwj, strlen(sun_zwj));
if (n != 1) {
printf(" *** BUG: ☀‍☀ = %zu graphemes (expected 1)\n\n", n);
} else {
printf(" OK: ☀‍☀ = 1 grapheme (GB11 applies)\n\n");
}
}

/* Test 3: CJK + ZWJ + CJK — should NOT join (CJK is not ExtPict). */
{
const char *cjk_zwj = "\xE4\xB8\xAD\xE2\x80\x8D\xE4\xB8\xAD";
walk_graphemes("中+ZWJ+中 (CJK not ExtPict)", cjk_zwj);
size_t n = gstrlen(cjk_zwj, strlen(cjk_zwj));
if (n != 2) {
printf(" *** BUG: 中‍中 = %zu graphemes (expected 2)\n\n", n);
} else {
printf(" OK: 中‍中 = 2 graphemes (CJK correctly not joined)\n\n");
}
}

/* Test 4: ♠ (U+2660) IS Extended_Pictographic.
* ♠+ZWJ+♠ = 1 grapheme (GB11 applies). */
{
const char *spade_zwj = "\xE2\x99\xA0\xE2\x80\x8D\xE2\x99\xA0";
walk_graphemes("♠+ZWJ+♠ (U+2660 IS ExtPict)", spade_zwj);
size_t n = gstrlen(spade_zwj, strlen(spade_zwj));
if (n != 1) {
printf(" *** BUG: ♠‍♠ = %zu graphemes (expected 1)\n\n", n);
} else {
printf(" OK: ♠‍♠ = 1 grapheme (GB11 applies, width=1 per EAW)\n\n");
}
}

/* Test 5: Check is_extended_pictographic directly for known values */
{
printf("=== is_extended_pictographic spot checks ===\n");
struct { uint32_t cp; const char *name; int expect; } checks[] = {
{0x2600, "☀ BLACK SUN", 1},          /* Extended_Pictographic=Yes */
{0x2614, "☔ UMBRELLA WITH RAIN", 1}, /* Extended_Pictographic=Yes */
{0x1F600, "😀 GRINNING FACE", 1},    /* Extended_Pictographic=Yes */
{0x2660, "♠ BLACK SPADE", 1},        /* Extended_Pictographic=Yes */
{0x2663, "♣ BLACK CLUB", 1},         /* Extended_Pictographic=Yes */
{0x2665, "♥ BLACK HEART", 1},        /* Extended_Pictographic=Yes */
{0x2666, "♦ BLACK DIAMOND", 1},      /* Extended_Pictographic=Yes */
{0x2605, "★ BLACK STAR", 0},         /* Extended_Pictographic=No */
{0x1F0A1, "🂡 PLAYING CARD ACE", 0}, /* Extended_Pictographic=No */
{0x4E2D, "中 CJK IDEOGRAPH", 0},     /* Extended_Pictographic=No */
{0x3041, "ぁ HIRAGANA", 0},          /* Extended_Pictographic=No */
{0xAC00, "가 HANGUL", 0},            /* Extended_Pictographic=No */
};
int num = sizeof(checks) / sizeof(checks[0]);
int bugs = 0;
for (int i = 0; i < num; i++) {
int got = is_extended_pictographic(checks[i].cp);
const char *status = (got == checks[i].expect) ? "OK" : "WRONG";
printf(" %s U+%04X %-25s got=%d expected=%d\n",
status, checks[i].cp, checks[i].name, got, checks[i].expect);
if (got != checks[i].expect) bugs++;
}
if (bugs > 0) {
printf(" *** %d/%d is_extended_pictographic checks FAILED\n", bugs, num);
}
printf("\n");
}

/* Replacement character itself */
  const char *repl = "a\xEF\xBF\xBDz";  /* a + U+FFFD + z */
  walk_graphemes("Embedded U+FFFD", repl);

  /* Invalid UTF-8 sequences */
  const char *bad1 = "a\x80z";        /* bare continuation byte */
  const char *bad2 = "a\xC0\xAFz";    /* overlong 2-byte */
  const char *bad3 = "a\xF0\x82\x82\xACz";  /* overlong 4-byte */
  const char *bad4 = "a\xED\xA0\x80z";      /* surrogate half */
  walk_graphemes("Invalid: bare continuation", bad1);
  walk_graphemes("Invalid: overlong 2-byte", bad2);
  walk_graphemes("Invalid: overlong 4-byte", bad3);
  walk_graphemes("Invalid: surrogate half", bad4);

  /* ================================================================
   * Width mismatch test: utf8_cpwidth vs terminal reality.
   * This is why the cursor skips in the editor.
   * ================================================================ */
  printf("=== utf8_cpwidth vs terminal width ===\n");
  printf("Characters from the problem line:\n\n");
  printf("  %-6s %-8s %-24s %-12s %-12s %s\n",
         "Char", "CP", "Name", "cpwidth()", "Terminal*", "Match?");
  printf("  %-6s %-8s %-24s %-12s %-12s %s\n",
         "----", "------", "------------------------", "---------", "---------", "------");

  struct {
    const char *ch;
    uint32_t cp;
    const char *name;
    int terminal_width;  /* what terminals actually render */
  } width_checks[] = {
    {"\xe2\x86\x92",  0x2192,  "RIGHTWARDS ARROW",       1},
    {"\xef\xbf\xbd",  0xFFFD,  "REPLACEMENT CHARACTER",   1},
    {"\xe2\x98\x85",  0x2605,  "BLACK STAR",              1},  /* ea=A, terminals: 1 */
    {"\xe2\x99\xa0",  0x2660,  "BLACK SPADE SUIT",        1},  /* ea=A, terminals: 1 */
    {"\xe2\x99\xa3",  0x2663,  "BLACK CLUB SUIT",         1},  /* ea=A, terminals: 1 */
    {"\xe2\x99\xa5",  0x2665,  "BLACK HEART SUIT",        1},  /* ea=A, terminals: 1 */
    {"\xe2\x99\xa6",  0x2666,  "BLACK DIAMOND SUIT",      1},  /* ea=A, terminals: 1 */
    {"\xf0\x9f\x82\xa1", 0x1F0A1, "PLAYING CARD ACE SPADES", 1},  /* ea=N, width 1 */
    {"\xf0\x9f\x82\xa2", 0x1F0A2, "PLAYING CARD TWO SPADES", 1},  /* ea=N, width 1 */
    {"\xf0\x9f\x98\x80", 0x1F600, "GRINNING FACE",           2},
    {"\xe6\x97\xa5", 0x65E5,  "CJK SUN/DAY",             2},  /* ea=W */
    {"\xef\xbc\xa8",  0xFF28,  "FULLWIDTH LATIN H",       2},  /* ea=F */
  };
  int num_checks = (int)(sizeof(width_checks) / sizeof(width_checks[0]));
  int width_bugs = 0;

  for (int i = 0; i < num_checks; i++) {
    int got = utf8_cpwidth(width_checks[i].cp);
    int expect = width_checks[i].terminal_width;
    const char *match = (got == expect) ? "OK" : "*** WRONG";
    if (got != expect) width_bugs++;
    printf("  %-6s U+%04X  %-24s %-12d %-12d %s\n",
           width_checks[i].ch, width_checks[i].cp, width_checks[i].name,
           got, expect, match);
  }

  printf("\n  %d/%d width checks failed.\n", width_bugs, num_checks);

  /* Simulate cursor walk across the problem substring */
  printf("\n  Cursor drift simulation for the end of line 36:\n");
  printf("  (showing cumulative column error as editor walks right)\n\n");

  struct { uint32_t cp; const char *ch; int terminal_w; } drift_seq[] = {
    {0x2192, ">", 1}, {0xFFFD, "?", 1}, {0x0020, " ", 1},
    {0x2605, "*", 1}, {0x0020, " ", 1},
    {0x2660, "S", 1}, {0x0020, " ", 1},
    {0x2663, "C", 1}, {0x0020, " ", 1},
    {0x2665, "H", 1}, {0x0020, " ", 1},
    {0x2666, "D", 1}, {0x0020, " ", 1},
    {0x1F0A1,"A",1}, {0x0020, " ", 1},
    {0x1F0A2,"2",1},
  };
  int drift_len = (int)(sizeof(drift_seq) / sizeof(drift_seq[0]));
  int editor_col = 0;
  int terminal_col = 0;
  int prev_drift = 0;

  printf("  %-6s  %-8s  %-10s  %-10s  %-10s\n",
         "Char", "CP", "editor_col", "term_col", "drift");
  for (int i = 0; i < drift_len; i++) {
    int ew = utf8_cpwidth(drift_seq[i].cp);
    if (ew < 1) ew = 1;
    int tw = drift_seq[i].terminal_w;
    editor_col += ew;
    terminal_col += tw;
    int drift = editor_col - terminal_col;
    printf("  %-6s  U+%04X    %-10d  %-10d  %-+10d",
           drift_seq[i].ch, drift_seq[i].cp,
           editor_col, terminal_col, drift);
    if (drift != prev_drift)
      printf("  <-- cursor jumps");
    printf("\n");
    prev_drift = drift;
  }
  printf("\n  Final drift: editor thinks col %d, terminal at col %d (off by %d)\n",
         editor_col, terminal_col, editor_col - terminal_col);
  if (editor_col != terminal_col)
    printf("  *** BUG: width table disagrees with expected terminal width.\n\n");
  else
    printf("  OK: zero drift, width table matches expected terminal width.\n\n");

  printf("Done.\n");
  return 0;
}
