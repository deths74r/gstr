# Specification: Fix InCB Linker and Consonant Tables, Auto-Generate from Unicode Data

## 1. Problem Statement

### 1.1 INCB_LINKERS: 23 Spurious Entries, 2 Missing Entries

The current `INCB_LINKERS` table in `include/gstr.h` (line 936) contains **43 entries**. The authoritative Unicode 17.0 `DerivedCoreProperties.txt` defines exactly **20** codepoints with `InCB=Linker`. The table has 23 spurious entries that are actually `InCB=Extend` (viramas/signs that happen to have virama-like behavior but are NOT classified as InCB Linkers), and is missing 2 entries added in Unicode 16.0/17.0.

**Spurious entries (23 codepoints present in table but NOT InCB=Linker in Unicode 17.0):**

| Codepoint | Character Name | Actual InCB Value |
|-----------|---------------|-------------------|
| U+103A | MYANMAR SIGN ASAT | Extend |
| U+1714 | TAGALOG SIGN VIRAMA | Extend |
| U+1715 | TAGALOG SIGN PAMUDPOD | Extend |
| U+1BAA | SUNDANESE SIGN PAMAAEH | Extend |
| U+A806 | SYLOTI NAGRI SIGN HASANTA | Extend |
| U+A8C4 | SAURASHTRA SIGN VIRAMA | Extend |
| U+11046 | BRAHMI VIRAMA | Extend |
| U+110B9 | KAITHI SIGN VIRAMA | Extend |
| U+11134 | CHAKMA MAAYYAA | Extend |
| U+111C0 | SHARADA SIGN VIRAMA | Extend |
| U+11235 | KHOJKI SIGN VIRAMA | Extend |
| U+1134D | GRANTHA SIGN VIRAMA | Extend |
| U+11442 | NEWA SIGN VIRAMA | Extend |
| U+114C2 | TIRHUTA SIGN VIRAMA | Extend |
| U+115BF | SIDDHAM SIGN VIRAMA | Extend |
| U+1163F | MODI SIGN VIRAMA | Extend |
| U+116B6 | TAKRI SIGN VIRAMA | Extend |
| U+1172B | AHOM SIGN KILLER | Extend |
| U+11839 | DOGRA SIGN VIRAMA | Extend |
| U+119E0 | NANDINAGARI SIGN VIRAMA | Extend |
| U+11A34 | ZANABAZAR SQUARE SIGN VIRAMA (actually cluster-final mark) | Extend |
| U+11C3F | BHAIKSUKI SIGN VIRAMA | Extend |
| U+11D45 | MASARAM GONDI VIRAMA | Extend |
| U+11D97 | GUNJALA GONDI VIRAMA | Extend |
| U+11F41 | KAWI SIGN KILLER | Extend |

Note: All 23 spurious entries are virama-like signs in various scripts. They have `InCB=Extend`, not `InCB=Linker`. While they function as conjunct-formers in their scripts' orthographies, Unicode's GB9c rule specifically requires the `InCB=Linker` classification, and only 20 codepoints carry it.

**Missing entries (2 codepoints absent from table but ARE InCB=Linker in Unicode 17.0):**

| Codepoint | Character Name | Script |
|-----------|---------------|--------|
| U+113D0 | TULU-TIGALARI CONJOINER | Tulu-Tigalari (added Unicode 16.0) |
| U+1193E | DIVES AKURU VIRAMA | Dives Akuru (added Unicode 14.0, InCB assigned later) |

### 1.2 INCB_CONSONANTS: Numerous Spurious and Missing Entries

The current `INCB_CONSONANTS` table contains ranges from many scripts that do NOT have `InCB=Consonant` in Unicode 17.0, and is missing ranges for scripts that do.

**Scripts with spurious consonant ranges (present in table, absent from Unicode 17.0 InCB=Consonant):**

