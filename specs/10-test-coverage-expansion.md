# gstr Test Coverage Expansion Specification

## Audit Summary

The current test suite comprises four files:
- `/home/edward/repos/gstr/test/test_gstr.c` -- primary unit tests (~130 test cases)
- `/home/edward/repos/gstr/test/test_gstr_stress.c` -- stress/consistency tests over a shared string table
- `/home/edward/repos/gstr/test/test_new_functions_stress.c` -- stress tests for 12 newer functions
- `/home/edward/repos/gstr/test/test_grapheme_walk.c` -- diagnostic/debugging tool (not a pass/fail test binary)

The public API surface consists of **53 functions** (static inline) across two layers: the UTF-8 layer (13 functions) and the grapheme layer (40 functions). The analysis below catalogs gaps by severity.

---

## 1. Completely Untested Functions

These functions have **zero dedicated test cases** in any test file. Each requires a new test group.

### 1.1 `gstr_grapheme_width(const char *s, size_t byte_len, int offset, int next)`

This is a key internal helper used by `gstrwidth`, `gstrwtrunc`, `gstrwlpad`, `gstrwrpad`, and `gstrwpad`. It is tested only indirectly through those callers. Direct tests are needed because its branching logic (ZWJ detection, regional indicator counting, codepoint width summation) is not fully exercised by callers.

| Test Case | Input | Expected Width |
|---|---|---|
| ASCII character 'A' | `"A", 1, 0, 1` | 1 |
| CJK ideograph U+4E2D | `"\xe4\xb8\xad", 3, 0, 3` | 2 |
| Simple emoji U+1F600 | `"\xf0\x9f\x98\x80", 4, 0, 4` | 2 |
| ZWJ sequence (man+ZWJ+laptop) | Full 11-byte sequence, offset=0, next=11 | 2 |
| Family ZWJ (4 person) | 25-byte family, offset=0, next=25 | 2 |
| Combining mark cluster (e + acute + circumflex) | `"e\xcc\x81\xcc\x82", 5, 0, 5` | 1 |
| Flag (two regional indicators) | `"\xf0\x9f\x87\xba\xf0\x9f\x87\xb8", 8, 0, 8` | 2 |
| Out-of-bounds offset (offset == next) | `"A", 1, 0, 0` | 0 |
| Empty string | `"", 0, 0, 0` | 0 |
| Emoji with skin tone modifier | `"\xf0\x9f\x91\x8b\xf0\x9f\x8f\xbd", 8, 0, 8` | 2 |
| Non-initial offset (second grapheme in "A日") | `"A\xe6\x97\xa5", 4, 1, 4` | 2 |

### 1.2 `utf8_prev_grapheme(const char *text, int offset)`

Zero test cases exist. `test_grapheme_walk.c` only uses `utf8_next_grapheme`. This is the most dangerous gap because backward navigation bugs would silently corrupt cursor movement in editors.

| Test Case | Description |
|---|---|
| Forward/backward equivalence | For every string in `TEST_STRINGS[]` from `test_gstr_stress.c`, walk forward with `utf8_next_grapheme` collecting offsets, then walk backward with `utf8_prev_grapheme` from end. Collected offset arrays must match in reverse. |
| Single ASCII codepoint | `utf8_prev_grapheme("A", 1)` == 0 |
| Single multi-byte codepoint | `utf8_prev_grapheme("\xc3\xa9", 2)` == 0 |
| ZWJ sequence (family) | From offset 18 (end of family), must return 0 (entire sequence is one grapheme) |
| Flag (two RIs) | From offset 8, must return 0 |
| Combining marks (e + 3 accents) | From offset 7 (end), must return 0 |
| Hangul Jamo L+V+T | Composed syllable traversal: prev from end returns start of syllable |
| offset=0 | Must return 0 |
| offset < 0 | Must return 0 |
| NULL text | Must return 0 |
| Three flags in a row (6 RIs) | Walk backward from end: must produce boundaries at offsets 0, 8, 16 |
| String with >128 combining marks | Verify it does not loop forever (hits `GRAPHEME_MAX_BACKTRACK` at 128) and returns a plausible offset |

