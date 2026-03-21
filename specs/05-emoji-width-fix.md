# Emoji Presentation Width Fix -- Specification

## 1. Problem Statement

The gstr library incorrectly computes terminal display width for emoji sequences that rely on **emoji presentation mode** triggered by Variation Selector 16 (VS16, U+FE0F) or the combining enclosing keycap (U+20E3). These sequences render as 2-column-wide glyphs in all modern terminal emulators, but gstr reports width 1 for many of them.

### Affected sequence types

**Keycap sequences** (base + VS16 + U+20E3): Characters `#`, `*`, and digits `0`-`9` followed by U+FE0F and U+20E3 form keycap emoji (e.g., `1️⃣` = U+0031 U+FE0F U+20E3). The base character is ASCII (width 1), VS16 is zero-width, and U+20E3 is zero-width. The current code sums these to width 1. Correct width: 2.

**Text-default Extended_Pictographic + VS16**: Characters like U+2764 HEAVY BLACK HEART have `East_Asian_Width=N` (Neutral) and default to text presentation. When followed by VS16, they switch to emoji presentation and render at width 2 in terminals. Example: `❤️` (U+2764 U+FE0F) currently computes as 1 + 0 = 1. Correct width: 2. Other affected characters include U+2622 RADIOACTIVE SIGN, U+2623 BIOHAZARD SIGN, U+261D WHITE UP POINTING INDEX, U+2620 SKULL AND CROSSBONES, U+270D WRITING HAND, U+2744 SNOWFLAKE, and dozens more in the Extended_Pictographic table that are not in the EAW Wide/Fullwidth table.

**Already-correct cases (by coincidence)**: Some Extended_Pictographic characters like U+2615 HOT BEVERAGE are already in `EAW_WIDE_RANGES`, so the base character alone produces width 2. Adding VS16 does not change the result. These happen to compute correctly despite the missing presentation logic.

## 2. Root Cause

The width computation in both `gstrwidth()` (line 1664) and `gstr_grapheme_width()` (line 3354) follows this decision tree:

1. Scan the grapheme cluster for ZWJ (U+200D) and regional indicator pairs.
2. If ZWJ or flag pair is found: return width 2.
3. **Otherwise**: iterate codepoints in the cluster, summing each individual `utf8_charwidth()` result. This delegates to `utf8_cpwidth()`, which checks `ZERO_WIDTH_RANGES` (returns 0), then `EAW_WIDE_RANGES` (returns 2), then defaults to 1.

The problem is in step 3. The "sum individual codepoint widths" approach is only correct for non-emoji grapheme clusters (e.g., base + combining diacriticals). For emoji presentation sequences, the entire grapheme cluster renders as a single 2-column glyph regardless of the individual codepoint widths. The code has no awareness of VS16-triggered emoji presentation mode.

Specifically:
- U+FE0F is in `ZERO_WIDTH_RANGES` (line 135: `{0xFE00, 0xFE0F}`), so it contributes width 0. This is correct for its role as a variation selector, but the code never acts on the *semantic meaning* of VS16 -- that it switches the preceding Extended_Pictographic character to emoji presentation (width 2).
- U+20E3 is in `ZERO_WIDTH_RANGES` (line 121: `{0x20D0, 0x20F0}`), so it contributes width 0. Again correct in isolation, but the code never recognizes that a keycap sequence is a single wide emoji.
- The base characters of affected sequences (digits, `#`, `*`, U+2764, etc.) are not in `EAW_WIDE_RANGES`, so they get width 1 from the default case in `utf8_cpwidth()`.

## 3. Detection Logic

Within the grapheme cluster scanning loop (which already exists to detect ZWJ and regional indicators), add detection of three additional signals:

### 3a. VS16 emoji presentation (ExtPict + U+FE0F)

**Rule**: If the grapheme cluster contains U+FE0F AND the base codepoint (first codepoint in the cluster) has the Extended_Pictographic property, the entire cluster has width 2.

Rationale: Per UTS #51, VS16 following an Extended_Pictographic character requests emoji presentation, which renders as a wide glyph. The `is_extended_pictographic()` function already exists and can be reused.

### 3b. Keycap sequences (base + U+FE0F + U+20E3)

**Rule**: If the grapheme cluster contains U+20E3 (COMBINING ENCLOSING KEYCAP), the entire cluster has width 2.

Rationale: The keycap base characters (`#` U+0023, `*` U+002A, `0`-`9` U+0030-U+0039) are NOT Extended_Pictographic, so rule 3a alone would not catch them. However, keycap sequences always render as 2-column emoji in terminals. Checking for U+20E3 is sufficient because U+20E3 only forms valid sequences with these specific base characters.

Note: In practice, keycap sequences always contain VS16 between the base and U+20E3 (e.g., `1` U+FE0F U+20E3), but checking for U+20E3 alone is more robust and handles edge cases where VS16 might be absent.

### 3c. VS15 text presentation override (ExtPict + U+FE0E)

