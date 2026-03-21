# Specification: Unicode Whitespace Detection and Unicode Case Operations for gstr

## Status

**Draft** -- Prepared 2026-03-21

## Scope

This specification addresses two related gaps in `gstr.h` where behavior is limited to ASCII:

1. `gstr_is_whitespace` only detects 6 ASCII whitespace characters (plus CR+LF), causing `gstrltrim`, `gstrrtrim`, and `gstrtrim` to leave Unicode whitespace untouched.
2. All case operations (`gstrlower`, `gstrupper`, `gstrcasecmp`, `gstrncasecmp`, `gstrcasestr`, `gstr_ascii_lower`, `gstr_ascii_upper`, `gstr_cmp_grapheme_icase`) fold only ASCII A-Z/a-z, silently passing through all non-ASCII casefolding pairs.

---

## Part A: Unicode Whitespace

### A.1 Complete Unicode White_Space Codepoints (Unicode 16.0, PropList.txt)

There are exactly 25 codepoints with the `White_Space` property:

| # | Codepoint | UTF-8 Bytes | Name |
|---|-----------|-------------|------|
| 1 | U+0009 | 1 | CHARACTER TABULATION (HT) |
| 2 | U+000A | 1 | LINE FEED (LF) |
| 3 | U+000B | 1 | LINE TABULATION (VT) |
| 4 | U+000C | 1 | FORM FEED (FF) |
| 5 | U+000D | 1 | CARRIAGE RETURN (CR) |
| 6 | U+0020 | 1 | SPACE |
| 7 | U+0085 | 2 (C2 85) | NEXT LINE (NEL) |
| 8 | U+00A0 | 2 (C2 A0) | NO-BREAK SPACE |
| 9 | U+1680 | 3 (E1 9A 80) | OGHAM SPACE MARK |
| 10 | U+2000 | 3 (E2 80 80) | EN QUAD |
| 11 | U+2001 | 3 (E2 80 81) | EM QUAD |
| 12 | U+2002 | 3 (E2 80 82) | EN SPACE |
| 13 | U+2003 | 3 (E2 80 83) | EM SPACE |
| 14 | U+2004 | 3 (E2 80 84) | THREE-PER-EM SPACE |
| 15 | U+2005 | 3 (E2 80 85) | FOUR-PER-EM SPACE |
| 16 | U+2006 | 3 (E2 80 86) | SIX-PER-EM SPACE |
| 17 | U+2007 | 3 (E2 80 87) | FIGURE SPACE |
| 18 | U+2008 | 3 (E2 80 88) | PUNCTUATION SPACE |
| 19 | U+2009 | 3 (E2 80 89) | THIN SPACE |
| 20 | U+200A | 3 (E2 80 8A) | HAIR SPACE |
| 21 | U+2028 | 3 (E2 80 A8) | LINE SEPARATOR |
| 22 | U+2029 | 3 (E2 80 A9) | PARAGRAPH SEPARATOR |
| 23 | U+202F | 3 (E2 80 AF) | NARROW NO-BREAK SPACE |
| 24 | U+205F | 3 (E2 81 9F) | MEDIUM MATHEMATICAL SPACE |
| 25 | U+3000 | 3 (E3 80 80) | IDEOGRAPHIC SPACE |

The 6 ASCII whitespace characters currently detected by `gstr_is_whitespace` (U+0009 through U+0020) are a subset of these 25. The remaining 19 are multi-byte UTF-8 sequences.

### A.2 Implementation Approach: Switch Statement vs. Range Table

**Recommendation: switch statement.**

Rationale:
- There are only 25 codepoints. A range table with `unicode_range_contains` binary search is overkill and would be slower than a direct switch (the compiler will generate a jump table or decision tree).
- The existing `unicode_range` / `unicode_range_contains` infrastructure is designed for property tables with hundreds of entries (GCB has 620+ ranges, EAW_WIDE has 50+ ranges). For 25 values, a switch is both clearer and faster.
- The codepoints cluster into analyzable groups (6 ASCII, 2 in U+00xx, 1 in U+1680, 11 contiguous in U+2000-U+200A, 4 scattered in U+2028-U+205F, 1 in U+3000), which a switch handles naturally.
- The White_Space property list has been stable for many Unicode versions and is unlikely to grow.

### A.3 Design for `gstr_is_whitespace`

