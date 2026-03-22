# gstr.h API Improvement Specification

**Version:** draft-1
**Date:** 2026-03-21
**Scope:** API additions, behavioral fixes, performance improvements, documentation gaps, and namespace hygiene for `gstr.h` (single-header grapheme string library, Unicode 17.0).

---

## 1. Grapheme Iterator API

### Problem

The current API forces users to choose between convenience and performance. `gstrat(s, len, i, &out)` is O(n) per call, making the idiomatic loop `for (i = 0; i < count; i++) gstrat(s, len, i, &out)` become O(n^2). The documented workaround -- calling `utf8_next_grapheme` directly -- exposes users to a low-level API with `int` offsets, manual state tracking, and no width information.

### Proposed Types

```c
struct gstr_iter {
    const char *str;      /* base string pointer (immutable) */
    size_t      byte_len; /* total byte length of str        */
    int         offset;   /* byte offset of current grapheme */
    int         next;     /* byte offset of next grapheme    */
    size_t      index;    /* 0-based grapheme index          */
};
```

### Proposed Functions

```c
static inline void gstr_iter_init(struct gstr_iter *it,
                                  const char *s, size_t byte_len);
```

Initialize an iterator positioned before the first grapheme. After `init`, the iterator is in a "pre-start" state; the caller must call `gstr_iter_next` to advance to grapheme 0.

- If `s` is NULL or `byte_len` is 0, the iterator is immediately exhausted (subsequent `gstr_iter_next` returns 0).
- Sets `it->offset = 0`, `it->next = 0`, `it->index = 0`.

```c
static inline int gstr_iter_next(struct gstr_iter *it);
```

Advance to the next grapheme cluster. Returns 1 if a grapheme is now current, 0 if the string is exhausted.

- On success, `it->offset` is the byte offset of the current grapheme and `it->next` is one-past-end.
- `it->index` is incremented on each successful advance.
- Once 0 is returned, all subsequent calls also return 0.

```c
static inline int gstr_iter_prev(struct gstr_iter *it);
```

Move to the previous grapheme cluster. Returns 1 if a grapheme is now current, 0 if already at the beginning.

- Calls `utf8_prev_grapheme(it->str, it->offset)` internally.
- On success, `it->next = it->offset` (old position), `it->offset` is updated to the new boundary, and `it->index` is decremented.

```c
static inline const char *gstr_iter_grapheme(const struct gstr_iter *it,
                                             size_t *out_len);
```

Return a pointer to the current grapheme's bytes, and optionally its byte length in `*out_len`. If the iterator is exhausted, returns NULL.

- The returned pointer is into the original string; it is valid as long as the string is.
- `out_len` may be NULL.

```c
static inline size_t gstr_iter_width(const struct gstr_iter *it);
```

Return the display column width of the current grapheme. Delegates to the existing `gstr_grapheme_width` helper. Returns 0 if the iterator is exhausted.

### Usage Example

```c
/* O(n) iteration: print each grapheme and its column width */
struct gstr_iter it;
gstr_iter_init(&it, text, text_len);
while (gstr_iter_next(&it)) {
    size_t glen;
    const char *g = gstr_iter_grapheme(&it, &glen);
    size_t w = gstr_iter_width(&it);
    printf("grapheme %zu: %.*s (width %zu)\n",
           it.index, (int)glen, g, w);
}
```

This replaces the O(n^2) pattern:

```c
/* OLD: O(n^2) */
size_t count = gstrlen(s, len);
for (size_t i = 0; i < count; i++) {
    size_t glen;
    const char *g = gstrat(s, len, i, &glen);  /* rescans from 0 */
}
```

### Design Notes

- The struct is small (40 bytes on 64-bit) and intended for stack allocation.
- `gstr_iter_prev` has O(backtrack) cost per call due to `utf8_prev_grapheme`; this is inherent to forward-only UAX #29 segmentation. Document this clearly.
- The iterator does not own memory and requires no cleanup function.

---

## 2. Missing Doc Comments

The following public functions lack a doc comment (the `/*` or `/**` block immediately preceding the function signature). Each entry below provides the exact text that should be added.

### 2.1 `utf8_decode` (line 1168)

