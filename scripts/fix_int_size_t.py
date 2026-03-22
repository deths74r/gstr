#!/usr/bin/env python3
"""
Convert int-based byte offsets/lengths to size_t in gstr.h.

This script performs the int->size_t migration for the UTF-8 layer functions
and eliminates (int)/(size_t) casts between layers.
"""

import re
import sys
from pathlib import Path

GSTR_H = Path(__file__).parent.parent / "include" / "gstr.h"


def main():
    content = GSTR_H.read_text()
    lines = content.split('\n')

    # =========================================================================
    # Phase 1: Change UTF-8 layer function signatures
    # =========================================================================

    # utf8_decode: int -> size_t for length and return
    content = content.replace(
        'static inline int utf8_decode(const char *bytes, int length,',
        'static inline size_t utf8_decode(const char *bytes, size_t length,'
    )

    # utf8_encode: int -> size_t for return
    content = content.replace(
        'static inline int utf8_encode(uint32_t codepoint, char *buffer) {',
        'static inline size_t utf8_encode(uint32_t codepoint, char *buffer) {'
    )

    # utf8_charwidth: int length, int offset -> size_t
    content = content.replace(
        'static inline int utf8_charwidth(const char *text, int length, int offset) {',
        'static inline int utf8_charwidth(const char *text, size_t length, size_t offset) {'
    )

    # utf8_next: int -> size_t
    content = content.replace(
        'static inline int utf8_next(const char *text, int length, int offset) {',
        'static inline size_t utf8_next(const char *text, size_t length, size_t offset) {'
    )

    # utf8_prev: int -> size_t
    content = content.replace(
        'static inline int utf8_prev(const char *text, int length, int offset) {',
        'static inline size_t utf8_prev(const char *text, size_t length, size_t offset) {'
    )

    # utf8_next_grapheme: int -> size_t
    content = content.replace(
        'static inline int utf8_next_grapheme(const char *text, int length, int offset) {',
        'static inline size_t utf8_next_grapheme(const char *text, size_t length, size_t offset) {'
    )

    # utf8_prev_grapheme: int -> size_t (already has length from spec 09)
    content = content.replace(
        'static inline int utf8_prev_grapheme(const char *text, int length, int offset) {',
        'static inline size_t utf8_prev_grapheme(const char *text, size_t length, size_t offset) {'
    )

    # utf8_valid: length -> size_t, error_offset -> size_t *
    content = content.replace(
        'static inline int utf8_valid(const char *text, int length, int *error_offset) {',
        'static inline int utf8_valid(const char *text, size_t length, size_t *error_offset) {'
    )

    # utf8_cpcount: int -> size_t
    content = content.replace(
        'static inline int utf8_cpcount(const char *text, int length) {',
        'static inline size_t utf8_cpcount(const char *text, size_t length) {'
    )

    # utf8_truncate: int -> size_t
    content = content.replace(
        'static inline int utf8_truncate(const char *text, int length, int max_cols) {',
        'static inline size_t utf8_truncate(const char *text, size_t length, size_t max_cols) {'
    )

    # gstr_grapheme_width: int offset, int next -> size_t
    content = content.replace(
        'static inline size_t gstr_grapheme_width(const char *s, size_t byte_len,\n                                         int offset, int next) {',
        'static inline size_t gstr_grapheme_width(const char *s, size_t byte_len,\n                                         size_t offset, size_t next) {'
    )
    # Also the forward declaration
    content = content.replace(
        'static inline size_t gstr_grapheme_width(const char *s, size_t byte_len,\n                                         int offset, int next);',
        'static inline size_t gstr_grapheme_width(const char *s, size_t byte_len,\n                                         size_t offset, size_t next);'
    )

    # =========================================================================
    # Phase 2: Fix internal variables and remove casts
    # =========================================================================

    # Remove (int)byte_len and (int)src_len casts - these become unnecessary
    content = content.replace('(int)byte_len', 'byte_len')
    content = content.replace('(int)src_len', 'src_len')
    content = content.replace('(int)s_len', 's_len')
    content = content.replace('(int)h_len', 'h_len')
    content = content.replace('(int)n_len', 'n_len')
    content = content.replace('(int)d_len', 'd_len')
    content = content.replace('(int)prefix_len', 'prefix_len')
    content = content.replace('(int)suffix_len', 'suffix_len')
    content = content.replace('(int)a_len', 'a_len')
    content = content.replace('(int)b_len', 'b_len')
    content = content.replace('(int)g_len', 'g_len')
    content = content.replace('(int)old_len', 'old_len')
    content = content.replace('(int)new_len', 'new_len')
    content = content.replace('(int)set_len', 'set_len')
    content = content.replace('(int)len', 'len')

    # Remove (size_t)offset and similar upward casts - now types match
    content = content.replace('(size_t)offset', 'offset')
    content = content.replace('(size_t)next', 'next')
    content = content.replace('(size_t)a_off', 'a_off')
    content = content.replace('(size_t)b_off', 'b_off')
    content = content.replace('(size_t)s_off', 's_off')
    content = content.replace('(size_t)suf_off', 'suf_off')
    content = content.replace('(size_t)p_off', 'p_off')
    content = content.replace('(size_t)h_off', 'h_off')
    content = content.replace('(size_t)h_pos', 'h_pos')
    content = content.replace('(size_t)n_pos', 'n_pos')
    content = content.replace('(size_t)cp_offset', 'cp_offset')
    content = content.replace('(size_t)n', 'n')

    # Change int local variables to size_t in gstr* functions
    # We need to be careful to only change offset/length variables, not loop counters
    # that are legitimately int (like cw for char width)

    # In utf8_decode internals
    content = content.replace(
        '  int sequence_length = 0;',
        '  size_t sequence_length = 0;'
    )

    # In utf8_next internals
    content = content.replace(
        '  int char_bytes = utf8_decode(text + offset, length - offset, &codepoint);',
        '  size_t char_bytes = utf8_decode(text + offset, length - offset, &codepoint);'
    )
    content = content.replace(
        '  int next_offset = offset + char_bytes;',
        '  size_t next_offset = offset + char_bytes;'
    )

    # In utf8_next_grapheme internals
    content = content.replace(
        '  int bytes = utf8_decode(text + offset, length - offset, &prev_cp);',
        '  size_t bytes = utf8_decode(text + offset, length - offset, &prev_cp);'
    )
    content = content.replace(
        '  int next_offset = offset + bytes;',
        '  size_t next_offset = offset + bytes;'
    )
    # The internal bytes variable in the while loop
    content = content.replace(
        '    bytes = utf8_decode(text + next_offset, length - next_offset, &curr_cp);',
        '    bytes = utf8_decode(text + next_offset, length - next_offset, &curr_cp);'
    )

    # In utf8_cpcount
    content = content.replace(
        '  int count = 0;\n  int offset = 0;\n  while (offset < length) {',
        '  size_t count = 0;\n  size_t offset = 0;\n  while (offset < length) {'
    )

    # In utf8_truncate
    content = content.replace(
        '  int offset = 0;\n  int cols = 0;\n  int last_valid = 0;',
        '  size_t offset = 0;\n  size_t cols = 0;\n  size_t last_valid = 0;'
    )

    # =========================================================================
    # Phase 3: Fix gstr layer internal int offset -> size_t offset
    # =========================================================================

    # This is the bulk change. We need to change all `int offset = 0;` etc.
    # in gstr layer functions to size_t.
    # We'll do targeted replacements.

    # Common pattern: int offset = 0;  -> size_t offset = 0;
    # But only for byte offset variables, not char width etc.

    # Change all "int offset = 0;" that appear in gstr functions
    # These are local variables used for byte offsets
    content = re.sub(r'(\n  )int offset = 0;', r'\1size_t offset = 0;', content)
    content = re.sub(r'(\n  )int next = ', r'\1size_t next = ', content)
    content = re.sub(r'(\n  )int a_off = ', r'\1size_t a_off = ', content)
    content = re.sub(r'(\n  )int b_off = ', r'\1size_t b_off = ', content)
    content = re.sub(r'(\n    )int a_next = ', r'\1size_t a_next = ', content)
    content = re.sub(r'(\n    )int b_next = ', r'\1size_t b_next = ', content)
    content = re.sub(r'(\n  )int s_off = ', r'\1size_t s_off = ', content)
    content = re.sub(r'(\n  )int suf_off = ', r'\1size_t suf_off = ', content)
    content = re.sub(r'(\n  )int p_off = ', r'\1size_t p_off = ', content)
    content = re.sub(r'(\n  )int h_off = ', r'\1size_t h_off = ', content)
    content = re.sub(r'(\n    )int h_pos = ', r'\1size_t h_pos = ', content)
    content = re.sub(r'(\n    )int n_pos = ', r'\1size_t n_pos = ', content)
    content = re.sub(r'(\n    )int h_next = ', r'\1size_t h_next = ', content)
    content = re.sub(r'(\n    )int n_next = ', r'\1size_t n_next = ', content)
    content = re.sub(r'(\n  )int cp_offset = ', r'\1size_t cp_offset = ', content)
    content = re.sub(r'(\n    )int cp_offset = ', r'\1size_t cp_offset = ', content)
    content = re.sub(r'(\n      )int cp_offset = ', r'\1size_t cp_offset = ', content)
    content = re.sub(r'(\n  )int fit_offset = ', r'\1size_t fit_offset = ', content)
    content = re.sub(r'(\n  )int last_complete = ', r'\1size_t last_complete = ', content)
    content = re.sub(r'(\n  )int start_offset = ', r'\1size_t start_offset = ', content)
    content = re.sub(r'(\n  )int src_off = ', r'\1size_t src_off = ', content)
    content = re.sub(r'(\n  )int current_end = ', r'\1size_t current_end = ', content)

    # In gstr_grapheme_in_set
    content = re.sub(r'(\n  )int set_off = ', r'\1size_t set_off = ', content)

    # Fix the (size_t)(next - offset) and similar difference casts
    content = content.replace('(size_t)(end - start)', '(end - start)')
    content = content.replace('(size_t)(next - offset)', '(next - offset)')
    content = content.replace('(size_t)(a_next - a_off)', '(a_next - a_off)')
    content = content.replace('(size_t)(b_next - b_off)', '(b_next - b_off)')
    content = content.replace('(size_t)(h_next - h_pos)', '(h_next - h_pos)')
    content = content.replace('(size_t)(n_next - n_pos)', '(n_next - n_pos)')
    content = content.replace('(size_t)(h_next - h_off)', '(h_next - h_off)')
    content = content.replace('(size_t)(current_end - start)', '(current_end - start)')

    # =========================================================================
    # Phase 4: Fix binary searches to use half-open intervals
    # =========================================================================

    # unicode_range_contains
    content = content.replace(
        '''static inline int unicode_range_contains(uint32_t codepoint,
                                           const struct unicode_range *ranges,
                                           int count) {
  int low = 0;
  int high = count - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (codepoint < ranges[mid].start) {
      high = mid - 1;
    } else if (codepoint > ranges[mid].end) {
      low = mid + 1;
    } else {
      return 1;
    }
  }
  return 0;
}''',
        '''static inline int unicode_range_contains(uint32_t codepoint,
                                           const struct unicode_range *ranges,
                                           size_t count) {
  if (count == 0) return 0;
  size_t low = 0;
  size_t high = count;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (codepoint < ranges[mid].start) {
      high = mid;
    } else if (codepoint > ranges[mid].end) {
      low = mid + 1;
    } else {
      return 1;
    }
  }
  return 0;
}'''
    )

    # get_gcb - fix binary search
    content = content.replace(
        '''  int low = 0;
  int high = GCB_RANGE_COUNT - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (cp < GCB_RANGES[mid].start) {
      high = mid - 1;
    } else if (cp > GCB_RANGES[mid].end) {
      low = mid + 1;''',
        '''  size_t low = 0;
  size_t high = GCB_RANGE_COUNT;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (cp < GCB_RANGES[mid].start) {
      high = mid;
    } else if (cp > GCB_RANGES[mid].end) {
      low = mid + 1;'''
    )

    # is_incb_linker - fix binary search
    content = content.replace(
        '''  int low = 0;
  int high = INCB_LINKER_COUNT - 1;
  while (low <= high) {
    int mid = (low + high) / 2;
    if (cp < INCB_LINKERS[mid]) {
      high = mid - 1;
    } else if (cp > INCB_LINKERS[mid]) {
      low = mid + 1;''',
        '''  size_t low = 0;
  size_t high = INCB_LINKER_COUNT;
  while (low < high) {
    size_t mid = low + (high - low) / 2;
    if (cp < INCB_LINKERS[mid]) {
      high = mid;
    } else if (cp > INCB_LINKERS[mid]) {
      low = mid + 1;'''
    )

    # =========================================================================
    # Phase 5: Fix utf8_prev and utf8_prev_grapheme for size_t
    # =========================================================================

    # Fix offset <= 0 -> offset == 0 in utf8_prev
    content = content.replace(
        '  if (!text || length <= 0 || offset <= 0) {\n    return 0;\n  }\n  if (offset > length) {\n    offset = length;\n  }\n  int pos = offset - 1;\n  int limit = (offset > 4) ? offset - 4 : 0;',
        '  if (!text || length == 0 || offset == 0) {\n    return 0;\n  }\n  if (offset > length) {\n    offset = length;\n  }\n  size_t pos = offset - 1;\n  size_t limit = (offset > 4) ? offset - 4 : 0;'
    )

    # Fix utf8_prev_grapheme guards
    content = content.replace(
        '  if (!text || length <= 0 || offset <= 0) {\n    return 0;\n  }\n  if (offset > length) {\n    offset = length;\n  }\n\n  int prev_start = utf8_prev(text, length, offset);',
        '  if (!text || length == 0 || offset == 0) {\n    return 0;\n  }\n  if (offset > length) {\n    offset = length;\n  }\n\n  size_t prev_start = utf8_prev(text, length, offset);'
    )

    # Fix the rest of utf8_prev_grapheme internals
    content = content.replace(
        '  int scan_start = prev_start;\n  int remaining = GRAPHEME_MAX_BACKTRACK;',
        '  size_t scan_start = prev_start;\n  int remaining = GRAPHEME_MAX_BACKTRACK;'
    )
    content = content.replace(
        '    int prev = utf8_prev(text, length, scan_start);',
        '    size_t prev = utf8_prev(text, length, scan_start);'
    )
    content = content.replace(
        '  int curr = scan_start;\n  int grapheme_start = scan_start;',
        '  size_t curr = scan_start;\n  size_t grapheme_start = scan_start;'
    )

    # Fix utf8_next_grapheme guard for offset < 0 (no longer possible with size_t)
    content = content.replace(
        '  if (!text || offset >= length || offset < 0) {',
        '  if (!text || offset >= length) {'
    )

    # =========================================================================
    # Phase 6: Add _Static_assert near top of file
    # =========================================================================

    # Add after the GSTR_UNICODE_VERSION block
    static_assert_block = '''
/* gstr requires int to be at least 32 bits for byte offset arithmetic. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(int) >= 4, "gstr requires sizeof(int) >= 4");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(int) >= 4, "gstr requires sizeof(int) >= 4");
#else
typedef char gstr_assert_int_at_least_32bit[sizeof(int) >= 4 ? 1 : -1];
#endif
'''

    if '_Static_assert(sizeof(int) >= 4' not in content:
        content = content.replace(
            '#define GSTR_UNICODE_VERSION_UPDATE 0\n',
            '#define GSTR_UNICODE_VERSION_UPDATE 0\n' + static_assert_block
        )

    # =========================================================================
    # Phase 7: Fix remaining (int) casts on size_t variables
    # =========================================================================

    # In utf8_truncate
    content = content.replace(
        '    int cw = utf8_charwidth(text, length, offset);\n    int nb = utf8_next(text, length, offset);',
        '    int cw = utf8_charwidth(text, length, offset);\n    size_t nb = utf8_next(text, length, offset);'
    )

    # Fix (int)max_cols
    content = content.replace('(int)max_cols', 'max_cols')

    GSTR_H.write_text(content)
    print("Done. gstr.h updated for int->size_t migration.", file=sys.stderr)


if __name__ == "__main__":
    main()