The current implementation (line 1610-1618) returns early with 0 for any grapheme with `g_len != 1` (except CR+LF). This is the root of the bug: multi-byte whitespace characters have `g_len` of 2 or 3, so they are rejected immediately.

The fix:

1. Decode the first codepoint of the grapheme using `utf8_decode` (already available in the library, line 1168).
2. Check the decoded codepoint against a switch covering all 25 White_Space values.
3. A single-codepoint grapheme that is whitespace returns 1. A multi-codepoint grapheme (e.g., a whitespace character followed by a combining mark -- unlikely but theoretically possible) should still return 1, since the base character is whitespace. However, the practical choice is to check only that the grapheme consists of exactly one codepoint that is whitespace. This matches POSIX `isspace` semantics extended to Unicode. A whitespace codepoint combined with a combining mark would form a multi-codepoint grapheme that is no longer "pure" whitespace; returning 0 for such cases is correct.
4. The CR+LF special case (g_len == 2, bytes are 0x0D 0x0A) must be preserved.

Proposed logic:

```
if grapheme is CR+LF -> return 1
decode first codepoint from grapheme bytes
if decoded bytes consumed != g_len -> return 0  (multi-codepoint grapheme)
switch on codepoint:
  case 0x0009..0x000D, 0x0020, 0x0085, 0x00A0, 0x1680,
       0x2000..0x200A, 0x2028, 0x2029, 0x202F, 0x205F, 0x3000:
    return 1
  default:
    return 0
```

The ASCII fast path is preserved: single-byte graphemes (g_len == 1) where the byte is < 0x80 will be decoded by `utf8_decode` in its fast path (a single comparison) and then hit the switch. This adds negligible overhead compared to the current six-comparison chain.

### A.4 Impact on Trim Functions

The three trim functions (`gstrltrim` at line 2691, `gstrrtrim` at line 2721, `gstrtrim` at line 2751) all delegate whitespace detection to `gstr_is_whitespace`. Once `gstr_is_whitespace` is updated, all three trim functions will automatically handle Unicode whitespace with **zero code changes** to the trim functions themselves.

This is the key advantage of the current architecture -- the trim functions are already grapheme-aware and pass grapheme pointers and lengths to `gstr_is_whitespace`. They iterate using `utf8_next_grapheme`, so multi-byte whitespace graphemes are already being identified as grapheme boundaries; the only failure is that `gstr_is_whitespace` rejects them.

Test cases that should pass after the fix:
- Trimming strings with U+00A0 (NO-BREAK SPACE) -- common in web-scraped text
- Trimming strings with U+3000 (IDEOGRAPHIC SPACE) -- common in CJK text
- Trimming strings with U+2003 (EM SPACE) -- common in typographic text
- Trimming strings with U+200A (HAIR SPACE) -- common in number formatting
- Trimming strings with mixed ASCII and Unicode whitespace

### A.5 Backward Compatibility

**Recommendation: rename the current function and add both variants.**

- Rename the current function to `gstr_is_whitespace_ascii` (preserving its exact current behavior).
- Make `gstr_is_whitespace` the new Unicode-aware version.
- Do NOT add `gstr_is_whitespace_unicode` as a separate name -- the unqualified name should have the most correct behavior.

Rationale:
- Users who call `gstr_is_whitespace` today are almost certainly expecting "whitespace" to mean "whitespace," not "ASCII whitespace." The current behavior is a bug, not a feature. Fixing it in place is the right default.
- Any user who specifically needs the old ASCII-only behavior (e.g., for protocol parsing where only ASCII whitespace is meaningful, such as HTTP headers) should use the explicitly-named `gstr_is_whitespace_ascii`.
- The trim functions should use the Unicode-aware `gstr_is_whitespace`. No ASCII-only trim variants are needed initially; they can be added later if demand arises.

This is a **behavioral change** that could affect existing users. The CHANGELOG should call this out explicitly. The risk is low because the change only *adds* characters that are detected as whitespace; it never *removes* any. Any string that trimmed correctly before will still trim correctly. Strings that previously failed to trim Unicode whitespace will now trim correctly.

---

## Part B: Unicode Case Operations

### B.1 Scope Assessment

Full Unicode case folding (from `CaseFolding.txt`) involves:

| Status | Description | Entry Count | Relevance |
|--------|-------------|-------------|-----------|
| C (Common) | Simple mappings that work in all contexts | ~1,100 | Primary target |
| S (Simple) | Additional simple mappings for completeness | ~100 | Include with C |
| F (Full) | Mappings that may change string length (1-to-many) | ~100 | Complex; defer or special-case |
| T (Turkic) | Turkish/Azerbaijani locale-specific mappings | 2 | Locale-dependent; defer |