**Rule**: If the grapheme cluster contains U+FE0E (VS15, Variation Selector 15) AND the base codepoint is Extended_Pictographic, do NOT apply emoji presentation width. Instead, fall through to the existing codepoint-width-summing logic, which will produce the text-presentation width (typically 1).

Rationale: VS15 explicitly requests text presentation. For example, `❤︎` (U+2764 U+FE0E) should render as a narrow text-style heart at width 1. This is the inverse of VS16.

Note: U+FE0E is already in `ZERO_WIDTH_RANGES` (within the `{0xFE00, 0xFE0F}` range), so it already contributes width 0 in the summing path. The key is to ensure that VS15 suppresses the emoji-presentation-width override rather than triggers it.

### Priority of checks

The evaluation should follow this order within the grapheme cluster scanning loop:

1. Check for ZWJ -> width 2 (existing logic, keep as-is)
2. Check for regional indicator pair -> width 2 (existing logic, keep as-is)
3. Check for U+20E3 (keycap) -> width 2
4. Check for U+FE0F with ExtPict base, in absence of U+FE0E -> width 2
5. Fall through to codepoint-width sum (existing logic)

## 4. Code Changes

### 4a. Extract shared width function

Both `gstrwidth()` (line 1664) and `gstr_grapheme_width()` (line 3354) contain identical grapheme width logic. The fix should first extract the shared logic into a single static inline function. The natural approach:

Make `gstr_grapheme_width()` the canonical implementation and have `gstrwidth()` call it for each grapheme cluster. `gstrwidth()` currently inlines the same logic; it should be refactored to:

```
while (offset < byte_len) {
    int next = utf8_next_grapheme(s, byte_len, offset);
    width += gstr_grapheme_width(s, byte_len, offset, next);
    offset = next;
}
```

### 4b. Modify the grapheme cluster scanning loop

In `gstr_grapheme_width()`, the existing scanning loop already iterates all codepoints to detect ZWJ and regional indicators. Extend it to also track:

- `has_vs16`: set to 1 if U+FE0F is encountered
- `has_vs15`: set to 1 if U+FE0E is encountered
- `has_keycap`: set to 1 if U+20E3 is encountered
- `base_cp`: the first codepoint in the cluster (captured on the first iteration)

### 4c. Add new width rules after the scanning loop

After the existing `if (has_zwj || regional_count == 2)` block, add:

```
/* Keycap sequences: base + FE0F + 20E3 → width 2 */
if (has_keycap) {
    return 2;
}

/* VS16 emoji presentation: ExtPict + FE0F (without VS15) → width 2 */
if (has_vs16 && !has_vs15 && is_extended_pictographic(base_cp)) {
    /* Only override if the base isn't already wide */
    int base_width = utf8_cpwidth(base_cp);
    if (base_width < 2) {
        return 2;
    }
}
```

When the base character is already EAW Wide (e.g., U+2615), `utf8_cpwidth()` returns 2, and we skip the override to avoid any change in behavior for already-correct cases. The fall-through to the sum-of-codepoint-widths path will produce 2 + 0 = 2 as before.

### 4d. Functions to modify

| Function | File | Line | Change |
|----------|------|------|--------|
| `gstr_grapheme_width()` | `gstr.h` | 3354 | Add VS16/VS15/keycap detection and width override logic |
| `gstrwidth()` | `gstr.h` | 1664 | Replace inline logic with call to `gstr_grapheme_width()` |

No changes to `utf8_cpwidth()`, `utf8_charwidth()`, or any of the Unicode range tables.

## 5. Edge Cases

### 5a. ExtPict characters already wide via EAW (e.g., U+2615 HOT BEVERAGE)

U+2615 is both Extended_Pictographic and in `EAW_WIDE_RANGES`. The sequence `☕️` (U+2615 U+FE0F) should remain width 2, not become width 4 or any other value. The proposed logic handles this: `utf8_cpwidth(base_cp)` returns 2 for U+2615, so the VS16 override is skipped, and the existing sum-of-widths path produces 2 + 0 = 2.

### 5b. ExtPict characters without VS16 that have default emoji presentation (e.g., U+1F600 GRINNING FACE)

Characters in the range U+1F300-U+1F9FF have `East_Asian_Width=W` and are in `EAW_WIDE_RANGES`. They already compute as width 2 via `utf8_cpwidth()`. No VS16 is needed and typically none is present. These are unaffected by this change.

### 5c. Non-ExtPict characters with VS16 (e.g., regular text + U+FE0F)

VS16 after a non-Extended_Pictographic character is semantically meaningless for emoji presentation. The proposed logic checks `is_extended_pictographic(base_cp)` before applying the width override, so non-ExtPict bases are unaffected. The VS16 will continue to contribute width 0 in the summing path, which is correct.

### 5d. Text presentation with VS15 (e.g., U+2764 U+FE0E)

`❤︎` (U+2764 U+FE0E) explicitly requests text presentation. The `has_vs15` flag suppresses the emoji presentation override even though the base is ExtPict. The summing path produces 1 + 0 = 1, which is correct for text-style rendering.

### 5e. Both VS15 and VS16 in the same cluster