### 1.3 `utf8_prev(const char *text, int offset)` and `utf8_next(const char *text, int length, int offset)`

These are byte-level codepoint navigators. They are used everywhere but tested only indirectly.

| Test Case | Function | Input | Expected |
|---|---|---|---|
| 1-byte (ASCII) | `utf8_next` | `"AB", 2, 0` | 1 |
| 2-byte | `utf8_next` | `"\xc3\xa9X", 3, 0` | 2 |
| 3-byte | `utf8_next` | `"\xe4\xb8\xadX", 4, 0` | 3 |
| 4-byte | `utf8_next` | `"\xf0\x9f\x98\x80X", 5, 0` | 4 |
| At end of string | `utf8_next` | `"A", 1, 0` | 1 |
| Past end | `utf8_next` | `"A", 1, 1` | 1 (clamped) |
| 1-byte backward | `utf8_prev` | `"AB", 2` | 1 |
| 2-byte backward | `utf8_prev` | `"A\xc3\xa9", 3` | 1 |
| 3-byte backward | `utf8_prev` | `"A\xe4\xb8\xad", 4` | 1 |
| 4-byte backward | `utf8_prev` | `"A\xf0\x9f\x98\x80", 5` | 1 |
| offset=0 | `utf8_prev` | `"A", 0` | 0 |
| Invalid UTF-8 (bare continuation) | `utf8_next` | `"\x80\x80\x80", 3, 0` | verify advances (does not hang) |
| Invalid UTF-8 backward | `utf8_prev` | `"\x80\x80\x80", 3` | verify retreats (does not hang) |

### 1.4 `utf8_charwidth(const char *text, int length, int offset)`

Only tested indirectly through `gstrwidth`. Needs direct spot-checks.

| Test Case | Input | Expected |
|---|---|---|
| ASCII 'A' | offset 0 in `"A"` | 1 |
| CJK U+4E2D | offset 0 in `"\xe4\xb8\xad"` | 2 |
| Combining acute U+0301 | offset 0 in `"\xcc\x81"` | 0 |
| Emoji U+1F600 | offset 0 in `"\xf0\x9f\x98\x80"` | 2 |
| ZWJ U+200D | offset 0 in `"\xe2\x80\x8d"` | 0 |
| Fullwidth H U+FF28 | offset 0 in `"\xef\xbc\xa8"` | 2 |
| Non-initial offset | offset 3 in `"AB\xe4\xb8\xad"` (5 bytes) | 2 |

### 1.5 `utf8_is_zerowidth(uint32_t codepoint)` and `utf8_is_wide(uint32_t codepoint)`

These are the binary-search lookup functions that back `utf8_cpwidth`. Never tested directly.

| Test Case | Function | Codepoint | Expected |
|---|---|---|---|
| First zero-width range start | `utf8_is_zerowidth` | 0x0300 | 1 |
| First zero-width range end | `utf8_is_zerowidth` | 0x036F | 1 |
| One before first range | `utf8_is_zerowidth` | 0x02FF | 0 |
| One after first range | `utf8_is_zerowidth` | 0x0370 | 0 |
| Skin tone modifier U+1F3FB | `utf8_is_zerowidth` | 0x1F3FB | 1 |
| Last range end U+E01EF | `utf8_is_zerowidth` | 0xE01EF | 1 |
| ASCII 'A' | `utf8_is_zerowidth` | 0x41 | 0 |
| First wide range start | `utf8_is_wide` | 0x1100 | 1 |
| First wide range end | `utf8_is_wide` | 0x115F | 1 |
| One before first wide | `utf8_is_wide` | 0x10FF | 0 |
| CJK U+4E00 | `utf8_is_wide` | 0x4E00 | 1 |
| Emoji U+1F600 | `utf8_is_wide` | 0x1F600 | 1 |
| ASCII 'Z' | `utf8_is_wide` | 0x5A | 0 |
| Card suit U+2660 (ambiguous, not wide) | `utf8_is_wide` | 0x2660 | 0 |

