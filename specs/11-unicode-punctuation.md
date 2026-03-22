# Specification: Unicode Punctuation Classification for gstr

## Status

**Draft** -- Prepared 2026-03-22

## Motivation

The md4s streaming markdown parser (https://github.com/deths74r/md4s) needs Unicode-aware word boundary detection for CommonMark emphasis delimiter flanking rules. The CommonMark specification (section 2.1) defines "Unicode punctuation character" as:

> A Unicode punctuation character is a character in the Unicode general categories Pc, Pd, Pe, Pf, Pi, Po, or Ps, or in the ASCII range that would be classified as punctuation by `ispunct()`.

Additionally, some implementations (including md4c, the reference parser) also classify Unicode Symbol categories (Sc, Sk, Sm, So) as punctuation for the purposes of delimiter flanking, since the spec's definition of "other" (non-whitespace, non-punctuation) maps to letters, digits, and "mark" characters only.

gstr currently provides `gstr_is_whitespace()` for the whitespace half of this classification but has no codepoint-level punctuation classification. This spec adds it.

## Scope

Add two public functions to `gstr.h`:

1. `gstr_is_unicode_punctuation(uint32_t cp)` -- classifies a codepoint against Unicode General_Category P* (all punctuation subcategories)
2. `gstr_is_unicode_punct_or_symbol(uint32_t cp)` -- classifies against P* + S* (punctuation and symbols, matching CommonMark's practical needs)

Add one static range table:

3. `UNICODE_PUNCT_RANGES[]` -- generated from `DerivedGeneralCategory.txt`, covering categories Pc, Pd, Pe, Pf, Pi, Po, Ps, Sc, Sk, Sm, So

---

## Part A: Unicode General Categories

### A.1 Relevant Categories

The Unicode General_Category property assigns every codepoint to one of 30 categories. The punctuation and symbol categories are:

**Punctuation (P):**

| Category | Name | Example Characters |
|----------|------|--------------------|
| Pc | Connector punctuation | `_` (U+005F), `‿` (U+203F) |
| Pd | Dash punctuation | `-` (U+002D), `—` (U+2014), `–` (U+2013) |
| Pe | Close punctuation | `)` (U+0029), `]` (U+005D), `}` (U+007D) |
| Pf | Final quote punctuation | `»` (U+00BB), `'` (U+2019), `"` (U+201D) |
| Pi | Initial quote punctuation | `«` (U+00AB), `'` (U+2018), `"` (U+201C) |
| Po | Other punctuation | `.` `!` `?` `#` `%` `&` `@` `\` `,` `:` `;` `/` and many more |
| Ps | Open punctuation | `(` (U+0028), `[` (U+005B), `{` (U+007B) |

**Symbols (S):**

| Category | Name | Example Characters |
|----------|------|--------------------|
| Sc | Currency symbol | `$` `€` `£` `¥` `₹` |
| Sk | Modifier symbol | `^` `` ` `` `¨` `¯` `˜` |
| Sm | Math symbol | `+` `<` `=` `>` `|` `~` `×` `÷` `≠` `≤` |
| So | Other symbol | `©` `®` `°` `†` `‡` `§` `¶` and many more |

### A.2 Codepoint Count

From Unicode 17.0.0 `DerivedGeneralCategory.txt`:

| Categories | Approximate Codepoint Count | Approximate Range Count |
|------------|---------------------------|------------------------|
| P* (all punctuation) | ~800 | ~90 ranges |
| S* (all symbols) | ~7,700 | ~180 ranges |
| P* + S* combined | ~8,500 | ~250 ranges (after merging adjacent) |

The P*-only table is compact (~90 ranges, ~720 bytes). The combined P*+S* table is larger (~250 ranges, ~2KB) but still well within acceptable bounds for a header-only library. For reference, the existing `GCB_RANGES` table has 620+ entries (~7.4KB).

### A.3 Why Include Symbols?

The CommonMark spec says "Unicode punctuation character" means General_Category P*. However, the spec also implicitly relies on the three-way classification {whitespace, punctuation, other} where "other" means letters and digits. ASCII symbols like `$`, `+`, `<`, `>`, `|`, `~`, `^` are all classified as "punctuation" by `ispunct()` in C, but their Unicode General_Category is S* (Symbol), not P* (Punctuation).

If we implement only P* and not S*, then `$` (Sc), `+` (Sm), `~` (Sm), `<` (Sm), `>` (Sm), `|` (Sm), `^` (Sk), and `` ` `` (Sk) would NOT be classified as punctuation. This would break emphasis flanking for these very common ASCII characters: `*$100*` would not produce italic because `$` wouldn't be considered punctuation/word-boundary.

The CommonMark spec handles this by saying the definition applies only to non-ASCII characters, and for ASCII characters, `ispunct()` is used. But providing a single unified function that covers both ASCII `ispunct()` and Unicode P*+S* is cleaner and avoids the caller needing to handle the ASCII/Unicode split.

**Recommendation: Provide both functions.** `gstr_is_unicode_punctuation()` for strict P*-only (matches the spec letter). `gstr_is_unicode_punct_or_symbol()` for P*+S* (matches practical needs). The md4s integration will use the P*+S* variant combined with an ASCII fast-path.

---

## Part B: Implementation

### B.1 Table Generation

Extend `scripts/gen_unicode_tables.py` to generate a new range table from `DerivedGeneralCategory.txt`:

```python
def gen_punct_ranges(dgc_text):
    """Generate P* (punctuation) ranges from DerivedGeneralCategory.txt."""
    punct_cats = {'Pc', 'Pd', 'Pe', 'Pf', 'Pi', 'Po', 'Ps'}
    ranges = parse_ranges(dgc_text, lambda p: p in punct_cats)
    return merge_adjacent(ranges)

def gen_punct_symbol_ranges(dgc_text):
    """Generate P* + S* (punctuation + symbol) ranges."""
    cats = {'Pc', 'Pd', 'Pe', 'Pf', 'Pi', 'Po', 'Ps',
            'Sc', 'Sk', 'Sm', 'So'}
    ranges = parse_ranges(dgc_text, lambda p: p in cats)
    return merge_adjacent(ranges)
```

The `merge_adjacent()` function should merge ranges that are contiguous or overlapping to minimize table size. The existing `gen_unicode_tables.py` already has the `parse_ranges()` infrastructure and downloads `DerivedGeneralCategory.txt`.

Output format matching existing tables:

```c
/* Unicode Punctuation + Symbol ranges (General_Category P* + S*) */
/* Generated by scripts/gen_unicode_tables.py from DerivedGeneralCategory.txt. */
/* Unicode 17.0.0 */

static const struct unicode_range UNICODE_PUNCT_SYMBOL_RANGES[] = {
    {0x0021, 0x002F},   /* ! " # $ % & ' ( ) * + , - . / */
    {0x003A, 0x0040},   /* : ; < = > ? @ */
    {0x005B, 0x0060},   /* [ \ ] ^ _ ` */
    {0x007B, 0x007E},   /* { | } ~ */
    {0x00A1, 0x00A9},   /* ¡ ¢ £ ¤ ¥ ¦ § ¨ © */
    /* ... hundreds more ranges ... */
};
#define UNICODE_PUNCT_SYMBOL_COUNT \
    (sizeof(UNICODE_PUNCT_SYMBOL_RANGES) / sizeof(UNICODE_PUNCT_SYMBOL_RANGES[0]))
```

If the team decides to also generate a P*-only table, emit it as a separate array:

```c
static const struct unicode_range UNICODE_PUNCT_RANGES[] = {
    /* P* categories only */
};
#define UNICODE_PUNCT_COUNT \
    (sizeof(UNICODE_PUNCT_RANGES) / sizeof(UNICODE_PUNCT_RANGES[0]))
```

### B.2 Table Placement in gstr.h

Place the new tables in the "Unicode Width Tables" section (currently starting at line 65), after the existing `EAW_WIDE_RANGES` table and before the "Internal Helpers" section. This groups all Unicode property tables together.

```
/* Unicode Width Tables */
    ZERO_WIDTH_RANGES[]          (existing)
    EAW_WIDE_RANGES[]            (existing)
    UNICODE_PUNCT_SYMBOL_RANGES[] (new)
    UNICODE_PUNCT_RANGES[]        (new, optional)
```

### B.3 Function Signatures

Place in the "Internal Helpers" section alongside the existing `is_extended_pictographic()`, `is_incb_linker()`, etc. functions:

```c
/*
 * Returns 1 if the codepoint is a Unicode punctuation character
 * (General_Category Pc, Pd, Pe, Pf, Pi, Po, or Ps), 0 otherwise.
 *
 * This matches the CommonMark specification's definition of
 * "Unicode punctuation character" (section 2.1).
 *
 * Note: ASCII punctuation (U+0021-U+002F, U+003A-U+0040,
 * U+005B-U+0060, U+007B-U+007E) is included in the P* categories,
 * so this function returns 1 for all characters where C's ispunct()
 * would return non-zero in the C locale, plus all non-ASCII
 * punctuation.
 */
static inline int gstr_is_unicode_punctuation(uint32_t cp) {
    return unicode_range_contains(cp, UNICODE_PUNCT_RANGES,
                                  UNICODE_PUNCT_COUNT);
}

/*
 * Returns 1 if the codepoint is a Unicode punctuation or symbol
 * character (General_Category P* or S*), 0 otherwise.
 *
 * This is the practical classification needed for CommonMark
 * emphasis delimiter flanking rules, where the three-way split
 * is {whitespace, punctuation/symbol, letter/digit/mark}.
 *
 * Covers all ASCII characters that C's ispunct() detects, plus
 * all Unicode punctuation (Pc, Pd, Pe, Pf, Pi, Po, Ps) and
 * symbols (Sc, Sk, Sm, So).
 */
static inline int gstr_is_unicode_punct_or_symbol(uint32_t cp) {
    return unicode_range_contains(cp, UNICODE_PUNCT_SYMBOL_RANGES,
                                  UNICODE_PUNCT_SYMBOL_COUNT);
}
```

### B.4 ASCII Fast Path

The `unicode_range_contains()` binary search is already fast (O(log n), ~8 comparisons for 250 ranges). However, since the overwhelming majority of calls will be for ASCII codepoints (< 0x80), an explicit ASCII fast path can avoid the binary search entirely:

```c
static inline int gstr_is_unicode_punct_or_symbol(uint32_t cp) {
    /* ASCII fast path */
    if (cp < 0x80) {
        return (cp >= 0x21 && cp <= 0x2F) ||
               (cp >= 0x3A && cp <= 0x40) ||
               (cp >= 0x5B && cp <= 0x60) ||
               (cp >= 0x7B && cp <= 0x7E);
    }
    return unicode_range_contains(cp, UNICODE_PUNCT_SYMBOL_RANGES,
                                  UNICODE_PUNCT_SYMBOL_COUNT);
}
```

This ASCII fast path matches exactly the characters that C's `ispunct()` returns non-zero for in the POSIX/C locale. The four ranges are:

| Range | Characters |
|-------|-----------|
| 0x21-0x2F | `! " # $ % & ' ( ) * + , - . /` |
| 0x3A-0x40 | `: ; < = > ? @` |
| 0x5B-0x60 | `` [ \ ] ^ _ ` `` |
| 0x7B-0x7E | `{ \| } ~` |

### B.5 Performance Analysis

**Binary search cost:** For ~250 ranges, `unicode_range_contains` performs at most ceil(log2(250)) = 8 comparisons. Each comparison is two `uint32_t` comparisons. Total: ~16 comparisons worst case.

**With ASCII fast path:** For codepoints < 0x80 (the vast majority in markdown text), the function performs 4 range checks (8 comparisons worst case) and never touches the range table.

**Comparison to existing functions:**
- `gstr_is_whitespace()` uses a switch statement on 25 values
- `is_extended_pictographic()` uses `unicode_range_contains` on ~140 ranges
- `utf8_is_zerowidth()` uses `unicode_range_contains` on ~130 ranges

The new function is comparable in cost to existing functions and is called only at emphasis delimiter boundaries (not for every byte of input), so the total performance impact on any calling application is negligible.

---

## Part C: Testing

### C.1 MC/DC Test Requirements

The function has the following compound decisions requiring MC/DC coverage:

1. **ASCII fast path** (4-way disjunction): each range independently tested
2. **Binary search** (`unicode_range_contains`): already tested by existing callers, but should have specific punctuation tests

### C.2 Required Test Cases

**ASCII punctuation (P* categories):**

| Test | Input | Expected | Category | Purpose |
|------|-------|----------|----------|---------|
| exclamation | `0x0021` (`!`) | 1 | Po | ASCII punct, first range start |
| slash | `0x002F` (`/`) | 1 | Po | ASCII punct, first range end |
| colon | `0x003A` (`:`) | 1 | Po | Second range start |
| at_sign | `0x0040` (`@`) | 1 | Po | Second range end |
| open_bracket | `0x005B` (`[`) | 1 | Ps | Third range start |
| backtick | `0x0060` (`` ` ``) | 1 | Sk→Symbol | Third range end |
| open_brace | `0x007B` (`{`) | 1 | Ps | Fourth range start |
| tilde | `0x007E` (`~`) | 1 | Sm→Symbol | Fourth range end |

**ASCII non-punctuation (letters, digits, control):**

| Test | Input | Expected | Purpose |
|------|-------|----------|---------|
| digit_0 | `0x0030` (`0`) | 0 | Between punct ranges |
| letter_A | `0x0041` (`A`) | 0 | Between punct ranges |
| letter_a | `0x0061` (`a`) | 0 | Between punct ranges |
| space | `0x0020` (` `) | 0 | Below first punct range |
| nul | `0x0000` | 0 | Edge case |
| del | `0x007F` | 0 | Above last ASCII punct range |

**Unicode punctuation (non-ASCII, P* categories):**

| Test | Input | Expected | Category | Purpose |
|------|-------|----------|----------|---------|
| inverted_exclamation | `0x00A1` (`¡`) | 1 | Po | First non-ASCII punct |
| left_guillemet | `0x00AB` (`«`) | 1 | Pi | Initial quote |
| right_guillemet | `0x00BB` (`»`) | 1 | Pf | Final quote |
| em_dash | `0x2014` (`—`) | 1 | Pd | Dash punct |
| en_dash | `0x2013` (`–`) | 1 | Pd | Dash punct |
| left_single_quote | `0x2018` (`'`) | 1 | Pi | Opening quote |
| right_single_quote | `0x2019` (`'`) | 1 | Pf | Closing quote |
| left_double_quote | `0x201C` (`"`) | 1 | Pi | Opening quote |
| right_double_quote | `0x201D` (`"`) | 1 | Pf | Closing quote |
| ellipsis | `0x2026` (`…`) | 1 | Po | Other punct |
| fullwidth_period | `0xFF0E` (`．`) | 1 | Po | CJK fullwidth |
| fullwidth_exclamation | `0xFF01` (`！`) | 1 | Po | CJK fullwidth |
| cjk_comma | `0x3001` (`、`) | 1 | Po | CJK punct |
| cjk_period | `0x3002` (`。`) | 1 | Po | CJK punct |
| left_corner_bracket | `0x300C` (`「`) | 1 | Ps | CJK open bracket |

**Unicode symbols (non-ASCII, S* categories) — for `punct_or_symbol` variant:**

| Test | Input | Expected | Category | Purpose |
|------|-------|----------|----------|---------|
| copyright | `0x00A9` (`©`) | 1 | So | Other symbol |
| registered | `0x00AE` (`®`) | 1 | So | Other symbol |
| degree | `0x00B0` (`°`) | 1 | So | Other symbol |
| euro | `0x20AC` (`€`) | 1 | Sc | Currency |
| yen | `0x00A5` (`¥`) | 1 | Sc | Currency |
| multiply | `0x00D7` (`×`) | 1 | Sm | Math |
| divide | `0x00F7` (`÷`) | 1 | Sm | Math |
| not_equal | `0x2260` (`≠`) | 1 | Sm | Math |
| section | `0x00A7` (`§`) | 1 | Po→Punct | Other punct |
| dagger | `0x2020` (`†`) | 1 | Po | Other punct |

**Unicode non-punctuation, non-symbol:**

| Test | Input | Expected | Purpose |
|------|-------|----------|---------|
| latin_a_umlaut | `0x00E4` (`ä`) | 0 | Ll (lowercase letter) |
| cyrillic_A | `0x0410` (`А`) | 0 | Lu (uppercase letter) |
| cjk_ideograph | `0x4E2D` (`中`) | 0 | Lo (other letter) |
| arabic_alef | `0x0627` (`ا`) | 0 | Lo (other letter) |
| combining_acute | `0x0301` | 0 | Mn (non-spacing mark) |
| devanagari_ka | `0x0915` (`क`) | 0 | Lo (other letter) |
| nb_space | `0x00A0` | 0 | Zs (space — whitespace, not punct) |
| em_space | `0x2003` | 0 | Zs (space) |
| line_separator | `0x2028` | 0 | Zl (line separator) |

**Boundary cases:**

| Test | Input | Expected | Purpose |
|------|-------|----------|---------|
| max_bmp | `0xFFFF` | depends | End of BMP |
| surrogate_start | `0xD800` | 0 | Surrogates are Cs, not P/S |
| supplementary | `0x10000` | 0 | First supplementary (Lo) |
| max_codepoint | `0x10FFFF` | 0 | Last valid codepoint |
| above_max | `0x110000` | 0 | Out of range |

### C.3 Test File

Add `test/test_unicode_punct.c` (or append to an existing test file). Follow the existing test conventions in `test/test_gstr.c`.

---

## Part D: gen_unicode_tables.py Changes

### D.1 New Generator Function

Add to `gen_unicode_tables.py`:

```python
def gen_punct_symbol_table(dgc_text, output_format="c"):
    """Generate Unicode Punctuation + Symbol range table.

    Extracts all codepoints with General_Category in:
      P*: Pc, Pd, Pe, Pf, Pi, Po, Ps (punctuation)
      S*: Sc, Sk, Sm, So (symbols)

    These are the categories needed for CommonMark emphasis
    delimiter flanking rules.
    """
    target_cats = {
        'Pc', 'Pd', 'Pe', 'Pf', 'Pi', 'Po', 'Ps',
        'Sc', 'Sk', 'Sm', 'So'
    }
    ranges = parse_ranges(dgc_text, lambda p: p in target_cats)
    merged = merge_adjacent_ranges([(s, e) for s, e, _ in ranges])

    if output_format == "c":
        print("/* Unicode Punctuation + Symbol ranges "
              "(General_Category P* + S*) */")
        print("/* Generated by scripts/gen_unicode_tables.py "
              "from DerivedGeneralCategory.txt. */")
        print(f"/* Unicode {UNICODE_VERSION} */")
        print()
        print("static const struct unicode_range "
              "UNICODE_PUNCT_SYMBOL_RANGES[] = {")
        for i, (start, end) in enumerate(merged):
            comma = "," if i < len(merged) - 1 else ""
            print(f"    {{0x{start:04X}, 0x{end:04X}}}{comma}")
        print("};")
        print(f"#define UNICODE_PUNCT_SYMBOL_COUNT \\")
        print(f"    (sizeof(UNICODE_PUNCT_SYMBOL_RANGES) / "
              f"sizeof(UNICODE_PUNCT_SYMBOL_RANGES[0]))")
        print()
        print(f"/* Total: {len(merged)} ranges covering "
              f"~{sum(e - s + 1 for s, e in merged)} codepoints */")


def gen_punct_only_table(dgc_text, output_format="c"):
    """Generate Unicode Punctuation-only range table (P* without S*)."""
    target_cats = {'Pc', 'Pd', 'Pe', 'Pf', 'Pi', 'Po', 'Ps'}
    ranges = parse_ranges(dgc_text, lambda p: p in target_cats)
    merged = merge_adjacent_ranges([(s, e) for s, e, _ in ranges])

    if output_format == "c":
        print("/* Unicode Punctuation ranges "
              "(General_Category P* only) */")
        print("/* Generated by scripts/gen_unicode_tables.py "
              "from DerivedGeneralCategory.txt. */")
        print(f"/* Unicode {UNICODE_VERSION} */")
        print()
        print("static const struct unicode_range "
              "UNICODE_PUNCT_RANGES[] = {")
        for i, (start, end) in enumerate(merged):
            comma = "," if i < len(merged) - 1 else ""
            print(f"    {{0x{start:04X}, 0x{end:04X}}}{comma}")
        print("};")
        print(f"#define UNICODE_PUNCT_COUNT \\")
        print(f"    (sizeof(UNICODE_PUNCT_RANGES) / "
              f"sizeof(UNICODE_PUNCT_RANGES[0]))")


def merge_adjacent_ranges(ranges):
    """Merge ranges that are contiguous or overlapping.

    Input: sorted list of (start, end) tuples.
    Output: merged list with no gaps between adjacent ranges.
    """
    if not ranges:
        return []
    ranges.sort()
    merged = [ranges[0]]
    for start, end in ranges[1:]:
        prev_start, prev_end = merged[-1]
        if start <= prev_end + 1:
            merged[-1] = (prev_start, max(prev_end, end))
        else:
            merged.append((start, end))
    return merged
```

### D.2 Integration with Existing Script

Add calls to `gen_punct_symbol_table()` and `gen_punct_only_table()` in the main output section of `gen_unicode_tables.py`, after the existing table generation calls. The script already downloads `DerivedGeneralCategory.txt` (it's in the `FILES` dict at line 41).

### D.3 splice_tables.py Integration

The existing `splice_tables.py` script handles replacing generated tables in `gstr.h`. Add markers for the new tables following the existing pattern. The team should add comments like:

```c
/* BEGIN GENERATED: UNICODE_PUNCT_SYMBOL_RANGES */
/* END GENERATED: UNICODE_PUNCT_SYMBOL_RANGES */
```

---

## Part E: Backward Compatibility

### E.1 No Breaking Changes

These are purely additive:
- New range tables (data, no code change)
- New functions (`gstr_is_unicode_punctuation`, `gstr_is_unicode_punct_or_symbol`)
- No existing function signatures change
- No existing behavior changes

### E.2 Binary Size Impact

| Component | Estimated Size |
|-----------|---------------|
| `UNICODE_PUNCT_RANGES[]` (~90 ranges) | ~720 bytes |
| `UNICODE_PUNCT_SYMBOL_RANGES[]` (~250 ranges) | ~2,000 bytes |
| `gstr_is_unicode_punctuation()` function | ~40 bytes (static inline + binary search call) |
| `gstr_is_unicode_punct_or_symbol()` function | ~60 bytes (inline with ASCII fast path) |
| **Total** | **~2.8 KB** |

For reference, the existing `GCB_RANGES` table is ~7.4 KB. The new tables add roughly 35% to the existing table footprint. Applications that don't call the new functions will not pull in the tables (static linkage + LTO/gc-sections).

---

## Part F: Relationship to Existing Specs

### F.1 Spec 06 (Unicode Whitespace)

Spec 06 added Unicode whitespace detection. This spec adds the complementary punctuation detection. Together, they enable the three-way classification needed by CommonMark:

```
is_whitespace(cp)           → gstr_is_whitespace()        [spec 06]
is_punctuation(cp)          → gstr_is_unicode_punct_or_symbol()  [this spec]
is_other(cp)                → !is_whitespace && !is_punctuation  [derived]
```

### F.2 Consumer: md4s

The primary consumer is `md4s.c` which will replace its ASCII-only `is_word_boundary()` function with:

```c
#include <gstr.h>

static int classify_flanking(const char *text, size_t len, size_t pos,
                             bool look_before)
{
    /* Returns: 0=whitespace, 1=punctuation, 2=other */
    if (look_before && pos == 0) return 0;
    if (!look_before && pos >= len) return 0;

    uint32_t cp;
    if (look_before) {
        size_t prev = utf8_prev(text, len, pos);
        utf8_decode(text + prev, len - prev, &cp);
    } else {
        utf8_decode(text + pos, len - pos, &cp);
    }

    if (cp == 0) return 0;
    if (gstr_is_whitespace_cp(cp)) return 0;
    if (gstr_is_unicode_punct_or_symbol(cp)) return 1;
    return 2;
}
```

Note: this also requires a codepoint-level `gstr_is_whitespace_cp(uint32_t cp)` variant of `gstr_is_whitespace()` that takes a decoded codepoint directly, rather than a grapheme `(bytes, len)` pair. If spec 06 hasn't added this yet, this spec recommends it as a convenience:

```c
static inline int gstr_is_whitespace_cp(uint32_t cp) {
    switch (cp) {
    case 0x0009: case 0x000A: case 0x000B: case 0x000C:
    case 0x000D: case 0x0020: case 0x0085: case 0x00A0:
    case 0x1680:
    case 0x2000: case 0x2001: case 0x2002: case 0x2003:
    case 0x2004: case 0x2005: case 0x2006: case 0x2007:
    case 0x2008: case 0x2009: case 0x200A:
    case 0x2028: case 0x2029: case 0x202F: case 0x205F:
    case 0x3000:
        return 1;
    default:
        return 0;
    }
}
```

---

## Summary

| Deliverable | Description |
|-------------|-------------|
| `UNICODE_PUNCT_RANGES[]` | Static range table, P* categories (~90 ranges) |
| `UNICODE_PUNCT_SYMBOL_RANGES[]` | Static range table, P*+S* categories (~250 ranges) |
| `gstr_is_unicode_punctuation()` | Binary search on P* table |
| `gstr_is_unicode_punct_or_symbol()` | ASCII fast path + binary search on P*+S* table |
| `gstr_is_whitespace_cp()` | Codepoint-level whitespace check (switch, 25 cases) |
| `gen_unicode_tables.py` changes | New generator functions for P* and P*+S* tables |
| `test/test_unicode_punct.c` | MC/DC test suite (~40 test cases) |