- **Bengali**: Ranges {0x09E0, 0x09E1} are included but NOT InCB=Consonant. The range {0x09DF, 0x09E1} should be just {0x09DF, 0x09DF}.
- **Gurmukhi (Punjabi)**: Entire block {0x0A15-0x0A74} is present but NO Gurmukhi codepoints have InCB=Consonant in Unicode 17.0.
- **Oriya**: Range {0x0B5F, 0x0B61} should be just {0x0B5F, 0x0B5F}. U+0B60-0B61 are vowels, not consonants.
- **Tamil**: The entire Tamil consonant block {0x0B95-0x0BB9} is present but NO Tamil codepoints have InCB=Consonant in Unicode 17.0.
- **Kannada**: The entire Kannada consonant block {0x0C95-0x0CE1} is present but NO Kannada codepoints have InCB=Consonant.
- **Telugu**: Ranges {0x0C58-0x0C5A}, {0x0C5D, 0x0C5D}, {0x0C60-0x0C61} are present but NOT InCB=Consonant.
- **Malayalam**: Ranges {0x0D54-0x0D56}, {0x0D5F-0x0D61}, {0x0D7A-0x0D7F} are present but NOT InCB=Consonant.
- **Myanmar**: Multiple over-inclusive ranges. The table has {0x1000, 0x1025}, {0x1027, 0x1027}, {0x1029, 0x102A} but the correct range is the single contiguous {0x1000, 0x102A} plus {0x103F}.
- **Tagalog (Philippine scripts)**: {0x1703-0x1711} is present but NOT InCB=Consonant.
- **Khmer**: Ranges {0x1780-0x17A2}, {0x17A5-0x17A7}, {0x17A9-0x17B3} should be the single range {0x1780-0x17B3}.
- **Limbu, Tai Tham, New Tai Lue, Tai Le, Tai Viet**: Multiple blocks present but NOT InCB=Consonant. Specifically: {0x1901-0x1931} (Limbu region), {0x1950-0x196D} (Tai Le), {0x1980-0x19C7} (New Tai Lue), {0x1A00-0x1A16} (Buginese).
- **Balinese**: {0x1B05-0x1B33} is over-inclusive. Correct ranges are {0x1B0B-0x1B0C}, {0x1B13-0x1B33}, {0x1B45-0x1B4C}.
- **Sundanese**: {0x1B83-0x1BA0} is present but missing {0x1BAE-0x1BAF} and {0x1BBB-0x1BBD}.
- **Syloti Nagri**: {0xA807-0xA822} is present but NOT InCB=Consonant.
- **Saurashtra**: {0xA882-0xA8B3} is present but NOT InCB=Consonant.
- **Rejang**: {0xA90A-0xA925} is present but NOT InCB=Consonant.
- **Cham**: {0xAA00-0xAA28} is present but NOT InCB=Consonant.
- **Javanese**: Over-inclusive. Table has {0xA989-0xA98B}, {0xA98D-0xA9B2} but correct is {0xA989-0xA98B}, {0xA98F-0xA9B2}.
- **Myanmar (extended)**: Missing ranges {0xA9E0-0xA9E4}, {0xA9E7-0xA9EF}, {0xA9FA-0xA9FE}.
- **Myanmar Khamti**: Ranges {0xAA60-0xAA76} are over-inclusive. Correct: {0xAA60-0xAA6F}, {0xAA71-0xAA73}, {0xAA7A}, {0xAA7E-0xAA7F}.
- **Meetei Mayek**: {0xAAE0-0xAAEA} is present but missing {0xABC0-0xABDA}.

**Scripts with missing consonant ranges (absent from table, present in Unicode 17.0 InCB=Consonant):**

- **Tulu-Tigalari**: Entirely absent. Should include {0x11380-0x11389}, {0x1138B}, {0x1138E}, {0x11390-0x113B5}.
- **Dives Akuru**: Entirely absent. Should include {0x11900-0x11906}, {0x11909}, {0x1190C-0x11913}, {0x11915-0x11916}, {0x11918-0x1192F}.
- **Meetei Mayek (base block)**: {0xABC0-0xABDA} is absent.
- **Gujarati**: Missing U+0AF9 (GUJARATI LETTER ZHA).
- **Myanmar**: Missing U+103F (MYANMAR LETTER GREAT SA).
- **Balinese**: Missing {0x1B0B-0x1B0C} and {0x1B45-0x1B4C}.
- **Sundanese**: Missing {0x1BAE-0x1BAF} and {0x1BBB-0x1BBD}.
- **Soyombo**: Range should be {0x11A50}, {0x11A5C-0x11A83} but table has {0x11A5C-0x11A89} (over-inclusive) and is missing {0x11A50}.
- **Chakma**: Table has {0x11107-0x1112B} and {0x11150-0x11172} which are incorrect. Correct: {0x11103-0x11126}, {0x11144}, {0x11147}.