---

## 2. Thin Coverage Functions

These functions have some tests but are missing important edge cases and boundary conditions.

### 2.1 `utf8_decode`

Currently tested: ASCII and one 2-byte sequence. Missing nearly all error paths.

| Test Case | Input bytes | Expected codepoint | Expected byte count |
|---|---|---|---|
| 3-byte: U+800 (first 3-byte) | `\xe0\xa0\x80` | 0x800 | 3 |
| 3-byte: U+FFFF (last BMP) | `\xef\xbf\xbf` | 0xFFFF | 3 |
| 4-byte: U+10000 (first SMP) | `\xf0\x90\x80\x80` | 0x10000 | 4 |
| 4-byte: U+10FFFF (max Unicode) | `\xf4\x8f\xbf\xbf` | 0x10FFFF | 4 |
| Boundary: U+7F (last 1-byte) | `\x7f` | 0x7F | 1 |
| Boundary: U+80 (first 2-byte) | `\xc2\x80` | 0x80 | 2 |
| Boundary: U+7FF (last 2-byte) | `\xdf\xbf` | 0x7FF | 2 |
| Overlong 2-byte NUL | `\xc0\x80` | 0xFFFD | 1 |
| Overlong 3-byte | `\xe0\x80\x80` | 0xFFFD | 1 |
| Overlong 4-byte | `\xf0\x80\x80\x80` | 0xFFFD | 1 |
| Surrogate half U+D800 | `\xed\xa0\x80` | 0xFFFD | 1 |
| Surrogate half U+DFFF | `\xed\xbf\xbf` | 0xFFFD | 1 |
| Above U+10FFFF | `\xf4\x90\x80\x80` | 0xFFFD | 1 |
| Truncated 2-byte (only lead) | `\xc3` with length=1 | 0xFFFD | 1 |
| Truncated 3-byte (2 of 3) | `\xe4\xb8` with length=2 | 0xFFFD | 1 |
| Truncated 4-byte (3 of 4) | `\xf0\x9f\x98` with length=3 | 0xFFFD | 1 |
| Bare continuation byte | `\x80` | 0xFFFD | 1 |
| Invalid leading byte 0xFF | `\xff` | 0xFFFD | 1 |
| Invalid leading byte 0xFE | `\xfe` | 0xFFFD | 1 |
| length=0 | any pointer | 0xFFFD | 0 |
| Roundtrip | `utf8_encode(cp, buf)` then `utf8_decode(buf, n, &out)` for each boundary value -- output must equal input |

### 2.2 `utf8_encode`

Currently tested: ASCII and one 2-byte case. Missing boundary values and error cases.

| Test Case | Codepoint | Expected bytes | Expected encoding |
|---|---|---|---|
| U+0000 (NUL) | 0 | 1 | `\x00` |
| U+7F | 0x7F | 1 | `\x7f` |
| U+80 | 0x80 | 2 | `\xc2\x80` |
| U+7FF | 0x7FF | 2 | `\xdf\xbf` |
| U+800 | 0x800 | 3 | `\xe0\xa0\x80` |
| U+FFFF | 0xFFFF | 3 | `\xef\xbf\xbf` |
| U+10000 | 0x10000 | 4 | `\xf0\x90\x80\x80` |
| U+10FFFF | 0x10FFFF | 4 | `\xf4\x8f\xbf\xbf` |
| Surrogate U+D800 | 0xD800 | 0 (error) | -- |
| Surrogate U+DFFF | 0xDFFF | 0 (error) | -- |
| Above max U+110000 | 0x110000 | 0 (error) | -- |
| U+FFFFFFFF | 0xFFFFFFFF | 0 (error) | -- |

### 2.3 `utf8_valid`

Currently tested: one valid and one invalid case. This function has complex branching that needs thorough exercise.

