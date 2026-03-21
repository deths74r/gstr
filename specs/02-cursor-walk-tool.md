# Specification: `gstr-cursor-walk` -- Interactive Grapheme Cluster Cursor Walk Tool

## 1. Purpose

`gstr-cursor-walk` is an interactive terminal tool and automated verifier for grapheme cluster segmentation in the gstr library. It lets a developer move a cursor through curated test strings one grapheme cluster at a time, exercising `utf8_next_grapheme` and `utf8_prev_grapheme` directly, while displaying full diagnostic information about each cluster's internal structure: byte offsets, codepoints, GCB properties, display widths, and raw hex. It also provides a non-interactive batch mode that mechanically verifies forward/backward navigation consistency across all test strings.

The tool is rebuilt whenever `make cursor-walk` is invoked. It links against gstr.h as a header-only include -- it does not embed generated tables separately, because gstr.h already contains them. When tables are regenerated via `gen_unicode_tables.py` and pasted into gstr.h, a subsequent `make cursor-walk` picks up the new tables automatically through the `#include`.

## 2. File Location and Naming

```
tools/cursor_walk.c       -- Single-file C source
```

The binary is built to `tools/cursor_walk`. This keeps it out of the `test/` directory, which is for automated test suites, and signals that it is a developer-facing diagnostic tool rather than a CI artifact.

## 3. Display Layout

The terminal screen is divided into four horizontal regions, drawn top-to-bottom. All rendering uses ANSI escape sequences (CSI codes). The tool clears the screen on entry and redraws fully on each navigation event (no incremental redraw; simplicity over performance).

### 3.1 Header Bar (line 1)

```
gstr-cursor-walk | Unicode 17.0 | String 3/14: "ZWJ family sequences"  [q]uit [b]ytes [p]rops
```