**Many additional ranges in the table (Brahmi, Kaithi, Sharada, Khojki, Khudawadi, Grantha, Newa, Tirhuta, Siddham, Modi, Takri, Ahom, Dogra, Nandinagari, Bhaiksuki, Marchen, Masaram Gondi, Gunjala Gondi, Makasar)** are either entirely absent from `InCB=Consonant` or have different boundaries than what Unicode 17.0 specifies.

### 1.3 Summary of Impact

The current tables were built by conflating "scripts that have viramas" with "codepoints that have InCB=Linker" and "letters in scripts with viramas" with "codepoints that have InCB=Consonant". This causes:

- **False positive conjunct joining**: 23 spurious linkers cause grapheme clusters to be incorrectly joined in scripts like Tagalog, Syloti Nagri, Saurashtra, Brahmi, Kaithi, Grantha, Newa, Tirhuta, Siddham, Modi, Takri, Ahom, Dogra, Nandinagari, Bhaiksuki, Masaram Gondi, and Gunjala Gondi.
- **Missing conjunct joining**: Tulu-Tigalari and Dives Akuru scripts will break grapheme clusters at conjuncts where they should not.
- **Over-segmentation in some scripts, under-segmentation in others**: The consonant table errors compound with linker errors to produce incorrect grapheme boundaries.

## 2. Root Cause

The tables were **hand-constructed** by enumerating virama/halanta codepoints and consonant letter ranges from script block descriptions, rather than being derived from the normative `Indic_Conjunct_Break` property in `DerivedCoreProperties.txt`.

Evidence:
1. The `gen_unicode_tables.py` script generates EAW, Extended_Pictographic, and GCB tables but does **not** generate InCB tables.
2. The INCB_LINKERS array contains every virama-like codepoint across Brahmic scripts, whereas only a specific subset has the `InCB=Linker` property. The author likely searched for "virama" or "sign virama" in the Unicode character database and included all matches.
3. The INCB_CONSONANTS array contains letter ranges from scripts that have viramas, which looks like it was generated from script block boundaries or `Lo` (Letter, other) ranges within Brahmic script blocks.
4. Neither table includes Tulu-Tigalari or Dives Akuru, which were assigned InCB properties in Unicode 16.0, suggesting the tables were authored against an older reference or a simplified source.

The `InCB` (Indic_Conjunct_Break) property was introduced in Unicode 15.1 and is a **derived** property specifically designed for UAX #29 grapheme cluster boundary rule GB9c. It is NOT simply "all viramas" or "all consonant letters in Brahmic scripts" -- it is a curated subset.

## 3. Data Source

### 3.1 InCB Property

- **File**: `DerivedCoreProperties.txt`
- **URL**: `https://www.unicode.org/Public/17.0.0/ucd/DerivedCoreProperties.txt`
- **Property values**: `InCB; Linker`, `InCB; Consonant`, `InCB; Extend`
- **Format**: Standard UCD semicolon-delimited, e.g.:
  ```
  094D          ; InCB; Linker # Mn       DEVANAGARI SIGN VIRAMA
  0915..0939    ; InCB; Consonant # Lo  [37] DEVANAGARI LETTER KA..DEVANAGARI LETTER HA
  0300..036F    ; InCB; Extend # Mn [112] COMBINING GRAVE ACCENT..COMBINING LATIN SMALL LETTER X
  ```
- **Note**: The InCB property has three fields separated by semicolons: codepoint/range, property name (`InCB`), and value (`Linker`, `Consonant`, or `Extend`). This differs from GBP and EAW files which have two fields.