| Test Case | Input | Expected valid? | Expected error offset |
|---|---|---|---|
| Valid: empty string | `"", 0` | 1 | -- |
| Valid: all ASCII | `"Hello World!", 12` | 1 | -- |
| Valid: mixed scripts | full multilingual string | 1 | -- |
| Valid: 4-byte at end | `"\xf0\x9f\x98\x80", 4` | 1 | -- |
| Invalid: overlong 2-byte | `"A\xc0\x80B", 4` | 0 | 1 |
| Invalid: overlong 3-byte | `"AB\xe0\x80\x80", 5` | 0 | 2 |
| Invalid: surrogate U+D800 | `"\xed\xa0\x80", 3` | 0 | 0 |
| Invalid: truncated at end | `"A\xc3", 2` | 0 | 1 |
| Invalid: 5-byte sequence | `"\xf8\x80\x80\x80\x80", 5` | 0 | 0 |
| Invalid: 6-byte sequence | `"\xfc\x80\x80\x80\x80\x80", 6` | 0 | 0 |
| Invalid: bare continuation | `"\x80", 1` | 0 | 0 |
| Mixed: valid then invalid | `"hello\xffworld", 11` | 0 | 5 |
| Valid with embedded NUL | `"A\x00B", 3` (pass byte_len=3) | 1 | -- |
| error_offset=NULL | `"\xff", 1, NULL` | 0 | (no crash) |

### 2.4 `gstrlower` / `gstrupper`

Currently tested: one basic ASCII test each, plus one emoji passthrough test for `gstrlower`. Missing edge cases.

| Test Case | Function | Input | Expected output |
|---|---|---|---|
| Empty string | `gstrlower` | `"", 0` | `""` |
| NULL input | `gstrlower` | `NULL, 5` | 0 bytes written |
| Already lowercase | `gstrlower` | `"hello", 5` | `"hello"` |
| Non-ASCII passthrough | `gstrlower` | `"\xc3\xa9", 2` (U+00E9) | unchanged bytes |
| Combining marks preserved | `gstrlower` | `"A\xcc\x81", 3` | `"a\xcc\x81"` |
| Buffer size = 1 | `gstrlower` | `"HELLO", 5`, dst_size=1 | `""`, returns 0 |
| Buffer exactly fits | `gstrlower` | `"AB", 2`, dst_size=3 | `"ab"`, returns 2 |
| Empty string | `gstrupper` | `"", 0` | `""` |
| Already uppercase | `gstrupper` | `"HELLO", 5` | `"HELLO"` |
| Non-ASCII passthrough | `gstrupper` | `"\xc3\xa9", 2` | unchanged |
| Mixed emoji and ASCII | `gstrupper` | `"a\xf0\x9f\x98\x80b", 6` | `"A\xf0\x9f\x98\x80B"` |
| Buffer overflow | `gstrupper` | `"hello", 5`, dst_size=4 | `"HEL"`, returns 3 |

### 2.5 `gstrellipsis`

Currently tested: no-truncate, basic truncate, emoji truncate. Missing important boundary conditions.

| Test Case | Input | max_graphemes | ellipsis | Expected |
|---|---|---|---|---|
| Exact length (no truncation needed) | `"hello", 5` | 5 | `"...", 3` | `"hello"` |
| One over boundary | `"helloo", 6` | 5 | `"...", 3` | `"he..."` |
| Ellipsis longer than max | `"hello", 5` | 2 | `"...", 3` | `""` or just ellipsis (verify behavior) |
| Empty string | `"", 0` | 5 | `"...", 3` | `""` |
| Empty ellipsis | `"hello world", 11` | 5 | `"", 0` | `"hello"` |
| Buffer overflow (dst_size too small) | `"hello world", 11` | 8 | `"...", 3` | verify graceful truncation |
| max_graphemes = 0 | `"hello", 5` | 0 | `"...", 3` | `""` |
| Single grapheme string, max=1 | `"\xf0\x9f\x98\x80", 4` | 1 | `".", 1` | `"\xf0\x9f\x98\x80"` (no truncation needed) |
| Multi-byte ellipsis | `"hello world", 11` | 5 | `"\xe2\x80\xa6", 3` (U+2026 ELLIPSIS) | verify correct grapheme counting |