The C+S entries total approximately 1,200 codepoint-to-codepoint mappings. This is well within the feasibility range for a header-only library. For comparison:

- `GCB_RANGES` already contains 620+ entries (line 340+), each 12 bytes (start, end, property).
- `ZERO_WIDTH_RANGES` contains 130+ entries.
- `EAW_WIDE_RANGES` contains 50+ entries.

A case folding table of 1,200 entries at 8 bytes each (two `uint32_t` values: source and target codepoint) would add approximately 9.6 KB to the header. This is comparable to the existing GCB table and well within acceptable bounds.

### B.2 Option A: Simple Case Folding (C+S from CaseFolding.txt)

Add a single static table mapping ~1,200 source codepoints to their case-folded equivalents. All mappings are 1-to-1 (one codepoint maps to one codepoint).

**Pros:**
- Covers Latin, Greek, Cyrillic, Armenian, Georgian, Cherokee, Deseret, and all other scripts with casing
- Case-folding (rather than separate upper/lower tables) is sufficient for case-insensitive comparison, which is the most important use case
- Single table serves `gstrcasecmp`, `gstrncasecmp`, `gstrcasestr`, and `gstr_cmp_grapheme_icase`
- Binary search on sorted table gives O(log n) lookup where n is approximately 1,200; that is approximately 11 comparisons

**Cons:**
- Does not support `gstrlower`/`gstrupper` correctly: case folding maps to lowercase, which works for lower but not for upper (case folding of 'a' is 'a', so you cannot derive the uppercase mapping from it)
- Would require a second table (or extending the first) for uppercase conversion
- Does not handle full case mappings (1-to-many)

### B.3 Option B: Stay ASCII-Only, Rename for Clarity

Rename all case-related functions to have an `_ascii` suffix, making the limitation explicit:
- `gstrcasecmp` -> `gstrcasecmp_ascii`
- `gstrncasecmp` -> `gstrncasecmp_ascii`
- `gstrcasestr` -> `gstrcasestr_ascii`
- `gstrlower` -> `gstrlower_ascii`
- `gstrupper` -> `gstrupper_ascii`

Reserve the unqualified names for future Unicode-aware implementations.

**Pros:**
- Zero risk, zero table size increase
- Makes the API contract honest
- Users who grep for "ascii" will find these functions and understand their limitations

**Cons:**
- Breaking API change with no functional improvement
- Kicks the can down the road indefinitely
- The unqualified names become "reserved" indefinitely, which is confusing

### B.4 Option C: Add Unicode Simple Case Folding Table, Provide Both Variants

Add a Unicode simple case folding table (C+S, ~1,200 entries). Provide both ASCII and Unicode variants of all affected functions. The existing ASCII functions keep their current names but gain `_ascii` aliases. New Unicode-aware functions get the unqualified names.

Concretely:

| Current Name | Becomes (ASCII) | New Unicode Variant |
|---|---|---|
| `gstr_ascii_lower` | stays as-is (internal helper) | `gstr_unicode_casefold` (new internal helper) |
| `gstr_ascii_upper` | stays as-is (internal helper) | `gstr_unicode_upper` (new internal helper) |
| `gstr_cmp_grapheme_icase` | `gstr_cmp_grapheme_icase_ascii` | `gstr_cmp_grapheme_icase` (now Unicode) |
| `gstrcasecmp` | `gstrcasecmp_ascii` | `gstrcasecmp` (now Unicode) |
| `gstrncasecmp` | `gstrncasecmp_ascii` | `gstrncasecmp` (now Unicode) |
| `gstrcasestr` | `gstrcasestr_ascii` | `gstrcasestr` (now Unicode) |
| `gstrlower` | `gstrlower_ascii` | `gstrlower` (now Unicode) |
| `gstrupper` | `gstrupper_ascii` | `gstrupper` (now Unicode) |

**Pros:**
- Complete solution: both ASCII (for performance/protocol parsing) and Unicode (for correctness)
- Unqualified names get the correct behavior
- ASCII variants remain available at zero extra cost (they are trivial inline functions)

**Cons:**
- Requires two tables: one for case folding (comparison) and one for uppercase mapping (conversion)
- More API surface to test and document
- Behavioral change on unqualified names may surprise existing users