```c
/*
 * Decodes a single UTF-8 character from the byte buffer.
 * Returns the number of bytes consumed (1-4), or 0 if the buffer is
 * empty/NULL. Writes the decoded codepoint to *codepoint.
 *
 * On invalid input (malformed sequence, overlong encoding, surrogate,
 * or out-of-range value), writes UTF8_REPLACEMENT_CHAR (U+FFFD) to
 * *codepoint and returns the number of bytes to skip (at least 1).
 *
 * @param bytes   Pointer to UTF-8 encoded bytes.
 * @param length  Number of available bytes starting from `bytes`.
 * @param codepoint  [out] Receives the decoded Unicode codepoint.
 * @return Number of bytes consumed, or 0 if bytes is NULL or length <= 0.
 */
```

### 2.2 `utf8_cpwidth` (line 1268)

```c
/*
 * Returns the display column width of a Unicode codepoint.
 *   - 0 for null (U+0000), combining marks, and format characters.
 *   - -1 for C0/C1 control characters (U+0001-U+001F, U+007F-U+009F).
 *   - 2 for East Asian Wide/Fullwidth characters (CJK, etc.).
 *   - 1 for all other printable characters.
 *
 * Note: This returns the width of a single codepoint, not a grapheme cluster.
 * For grapheme-level width (which handles ZWJ sequences, flags, etc.),
 * use gstr_grapheme_width() or gstrwidth().
 *
 * @param codepoint  A Unicode codepoint.
 * @return Column width (-1, 0, 1, or 2).
 */
```

### 2.3 `utf8_next` (line 1331)

```c
/*
 * Returns the byte offset of the next UTF-8 codepoint after the one at
 * `offset`. Decodes the character at `offset` to determine its byte
 * length, then returns offset + byte_length.
 *
 * If text is NULL or offset >= length, returns length.
 *
 * Note: This navigates by codepoint, not grapheme. A single grapheme
 * may span multiple codepoints. Use utf8_next_grapheme() for grapheme
 * navigation.
 *
 * @param text    Pointer to UTF-8 string.
 * @param length  Byte length of the string.
 * @param offset  Current byte offset.
 * @return Byte offset of the next codepoint, or length if at end.
 */
```

### 2.4 `utf8_next_grapheme` (line 1362)

```c
/*
 * Returns the byte offset of the next grapheme cluster boundary after
 * the grapheme starting at `offset`. Implements UAX #29 extended
 * grapheme cluster segmentation rules (GB3-GB13, GB999), including
 * GB9c (Indic conjunct sequences) and GB11 (ZWJ emoji sequences).
 *
 * If text is NULL, offset >= length, or offset < 0, returns length.
 *
 * @param text    Pointer to UTF-8 string.
 * @param length  Byte length of the string.
 * @param offset  Byte offset of the current grapheme's start.
 * @return Byte offset of the next grapheme cluster, or length if at end.
 */
```

### 2.5 `utf8_valid` (line 1462)

```c
/*
 * Validates that the given byte sequence is well-formed UTF-8.
 * Returns 1 if valid, 0 if any byte sequence is malformed.
 *
 * If text is NULL, returns 0 and sets *error_offset to 0.
 *
 * @param text          Pointer to the byte sequence to validate.
 * @param length        Number of bytes to validate.
 * @param error_offset  [out] If non-NULL and validation fails, receives
 *                      the byte offset of the first invalid sequence.
 *                      Unchanged on success.
 * @return 1 if all bytes form valid UTF-8, 0 otherwise.
 */
```

### 2.6 `gstrlen` (line 1625)

```c
/*
 * Counts the number of grapheme clusters in the string.
 * Equivalent to the "user-perceived character count."
 *
 * Returns 0 if s is NULL or byte_len is 0.
 *
 * @param s         Pointer to UTF-8 string.
 * @param byte_len  Byte length of the string.
 * @return Number of grapheme clusters.
 */
```

### 2.7 `gstrcmp` (line 1790)