### 2.6 `gstrfill` / `gstrlpad` / `gstrrpad` / `gstrpad`

Currently tested: basic cases. Missing edge conditions.

| Test Case | Function | Notes |
|---|---|---|
| count=0 | `gstrfill` | `gstrfill(buf, 32, "-", 1, 0)` must return 0, `buf[0]=='\0'` |
| Multi-byte grapheme fill | `gstrfill` | Fill with `"\xf0\x9f\x98\x80"` (4 bytes) x 3 = 12 bytes |
| NULL grapheme | `gstrfill` | Must not crash, return 0 |
| Buffer-exact-fit | `gstrfill` | `buf[5]`, fill with "ab" x 2 = 4 bytes + NUL = exactly 5 |
| Emoji padding | `gstrlpad` | Pad `"x"` to 5 with emoji -- verify grapheme count not byte count |
| Already wider | `gstrrpad` | `gstrrpad(buf, 32, "hello", 5, 3, " ", 1)` -- must copy original |
| pad_len = 0 | `gstrlpad` | `gstrlpad(buf, 32, "hi", 2, 5, "", 0)` -- verify behavior |
| count=0 | `gstrpad` | `gstrpad(buf, 32, "hi", 2, 0, " ", 1)` -- verify copies "hi" or returns empty |
| Odd total padding (center pad) | `gstrpad` | `gstrpad(buf, 32, "X", 1, 4, "-", 1)` -- verify left=1, right=2 split |

---

## 3. Cross-Cutting Edge Cases

The following edge cases should be added **across all functions** that accept string input. They exercise code paths that current tests consistently miss.

### 3.1 Invalid UTF-8

All grapheme-level functions must handle invalid UTF-8 without crashing. Tests should verify they produce deterministic output (even if "wrong"), never read past `byte_len`, and never enter infinite loops.

- **Bare continuation bytes**: `"\x80\x81\x82", 3` -- verify `gstrlen` returns a count, `gstrcpy` produces output
- **Overlong encoding**: `"\xc0\xaf", 2` -- verify treated as error, not as U+002F
- **Above U+10FFFF**: `"\xf4\x90\x80\x80", 4`

### 3.2 Embedded NUL Bytes

All `gstr*` functions take explicit `byte_len` and must not stop at NUL. Test with:
- `"A\x00B", 3` -- `gstrlen` should return 3, `gstrcpy` should copy all 3 bytes, `gstrcmp` on identical inputs should return 0

### 3.3 Buffer Size = 1

Every function that writes to a destination buffer should be tested with `dst_size = 1`:
- `gstrcpy`, `gstrncpy`, `gstrsub`, `gstrcat`, `gstrncat`, `gstrlower`, `gstrupper`, `gstrellipsis`, `gstrfill`, `gstrlpad`, `gstrrpad`, `gstrpad`, `gstrrev`, `gstrreplace`, `gstrltrim`, `gstrrtrim`, `gstrtrim`, `gstrwtrunc`, `gstrwlpad`, `gstrwrpad`, `gstrwpad`
- All must write only `'\0'` at `buf[0]` and return 0

### 3.4 CRLF as Single Grapheme

Per UAX #29, `\r\n` is a single grapheme cluster (rule GB3). Test:
- `gstrlen("\r\n", 2)` == 1
- `gstrlen("A\r\nB", 4)` == 3
- `gstrat("A\r\nB", 4, 1, &len)` should return pointer to `\r` with len=2
- `gstrrev` on `"A\r\nB"` should produce `"B\r\nA"` (CRLF stays together)

### 3.5 Hangul Jamo L+V+T Composition

Hangul Jamo sequences that compose into syllables per UAX #29 (rules GB6/GB7/GB8):
- L + V: `"\xe1\x84\x80\xe1\x85\xa1"` (U+1100 + U+1161) == 1 grapheme
- L + V + T: `"\xe1\x84\x80\xe1\x85\xa1\xe1\x86\xa8"` (U+1100 + U+1161 + U+11A8) == 1 grapheme
- V + V: `"\xe1\x85\xa1\xe1\x85\xa2"` == 1 grapheme
- T + T: `"\xe1\x86\xa8\xe1\x86\xa9"` == 1 grapheme
- L + T (no V): `"\xe1\x84\x80\xe1\x86\xa8"` == 2 graphemes (should break)

