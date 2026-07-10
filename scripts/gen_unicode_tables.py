#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
"""
Generate Unicode property tables for gstr.h from official Unicode data files.

Downloads Unicode 17.0 data files (cached locally) and produces C array
literals for:
  - ZERO_WIDTH_RANGES[]           (General_Category = Mn, Me, Cf + skin tones)
  - EAW_WIDE_RANGES[]             (East_Asian_Width = W or F)
  - EXTENDED_PICTOGRAPHIC_RANGES[] (Extended_Pictographic = Yes)
  - GCB_RANGES[]                  (Grapheme_Cluster_Break, non-overlapping)
  - INCB_LINKERS[]                (InCB = Linker)
  - INCB_CONSONANTS[]             (InCB = Consonant)
  - UNICODE_PUNCT_SYMBOL_RANGES[] (General_Category P* + S*)

Usage:
    python3 scripts/gen_unicode_tables.py > /dev/null  # just validate
    python3 scripts/gen_unicode_tables.py              # print C tables to stdout

Paste the output into include/gstr.h to replace the hand-maintained tables.
"""

import argparse
import os
import re
import sys
import urllib.request
from pathlib import Path

UNICODE_VERSION = "17.0.0"
BASE_URL = f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd"
EMOJI_URL = f"https://www.unicode.org/Public/{UNICODE_VERSION}/ucd/emoji"

CACHE_DIR = Path(__file__).parent / ".unicode_cache"

FILES = {
    "EastAsianWidth.txt": f"{BASE_URL}/EastAsianWidth.txt",
    "emoji-data.txt": f"{EMOJI_URL}/emoji-data.txt",
    "GraphemeBreakProperty.txt": f"{BASE_URL}/auxiliary/GraphemeBreakProperty.txt",
    "DerivedCoreProperties.txt": f"{BASE_URL}/DerivedCoreProperties.txt",
    "DerivedGeneralCategory.txt": f"{BASE_URL}/extracted/DerivedGeneralCategory.txt",
}


def download(name, url):
    """Download a Unicode data file, caching locally."""
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    path = CACHE_DIR / name
    if path.exists():
        return path.read_text()
    print(f"Downloading {url} ...", file=sys.stderr)
    req = urllib.request.Request(url, headers={"User-Agent": "gstr-gen/1.0"})
    with urllib.request.urlopen(req) as resp:
        data = resp.read().decode("utf-8")
    path.write_text(data)
    return data


def parse_ranges(text, filter_fn):
    """Parse Unicode data file lines into (start, end, property) tuples.

    Each non-comment, non-blank line has the format:
        XXXX..YYYY ; Property # comment
    or  XXXX       ; Property # comment
    """
    results = []
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if not line:
            continue
        m = re.match(r"([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*(\S+)", line)
        if not m:
            continue
        start = int(m.group(1), 16)
        end = int(m.group(2), 16) if m.group(2) else start
        prop = m.group(3)
        if filter_fn(prop):
            results.append((start, end, prop))
    results.sort()
    return results


def parse_incb(text, value):
    """Parse InCB entries from DerivedCoreProperties.txt.

    InCB lines have the three-field format:
        XXXX(.YYYY)? ; InCB ; (Linker|Consonant|Extend)
    """
    results = []
    for line in text.splitlines():
        comment_stripped = line.split("#")[0].strip()
        if not comment_stripped:
            continue
        if "; InCB;" not in comment_stripped and "; InCB ;" not in comment_stripped:
            continue
        m = re.match(
            r"([0-9A-Fa-f]+)(?:\.\.([0-9A-Fa-f]+))?\s*;\s*InCB\s*;\s*(\S+)",
            comment_stripped,
        )
        if not m:
            continue
        start = int(m.group(1), 16)
        end = int(m.group(2), 16) if m.group(2) else start
        incb_value = m.group(3)
        if incb_value == value:
            results.append((start, end))
    results.sort()
    return results


def merge_adjacent(ranges):
    """Merge adjacent/overlapping ranges with same property into single ranges."""
    if not ranges:
        return []
    merged = [list(ranges[0])]
    for start, end, prop in ranges[1:]:
        prev = merged[-1]
        if prop == prev[2] and start <= prev[1] + 1:
            prev[1] = max(prev[1], end)
        else:
            merged.append([start, end, prop])
    return [tuple(r) for r in merged]