### B.5 Recommendation: Option C (Both Variants)

**Option C is the recommended approach,** for the following reasons:

1. **Correctness by default.** A library that handles grapheme clusters, extended pictographic sequences, and UAX #29 segmentation should not silently ignore non-ASCII case folding. Users who write `gstrcasecmp(a, a_len, b, b_len)` reasonably expect it to handle "Straße" vs "STRASSE" or "МОСКВА" vs "москва".

2. **Table size is acceptable.** Two tables (case fold + uppercase) at ~1,200 entries each add approximately 19 KB to the header. The library already contains approximately 15 KB of Unicode tables. This is a 60% increase in table size, but the total remains well under 40 KB -- trivial for any system that processes Unicode text.

3. **ASCII fast path is trivially preserved.** The lookup function can check `codepoint < 0x80` and fall through to the existing ASCII logic before touching the table. The common case (ASCII text) pays zero table-lookup cost.

4. **The `_ascii` aliases provide an explicit opt-out** for users who want the old behavior or need protocol-level ASCII-only semantics.

5. **Incremental implementation.** Case folding (for comparison functions) can be implemented first. Uppercase/lowercase conversion is a second step that can follow.

### B.6 Table Format and Generation

#### B.6.1 Table Structure

Two tables are needed. Both use the same struct:

```c
struct unicode_case_mapping {
    uint32_t from;  /* source codepoint */
    uint32_t to;    /* target codepoint */
};
```

**Table 1: `UNICODE_CASEFOLD_TABLE`** -- Generated from `CaseFolding.txt`, status C and S entries only. Each entry maps a codepoint to its simple case fold. The table is sorted by `from` for binary search. Approximately 1,200 entries.

Purpose: Used by `gstr_cmp_grapheme_icase`, `gstrcasecmp`, `gstrncasecmp`, `gstrcasestr`.

**Table 2: `UNICODE_TOUPPER_TABLE`** -- Generated from `UnicodeData.txt`, field 12 (Simple_Uppercase_Mapping). Each entry maps a codepoint to its uppercase equivalent. The table is sorted by `from` for binary search. Approximately 1,100 entries.

**Table 3: `UNICODE_TOLOWER_TABLE`** -- Generated from `UnicodeData.txt`, field 13 (Simple_Lowercase_Mapping). Each entry maps a codepoint to its lowercase equivalent. The table is sorted by `from` for binary search. Approximately 1,100 entries.

Note: The case fold table is NOT the same as the tolower table. Case folding is designed for caseless matching (e.g., U+00B5 MICRO SIGN case-folds to U+03BC GREEK SMALL LETTER MU), while tolower is designed for case conversion (U+00B5 has no lowercase mapping because it is already lowercase). The distinction matters for comparison vs. conversion.

#### B.6.2 Generation Script

A script (Python or shell) should:

1. Download or read `CaseFolding.txt` from Unicode 16.0.
2. Filter for lines with status `C` or `S`.
3. Parse source and target codepoints.
4. Sort by source codepoint.
5. Emit C table entries in the same format as existing tables (hex literals, 3 entries per line).
6. Similarly process `UnicodeData.txt` for upper/lower tables.

The script should be committed to the repository (e.g., `scripts/gen_case_tables.py`) for reproducibility, similar to how other Unicode data scripts are presumably managed.

#### B.6.3 Binary Search Lookup

A new lookup function:

```c
static uint32_t unicode_casefold(uint32_t codepoint,
                                 const struct unicode_case_mapping *table,
                                 int count)
```

This function performs binary search on the sorted table. If the codepoint is found, it returns `table[mid].to`. If not found, it returns the input codepoint unchanged. This is the same pattern as `unicode_range_contains` (line 1010) but returns a mapped value instead of a boolean.

An ASCII fast path should precede the binary search:

```
if codepoint < 0x80:
    if codepoint >= 'A' && codepoint <= 'Z': return codepoint + 32
    return codepoint
// fall through to binary search
```

This ensures that pure-ASCII strings never touch the table and performance is identical to the current implementation.

#### B.6.4 Integration Points

**`gstr_cmp_grapheme_icase` (line 1573):** Currently calls `gstr_ascii_lower` on each byte. The Unicode-aware version must:
1. Decode each grapheme's codepoints using `utf8_decode`.
2. Case-fold each codepoint using `unicode_casefold`.
3. Compare the folded codepoint sequences.