### 3.2 Zero-Width Ranges (General Category)

- **File**: `DerivedGeneralCategory.txt` or equivalently `UnicodeData.txt`
- **URL**: `https://www.unicode.org/Public/17.0.0/ucd/extracted/DerivedGeneralCategory.txt`
- **Categories**: `Mn` (Nonspacing_Mark), `Me` (Enclosing_Mark), `Cf` (Format)
- **Format**: Standard UCD semicolon-delimited:
  ```
  0300..036F    ; Mn # [112] COMBINING GRAVE ACCENT..COMBINING LATIN SMALL LETTER X
  0600..0605    ; Cf #   [6] ARABIC NUMBER SIGN..ARABIC NUMBER MARK ABOVE
  ```

## 4. Script Changes to `gen_unicode_tables.py`

### 4.1 Add DerivedCoreProperties.txt to Downloads

Add to the `FILES` dictionary:
```
"DerivedCoreProperties.txt": f"{BASE_URL}/DerivedCoreProperties.txt"
```

**Warning**: This file is large (~1.5 MB). Caching is already handled by the `download()` function.

### 4.2 Add DerivedGeneralCategory.txt to Downloads

Add to the `FILES` dictionary:
```
"DerivedGeneralCategory.txt": f"{BASE_URL}/extracted/DerivedGeneralCategory.txt"
```

### 4.3 New Parsing Function for InCB

The existing `parse_ranges()` function expects a two-field format (`codepoint ; property`). The InCB entries in `DerivedCoreProperties.txt` use a three-field format (`codepoint ; InCB ; value`). A new parsing approach is needed:

1. Filter lines containing `; InCB;`
2. Parse the three-field format: `XXXX(.YYYY)? ; InCB ; (Linker|Consonant|Extend)`
3. The regex should be: `([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*InCB\s*;\s*(\S+)`

### 4.4 Generate INCB_LINKERS

1. Collect all entries where the InCB value is `Linker`.
2. Each entry is a single codepoint (linkers are always single codepoints, not ranges).
3. Sort the codepoints numerically.
4. Output as a flat `uint32_t` array (matching current format).
5. Output the `INCB_LINKER_COUNT` macro.

Expected output (20 entries for Unicode 17.0):
```c
static const uint32_t INCB_LINKERS[] = {
    0x094D, 0x09CD, 0x0ACD, 0x0B4D, 0x0C4D, 0x0D4D, 0x1039, 0x17D2,
    0x1A60, 0x1B44, 0x1BAB, 0xA9C0, 0xAAF6, 0x10A3F, 0x11133, 0x113D0,
    0x1193E, 0x11A47, 0x11A99, 0x11F42,
};
#define INCB_LINKER_COUNT (sizeof(INCB_LINKERS) / sizeof(INCB_LINKERS[0]))
```

### 4.5 Generate INCB_CONSONANTS

1. Collect all entries where the InCB value is `Consonant`.
2. Each entry may be a single codepoint or a range.
3. Sort by start codepoint.
4. Merge adjacent/overlapping ranges using `merge_adjacent_no_prop()`.
5. Output as a `struct unicode_range` array (matching current format).
6. Output the `INCB_CONSONANT_COUNT` macro.

### 4.6 Output Format

The C output should match the existing style:
- Hex values: `0x` prefix, uppercase hex digits, 4-digit minimum for BMP, 5-digit for SMP.
- 3 entries per line for `unicode_range` arrays, 8 entries per line for flat `uint32_t` arrays.
- Trailing comma on each line.
- Standard `sizeof`-based count macros.

The Hare output should follow the existing Hare table format in `gstr-hare/gstr/tables_gcb.ha`.

### 4.7 InCB=Extend (Not Needed Separately)

The `InCB=Extend` values should NOT be generated as a separate table. They overlap almost exactly with `GCB=Extend` (plus ZWJ), and the GB9c rule implementation in `gstr.h` already uses the GCB Extend property for the "Extend*" portion of the rule. Only `Linker` and `Consonant` need dedicated tables.

