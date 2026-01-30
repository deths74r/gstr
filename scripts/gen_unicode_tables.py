#!/usr/bin/env python3
"""
Generate Unicode property tables for gstr.h from official Unicode data files.

Downloads Unicode 17.0 data files (cached locally) and produces C array
literals for:
  - EAW_WIDE_RANGES[]           (East_Asian_Width = W or F)
  - EXTENDED_PICTOGRAPHIC_RANGES[] (Extended_Pictographic = Yes)
  - GCB_RANGES[]                (Grapheme_Cluster_Break, non-overlapping)

Usage:
    python3 scripts/gen_unicode_tables.py > /dev/null  # just validate
    python3 scripts/gen_unicode_tables.py              # print tables to stdout

Paste the output into include/gstr.h to replace the hand-maintained tables.
"""

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


def main():
    # Download data files
    eaw_text = download("EastAsianWidth.txt", FILES["EastAsianWidth.txt"])
    emoji_text = download("emoji-data.txt", FILES["emoji-data.txt"])
    gbp_text = download(
        "GraphemeBreakProperty.txt", FILES["GraphemeBreakProperty.txt"]
    )

    # Generate tables
    eaw_ranges = generate_eaw_wide(eaw_text)
    extpict_ranges = generate_extended_pictographic(emoji_text)
    gcb_ranges = generate_gcb(gbp_text)

    # Validate
    ok = True
    ok &= validate_no_overlaps(eaw_ranges, "EAW_WIDE_RANGES")
    ok &= validate_no_overlaps(extpict_ranges, "EXTENDED_PICTOGRAPHIC_RANGES")
    ok &= validate_no_overlaps(gcb_ranges, "GCB_RANGES")

    if not ok:
        print("VALIDATION FAILED - overlaps detected!", file=sys.stderr)
        sys.exit(1)

    # Print stats
    print(f"// Generated from Unicode {UNICODE_VERSION}", file=sys.stderr)
    print(f"// EAW_WIDE_RANGES: {len(eaw_ranges)} entries", file=sys.stderr)
    print(
        f"// EXTENDED_PICTOGRAPHIC_RANGES: {len(extpict_ranges)} entries",
        file=sys.stderr,
    )
    print(f"// GCB_RANGES: {len(gcb_ranges)} entries", file=sys.stderr)

    # Output C tables
    print("/* ====== EAW_WIDE_RANGES ====== */")
    print(f"/* East_Asian_Width W/F (Unicode {UNICODE_VERSION}) */")
    print()

    # Format EAW table
    print("static const struct unicode_range EAW_WIDE_RANGES[] = {")
    for i in range(0, len(eaw_ranges), 3):
        group = eaw_ranges[i : i + 3]
        parts = []
        for s, e in group:
            if s > 0xFFFF:
                parts.append(f"{{0x{s:05X}, 0x{e:05X}}}")
            else:
                parts.append(f"{{0x{s:04X}, 0x{e:04X}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print(
        "#define EAW_WIDE_COUNT \\"
    )
    print("  (sizeof(EAW_WIDE_RANGES) / sizeof(EAW_WIDE_RANGES[0]))")
    print()

    print("/* ====== EXTENDED_PICTOGRAPHIC_RANGES ====== */")
    print(f"/* Extended_Pictographic=Yes (Unicode {UNICODE_VERSION}) */")
    print()
    print(
        "static const struct unicode_range EXTENDED_PICTOGRAPHIC_RANGES[] = {"
    )
    for i in range(0, len(extpict_ranges), 3):
        group = extpict_ranges[i : i + 3]
        parts = []
        for s, e in group:
            if s > 0xFFFF:
                parts.append(f"{{0x{s:05X}, 0x{e:05X}}}")
            else:
                parts.append(f"{{0x{s:04X}, 0x{e:04X}}}")
        print("    " + ",   ".join(parts) + ",")
    print("};")
    print("#define EXTENDED_PICTOGRAPHIC_COUNT \\")
    print(
        "  (sizeof(EXTENDED_PICTOGRAPHIC_RANGES) / sizeof(EXTENDED_PICTOGRAPHIC_RANGES[0]))"
    )
    print()

    print("/* ====== GCB_RANGES ====== */")
    print(
        f"/* Grapheme_Cluster_Break properties (Unicode {UNICODE_VERSION}, non-overlapping) */"
    )
    print("/* LV/LVT computed algorithmically for Hangul U+AC00-U+D7A3 */")
    print()
    print("static const struct gcb_range GCB_RANGES[] = {")
    for s, e, prop in gcb_ranges:
        if s > 0xFFFF:
            print(f"    {{0x{s:05X}, 0x{e:05X}, {prop}}},")
        else:
            print(f"    {{0x{s:04X}, 0x{e:04X}, {prop}}},")
    print("};")
    print("#define GCB_RANGE_COUNT (sizeof(GCB_RANGES) / sizeof(GCB_RANGES[0]))")


if __name__ == "__main__":
    main()