### 3.6 Odd-Count Regional Indicators

Three regional indicator codepoints should produce 1 flag + 1 orphan RI:
- `"\xf0\x9f\x87\xba\xf0\x9f\x87\xb8\xf0\x9f\x87\xa8"` (U+1F1FA U+1F1F8 U+1F1E8) == 2 graphemes
- Five RIs == 2 flags + 1 orphan == 3 graphemes

### 3.7 Variation Selectors

- VS15 (text presentation, U+FE0E): `"\xe2\x9d\xa4\xef\xb8\x8e"` (heart + VS15) == 1 grapheme, width 1
- VS16 (emoji presentation, U+FE0F): `"\xe2\x9d\xa4\xef\xb8\x8f"` (heart + VS16) == 1 grapheme, width 2
- Verify `gstrwidth` returns different widths for VS15 vs VS16

### 3.8 Deeply Stacked Combining Marks

- 10 combining marks on one base: `"a" + 10x "\xcc\x81"` == 1 grapheme
- Verify `gstrlen`, `gstrat`, `gstrrev`, `gstrsub` all treat as single unit

### 3.9 RTL Embedding Marks

These are zero-width format characters. Verify they are counted as zero-width and don't break grapheme clustering:
- U+200F (RLM): `"\xe2\x80\x8f"` -- `utf8_cpwidth` == 0
- U+202B (RLE): `"\xe2\x80\xab"` -- `utf8_cpwidth` == 0
- Embedded in string: `"A\xe2\x80\x8fB"` should have `gstrwidth` == 2

---

## 4. Negative/Adversarial Testing

### 4.1 GRAPHEME_MAX_BACKTRACK Limit

`utf8_prev_grapheme` limits backward scanning to 128 codepoints. Test with:
- Base character + 200 combining marks (>128): Build a string `"a" + 200x "\xcc\x81"`. Call `utf8_prev_grapheme(text, total_len)`. Verify it returns an offset >= 0 and does not hang. The result need not be perfect (it may return a mid-cluster offset) but must terminate.

### 4.2 Very Long Single-Grapheme Clusters

- ZWJ chain: 10 ExtPict emoji joined by ZWJ (83+ bytes, 1 grapheme). Verify `gstrlen` == 1, `gstrat(s, len, 0, &n)` returns full length, `gstrrev` returns identical string.

### 4.3 All-Zero-Width String

- `"\xcc\x81\xcc\x82\xcc\x83"` (3 combining marks, no base). Per UAX #29, orphan combining marks each start their own cluster: `gstrlen` == 3 (first mark is its own cluster, subsequent marks extend if they follow Extend). Actually per rule GB9, each Extend after the initial one extends the previous cluster, so the first orphan combining mark is one cluster extended by the next two. Verify `gstrlen` == 1.
- `gstrwidth` of this string should be 0 (all zero-width).

### 4.4 String That Is One Long Grapheme

- Base + 50 combining marks: `"X" + 50x "\xcc\x81"` (101 bytes). Verify:
  - `gstrlen` == 1
  - `gstrat(s, 101, 0, &len)` returns len == 101
  - `gstrsub(buf, 256, s, 101, 0, 1)` copies all 101 bytes
  - `gstrncpy(buf, 256, s, 101, 1)` copies all 101 bytes
  - `gstrrev` returns identical output

### 4.5 NULL and Zero-Length Combinations

Systematic NULL/zero testing for every function that doesn't already have it:
- `gstrwidth(NULL, 0)` == 0
- `gstrwtrunc(buf, 32, NULL, 0, 10)` == 0
- `gstr_grapheme_width(NULL, 0, 0, 0)` -- must not crash
- `gstrlower(buf, 32, NULL, 0)` == 0
- `gstrellipsis(buf, 32, NULL, 0, 5, "...", 3)` == 0