This is more complex than byte-by-byte comparison because a single grapheme can contain multiple codepoints (base + combining marks). However, combining marks almost never have case foldings, so in practice only the first codepoint per grapheme needs folding. The implementation should fold all codepoints for correctness.

Note on buffer requirements: Since simple case folding is always 1-to-1 (one codepoint to one codepoint), the byte length of a case-folded codepoint may differ from the original (e.g., U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE, 2 bytes in UTF-8, case-folds to U+0069 LATIN SMALL LETTER I, 1 byte). For comparison, this is not an issue -- codepoints are compared as integers, not as byte sequences. For conversion (gstrlower/gstrupper), the output buffer may be shorter or longer than the input; see section B.6.5.

**`gstrcasecmp` (line 1873):** Delegates to `gstr_cmp_grapheme_icase`. Once the latter is updated, `gstrcasecmp` needs no changes.

**`gstrncasecmp` (line 1911):** Same as `gstrcasecmp` -- delegates to `gstr_cmp_grapheme_icase`.

**`gstrcasestr` (line 2209):** Same delegation pattern. No changes needed once `gstr_cmp_grapheme_icase` is updated.

**`gstrlower` (line 2958):** Currently iterates bytes and calls `gstr_ascii_lower` on each. The Unicode-aware version must:
1. Iterate codepoints (not bytes) using `utf8_decode`.
2. Look up each codepoint in `UNICODE_TOLOWER_TABLE`.
3. Encode the result with `utf8_encode` into the destination buffer.
4. Track destination buffer usage since the output byte length may differ from the input (see B.6.5).

**`gstrupper` (line 2999):** Same approach as `gstrlower` but using `UNICODE_TOUPPER_TABLE`.

#### B.6.5 Output Buffer Sizing for Case Conversion

Simple case mapping is 1-to-1 at the codepoint level, but UTF-8 encoding lengths can change:

- U+0130 (C4 B0, 2 bytes) uppercases to itself, lowercases to U+0069 (69, 1 byte): output shrinks by 1 byte.
- U+00FF (C3 BF, 2 bytes) uppercases to U+0178 (C5 B8, 2 bytes): same size.
- U+0250 (C9 90, 2 bytes) uppercases to U+2C6F (E2 B1 AF, 3 bytes): output grows by 1 byte.

In practice, the output is almost always the same length as the input, and growth is at most 1 byte per codepoint. The conversion functions must:
1. Write to the destination buffer and track remaining space.
2. Stop at the last complete grapheme that fits, same as the current approach.
3. The existing grapheme-boundary-respecting loop structure can be preserved; only the inner byte-copy loop changes to a decode-map-encode loop.

Callers should provide a destination buffer at least as large as the source. For pathological cases, a buffer 1.5x the source size guarantees no truncation, but this is an edge case that does not require API changes (the functions already handle truncation gracefully).

### B.7 Special Cases

#### B.7.1 German Eszett (U+00DF)

The lowercase eszett `ß` (U+00DF) case-folds to `ss` (two characters) under *full* case folding (status F in CaseFolding.txt). Under *simple* case folding (status C), U+00DF maps to itself -- it has no single-codepoint case fold.

Since Unicode 5.1, there is also `U+1E9E LATIN CAPITAL LETTER SHARP S` (ẞ). Its simple case fold (status C) is U+00DF.

Impact on gstr:
- `gstrcasecmp` with simple case folding will correctly match ẞ (U+1E9E) to ß (U+00DF).
- `gstrcasecmp` will NOT consider "ß" equal to "ss". This is the expected behavior for simple case folding and matches the behavior of `strcasecmp` in glibc, ICU's `u_strCaseCompare` with `U_FOLD_CASE_DEFAULT`, and most programming language standard libraries.
- `gstrupper` with simple uppercase mapping will convert ß to ẞ (U+1E9E), which is the modern correct behavior (German orthography reform of 2017 officially adopted ẞ).
- Users who need ß/SS equivalence must use full case folding, which is out of scope for this specification. A future `gstrcasecmp_full` could be added if demand arises.

#### B.7.2 Turkish I (Locale-Dependent Dotted/Dotless I)

Turkish and Azerbaijani have special casing rules:
- `I` (U+0049) lowercases to `ı` (U+0131) LATIN SMALL LETTER DOTLESS I (not `i`)
- `İ` (U+0130) LATIN CAPITAL LETTER I WITH DOT ABOVE lowercases to `i` (U+0069) (not `ı`)