def merge_adjacent_no_prop(ranges):
    """Merge adjacent/overlapping (start, end) ranges (no property field)."""
    if not ranges:
        return []
    merged = [list(ranges[0])]
    for start, end in ranges[1:]:
        prev = merged[-1]
        if start <= prev[1] + 1:
            prev[1] = max(prev[1], end)
        else:
            merged.append([start, end])
    return [tuple(r) for r in merged]


def generate_eaw_wide(eaw_text):
    """Generate EAW_WIDE_RANGES from EastAsianWidth.txt (W and F only)."""
    ranges = parse_ranges(eaw_text, lambda p: p in ("W", "F"))
    # Flatten to (start, end) pairs and merge adjacent
    pairs = [(s, e) for s, e, _ in ranges]
    pairs.sort()
    merged = merge_adjacent_no_prop(pairs)
    return merged


def generate_extended_pictographic(emoji_text):
    """Generate EXTENDED_PICTOGRAPHIC_RANGES from emoji-data.txt."""
    ranges = parse_ranges(emoji_text, lambda p: p == "Extended_Pictographic")
    pairs = [(s, e) for s, e, _ in ranges]
    pairs.sort()
    merged = merge_adjacent_no_prop(pairs)
    return merged


def generate_zero_width(gc_text):
    """Generate ZERO_WIDTH_RANGES from DerivedGeneralCategory.txt.

    Includes Mn (Nonspacing Mark), Me (Enclosing Mark), Cf (Format).
    Also includes U+1F3FB..U+1F3FF (emoji skin tone modifiers, category Sk)
    which are zero-width when modifying a preceding emoji, and the conjoining
    Hangul jamo vowels/finals (U+1160..U+11FF, U+D7B0..U+D7C6,
    U+D7CB..U+D7FB, category Lo): wcwidth-heritage terminals (xterm, kitty,
    foot) render them zero-width, so cursor math must agree — and decomposed
    Hangul (L+V+T) then sums to the syllable width of 2.
    """
    ranges = parse_ranges(gc_text, lambda p: p in ("Mn", "Me", "Cf"))
    pairs = [(s, e) for s, e, _ in ranges]
    # Add skin tone modifiers (Sk but zero-width in emoji contexts)
    pairs.append((0x1F3FB, 0x1F3FF))
    # Add conjoining Hangul jamo vowels/finals (Lo but zero-width in terminals)
    pairs.append((0x1160, 0x11FF))
    pairs.append((0xD7B0, 0xD7C6))
    pairs.append((0xD7CB, 0xD7FB))
    pairs.sort()
    merged = merge_adjacent_no_prop(pairs)
    return merged


def generate_incb_linkers(dcp_text):
    """Generate INCB_LINKERS from DerivedCoreProperties.txt (InCB=Linker)."""
    ranges = parse_incb(dcp_text, "Linker")
    # Linkers are always single codepoints, flatten
    codepoints = []
    for start, end in ranges:
        for cp in range(start, end + 1):
            codepoints.append(cp)
    codepoints.sort()
    return codepoints


def generate_incb_consonants(dcp_text):
    """Generate INCB_CONSONANTS from DerivedCoreProperties.txt (InCB=Consonant)."""
    ranges = parse_incb(dcp_text, "Consonant")
    merged = merge_adjacent_no_prop(ranges)
    return merged


def generate_punct_symbol(gc_text):
    """Generate UNICODE_PUNCT_SYMBOL_RANGES from DerivedGeneralCategory.txt.

    Includes all Punctuation (Pc, Pd, Pe, Pf, Pi, Po, Ps) and
    Symbol (Sc, Sk, Sm, So) categories. This matches the CommonMark 0.31.2
    definition of "Unicode punctuation character."
    """
    target_cats = {'Pc', 'Pd', 'Pe', 'Pf', 'Pi', 'Po', 'Ps',
                   'Sc', 'Sk', 'Sm', 'So'}
    ranges = parse_ranges(gc_text, lambda p: p in target_cats)
    pairs = [(s, e) for s, e, _ in ranges]
    pairs.sort()
    merged = merge_adjacent_no_prop(pairs)
    return merged


# GCB property name to C enum mapping
GCB_MAP = {
    "CR": "GCB_CR",
    "LF": "GCB_LF",
    "Control": "GCB_CONTROL",
    "Extend": "GCB_EXTEND",
    "ZWJ": "GCB_ZWJ",
    "Regional_Indicator": "GCB_REGIONAL_INDICATOR",
    "Prepend": "GCB_PREPEND",
    "SpacingMark": "GCB_SPACING_MARK",
    "L": "GCB_L",
    "V": "GCB_V",
    "T": "GCB_T",
    "LV": "GCB_LV",
    "LVT": "GCB_LVT",
}