Fixed at the top. Shows the tool name, the Unicode version compiled into the tables (extracted from `GSTR_VERSION` or hard-coded to match `gen_unicode_tables.py`'s `UNICODE_VERSION`), the current test string index and total count, the test string's human-readable label, and a keybinding reminder. Rendered with bold (`\033[1m`) on the label text.

### 3.2 Rendered String (lines 3-4)

```
Line 3:  <test string rendered normally with the current grapheme cluster in reverse video>
Line 4:  <column ruler -- caret markers showing cluster column span>
```

The entire test string is printed in its natural rendering. The current grapheme cluster is highlighted using reverse video (`\033[7m`). For grapheme clusters that contain control characters or are zero-width, the tool renders them as a `<U+XXXX>` placeholder in dim text (`\033[2m`) so they remain visible.

Below the string, a ruler line shows caret characters (`^` repeated for the cluster's display width) aligned under the highlighted cluster's terminal columns. For zero-width clusters, a single `^` is shown.

If the test string would exceed the terminal width, it is displayed with horizontal scrolling: the view is centered on the current cluster with `...` ellipsis indicators at the truncated edges.

### 3.3 Cluster Detail Panel (lines 6-14)

This is the primary diagnostic area. It always shows the following fields for the currently highlighted grapheme cluster:

```
Grapheme index:    3          (0-based grapheme position in the string)
Byte offset:       12..24     (start byte offset .. end byte offset, exclusive)
Byte length:       12
Display width:     2 columns  (result of gstr_grapheme_width for this cluster)
Codepoint count:   4

Codepoints:
  [0] U+1F468  MAN                          GCB=ExtPict  width=2  bytes=F0 9F 91 A8
  [1] U+200D   ZERO WIDTH JOINER            GCB=ZWJ      width=0  bytes=E2 80 8D
  [2] U+1F469  WOMAN                        GCB=ExtPict  width=2  bytes=F0 9F 91 A9
  [3] U+200D   ZERO WIDTH JOINER            GCB=ZWJ      width=0  bytes=E2 80 8D
```

Each codepoint line shows:
- Index within the cluster (0-based)
- The codepoint in U+XXXX notation
- The Unicode character name (looked up from a compiled-in table of names for the test string codepoints only, or "<?>" if unavailable -- see Section 9)
- GCB property name (human-readable string from the `enum gcb_property`, e.g., "EXTEND", "ZWJ", "REGIONAL_INDICATOR", "OTHER")
- Display width as returned by `utf8_cpwidth`
- Raw UTF-8 bytes in hex

The GCB property name mapping is a simple `const char*` array indexed by `enum gcb_property`:

```c
static const char *GCB_NAMES[] = {
    "OTHER", "CR", "LF", "CONTROL", "EXTEND", "ZWJ",
    "REGIONAL_INDICATOR", "PREPEND", "SPACING_MARK",
    "L", "V", "T", "LV", "LVT"
};
```

### 3.4 Toggle Views

When **byte view** is active (toggled with `b`), the Cluster Detail Panel additionally shows a hex dump row:

```
Raw bytes:  F0 9F 91 A8 E2 80 8D F0 9F 91 A9 E2 80 8D F0 9F 91 A7
            |  man     | zwj   |  woman   | zwj   |  girl    |
```

With vertical bars separating codepoint byte boundaries and labels beneath.

When **property view** is active (toggled with `p`), the panel additionally shows the GCB rule chain that kept these codepoints within a single cluster:

```
GCB rule chain:
  U+1F468 (ExtPict) --[GB9: x Extend|ZWJ]--> U+200D (ZWJ) ... no break
  U+200D  (ZWJ)     --[GB11: ExtPict Ext* ZWJ x ExtPict]--> U+1F469 (ExtPict) ... no break
  U+1F469 (ExtPict) --[GB9: x Extend|ZWJ]--> U+200D (ZWJ) ... no break
  U+200D  (ZWJ)     --[GB11: ExtPict Ext* ZWJ x ExtPict]--> U+1F467 (ExtPict) ... no break
```

This traces the actual rule from `is_grapheme_break` that returned 0 (no break) between each consecutive codepoint pair in the cluster. Where a break IS detected (i.e., at the cluster boundary), the chain shows `BREAK (GB999)` or the specific rule number.

Both toggles are independent and can be active simultaneously.

### 3.5 Status Bar (last line)

```
LEFT/RIGHT: grapheme  UP/DOWN: string  b: bytes  p: props  q: quit
```

Rendered in reverse video spanning the full terminal width.

## 4. Navigation

### 4.1 Key Bindings

| Key | Action |
|-----|--------|
| Right Arrow (`\033[C`) | Move cursor to the next grapheme cluster. Calls `utf8_next_grapheme(text, length, current_offset)`. If already at the end, do nothing (no wrap). |
| Left Arrow (`\033[D`) | Move cursor to the previous grapheme cluster. Calls `utf8_prev_grapheme(text, current_offset)`. If already at offset 0, do nothing. |
| Down Arrow (`\033[B`) | Switch to the next test string (index + 1, wrapping to 0). Reset cursor to offset 0. |
| Up Arrow (`\033[A`) | Switch to the previous test string (index - 1, wrapping to last). Reset cursor to offset 0. |
| `q` | Quit. Restore terminal to cooked mode and exit 0. |
| `b` | Toggle byte view on/off. |
| `p` | Toggle property/rule chain view on/off. |
| `g` | Go to grapheme index: reads a number from a mini-prompt at the status bar, then navigates to that grapheme's offset by calling `utf8_next_grapheme` that many times from offset 0. |
| `0` | Jump to the first grapheme (offset 0). |
| `$` | Jump to the last grapheme (walk forward until `utf8_next_grapheme` returns `length`). |
| `v` | Toggle verification overlay: show `utf8_prev_grapheme(text, next_offset)` result alongside the current offset, highlighting in green if they match and red if they diverge. This is an at-a-glance consistency check while browsing. |

### 4.2 Cursor State

The cursor state is a single `int current_offset` representing the byte offset of the start of the current grapheme cluster. The end of the cluster is computed on each redraw as `utf8_next_grapheme(text, length, current_offset)`.

## 5. Built-In Test Strings

Each test string is a struct:

```c
struct test_string {
    const char *label;       /* Human-readable description */
    const char *data;        /* UTF-8 byte data */
    int         length;      /* Byte length (not null-terminated length) */
};
```

The curated set, defined as a static array:

### 5.1 ZWJ Emoji Sequences

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 1 | "ZWJ: couple with heart" | U+1F469 U+200D U+2764 U+FE0F U+200D U+1F468 | 1 grapheme, 2 cols |
| 2 | "ZWJ: family MWGB" | U+1F468 U+200D U+1F469 U+200D U+1F467 U+200D U+1F466 | 1 grapheme, 2 cols |
| 3 | "ZWJ: family MWG" | U+1F468 U+200D U+1F469 U+200D U+1F467 | 1 grapheme, 2 cols |
| 4 | "ZWJ: person + laptop" | U+1F9D1 U+200D U+1F4BB | 1 grapheme, 2 cols |
| 5 | "ZWJ: woman firefighter skin 5" | U+1F469 U+1F3FF U+200D U+1F692 | 1 grapheme, 2 cols |

### 5.2 Flag Sequences (Regional Indicators)

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 6 | "Flags: US JP DE" | U+1F1FA U+1F1F8 U+1F1EF U+1F1F5 U+1F1E9 U+1F1EA | 3 graphemes (3 flag pairs), 6 cols |
| 7 | "Flags: odd RI count (5)" | U+1F1FA U+1F1F8 U+1F1EF U+1F1F5 U+1F1E9 | 3 graphemes: 2 flags + 1 lone RI |
| 8 | "Flags: single RI" | U+1F1FA | 1 grapheme, lone indicator |

### 5.3 Keycap Sequences

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 9 | "Keycaps: #\uFE0F\u20E3 1\uFE0F\u20E3 9\uFE0F\u20E3" | U+0023 U+FE0F U+20E3, U+0031 U+FE0F U+20E3, U+0039 U+FE0F U+20E3 | 3 graphemes |

### 5.4 Indic Conjunct Sequences (GB9c)

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 10 | "Devanagari: ksha (क + ् + ष)" | U+0915 U+094D U+0937 | 1 grapheme (InCB conjunct) |
| 11 | "Devanagari: triple conjunct" | U+0915 U+094D U+0937 U+094D U+0924 | 1 grapheme (consonant+virama+consonant+virama+consonant) |
| 12 | "Bengali: ksha" | U+0995 U+09CD U+09B7 | 1 grapheme |
| 13 | "Telugu: conjunct" | U+0C15 U+0C4D U+0C37 | 1 grapheme |
| 14 | "Devanagari: non-conjunct boundary" | U+0915 U+094D U+0020 U+0937 | Multiple graphemes (space breaks conjunct) |

### 5.5 Hangul Jamo

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 15 | "Hangul L+V+T composition" | U+1100 U+1161 U+11A8 | 1 grapheme (Jamo L + V + T) |
| 16 | "Hangul LV+T" | U+AC00 U+11A8 | 1 grapheme (precomposed LV + trailing T) |
| 17 | "Hangul LVT standalone" | U+AC01 | 1 grapheme (precomposed LVT) |
| 18 | "Hangul mixed: LV+T then L+V" | U+AC00 U+11A8 U+1100 U+1161 | 2 graphemes |

### 5.6 Stacked Combining Marks

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 19 | "Combining: 5 diacritics on 'a'" | U+0061 + 5x U+0300 | 1 grapheme |
| 20 | "Combining: 10 diacritics on 'a'" | U+0061 + 10x U+0300 | 1 grapheme |
| 21 | "Combining: 20 diacritics on 'a'" | U+0061 + 20x U+0300 | 1 grapheme (stress-test backtracking) |
| 22 | "Combining: mixed marks" | U+0065 U+0301 U+0327 U+0308 | 1 grapheme (e + acute + cedilla + diaeresis) |

### 5.7 CR/LF

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 23 | "CRLF sequence" | U+000D U+000A | 1 grapheme (GB3: CR x LF) |
| 24 | "CR alone then LF alone" | U+000D U+0061 U+000A | 3 graphemes (CR / a / LF -- GB4/GB5 breaks) |
| 25 | "Multiple CRLF" | (U+000D U+000A) x 3 | 3 graphemes |

### 5.8 Mixed Scripts

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 26 | "Mixed: Arabic+Latin+CJK+emoji" | U+0639 U+0631 U+0628 U+0069 U+4E16 U+754C U+1F30D | 7 graphemes, varying widths (1+1+1+1+2+2+2 = 10 cols) |
| 27 | "Mixed: combining + wide + emoji" | U+0065 U+0301 U+4E2D U+1F600 | 3 graphemes: e-with-acute (1 col), CJK (2 cols), emoji (2 cols) |

### 5.9 Variation Selectors

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 28 | "VS15: text presentation" | U+2764 U+FE0E | 1 grapheme (heart, text style) |
| 29 | "VS16: emoji presentation" | U+2764 U+FE0F | 1 grapheme (heart, emoji style) |
| 30 | "VS side by side" | U+2764 U+FE0E U+2764 U+FE0F | 2 graphemes |
| 31 | "CJK VS: U+9089 + VS17" | U+9089 U+E0100 | 1 grapheme (IVS) |

### 5.10 Invalid UTF-8

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 32 | "Invalid: lone continuation byte" | `\x80` | Tests `utf8_decode` returning U+FFFD |
| 33 | "Invalid: truncated 2-byte" | `\xC3` | Missing continuation |
| 34 | "Invalid: truncated 3-byte" | `\xE0\xA0` | Missing final byte |
| 35 | "Invalid: truncated 4-byte" | `\xF0\x9F\x91` | Missing final byte of emoji |
| 36 | "Invalid: overlong 2-byte NUL" | `\xC0\x80` | Overlong encoding |
| 37 | "Invalid: mixed valid+invalid" | `Hello\xFEWorld` | Valid ASCII, then 0xFE (never valid), then ASCII |
| 38 | "Invalid: lone surrogates" | `\xED\xA0\x80` | Encoded U+D800 (surrogate half) |

### 5.11 Edge Cases

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 39 | "Empty string" | `""` | Length 0. Cursor should show "(empty)" |
| 40 | "Single ASCII" | `"a"` | 1 grapheme |
| 41 | "Single emoji" | U+1F600 | 1 grapheme |
| 42 | "Prepend character" | U+0600 U+0628 | 1 grapheme (Prepend x, GB9b) |
| 43 | "Null byte" | `\x00` | Length 1. Tests handling of embedded NUL |
| 44 | "SpacingMark: Devanagari vowel sign" | U+0915 U+093E | 1 grapheme (consonant + SpacingMark, GB9a) |
| 45 | "Extend after emoji" | U+1F600 U+1F3FB | 1 grapheme (emoji + skin tone modifier/Extend) |

### 5.12 Long/Realistic Strings

| # | Label | Content | Notes |
|---|-------|---------|-------|
| 46 | "Full sentence with emoji" | "Hello \xF0\x9F\x91\x8B\xF0\x9F\x8C\x8D world! \xE2\x9C\xA8" | Realistic mixed content |
| 47 | "Thai script (no spaces)" | U+0E2A U+0E27 U+0E31 U+0E2A U+0E14 U+0E35 | Tests GCB_EXTEND behavior in Thai |

## 6. Generation Integration

### 6.1 Relationship to `gen_unicode_tables.py`

The cursor walk tool does NOT have tables generated into it by the Python script. It relies solely on `#include <gstr.h>`, which contains all tables. This is the correct design because:

- gstr.h is the single source of truth for all Unicode tables.
- When a developer runs `gen_unicode_tables.py` and pastes new output into gstr.h, the next `make cursor-walk` automatically picks up the changes.
- There is no synchronization problem, no risk of stale embedded tables.

### 6.2 Optional: Post-Generation Smoke Test

The `gen_unicode_tables.py` script could optionally be extended (as a future enhancement) to invoke `make cursor-walk-verify` after generating tables, producing immediate pass/fail feedback. This is NOT a requirement for the initial implementation but should be planned for in the Makefile target structure.

## 7. Terminal Requirements

### 7.1 Raw Mode

The tool must switch the terminal to raw mode on startup and restore the original `termios` settings on exit (including on signal delivery). Implementation:

- Save original `termios` via `tcgetattr(STDIN_FILENO, &orig_termios)`.
- Enter raw mode: disable `ICANON`, `ECHO`, `ISIG`; set `VMIN=1`, `VTIME=0`.
- Register `atexit()` handler and `SIGINT`/`SIGTERM` signal handlers that call `tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios)`.

### 7.2 Escape Sequence Handling

Input parsing must handle multi-byte escape sequences for arrow keys:
- `\033[A` (Up), `\033[B` (Down), `\033[C` (Right), `\033[D` (Left)
- Single-byte keys: `q`, `b`, `p`, `g`, `0`, `$`, `v`

Read input byte-by-byte. On receiving `\033`, read the next byte with a short timeout (50ms via `select()` or `poll()`). If `[` follows, read the direction byte. If no second byte arrives within the timeout, treat it as a bare Escape press (ignored).

### 7.3 Output Escape Sequences Used

| Sequence | Purpose |
|----------|---------|
| `\033[2J` | Clear screen |
| `\033[H` | Cursor to home |
| `\033[K` | Clear to end of line |
| `\033[{row};{col}H` | Position cursor |
| `\033[7m` / `\033[27m` | Reverse video on/off |
| `\033[1m` / `\033[22m` | Bold on/off |
| `\033[2m` / `\033[22m` | Dim on/off |
| `\033[31m`, `\033[32m` | Red, Green foreground |
| `\033[0m` | Reset all attributes |

No 256-color or true-color sequences. The tool must work on any terminal supporting basic ANSI/VT100 SGR codes.

### 7.4 Minimum Terminal Size

80 columns x 24 rows. On startup, query the terminal size via `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`. If smaller than 80x24, print a warning but continue (the display will be clipped, not crashed). Handle `SIGWINCH` to update dimensions on resize.

### 7.5 Alternate Screen Buffer

Enter the alternate screen buffer on startup (`\033[?1049h`) and leave it on exit (`\033[?1049l`). This ensures the user's scrollback is not polluted and the tool feels like a modal application.

## 8. Build Integration

### 8.1 Makefile Targets

Add to the existing Makefile:

```makefile
# ============================================================================
# Cursor Walk Tool (developer diagnostic)
# ============================================================================
TOOLSDIR = tools

cursor-walk: $(TOOLSDIR)/cursor_walk.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< -o $(TOOLSDIR)/cursor_walk

cursor-walk-verify: cursor-walk
	./$(TOOLSDIR)/cursor_walk --verify

clean:
	rm -f $(TESTDIR)/test_gstr $(TOOLSDIR)/cursor_walk
```

### 8.2 Dependencies

- **gstr.h** -- the only library dependency (header-only, no linking)
- **POSIX terminal APIs** -- `<termios.h>`, `<sys/ioctl.h>`, `<unistd.h>`, `<signal.h>`, `<sys/select.h>`
- **Standard C17** -- `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<stdint.h>`
- **No external dependencies** -- no ncurses, no libunistring, no ICU

### 8.3 Compiler Flags

Uses `CFLAGS_DEBUG` (with `-g -O0`) rather than `CFLAGS` because this is a diagnostic tool, not a production artifact. Debug symbols make it useful with `gdb`/`lldb` for investigating segmentation issues in the grapheme walker.

## 9. Character Name Lookup

Full Unicode character names (UnicodeData.txt) are 1.8MB and inappropriate to compile into a diagnostic tool. Instead:

- Define a static lookup table covering ONLY the codepoints that appear in the built-in test strings (approximately 100-150 entries).
- Each entry is `{ uint32_t codepoint; const char *name; }`.
- For any codepoint not in the table, display `"<?>"`.
- This table is hand-maintained in the source file. It does not need to be generated.

## 10. Boundary Verification Mode (`--verify`)

### 10.1 Invocation

```
./tools/cursor_walk --verify
```

No terminal interaction. Writes to stdout. Suitable for CI pipelines.

### 10.2 Algorithm

For each test string (skipping the empty string and invalid UTF-8 strings that may not roundtrip):

1. **Forward pass**: Walk the string from offset 0 to the end using `utf8_next_grapheme`, collecting every boundary offset into an array `boundaries[]`. Include 0 as the first entry and `length` as the last.

2. **Backward pass**: Walk the string from offset `length` to 0 using `utf8_prev_grapheme`, collecting every boundary offset into a second array `reverse_boundaries[]` (in reverse order). Reverse this array.

3. **Comparison**: Assert that `boundaries` and `reverse_boundaries` are identical element-by-element. If they differ, report the first divergence point with full context.

4. **Forward-backward roundtrip**: For every boundary offset `b` in `boundaries` (except the last), verify:
   - `utf8_prev_grapheme(text, utf8_next_grapheme(text, length, b)) == b`
   - That is: stepping forward one grapheme then backward one grapheme returns to the exact starting offset.

5. **Backward-forward roundtrip**: For every boundary offset `b` in `boundaries` (except the first), verify:
   - `utf8_next_grapheme(text, length, utf8_prev_grapheme(text, b)) == b`

### 10.3 Output Format

```
[PASS] String  1: "ZWJ: couple with heart"           (6 bytes, 1 grapheme, 2 boundaries)
[PASS] String  2: "ZWJ: family MWGB"                 (25 bytes, 1 grapheme, 2 boundaries)
...
[FAIL] String 37: "Invalid: mixed valid+invalid"      boundary mismatch at index 2:
       forward=6  backward=5
       forward boundaries:  [0, 5, 6, 11]
       backward boundaries: [0, 5, 11]
...
============================================================
Results: 44 passed, 1 failed, 2 skipped (empty/known-invalid)
```

### 10.4 Exit Code

- Exit 0 if all applicable strings pass.
- Exit 1 if any string fails.
- Skipped strings (empty, known-invalid) do not affect the exit code but are reported.

### 10.5 Invalid UTF-8 Handling in Verify Mode

Invalid UTF-8 test strings (Section 5.10) are still run through verification, but with relaxed expectations. The tool reports whether `utf8_prev_grapheme` is the inverse of `utf8_next_grapheme` for these strings; if it is not, the failure is reported as `[WARN]` rather than `[FAIL]`, and does not affect the exit code. The rationale: gstr's behavior on invalid input is best-effort, and the primary contract is correct behavior on valid UTF-8. The warnings still surface regressions for the developer's attention.

## 11. Internal Architecture

### 11.1 Data Structures

```c
struct test_string {
    const char *label;
    const char *data;
    int         length;
    int         flags;       /* TEST_INVALID_UTF8, TEST_EMPTY, etc. */
};

struct cursor_state {
    int string_index;        /* Index into test_strings[] */
    int byte_offset;         /* Current grapheme start offset */
    int show_bytes;          /* Toggle: byte view */
    int show_props;          /* Toggle: property chain view */
    int show_verify;         /* Toggle: verification overlay */
    int terminal_rows;
    int terminal_cols;
};
```

### 11.2 Core Loop

```
initialize terminal (raw mode, alt screen)
set cursor_state to {0, 0, 0, 0, 0, rows, cols}
loop:
    draw_screen(&cursor_state)
    key = read_key()
    switch (key):
        case KEY_RIGHT: advance_grapheme(&cursor_state)
        case KEY_LEFT:  retreat_grapheme(&cursor_state)
        case KEY_DOWN:  next_string(&cursor_state)
        case KEY_UP:    prev_string(&cursor_state)
        case 'q':       break loop
        case 'b':       toggle show_bytes
        case 'p':       toggle show_props
        case 'v':       toggle show_verify
        ...
restore terminal
```

### 11.3 Function Decomposition

| Function | Responsibility |
|----------|---------------|
| `main()` | Arg parsing (`--verify` vs interactive), terminal setup/teardown |
| `run_interactive()` | The event loop described above |
| `run_verify()` | The batch verification mode (Section 10) |
| `draw_screen()` | Full screen redraw: header, string, detail panel, status bar |
| `draw_string_with_highlight()` | Render the test string with reverse-video on the current cluster |
| `draw_cluster_detail()` | Render the codepoint breakdown panel |
| `draw_byte_view()` | Render the hex dump (when toggled on) |
| `draw_property_chain()` | Render the GCB rule chain (when toggled on) |
| `read_key()` | Blocking read with escape sequence parsing |
| `advance_grapheme()` | `offset = utf8_next_grapheme(text, len, offset)` with bounds check |
| `retreat_grapheme()` | `offset = utf8_prev_grapheme(text, offset)` |
| `identify_gcb_rule()` | Given prev_prop, curr_prop, and state, return the GB rule name string (e.g., "GB9", "GB11", "GB999") |
| `lookup_cp_name()` | Binary search the codepoint name table |
| `get_terminal_size()` | `ioctl` wrapper |

## 12. Limitations and Non-Goals

- **Not a fuzzer.** The tool uses curated, static test strings. Fuzz testing of grapheme segmentation should use a separate tool (e.g., feeding random bytes into `utf8_next_grapheme`).
- **Not a conformance test.** UAX #29 conformance testing requires running the official `GraphemeBreakTest.txt` test vectors. This tool is complementary: it provides interactive inspection, not exhaustive conformance.
- **No mouse support.** Keyboard only.
- **No Unicode character name database.** Only names for codepoints appearing in the test strings are included (Section 9).
- **No configuration file.** Test strings are compiled in. Custom strings require editing the source and rebuilding. A future enhancement could accept strings via command-line arguments.
- **Single-platform target.** POSIX only. No Windows support (no `<windows.h>` console API). Works on Linux, macOS, and BSDs.