These are status `T` (Turkic) entries in CaseFolding.txt and are excluded from the C+S table.

Impact on gstr:
- gstr uses non-locale-aware case folding, same as C `tolower`/`toupper`. Turkish users will get incorrect results for I/i, which is the same limitation as virtually every non-ICU string library.
- This is a known limitation that should be documented.
- A future `gstrcasecmp_locale` taking a locale parameter could address this, but it is out of scope.
- The documentation for the case functions should note: "Case folding follows Unicode Simple Case Folding (non-locale-sensitive). Turkish/Azerbaijani dotless-I rules are not applied."

#### B.7.3 Greek Final Sigma

Greek has context-dependent lowercase for sigma:
- `Σ` (U+03A3) GREEK CAPITAL LETTER SIGMA lowercases to `σ` (U+03C3) in most positions, but to `ς` (U+03C2) GREEK SMALL LETTER FINAL SIGMA at the end of a word.

This is a *context-dependent* mapping specified in `SpecialCasing.txt`, not in `CaseFolding.txt`. Simple case folding maps:
- U+03A3 (Σ) -> U+03C3 (σ)
- U+03C2 (ς) -> U+03C3 (σ)

Impact on gstr:
- **Case-insensitive comparison works correctly.** Both Σ and ς fold to σ, so "ΟΔΥΣΣΕΥΣ", "Οδυσσεύς", and "οδυσσευς" will all compare equal. This is correct.
- **`gstrlower` will produce σ in all positions**, never ς. The output "οδυσσευσ" is technically incorrect Greek (should be "οδυσσευς" with final sigma). This is the expected behavior for simple case mapping and matches ICU's `u_strToLower` without a locale. Context-dependent sigma lowering requires word-boundary detection and is out of scope.
- This should be documented as a known limitation of `gstrlower`.

---

## Implementation Order

The recommended implementation order is:

1. **Phase 1: Unicode Whitespace** -- Low risk, small change, high impact. Modify `gstr_is_whitespace`, add `gstr_is_whitespace_ascii`, verify trim functions work with Unicode whitespace. This can be done independently and shipped immediately.

2. **Phase 2: Case Folding Table + Comparison Functions** -- Add `UNICODE_CASEFOLD_TABLE`, `unicode_casefold` lookup, update `gstr_cmp_grapheme_icase`. This automatically fixes `gstrcasecmp`, `gstrncasecmp`, and `gstrcasestr`. Add `_ascii` variants for backward compatibility.

3. **Phase 3: Case Conversion Tables + `gstrlower`/`gstrupper`** -- Add `UNICODE_TOLOWER_TABLE` and `UNICODE_TOUPPER_TABLE`, update `gstrlower`/`gstrupper` with decode-map-encode loops. Add `gstrlower_ascii`/`gstrupper_ascii` aliases.

4. **Phase 4: Generation Script** -- Commit `scripts/gen_case_tables.py` that reads Unicode data files and emits the C tables, ensuring reproducibility for future Unicode version updates.

Each phase is independently useful and independently testable. Phase 1 has zero dependency on Phases 2-4. Phases 2 and 3 share the table infrastructure but can be shipped separately.

---

## Testing Requirements

### Part A Tests
- Trim each of the 25 whitespace codepoints individually from leading, trailing, and both positions.
- Trim a string containing mixed ASCII and Unicode whitespace (e.g., `"\t\u00A0\u3000Hello\u2003\n"`).
- Confirm that `gstr_is_whitespace_ascii` rejects U+00A0, U+3000, etc.
- Confirm that non-whitespace characters with similar encodings are not false-positived (e.g., U+2001 is whitespace but U+2010 HYPHEN is not).

### Part B Tests
- Case-insensitive comparison of Latin accented characters: "Aequerite" vs "AEQUERITE" (not affected by simple folding -- accented chars have their own entries).
- Case-insensitive comparison of Cyrillic: "Москва" vs "МОСКВА".
- Case-insensitive comparison of Greek with final sigma: "Οδυσσεύς" vs "ΟΔΥΣΣΕΥΣ".
- `gstrlower`/`gstrupper` roundtrip for Latin, Greek, Cyrillic.
- Eszett: confirm ẞ (U+1E9E) folds to ß (U+00DF); confirm ß does not fold to "ss".
- ASCII fast path: confirm performance is not regressed for pure-ASCII strings (benchmark, not unit test).
- Confirm `_ascii` variants maintain exact previous behavior.