## 5. Auto-Generate ZERO_WIDTH_RANGES

### 5.1 Current State

The `ZERO_WIDTH_RANGES` table (lines 63-190 in `gstr.h`) is hand-maintained and covers General Categories Mn, Me, and Cf. The comment says "Unicode 17.0" but there is no automated generation or validation.

### 5.2 Data Source

- **File**: `DerivedGeneralCategory.txt`
- **URL**: `https://www.unicode.org/Public/17.0.0/ucd/extracted/DerivedGeneralCategory.txt`
- **Format**: `XXXX(.YYYY)? ; Gc # comment` where Gc is the two-letter General Category code.

### 5.3 Generation Logic

1. Parse all entries with General Category `Mn`, `Me`, or `Cf`.
2. Convert to `(start, end)` pairs.
3. Sort by start codepoint.
4. Merge adjacent/overlapping ranges using `merge_adjacent_no_prop()`.
5. **Exclude** the following codepoints that are already handled specially in `utf8_display_width()` or should not be zero-width:
   - U+00AD (SOFT HYPHEN) -- handled separately with width=1 return before ZERO_WIDTH check
   - U+1160..U+11FF (Hangul Jungseong/Jongseong) -- these are Mn but are handled by GCB Hangul logic
   - Actually, review the current table: it does NOT include Hangul Jungseong, so exclude them.
6. **Include** U+1F3FB..U+1F3FF (EMOJI MODIFIER FITZPATRICK types) -- these are `Sk` (Symbol, modifier) not Mn/Me/Cf, but the current table includes them because they are zero-width when modifying a preceding emoji. **Decision needed**: either keep this as a manual addition, or document it as a deliberate exception.
7. Output as `struct unicode_range` array matching current format.

### 5.4 Special Considerations

The current ZERO_WIDTH_RANGES includes some entries that are NOT Mn/Me/Cf:
- U+1F3FB..U+1F3FF: Category `Sk` (emoji skin tone modifiers). These are GCB=Extend and render as zero-width when following an emoji base. They should be retained as a manual addition or the generation logic should also include GCB=Extend codepoints with `Sk` category.

The generation function should:
1. Collect Mn + Me + Cf ranges from DerivedGeneralCategory.txt.
2. Add the known `Sk` exceptions (U+1F3FB..U+1F3FF).
3. Exclude BOM-like codepoints if they are already handled elsewhere (review needed).
4. Merge and sort.

### 5.5 Output

```c
/* Zero-width character ranges (Unicode 17.0).
 * Mn (Nonspacing Mark), Me (Enclosing Mark), Cf (Format).
 * Generated by scripts/gen_unicode_tables.py from DerivedGeneralCategory.txt.
 */
static const struct unicode_range ZERO_WIDTH_RANGES[] = {
    ...
};
#define ZERO_WIDTH_COUNT (sizeof(ZERO_WIDTH_RANGES) / sizeof(ZERO_WIDTH_RANGES[0]))
```

## 6. Table Validation

### 6.1 Automated Validation in the Script

Add the following validation steps to `gen_unicode_tables.py`:

1. **No overlaps**: Apply `validate_no_overlaps()` to all generated range tables (already done for EAW, ExtPict, GCB; extend to INCB_CONSONANTS and ZERO_WIDTH_RANGES).
2. **Sorted order**: Verify each table is sorted by start codepoint.
3. **Count verification**: Print entry counts to stderr for manual review.
4. **Linker count sanity check**: Warn if INCB_LINKERS count differs from expected (20 for Unicode 17.0). This catches parsing errors.
5. **Cross-table consistency**:
   - Every INCB_LINKER codepoint should also appear in GCB_RANGES as GCB_EXTEND (since all InCB=Linker codepoints have GCB=Extend or SpacingMark).
   - No INCB_CONSONANT range should overlap with any INCB_LINKER codepoint.
   - No INCB_CONSONANT range should overlap with GCB_RANGES entries that have GCB_EXTEND.

### 6.2 External Validation