```c
/*
 * Compares two grapheme strings lexicographically.
 * Comparison is grapheme-by-grapheme using raw byte values (no
 * Unicode normalization or locale-aware collation).
 *
 * NULL is ordered before any non-NULL string. Two NULLs are equal.
 *
 * @param a      First string.
 * @param a_len  Byte length of first string.
 * @param b      Second string.
 * @param b_len  Byte length of second string.
 * @return Negative if a < b, positive if a > b, 0 if equal.
 */
```

### 2.8 `gstrstartswith` (line 1957)

```c
/*
 * Returns 1 if the string starts with the given prefix, 0 otherwise.
 * Comparison is grapheme-by-grapheme (byte-exact, no normalization).
 *
 * An empty prefix (prefix_len == 0) always matches.
 * Returns 0 if s or prefix is NULL.
 *
 * @param s           The string to test.
 * @param s_len       Byte length of s.
 * @param prefix      The prefix to search for.
 * @param prefix_len  Byte length of prefix.
 * @return 1 if s starts with prefix, 0 otherwise.
 */
```

### 2.9 `gstrchr` (line 2043)

```c
/*
 * Finds the first occurrence of a grapheme in the string.
 * Returns a pointer to the matching grapheme, or NULL if not found.
 *
 * Returns NULL if s, grapheme is NULL, or either length is 0.
 *
 * @param s         The string to search in.
 * @param len       Byte length of s.
 * @param grapheme  The grapheme to search for.
 * @param g_len     Byte length of grapheme.
 * @return Pointer to the first match within s, or NULL.
 */
```

### 2.10 `gstrsub` (line 2377)

```c
/*
 * Extracts a substring of `count` grapheme clusters starting at
 * grapheme index `start_grapheme`, copying into dst.
 *
 * Always null-terminates dst. If the extracted region doesn't fit
 * in dst_size, truncates at a grapheme boundary. Returns bytes
 * written (excluding null terminator).
 *
 * Returns 0 if dst is NULL, dst_size is 0, src is NULL, src_len is 0,
 * count is 0, or start_grapheme is past end of string.
 *
 * @param dst              Destination buffer.
 * @param dst_size         Size of dst in bytes (including space for null).
 * @param src              Source string.
 * @param src_len          Byte length of src.
 * @param start_grapheme   0-based grapheme index of the start position.
 * @param count            Maximum number of graphemes to extract.
 * @return Bytes written to dst (excluding null terminator).
 */
```

### 2.11 `gstrcpy` (line 2429)

```c
/*
 * Copies a grapheme string from src to dst, preserving grapheme
 * boundaries on truncation. Always null-terminates.
 *
 * If src does not fit in dst_size, copies the maximum number of
 * complete graphemes that fit. Returns bytes written (excluding null).
 *
 * Returns 0 if dst is NULL, dst_size is 0, src is NULL, or src_len is 0.
 *
 * Behavior is undefined if dst and src overlap. Use memmove-based
 * copy if overlap is possible.
 *
 * @param dst       Destination buffer.
 * @param dst_size  Size of dst in bytes (including space for null).
 * @param src       Source string.
 * @param src_len   Byte length of src.
 * @return Bytes written (excluding null terminator).
 */
```

### 2.12 `gstrcat` (line 2512)

```c
/*
 * Appends src to the null-terminated string already in dst.
 * Always null-terminates. Preserves grapheme boundaries on truncation.
 * Returns total bytes in dst after append (excluding null).
 *
 * Returns 0 if dst is NULL or dst_size is 0. Returns current dst length
 * if src is NULL or src_len is 0.
 *
 * @param dst       Destination buffer (must already contain a null-
 *                  terminated string).
 * @param dst_size  Total size of dst in bytes.
 * @param src       Source string to append.
 * @param src_len   Byte length of src.
 * @return Total bytes in dst after append (excluding null terminator).
 */
```

### 2.13 `gstrdup` (line 2601)

```c
/*
 * Allocates a new null-terminated copy of the string.
 * The caller must free the returned pointer.
 *
 * Returns NULL if s is NULL or if malloc fails.
 *
 * @param s    Source string.
 * @param len  Byte length of s (bytes to copy; need not include null).
 * @return Newly allocated copy, or NULL on failure.
 */
```

### 2.14 `gstrsep` (line 2641)