def generate_gcb(gbp_text):
    """Generate non-overlapping GCB_RANGES from GraphemeBreakProperty.txt.

    Strategy: collect all (start, end, property) ranges, then for each
    codepoint in overlapping regions, the more specific (narrower/later)
    entry wins. We flatten to per-codepoint assignments, then re-merge.
    """
    raw = parse_ranges(gbp_text, lambda p: p in GCB_MAP)

    # Filter out Hangul syllables (LV/LVT computed algorithmically in C code)
    raw = [(s, e, p) for s, e, p in raw if not (p in ("LV", "LVT"))]

    # Build per-codepoint map for overlapping ranges.
    # To handle overlaps properly: sort by range width descending (broadest first),
    # then narrower ranges overwrite broader ones.
    raw_sorted = sorted(raw, key=lambda r: (r[1] - r[0]), reverse=True)

    # Find all codepoints that appear in any range
    cp_map = {}
    for start, end, prop in raw_sorted:
        for cp in range(start, end + 1):
            cp_map[cp] = prop

    # Now apply narrower ranges on top (they should win)
    raw_narrow = sorted(raw, key=lambda r: (r[1] - r[0]))
    for start, end, prop in raw_narrow:
        for cp in range(start, end + 1):
            cp_map[cp] = prop

    # Convert back to sorted ranges
    if not cp_map:
        return []

    cps = sorted(cp_map.keys())
    ranges = []
    start = cps[0]
    prev_cp = cps[0]
    prev_prop = cp_map[cps[0]]

    for cp in cps[1:]:
        prop = cp_map[cp]
        if cp == prev_cp + 1 and prop == prev_prop:
            prev_cp = cp
        else:
            ranges.append((start, prev_cp, GCB_MAP[prev_prop]))
            start = cp
            prev_cp = cp
            prev_prop = prop
    ranges.append((start, prev_cp, GCB_MAP[prev_prop]))

    return ranges


def format_hex(cp):
    """Format a codepoint as hex with appropriate width."""
    if cp > 0xFFFF:
        return f"0x{cp:05X}"
    else:
        return f"0x{cp:04X}"


def format_range_table(name, ranges, with_property=False):
    """Format ranges as a C array."""
    lines = []
    if with_property:
        lines.append(f"static const struct gcb_range {name}[] = {{")
    else:
        lines.append(f"static const struct unicode_range {name}[] = {{")

    entries_per_line = 3 if not with_property else 2
    entries = []
    for r in ranges:
        if with_property:
            start, end, prop = r
            entries.append(f"    {{{hex(start)}, {hex(end)}, {prop}}}")
        else:
            start, end = r
            entries.append(f"    {{{hex(start)}, {hex(end)}}}")

    # Format entries in groups
    for i in range(0, len(entries), entries_per_line):
        group = entries[i : i + entries_per_line]
        line = ",".join(e.strip() for e in group)
        lines.append(f"    {line},")

    lines.append("};")
    return "\n".join(lines)


def validate_no_overlaps(ranges, name):
    """Verify no ranges overlap."""
    for i in range(len(ranges) - 1):
        curr_end = ranges[i][1]
        next_start = ranges[i + 1][0]
        if curr_end >= next_start:
            print(
                f"OVERLAP in {name}: {hex(ranges[i][0])}-{hex(curr_end)} "
                f"overlaps {hex(next_start)}-{hex(ranges[i+1][1])}",
                file=sys.stderr,
            )
            return False
    return True