This is not a valid Unicode sequence, but defensively: `has_vs15` takes precedence, suppressing the emoji width override. This matches the principle that VS15 explicitly requests text mode.

### 5f. Skin tone modifiers on ExtPict

Sequences like U+261D U+1F3FB (pointing up + light skin tone) form a single grapheme cluster via GB9 (Extend). The skin tone modifier U+1F3FB is in `ZERO_WIDTH_RANGES` (line 188: `{0x1F3FB, 0x1F3FF}`). These sequences typically also contain VS16 (U+261D U+FE0F U+1F3FB), which the proposed logic handles: VS16 + ExtPict base = width 2. Without VS16, the base U+261D has width 1 from EAW, and the modifier has width 0, producing width 1 -- which is correct for text-presentation mode of that character.

### 5g. Tag sequences (e.g., flag subdivisions)

Sequences like U+1F3F4 U+E0067 U+E0062 U+E0073 U+E0063 U+E0074 U+E007F (Scotland flag) contain tag characters in `ZERO_WIDTH_RANGES`. These do not contain VS16, and U+1F3F4 is in `EAW_WIDE_RANGES`, so they already compute correctly at width 2.

### 5h. Keycap without VS16 (e.g., U+0031 U+20E3)

Rare but possible: a bare keycap without VS16. The `has_keycap` check catches this unconditionally, producing width 2, which matches terminal rendering behavior.

## 6. Code Duplication

The grapheme width logic is currently duplicated verbatim between two functions:

- **`gstrwidth()`** at line 1664-1711: iterates grapheme clusters, inlines the scanning + width logic
- **`gstr_grapheme_width()`** at line 3354-3390: identical scanning + width logic, extracted as a helper

This duplication means every width-related fix must be applied in two places, increasing the risk of divergence. The duplication already exists today -- the two copies are identical.

**Recommendation**: As part of this fix, refactor `gstrwidth()` to delegate to `gstr_grapheme_width()` for each grapheme cluster. The resulting `gstrwidth()` becomes a simple loop:

```
static inline size_t gstrwidth(const char *s, size_t byte_len) {
    if (!s || byte_len == 0) return 0;
    size_t width = 0;
    int offset = 0;
    while ((size_t)offset < byte_len) {
        int next = utf8_next_grapheme(s, (int)byte_len, offset);
        width += gstr_grapheme_width(s, byte_len, offset, next);
        offset = next;
    }
    return width;
}
```

This ensures a single source of truth for width calculation and makes the emoji presentation fix a one-place change.

Note: `gstr_grapheme_width()` is defined at line 3354, after `gstrwidth()` at line 1664. Either `gstr_grapheme_width()` must be moved earlier in the file (before `gstrwidth()`), or a forward declaration must be added. Moving the function is preferred to keep the code self-contained without forward declarations.

## 7. Testing

The following test cases should be added, each asserting the expected terminal column width:

| Input | Codepoints | Expected Width | Rationale |
|-------|-----------|---------------|-----------|
| `1️⃣` | U+0031 U+FE0F U+20E3 | 2 | Keycap sequence; currently produces 1 |
| `#️⃣` | U+0023 U+FE0F U+20E3 | 2 | Keycap with non-digit base |
| `❤️` | U+2764 U+FE0F | 2 | Text-default ExtPict + VS16; currently produces 1 |
| `☕️` | U+2615 U+FE0F | 2 | Already-wide ExtPict + VS16; should remain 2 |
| `❤︎` | U+2764 U+FE0E | 1 | Text presentation via VS15; should be 1 |
| `😀` | U+1F600 | 2 | Default emoji presentation, EAW=W; no VS16 needed |
| `🇺🇸` | U+1F1FA U+1F1F8 | 2 | Regional indicator flag pair; existing logic |
| `👨‍👩‍👧` | U+1F468 U+200D U+1F469 U+200D U+1F467 | 2 | ZWJ family sequence; existing logic |
| `☠️` | U+2620 U+FE0F | 2 | Skull and crossbones; text-default ExtPict + VS16 |
| `✈️` | U+2708 U+FE0F | 2 | Airplane; text-default ExtPict + VS16 |
| `❤` | U+2764 (bare, no VS) | 1 | Bare text-default ExtPict without VS16; text width |
| `☕` | U+2615 (bare, no VS) | 2 | Bare emoji-default ExtPict; already EAW wide |
| `A️` | U+0041 U+FE0F | 1 | Non-ExtPict + VS16; VS16 should be ignored for width |
| `*️⃣` | U+002A U+FE0F U+20E3 | 2 | Keycap asterisk |
| `↔️` | U+2194 U+FE0F | 2 | Left-right arrow; ExtPict + VS16, EAW=N |
| `©️` | U+00A9 U+FE0F | 2 | Copyright sign; ExtPict + VS16, EAW=N |
| `1⃣` | U+0031 U+20E3 | 2 | Keycap without VS16 (edge case) |

Tests should use raw byte sequences (not source-file emoji literals) to avoid encoding ambiguity, and should call both `gstrwidth()` and `gstr_grapheme_width()` to verify they produce identical results after the refactoring.