```c
/*
 * Extracts the next token from the string, splitting on any grapheme
 * in the delimiter set. Similar to POSIX strsep() but grapheme-aware.
 *
 * On each call, returns a pointer to the token start and advances
 * *stringp past the delimiter. Writes the token's byte length to
 * *tok_len. When no more tokens remain, sets *stringp to NULL and
 * returns NULL.
 *
 * If delim is NULL or d_len is 0, the entire remaining string is
 * returned as one token.
 *
 * @param stringp   [in/out] Pointer to current position in the string.
 * @param lenp      [in/out] Pointer to remaining byte length.
 * @param delim     Set of delimiter graphemes.
 * @param d_len     Byte length of delim.
 * @param tok_len   [out] Receives the token's byte length. May be NULL.
 * @return Pointer to the token start, or NULL if no tokens remain.
 */
```

### 2.15 `gstrltrim` (line 2691)

```c
/*
 * Copies src to dst with leading whitespace removed.
 * Whitespace is ASCII only: space, tab, newline, carriage return,
 * vertical tab, form feed, and the CRLF pair.
 *
 * Always null-terminates. Returns bytes written (excluding null).
 *
 * @param dst       Destination buffer.
 * @param dst_size  Size of dst in bytes.
 * @param src       Source string.
 * @param src_len   Byte length of src.
 * @return Bytes written (excluding null terminator).
 */
```

### 2.16 `gstrrev` (line 2798)

```c
/*
 * Reverses the order of grapheme clusters in src and writes the
 * result to dst. Always null-terminates.
 *
 * Current implementation allocates a temporary array via malloc. Returns
 * 0 if allocation fails, dst is NULL, dst_size is 0, src is NULL, or
 * src_len is 0.
 *
 * Behavior is undefined if dst and src overlap.
 *
 * @param dst       Destination buffer.
 * @param dst_size  Size of dst in bytes.
 * @param src       Source string.
 * @param src_len   Byte length of src.
 * @return Bytes written (excluding null terminator), or 0 on failure.
 */
```

### 2.17 `gstrlower` (line 2958)

```c
/*
 * Converts ASCII uppercase letters (A-Z) to lowercase, copying to dst.
 * Non-ASCII bytes are copied unchanged. Does not perform Unicode case
 * folding (e.g., German sharp-s, Turkish dotless-i are unaffected).
 *
 * Always null-terminates. Preserves grapheme boundaries on truncation.
 *
 * @param dst       Destination buffer.
 * @param dst_size  Size of dst in bytes.
 * @param src       Source string.
 * @param src_len   Byte length of src.
 * @return Bytes written (excluding null terminator).
 */
```

### 2.18 `gstrfill` (line 3110)

```c
/*
 * Fills dst with `count` repetitions of the given grapheme.
 * Always null-terminates. Stops early if dst_size would be exceeded.
 *
 * Returns 0 if dst is NULL, dst_size is 0, grapheme is NULL,
 * g_len is 0, or count is 0.
 *
 * @param dst       Destination buffer.
 * @param dst_size  Size of dst in bytes.
 * @param grapheme  The grapheme to repeat.
 * @param g_len     Byte length of grapheme.
 * @param count     Number of repetitions.
 * @return Bytes written (excluding null terminator).
 */
```

### 2.19 `gstrspn` (line 2296)

```c
/*
 * Returns the count of leading graphemes in s that are present in
 * the accept set. Analogous to strspn() but operates on grapheme
 * clusters.
 *
 * Each grapheme in s is compared byte-for-byte against every grapheme
 * in accept. No normalization is applied.
 *
 * Returns 0 if s or accept is NULL, or either length is 0.
 *
 * @param s       The string to scan.
 * @param len     Byte length of s.
 * @param accept  Set of accepted graphemes.
 * @param a_len   Byte length of accept.
 * @return Number of leading graphemes in the accept set.
 */
```

---

## 3. `gstrrstr` Empty-Needle Fix

### Current Behavior

```c
if (n_len == 0)
    return haystack + h_len;  /* points one past the last byte */
```

### Problem