def output_c_tables(zw_ranges, eaw_ranges, extpict_ranges, gcb_ranges,
                    incb_linkers, incb_consonants, punct_symbol_ranges):
    """Output tables in C syntax."""

    # ZERO_WIDTH_RANGES
    print("/* ====== ZERO_WIDTH_RANGES ====== */")
    print(f"/* Zero-width character ranges (Unicode {UNICODE_VERSION}). */")
    print("/* Mn (Nonspacing Mark), Me (Enclosing Mark), Cf (Format). */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from DerivedGeneralCategory.txt. */")
    print("/* U+1F3FB..U+1F3FF (emoji skin tone modifiers) added manually. */")
    print("/* U+1160..U+11FF, U+D7B0..U+D7C6, U+D7CB..U+D7FB (conjoining Hangul jamo */")
    print("/* vowels/finals) added manually: terminals render them zero-width. */")
    print()
    print("static const struct unicode_range ZERO_WIDTH_RANGES[] = {")
    for i in range(0, len(zw_ranges), 3):
        group = zw_ranges[i : i + 3]
        parts = []
        for s, e in group:
            parts.append(f"{{{format_hex(s)}, {format_hex(e)}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print("#define ZERO_WIDTH_COUNT \\")
    print("  (sizeof(ZERO_WIDTH_RANGES) / sizeof(ZERO_WIDTH_RANGES[0]))")
    print()

    # EAW_WIDE_RANGES
    print("/* ====== EAW_WIDE_RANGES ====== */")
    print(f"/* East_Asian_Width W/F (Unicode {UNICODE_VERSION}) */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from EastAsianWidth.txt. */")
    print()

    print("static const struct unicode_range EAW_WIDE_RANGES[] = {")
    for i in range(0, len(eaw_ranges), 3):
        group = eaw_ranges[i : i + 3]
        parts = []
        for s, e in group:
            parts.append(f"{{{format_hex(s)}, {format_hex(e)}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print(
        "#define EAW_WIDE_COUNT \\"
    )
    print("  (sizeof(EAW_WIDE_RANGES) / sizeof(EAW_WIDE_RANGES[0]))")
    print()

    # EXTENDED_PICTOGRAPHIC_RANGES
    print("/* ====== EXTENDED_PICTOGRAPHIC_RANGES ====== */")
    print(f"/* Extended_Pictographic=Yes (Unicode {UNICODE_VERSION}) */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from emoji-data.txt. */")
    print()
    print(
        "static const struct unicode_range EXTENDED_PICTOGRAPHIC_RANGES[] = {"
    )
    for i in range(0, len(extpict_ranges), 3):
        group = extpict_ranges[i : i + 3]
        parts = []
        for s, e in group:
            parts.append(f"{{{format_hex(s)}, {format_hex(e)}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print("#define EXTENDED_PICTOGRAPHIC_COUNT \\")
    print(
        "  (sizeof(EXTENDED_PICTOGRAPHIC_RANGES) / sizeof(EXTENDED_PICTOGRAPHIC_RANGES[0]))"
    )
    print()

    # GCB_RANGES
    print("/* ====== GCB_RANGES ====== */")
    print(
        f"/* Grapheme_Cluster_Break properties (Unicode {UNICODE_VERSION}, non-overlapping) */"
    )
    print("/* LV/LVT computed algorithmically for Hangul U+AC00-U+D7A3 */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from GraphemeBreakProperty.txt. */")
    print()
    print("static const struct gcb_range GCB_RANGES[] = {")
    for s, e, prop in gcb_ranges:
        print(f"    {{{format_hex(s)}, {format_hex(e)}, {prop}}},")
    print("};")
    print("#define GCB_RANGE_COUNT (sizeof(GCB_RANGES) / sizeof(GCB_RANGES[0]))")
    print()

    # INCB_LINKERS
    print("/* ====== INCB_LINKERS ====== */")
    print(f"/* InCB=Linker codepoints (Unicode {UNICODE_VERSION}) */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from DerivedCoreProperties.txt. */")
    print()
    print("static const uint32_t INCB_LINKERS[] = {")
    for i in range(0, len(incb_linkers), 8):
        group = incb_linkers[i : i + 8]
        parts = [format_hex(cp) for cp in group]
        print("    " + ",  ".join(parts) + ",")
    print("};")
    print("#define INCB_LINKER_COUNT (sizeof(INCB_LINKERS) / sizeof(INCB_LINKERS[0]))")
    print()

    # INCB_CONSONANTS
    print("/* ====== INCB_CONSONANTS ====== */")
    print(f"/* InCB=Consonant ranges (Unicode {UNICODE_VERSION}) */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from DerivedCoreProperties.txt. */")
    print()
    print("static const struct unicode_range INCB_CONSONANTS[] = {")
    for i in range(0, len(incb_consonants), 3):
        group = incb_consonants[i : i + 3]
        parts = []
        for s, e in group:
            parts.append(f"{{{format_hex(s)}, {format_hex(e)}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print("#define INCB_CONSONANT_COUNT \\")
    print("  (sizeof(INCB_CONSONANTS) / sizeof(INCB_CONSONANTS[0]))")
    print()

    # UNICODE_PUNCT_SYMBOL_RANGES
    print("/* ====== UNICODE_PUNCT_SYMBOL_RANGES ====== */")
    print(f"/* Unicode Punctuation + Symbol ranges (General_Category P* + S*) */")
    print(f"/* CommonMark 0.31.2 \"Unicode punctuation character\" definition. */")
    print(f"/* Generated by scripts/gen_unicode_tables.py from DerivedGeneralCategory.txt. */")
    print(f"/* Unicode {UNICODE_VERSION} */")
    print()
    print("static const struct unicode_range UNICODE_PUNCT_SYMBOL_RANGES[] = {")
    for i in range(0, len(punct_symbol_ranges), 3):
        group = punct_symbol_ranges[i : i + 3]
        parts = []
        for s, e in group:
            parts.append(f"{{{format_hex(s)}, {format_hex(e)}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print("#define UNICODE_PUNCT_SYMBOL_COUNT \\")
    print("  (sizeof(UNICODE_PUNCT_SYMBOL_RANGES) / sizeof(UNICODE_PUNCT_SYMBOL_RANGES[0]))")


def main():
    parser = argparse.ArgumentParser(
        description="Generate Unicode property tables for gstr"
    )
    parser.parse_args()

    # Download data files
    eaw_text = download("EastAsianWidth.txt", FILES["EastAsianWidth.txt"])
    emoji_text = download("emoji-data.txt", FILES["emoji-data.txt"])
    gbp_text = download(
        "GraphemeBreakProperty.txt", FILES["GraphemeBreakProperty.txt"]
    )
    dcp_text = download(
        "DerivedCoreProperties.txt", FILES["DerivedCoreProperties.txt"]
    )
    gc_text = download(
        "DerivedGeneralCategory.txt", FILES["DerivedGeneralCategory.txt"]
    )

    # Generate tables
    zw_ranges = generate_zero_width(gc_text)
    eaw_ranges = generate_eaw_wide(eaw_text)
    extpict_ranges = generate_extended_pictographic(emoji_text)
    gcb_ranges = generate_gcb(gbp_text)
    incb_linkers = generate_incb_linkers(dcp_text)
    incb_consonants = generate_incb_consonants(dcp_text)
    punct_symbol_ranges = generate_punct_symbol(gc_text)

    # Validate
    ok = True
    ok &= validate_no_overlaps(zw_ranges, "ZERO_WIDTH_RANGES")
    ok &= validate_no_overlaps(eaw_ranges, "EAW_WIDE_RANGES")
    ok &= validate_no_overlaps(extpict_ranges, "EXTENDED_PICTOGRAPHIC_RANGES")
    ok &= validate_no_overlaps(gcb_ranges, "GCB_RANGES")
    ok &= validate_no_overlaps(incb_consonants, "INCB_CONSONANTS")
    ok &= validate_no_overlaps(punct_symbol_ranges, "UNICODE_PUNCT_SYMBOL_RANGES")

    # Validate linkers are sorted
    for i in range(len(incb_linkers) - 1):
        if incb_linkers[i] >= incb_linkers[i + 1]:
            print(
                f"UNSORTED in INCB_LINKERS: {hex(incb_linkers[i])} >= {hex(incb_linkers[i+1])}",
                file=sys.stderr,
            )
            ok = False

    if not ok:
        print("VALIDATION FAILED - overlaps detected!", file=sys.stderr)
        sys.exit(1)

    # Print stats
    print(f"// Generated from Unicode {UNICODE_VERSION}", file=sys.stderr)
    print(f"// ZERO_WIDTH_RANGES: {len(zw_ranges)} entries", file=sys.stderr)
    print(f"// EAW_WIDE_RANGES: {len(eaw_ranges)} entries", file=sys.stderr)
    print(
        f"// EXTENDED_PICTOGRAPHIC_RANGES: {len(extpict_ranges)} entries",
        file=sys.stderr,
    )
    print(f"// GCB_RANGES: {len(gcb_ranges)} entries", file=sys.stderr)
    print(f"// INCB_LINKERS: {len(incb_linkers)} entries", file=sys.stderr)
    print(f"// INCB_CONSONANTS: {len(incb_consonants)} entries", file=sys.stderr)
    print(f"// UNICODE_PUNCT_SYMBOL_RANGES: {len(punct_symbol_ranges)} entries", file=sys.stderr)

    output_c_tables(zw_ranges, eaw_ranges, extpict_ranges, gcb_ranges,
                    incb_linkers, incb_consonants, punct_symbol_ranges)


if __name__ == "__main__":
    main()