To verify against official Unicode test data:
1. Download `GraphemeBreakTest.txt` from `https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakTest.txt`.
2. This file contains test cases for grapheme cluster boundaries including GB9c (Indic conjunct) sequences.
3. Run the existing test suite against these cases after the table update.

### 6.3 Spot-Check Script

Add a `--validate` flag to the script that:
1. Downloads `DerivedCoreProperties.txt`.
2. Extracts all InCB=Linker and InCB=Consonant codepoints.
3. Compares them against the generated tables entry-by-entry.
4. Reports any discrepancies.

## 7. Testing Impact

### 7.1 Files Requiring Updates

| File | Change Required |
|------|----------------|
| `include/gstr.h` | Replace INCB_LINKERS and INCB_CONSONANTS tables with generated versions |
| `gstr-hare/gstr/tables_gcb.ha` | Replace Hare INCB_LINKERS and INCB_CONSONANTS tables |
| `scripts/gen_unicode_tables.py` | Add InCB and ZERO_WIDTH generation |
| `test/test_gstr_stress.c` | Review Devanagari conjunct test case (line 142-146); may need additional InCB-specific tests |
| `test/test_grapheme_walk.c` | Add test cases for Indic conjunct sequences |

### 7.2 New Tests to Add

1. **Basic InCB conjunct test**: Devanagari KA + VIRAMA + KA should be 1 grapheme cluster.
2. **Tulu-Tigalari conjunct test**: Tulu-Tigalari consonant + U+113D0 (CONJOINER) + consonant should be 1 grapheme cluster.
3. **Dives Akuru conjunct test**: Dives Akuru consonant + U+1193E (VIRAMA) + consonant should be 1 grapheme cluster.
4. **Non-linker virama test**: BRAHMI VIRAMA (U+11046) should NOT cause conjunct joining (it is Extend, not Linker). A Brahmi consonant + U+11046 + Brahmi consonant should be 2 grapheme clusters (the virama extends the first cluster but does not link to the next consonant).
5. **Regression test for spurious linkers**: For each of the 23 removed spurious linkers, verify that sequences like `consonant + spurious_linker + consonant` produce 2 grapheme clusters, not 1.
6. **Meetei Mayek test**: U+ABC0 (MEETEI MAYEK LETTER KOK) + U+AAF6 (MEETEI MAYEK VIRAMA) + U+ABC1 should be 1 grapheme cluster (was missing from consonant table).

### 7.3 Existing Tests That May Change Behavior

The `test_gstr_stress.c` Hindi test case (line 142):
```
"\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87"
```
This is: NA + MA + SA + VIRAMA(094D) + TA + E-MATRA. VIRAMA U+094D is a correct InCB=Linker, so this test should continue to pass unchanged (SA + VIRAMA + TA forms a conjunct). Verify the expected grapheme count (currently 4) is still correct.

## 8. Version Tracking

### 8.1 Current State

The Unicode version is tracked in two ways:
1. A Python constant: `UNICODE_VERSION = "17.0.0"` in `gen_unicode_tables.py` (line 27).
2. Comments in `gstr.h`: `/* Unicode 17.0 */` on each table.

### 8.2 Recommended Improvements

1. **Add a C macro**: Define `GSTR_UNICODE_VERSION "17.0.0"` in `gstr.h` at the top of the Unicode tables section. This makes it machine-readable from C code.

2. **Add a generation timestamp comment**: Each generated table should include a comment like:
   ```c
   /* Generated by gen_unicode_tables.py from Unicode 17.0.0 data */
   ```

3. **Script version assertion**: In `gen_unicode_tables.py`, after downloading, parse the `@missing` line or file header from each Unicode data file to confirm the version matches `UNICODE_VERSION`. Example: `DerivedCoreProperties.txt` contains the line `# DerivedCoreProperties-17.0.0.txt`. Verify this matches.

4. **Hare version tracking**: Add a constant to the Hare tables:
   ```hare
   export const UNICODE_VERSION: str = "17.0.0";
   ```

5. **Future Unicode upgrades**: When Unicode 18.0 is released, changing `UNICODE_VERSION` in the script and re-running should be sufficient to regenerate all tables. Document this in the script's docstring.

---