`gstrstr` returns `haystack` on empty needle (line 2099), consistent with the C standard's `strstr("")` behavior. `gstrrstr` returns `haystack + h_len`, which is inconsistent. While there is a conceptual argument that the "last occurrence of the empty string" is at the end, this creates a divergence that violates the principle of least surprise and produces a pointer that may point at the null terminator or, if the haystack was not null-terminated, past valid memory.

### Specification

**Change `gstrrstr` to return `haystack` when `n_len == 0`**, matching `gstrstr` and the C standard `strstr` precedent.

Rationale:
1. Consistency: `gstrstr` and `gstrrstr` should agree on the empty-needle edge case.
2. Safety: `haystack + h_len` is a past-the-end pointer. If the caller dereferences it or uses it as a string pointer, behavior is undefined. Returning `haystack` is always a valid, dereferenceable pointer (the string itself).
3. Precedent: Both POSIX `strstr` and glibc `memmem` return the haystack pointer for empty needles. There is no standard `strrstr` to follow, so deferring to `strstr` is the least-surprising choice.

The duplicate check on line 2162-2163 (`if (needle_graphemes == 0) return haystack + h_len`) should also be changed to `return haystack`.

---

## 4. `gstrrev` Malloc Elimination

### Current Implementation

The function (line 2798) calls `malloc` to allocate a temporary `int` array of grapheme boundary offsets. This is the only `gstr*` function (aside from `gstrdup`/`gstrndup`, whose allocation is inherent) that heap-allocates. A malloc failure causes the function to silently return 0 with an empty dst, which is indistinguishable from a zero-length input.

### Proposed Algorithm: Two-Pass Stack-Only Reversal

**Pass 1 (measure):** Walk the source string forward with `utf8_next_grapheme`, counting graphemes and computing the total byte length to copy (the minimum of `src_len` and what fits in `dst_size - 1`). This pass determines `n`, the number of graphemes that will fit.

**Pass 2 (copy in reverse):** Use `utf8_prev_grapheme` to walk backwards from the end of the source. For each grapheme found by walking backward:

1. Determine the grapheme span: `start = utf8_prev_grapheme(src, current_end)`, span is `src[start..current_end)`.
2. `memcpy(dst + write_pos, src + start, current_end - start)`.
3. Advance `write_pos`, set `current_end = start`.
4. Repeat until `n` graphemes have been copied or `current_end == 0`.

This eliminates the heap allocation entirely. The dst buffer itself is the only output storage. No temporary array is needed because `utf8_prev_grapheme` can reconstruct boundaries on demand.

### Complexity

- Pass 1: O(n) forward scan.
- Pass 2: O(n * backtrack) backward scan where backtrack is bounded by `GRAPHEME_MAX_BACKTRACK` (128 codepoints per `utf8_prev_grapheme` call). Effectively O(n) for typical text.
- Space: O(1) auxiliary (no heap allocation).

### Behavioral Change

With this change, `gstrrev` can never fail due to allocation. The return value of 0 unambiguously means "nothing to reverse" rather than "malloc failed."

### Alternative Considered: Stack Buffer with Fallback

An alternative is to use a fixed-size stack array (e.g., `int offsets[512]`) and fall back to malloc only for strings exceeding 512 graphemes. This avoids the O(n * backtrack) cost of repeated `utf8_prev_grapheme` calls. However, it adds complexity and the 512-limit is arbitrary. The two-pass approach is simpler and the backtrack cost is negligible for real-world strings.

---

## 5. `gstrendswith` Optimization

### Current Implementation

```c
size_t s_graphemes = gstrlen(s, s_len);           /* Pass 1: O(n) */
size_t suffix_graphemes = gstrlen(suffix, suffix_len);
size_t s_offset = gstroff(s, s_len, start_grapheme); /* Pass 2: O(n) */
/* Pass 3: compare remaining graphemes */           /* Pass 3: O(suffix_len) */
```

Total cost: O(n) + O(n) + O(suffix_len) = O(3n) where n is the byte length of the main string. For checking whether a long string ends with a short suffix, this is unnecessarily expensive.

### Proposed Algorithm: Backward Walk

```
1. Walk backward through s using utf8_prev_grapheme, collecting the last
   suffix_graphemes grapheme boundaries.
2. Walk backward through suffix using utf8_prev_grapheme, comparing each
   grapheme span byte-for-byte.
3. If all graphemes match, return 1. If any mismatch or s runs out before
   suffix, return 0.
```

