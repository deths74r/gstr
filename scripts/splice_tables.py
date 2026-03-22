#!/usr/bin/env python3
"""
Splice generated Unicode tables into include/gstr.h.

Reads the generated tables from stdin (output of gen_unicode_tables.py)
and replaces the corresponding table sections in gstr.h.
Also adds the GSTR_UNICODE_VERSION macro if not present.
"""

import re
import sys
from pathlib import Path

GSTR_H = Path(__file__).parent.parent / "include" / "gstr.h"
UNICODE_VERSION = "17.0.0"
UNICODE_MAJOR, UNICODE_MINOR, UNICODE_UPDATE = UNICODE_VERSION.split(".")


def find_table_bounds(lines, table_name, is_define_suffix=None):
    """Find start and end line indices for a table definition.

    Returns (comment_start, define_end) where:
    - comment_start: first line of the block comment before the table
    - define_end: line after the #define COUNT macro
    """
    # Find the array declaration line
    decl_pattern = re.compile(
        rf"static\s+const\s+(?:struct\s+\w+|uint32_t)\s+{re.escape(table_name)}\s*\[\s*\]\s*="
    )
    decl_line = None
    for i, line in enumerate(lines):
        if decl_pattern.search(line):
            decl_line = i
            break

    if decl_line is None:
        print(f"WARNING: Could not find table {table_name}", file=sys.stderr)
        return None

    # Walk backward to find start of comment block
    comment_start = decl_line
    for i in range(decl_line - 1, -1, -1):
        stripped = lines[i].strip()
        if stripped.startswith("/*") or stripped.startswith("*") or stripped.endswith("*/"):
            comment_start = i
        elif stripped == "":
            comment_start = i
        else:
            break

    # Walk forward to find end of #define line(s)
    # First find closing brace of array
    brace_end = decl_line
    for i in range(decl_line, len(lines)):
        if lines[i].strip().startswith("};"):
            brace_end = i
            break

    # Now find the #define COUNT line after the closing brace
    define_end = brace_end + 1
    for i in range(brace_end + 1, min(brace_end + 5, len(lines))):
        stripped = lines[i].strip()
        if stripped.startswith("#define"):
            define_end = i + 1
            # Check if next line is continuation
            if stripped.endswith("\\"):
                define_end = i + 2
                if i + 2 < len(lines) and lines[i + 2].strip().startswith("(sizeof"):
                    define_end = i + 3
        elif stripped.startswith("(sizeof"):
            define_end = i + 1
        elif stripped == "":
            continue
        else:
            break

    return (comment_start, define_end)


def parse_generated_sections(text):
    """Parse the generated output into named sections."""
    sections = {}
    current_name = None
    current_lines = []

    for line in text.splitlines():
        if line.startswith("/* ====== ") and line.endswith(" ====== */"):
            if current_name:
                # Strip trailing blank lines
                while current_lines and current_lines[-1].strip() == "":
                    current_lines.pop()
                sections[current_name] = "\n".join(current_lines)
            current_name = line.split("======")[1].strip()
            current_lines = []
        else:
            if current_name:
                current_lines.append(line)

    if current_name:
        while current_lines and current_lines[-1].strip() == "":
            current_lines.pop()
        sections[current_name] = "\n".join(current_lines)

    return sections


def main():
    generated = sys.stdin.read()
    sections = parse_generated_sections(generated)

    content = GSTR_H.read_text()
    lines = content.splitlines()

    # Map of table array name -> section key in generated output
    TABLE_MAP = [
        ("ZERO_WIDTH_RANGES", "ZERO_WIDTH_RANGES"),
        ("EAW_WIDE_RANGES", "EAW_WIDE_RANGES"),
        ("EXTENDED_PICTOGRAPHIC_RANGES", "EXTENDED_PICTOGRAPHIC_RANGES"),
        ("GCB_RANGES", "GCB_RANGES"),
        ("INCB_LINKERS", "INCB_LINKERS"),
        ("INCB_CONSONANTS", "INCB_CONSONANTS"),
    ]

    # Process tables in reverse order (so line numbers don't shift)
    replacements = []
    for array_name, section_key in TABLE_MAP:
        if section_key not in sections:
            print(f"WARNING: Section {section_key} not in generated output", file=sys.stderr)
            continue
        bounds = find_table_bounds(lines, array_name)
        if bounds is None:
            continue
        start, end = bounds
        replacements.append((start, end, sections[section_key]))
        print(f"  {array_name}: replacing lines {start+1}-{end} ({end-start} lines)", file=sys.stderr)

    # Sort by start line descending to process from bottom to top
    replacements.sort(key=lambda r: r[0], reverse=True)

    for start, end, new_content in replacements:
        new_lines = new_content.splitlines()
        lines[start:end] = new_lines

    # Add GSTR_UNICODE_VERSION macro if not present
    version_macro = f'#define GSTR_UNICODE_VERSION "{UNICODE_VERSION}"'
    has_version = any("GSTR_UNICODE_VERSION" in line for line in lines)
    if not has_version:
        # Insert after GSTR_BUILD_ID block
        for i, line in enumerate(lines):
            if "GSTR_BUILD_ID" in line and line.strip().startswith("#define GSTR_BUILD_ID"):
                # Find end of the #endif block
                insert_at = i + 1
                if i + 1 < len(lines) and lines[i + 1].strip() == "#endif":
                    insert_at = i + 2
                lines.insert(insert_at, "")
                lines.insert(insert_at + 1, f"/* Unicode standard version used to generate the character property tables. */")
                lines.insert(insert_at + 2, f'#define GSTR_UNICODE_VERSION "{UNICODE_VERSION}"')
                lines.insert(insert_at + 3, f"#define GSTR_UNICODE_VERSION_MAJOR {UNICODE_MAJOR}")
                lines.insert(insert_at + 4, f"#define GSTR_UNICODE_VERSION_MINOR {UNICODE_MINOR}")
                lines.insert(insert_at + 5, f"#define GSTR_UNICODE_VERSION_UPDATE {UNICODE_UPDATE}")
                print(f"  Added GSTR_UNICODE_VERSION macros after line {insert_at}", file=sys.stderr)
                break

    result = "\n".join(lines) + "\n"
    GSTR_H.write_text(result)
    print("Done. gstr.h updated.", file=sys.stderr)


if __name__ == "__main__":
    main()