---

## 5. File Organization

### Recommended Structure

```
test/
  test_gstr.c                      # Existing -- keep as primary unit tests
  test_gstr_stress.c               # Existing -- keep as consistency/stress tests
  test_new_functions_stress.c      # Existing -- keep as stress tests for newer functions
  test_grapheme_walk.c             # Existing -- keep as diagnostic tool (not run by `make test`)
  test_utf8_layer.c                # NEW -- all utf8_* function tests from Section 1.3-1.5 and Section 2.1-2.3
  test_coverage_gaps.c             # NEW -- all Section 1.1-1.2 tests and Section 2.4-2.6 additions
  test_edge_cases.c                # NEW -- all Section 3 and Section 4 tests
```

**Rationale for three new files rather than expanding existing ones:**

1. `test_utf8_layer.c`: The UTF-8 layer functions (`utf8_decode`, `utf8_encode`, `utf8_valid`, `utf8_prev`, `utf8_next`, `utf8_is_zerowidth`, `utf8_is_wide`, `utf8_charwidth`) form a coherent unit. They have zero or near-zero direct test coverage and need ~80 test cases. Mixing these into `test_gstr.c` (which is already ~1,770 lines) would make it unwieldy.

2. `test_coverage_gaps.c`: Contains tests for `gstr_grapheme_width`, `utf8_prev_grapheme`, and the missing edge cases for `gstrlower`/`gstrupper`/`gstrellipsis`/`gstrfill`/`gstrlpad`/`gstrrpad`/`gstrpad`. These are all grapheme-layer tests but they cover functions that are either untested or critically thin.

3. `test_edge_cases.c`: Cross-cutting adversarial tests (invalid UTF-8 across all functions, CRLF, Hangul Jamo, odd RI counts, variation selectors, deeply stacked combining marks, GRAPHEME_MAX_BACKTRACK stress). These don't belong to any single function -- they exercise the library holistically.

### Build System Changes

The `Makefile` currently only builds/runs `test_gstr.c`. It should be updated to build and run all test binaries:

```makefile
TEST_SRCS = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(TEST_SRCS:.c=)

test: $(TEST_BINS)
	@for t in $(TEST_BINS); do echo "=== $$t ==="; ./$$t || exit 1; done

$(TESTDIR)/test_%: $(TESTDIR)/test_%.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< -o $@
```

Exclude `test_grapheme_walk` from the automatic test run (it is a diagnostic tool that requires file arguments), or add it conditionally.

### Test Framework

All new files should reuse the same lightweight macros (`ASSERT`, `ASSERT_EQ`, `ASSERT_EQ_SIZE`, `ASSERT_STR_EQ`, `ASSERT_NULL`, `ASSERT_NOT_NULL`, `TEST`, `RUN`) defined in `test_gstr.c`. Extract these into a shared `test/test_macros.h` header to avoid duplication. The current copy-paste pattern (each file redefines its own macros with slight variations) is a maintenance hazard.

---

## 6. Coverage Measurement

### Running Coverage

Since gstr is a header-only library with static inline functions, coverage must be measured from the test binaries.

**With GCC/gcov:**

```bash
# Build with coverage instrumentation
gcc -O0 -g --coverage -I include test/test_gstr.c -o test/test_gstr_cov
gcc -O0 -g --coverage -I include test/test_utf8_layer.c -o test/test_utf8_layer_cov
gcc -O0 -g --coverage -I include test/test_coverage_gaps.c -o test/test_coverage_gaps_cov
gcc -O0 -g --coverage -I include test/test_edge_cases.c -o test/test_edge_cases_cov

# Run all tests (generates .gcda files)
./test/test_gstr_cov
./test/test_utf8_layer_cov
./test/test_coverage_gaps_cov
./test/test_edge_cases_cov

# Generate report (merge all .gcda files)
gcov -o test/ test/test_gstr_cov-test_gstr.gcno
# Or use lcov for HTML reports:
lcov --capture --directory test/ --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

**With Clang/llvm-cov:**

```bash
clang -O0 -g -fprofile-instr-generate -fcoverage-mapping -I include \
  test/test_gstr.c -o test/test_gstr_cov