Detailed steps:

1. Compute `suffix_graphemes = gstrlen(suffix, suffix_len)`. This is O(suffix_len), which is unavoidable since we must know how many graphemes to match.
2. Walk backward from `s + s_len` and from `suffix + suffix_len` simultaneously:
   - `s_end = s_len, suf_end = suffix_len`
   - For each of `suffix_graphemes` iterations:
     - `s_start = utf8_prev_grapheme(s, s_end)`
     - `suf_start = utf8_prev_grapheme(suffix, suf_end)`
     - Compare `s[s_start..s_end)` with `suffix[suf_start..suf_end)`.
     - If mismatch, return 0.
     - `s_end = s_start; suf_end = suf_start`
3. Return 1.

### Complexity

- O(suffix_len * backtrack) for the backward walks, where backtrack is bounded by `GRAPHEME_MAX_BACKTRACK`.
- Effectively O(suffix_len) for typical text, down from O(3n).
- No heap allocation.

### Edge Cases

- Empty suffix: return 1 (unchanged behavior).
- `suffix_len > s_len` (byte comparison): return 0 early.
- During backward walk, if `s_start` reaches 0 before all suffix graphemes are matched, return 0.

---

## 6. Overlapping Buffer Documentation

### Functions Where dst==src Is Undefined Behavior

Every function that takes both a `dst` output buffer and a `src`/`const char *` input, and that uses `memcpy` internally, exhibits undefined behavior when the buffers overlap. These functions should receive a doc comment addendum stating:

> **Behavior is undefined if dst and src overlap.** The caller must ensure the output buffer is disjoint from the input string.

**Functions requiring this annotation (18 total):**

| Function | Line | Uses `memcpy` |
|----------|------|---------------|
| `gstrsub` | 2377 | Yes |
| `gstrcpy` | 2429 | Yes |
| `gstrncpy` | 2466 | Yes |
| `gstrcat` | 2512 | Yes |
| `gstrncat` | 2552 | Yes |
| `gstrltrim` | 2691 | Yes (via `gstrcpy`) |
| `gstrrtrim` | 2721 | Yes (via `gstrcpy`) |
| `gstrtrim` | 2751 | Yes (via `gstrcpy`) |
| `gstrrev` | 2798 | Yes |
| `gstrreplace` | 2849 | Yes |
| `gstrlower` | 2958 | Yes (byte-by-byte, but from `src[i]` to `dst[i]`) |
| `gstrupper` | 2999 | Yes (byte-by-byte, same pattern) |
| `gstrellipsis` | 3041 | Yes (via `gstrncpy`) |
| `gstrfill` | 3110 | Yes |
| `gstrlpad` | 3138 | Yes |
| `gstrrpad` | 3206 | Yes |
| `gstrpad` | 3270 | Yes |
| `gstrwtrunc` | 3396 | Yes |
| `gstrwlpad` | 3453 | Yes |
| `gstrwrpad` | 3530 | Yes |
| `gstrwpad` | 3604 | Yes |

### Candidates for Overlap-Safe Behavior

Two functions have strong use cases for in-place operation and should be made overlap-safe using `memmove`:

1. **`gstrlower` / `gstrupper`** -- These perform byte-by-byte copy (`dst[i] = transform(src[i])`) where the output is the same length as the input. They already work correctly when `dst == src` because they write byte `i` after reading byte `i`. However, this is technically UB under the C standard if the pointers alias. The fix is trivial: no change needed to the loop structure, but add an explicit doc comment that `dst == src` is permitted for these two functions, since the transformation is byte-for-byte in-place and never changes byte count.

2. **`gstrcpy`** -- The most common "I want to truncate in place" pattern is `gstrcpy(buf, new_smaller_size, buf, old_len)`. Replacing the single `memcpy` call with `memmove` would make this safe with negligible performance impact. Recommended.

All other functions involve complex multi-source assembly (padding characters, replacement strings, reordering) where overlap safety would require a complete temporary buffer, defeating the purpose. These should remain documented as UB on overlap.

---

## 7. `gstrwellipsis` -- Width-Aware Ellipsis

### Problem

`gstrellipsis` truncates by grapheme count. But terminal applications need to truncate by column width. A string like `"Hello世界World"` (13 columns) truncated to 10 graphemes would keep 10 graphemes, but these could be 12+ columns wide. There is no way to truncate to a column budget and append an ellipsis marker.

### Proposed Signature

```c
static inline size_t gstrwellipsis(char *dst, size_t dst_size,
                                   const char *src, size_t src_len,
                                   size_t max_cols,
                                   const char *ellipsis,
                                   size_t ellipsis_len);
```

### Behavior

1. Compute `src_width = gstrwidth(src, src_len)`.
2. If `src_width <= max_cols`, copy src to dst unchanged (same as `gstrellipsis` pass-through).
3. Compute `ell_width = gstrwidth(ellipsis, ellipsis_len)`. If ellipsis is NULL, default to `"..."` (width 3).
4. If `ell_width >= max_cols`, truncate the ellipsis itself to fit `max_cols` columns using `gstrwtrunc`.
5. Otherwise, compute `text_cols = max_cols - ell_width`. Use `gstrwtrunc` to copy at most `text_cols` columns of source text, then append the ellipsis.
6. Always null-terminate. Return bytes written.

### Difference from `gstrellipsis`

| | `gstrellipsis` | `gstrwellipsis` |
|---|---|---|
| Budget unit | Grapheme count | Terminal columns |
| `"Hello世界"` truncated to 8 | `"Hello世界"` (8 graphemes, 11 cols) | `"Hello..."` (8 cols) |
| Appropriate for | Logical character limits (DB fields) | Terminal/TUI display |

### Returns

Bytes written to dst (excluding null terminator).

---

## 8. Internal Helpers: `static` to `static inline`

The following internal helper functions are declared `static` without `inline`. In a header-only library where every translation unit includes the header, `static` (non-inline) functions generate a separate copy in every `.o` file with no hint to the compiler that inlining is desired. Adding `inline` allows the compiler to inline them at call sites (they are small, frequently called) and avoids bloating object files in multi-TU builds.

| Function | Line | Rationale |
|----------|------|-----------|
| `unicode_range_contains` | 1010 | Called by every width/GCB lookup; 16 lines; hot path |
| `get_gcb` | 1031 | Called once per codepoint during segmentation; 20 lines |
| `is_extended_pictographic` | 1056 | Wrapper around `unicode_range_contains`; 3 lines |
| `is_incb_linker` | 1064 | Binary search; 14 lines; called during segmentation |
| `is_incb_consonant` | 1083 | Wrapper around `unicode_range_contains`; 2 lines |
| `is_grapheme_break` | 1091 | Core segmentation decision; 70 lines; called per codepoint |
| `gstr_cmp_grapheme` | 1539 | Comparison helper; 11 lines; called per grapheme in cmp/search |
| `gstr_ascii_lower` | 1555 | Single-branch conversion; 4 lines; called per byte in case ops |
| `gstr_ascii_upper` | 1564 | Single-branch conversion; 4 lines |
| `gstr_cmp_grapheme_icase` | 1573 | Case-insensitive comparison; 14 lines |
| `gstr_grapheme_in_set` | 1592 | Set membership test; 13 lines; called in span/tokenize |
| `gstr_is_whitespace` | 1610 | Whitespace check; 8 lines; called in trim functions |

All 12 functions should become `static inline`.

---

## 9. Namespace Prefixing

The following symbols defined in `gstr.h` lack a `GSTR_` or `gstr_` prefix. They risk colliding with user code or other libraries, particularly the very generic names like `unicode_range` and `gcb_property`.

### Macros

| Current Name | Proposed Name |
|---|---|
| `UTF8_REPLACEMENT_CHAR` | `GSTR_UTF8_REPLACEMENT_CHAR` |
| `UTF8_MAX_BYTES` | `GSTR_UTF8_MAX_BYTES` |
| `ZERO_WIDTH_COUNT` | `GSTR_ZERO_WIDTH_COUNT` |
| `EAW_WIDE_COUNT` | `GSTR_EAW_WIDE_COUNT` |
| `EXTENDED_PICTOGRAPHIC_COUNT` | `GSTR_EXTENDED_PICTOGRAPHIC_COUNT` |
| `HANGUL_SBASE` | `GSTR_HANGUL_SBASE` |
| `HANGUL_SEND` | `GSTR_HANGUL_SEND` |
| `HANGUL_TCOUNT` | `GSTR_HANGUL_TCOUNT` |
| `GRAPHEME_MAX_BACKTRACK` | `GSTR_GRAPHEME_MAX_BACKTRACK` |