./test/test_gstr_cov
llvm-profdata merge default.profraw -o default.profdata
llvm-cov report ./test/test_gstr_cov -instr-profile=default.profdata
llvm-cov show ./test/test_gstr_cov -instr-profile=default.profdata \
  --format=html -output-dir=coverage_html
```

### Recommended Makefile Target

```makefile
coverage:
	$(CC) -O0 -g --coverage -I$(INCDIR) $(VERSION_FLAGS) \
	  test/test_gstr.c -o test/test_gstr_cov
	# ... repeat for each test file ...
	@for t in test/*_cov; do ./$$t; done
	lcov --capture --directory test/ --output-file coverage.info --rc lcov_branch_coverage=1
	genhtml coverage.info --output-directory coverage_html --branch-coverage
	@echo "Coverage report: coverage_html/index.html"
```

### Target Coverage Percentages

| Function Group | Current Est. Line Coverage | Target Line Coverage | Target Branch Coverage |
|---|---|---|---|
| `utf8_decode` | ~30% (2 of ~20 paths) | 95% | 90% |
| `utf8_encode` | ~25% (2 of ~8 paths) | 95% | 90% |
| `utf8_valid` | ~15% (2 cases) | 95% | 90% |
| `utf8_prev` / `utf8_next` | 0% direct | 90% | 85% |
| `utf8_prev_grapheme` | 0% | 90% | 85% |
| `utf8_next_grapheme` | ~60% (via gstrlen tests) | 95% | 90% |
| `utf8_is_zerowidth` / `utf8_is_wide` | 0% direct | 85% | 80% |
| `utf8_charwidth` / `utf8_cpwidth` | ~40% | 90% | 85% |
| `gstr_grapheme_width` | 0% direct | 90% | 85% |
| `gstrlower` / `gstrupper` | ~30% | 90% | 85% |
| `gstrellipsis` | ~40% | 90% | 85% |
| `gstrfill` / `gstrlpad` / `gstrrpad` / `gstrpad` | ~30% | 85% | 80% |
| All other `gstr*` functions | ~60-80% | 90% | 85% |
| **Overall** | **~50%** | **90%** | **85%** |

### Which Functions Need Branch-Level Coverage

Branch coverage is critical (not just line coverage) for these functions due to complex conditional logic:

- **`utf8_decode`**: Has nested validity checks for overlong, surrogates, truncation, and continuation byte validation. Each branch maps to a distinct error class.
- **`utf8_valid`**: Similar branching to `utf8_decode` plus length-checking loops.
- **`utf8_next_grapheme`** / **`utf8_prev_grapheme`**: The grapheme break algorithm (`is_grapheme_break`) has ~15 rules with compound conditions (GB3, GB4, GB5, GB6-GB8, GB9, GB9a, GB9b, GB9c, GB11, GB12-GB13, GB999). Each rule is a distinct branch.
- **`gstr_grapheme_width`**: Three code paths (ZWJ, regional indicator, regular) with sub-loops.
- **`gstrreplace`**: Match/no-match loop, buffer overflow truncation, empty-old early return.
- **`gstrellipsis`**: Truncation threshold comparison, ellipsis-longer-than-max guard.

For simpler functions like `gstrcpy`, `gstrcat`, `gstrcmp`, line coverage alone is sufficient since their branch structure is straightforward (null check, loop, buffer check).

---

## Estimated Effort

| New File | Approximate Test Cases | Estimated Lines of C |
|---|---|---|
| `test/test_utf8_layer.c` | ~80 | ~600 |
| `test/test_coverage_gaps.c` | ~60 | ~500 |
| `test/test_edge_cases.c` | ~50 | ~450 |
| `test/test_macros.h` (shared) | -- | ~70 |
| Makefile updates | -- | ~15 |
| **Total** | **~190 new test cases** | **~1,635 lines** |