### Structs

| Current Name | Proposed Name |
|---|---|
| `struct unicode_range` | `struct gstr_unicode_range` |
| `struct gcb_range` | `struct gstr_gcb_range` |

### Enums

| Current Name | Proposed Name |
|---|---|
| `enum gcb_property` | `enum gstr_gcb_property` |
| `GCB_OTHER` | `GSTR_GCB_OTHER` |
| `GCB_CR` | `GSTR_GCB_CR` |
| `GCB_LF` | `GSTR_GCB_LF` |
| `GCB_CONTROL` | `GSTR_GCB_CONTROL` |
| `GCB_EXTEND` | `GSTR_GCB_EXTEND` |
| `GCB_ZWJ` | `GSTR_GCB_ZWJ` |
| `GCB_REGIONAL_INDICATOR` | `GSTR_GCB_REGIONAL_INDICATOR` |
| `GCB_PREPEND` | `GSTR_GCB_PREPEND` |
| `GCB_SPACING_MARK` | `GSTR_GCB_SPACING_MARK` |
| `GCB_L` | `GSTR_GCB_L` |
| `GCB_V` | `GSTR_GCB_V` |
| `GCB_T` | `GSTR_GCB_T` |
| `GCB_LV` | `GSTR_GCB_LV` |
| `GCB_LVT` | `GSTR_GCB_LVT` |

### Static Data Arrays

| Current Name | Proposed Name |
|---|---|
| `ZERO_WIDTH_RANGES` | `gstr_zero_width_ranges` |
| `EAW_WIDE_RANGES` | `gstr_eaw_wide_ranges` |
| `EXTENDED_PICTOGRAPHIC_RANGES` | `gstr_extended_pictographic_ranges` |
| `GCB_RANGES` | `gstr_gcb_ranges` |
| `INCB_LINKERS` | `gstr_incb_linkers` |
| `INCB_CONSONANTS` | `gstr_incb_consonants` |

### Internal Helper Functions

| Current Name | Proposed Name |
|---|---|
| `unicode_range_contains` | `gstr_unicode_range_contains` |
| `get_gcb` | `gstr_get_gcb` |
| `is_extended_pictographic` | `gstr_is_extended_pictographic` |
| `is_incb_linker` | `gstr_is_incb_linker` |
| `is_incb_consonant` | `gstr_is_incb_consonant` |
| `is_grapheme_break` | `gstr_is_grapheme_break` |

**Note on `utf8_*` functions:** The 14 public `utf8_*` functions (`utf8_decode`, `utf8_encode`, `utf8_cpwidth`, `utf8_charwidth`, `utf8_is_zerowidth`, `utf8_is_wide`, `utf8_next`, `utf8_prev`, `utf8_next_grapheme`, `utf8_prev_grapheme`, `utf8_valid`, `utf8_cpcount`, `utf8_truncate`) use a `utf8_` prefix which, while not `gstr_`, is reasonably distinctive and matches the two-layer naming convention documented in the README. These should be left as-is unless a collision is reported, but could optionally be aliased as `gstr_utf8_*` with backward-compatible macros.

### Migration Strategy

To avoid breaking existing users, the old names should be kept as macros aliasing to the new names, guarded behind a deprecation opt-out:

```c
#ifndef GSTR_NO_COMPAT
#define UTF8_REPLACEMENT_CHAR  GSTR_UTF8_REPLACEMENT_CHAR
#define UTF8_MAX_BYTES         GSTR_UTF8_MAX_BYTES
#define unicode_range          gstr_unicode_range
/* ... etc ... */
#endif
```

This lets existing code compile unchanged while new code can define `GSTR_NO_COMPAT` to get a clean namespace.