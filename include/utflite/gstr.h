/*
 * gstr.h - Grapheme-based string library
 *
 * A grapheme-first equivalent to string.h. Where string.h operates on bytes,
 * gstr.h operates on grapheme clusters - the units humans perceive as
 * "characters".
 *
 * Example:
 *   strlen("👨‍👩‍👧")  → 18 (bytes)
 *   gstrlen("👨‍👩‍👧", 18) → 1  (graphemes)
 *
 * Naming Convention:
 *   - utf8_* - Low-level UTF-8 engine (encoding, decoding, validation,
 * navigation)
 *   - gstr*  - High-level grapheme string API (string.h equivalents for
 * graphemes)
 *
 * Design decisions:
 *   - Unit of operation: grapheme cluster (UAX #29)
 *   - Equality: byte-exact (no normalization)
 *   - Search: complete graphemes only (won't find 👩 inside
 * 👨‍👩‍👧)
 *   - All functions take (ptr, len) pairs for explicit bounds
 *
 * Usage:
 *   #include <utflite/gstr.h>
 *
 * Or use the single-header version:
 *   #define GSTR_IMPLEMENTATION
 *   #include "gstr.h"
 */

#ifndef GSTR_H
#define GSTR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Version info (set by build system, fallback for header-only use) */
#ifndef GSTR_VERSION
#define GSTR_VERSION "0.0.0+dev"
#endif
#ifndef GSTR_BUILD_ID
#define GSTR_BUILD_ID "dev"
#endif

/* ============================================================================
 * Constants
 * ============================================================================
 */

/* Unicode replacement character (returned on decode errors) */
#define UTF8_REPLACEMENT_CHAR 0xFFFD

/* Maximum bytes needed to encode any Unicode codepoint */
#define UTF8_MAX_BYTES 4

/* ============================================================================
 * UTF-8 Layer: Encoding/Decoding
 * ============================================================================
 */

/*
 * Decodes UTF-8 bytes into a codepoint. The bytes parameter points to UTF-8
 * data, length specifies available bytes, and codepoint receives the decoded
 * value (U+FFFD on error). Returns bytes consumed (1-4), or 0 for null/empty.
 * Rejects overlong encodings, surrogates, and values above U+10FFFF.
 */
int utf8_decode(const char *bytes, int length, uint32_t *codepoint);

/*
 * Encodes a codepoint as UTF-8. The codepoint must be 0x0000-0x10FFFF, buffer
 * needs UTF8_MAX_BYTES space. Returns bytes written (1-4), or 0 for invalid
 * codepoints. Does not null-terminate.
 */
int utf8_encode(uint32_t codepoint, char *buffer);

/* ============================================================================
 * UTF-8 Layer: Validation
 * ============================================================================
 */

/*
 * Validates UTF-8 encoding. Returns 1 if valid, 0 if invalid. Sets error_offset
 * (if provided) to first error position.
 */
int utf8_valid(const char *text, int length, int *error_offset);

/* ============================================================================
 * UTF-8 Layer: Codepoint Navigation
 * ============================================================================
 */

/*
 * Returns byte offset of next UTF-8 codepoint. Advances past current char.
 * Returns length if at end.
 */
int utf8_next(const char *text, int length, int offset);

/*
 * Returns byte offset where previous UTF-8 codepoint starts. Returns 0 if at
 * beginning.
 */
int utf8_prev(const char *text, int offset);

/*
 * Counts Unicode codepoints (not bytes or graphemes). Returns 0 for null.
 */
int utf8_cpcount(const char *text, int length);

/* ============================================================================
 * UTF-8 Layer: Grapheme Navigation
 * ============================================================================
 */

/*
 * Returns byte offset of next grapheme cluster boundary per UAX #29. Handles
 * emoji ZWJ sequences, combining marks, flags, and Hangul.
 */
int utf8_next_grapheme(const char *text, int length, int offset);

/*
 * Returns byte offset of previous grapheme cluster boundary per UAX #29.
 */
int utf8_prev_grapheme(const char *text, int offset);

/* ============================================================================
 * UTF-8 Layer: Display Width
 * ============================================================================
 */

/*
 * Returns display width of codepoint in terminal columns.
 * Returns -1 for control chars, 0 for combining marks, 1 for normal, 2 for
 * wide.
 */
int utf8_cpwidth(uint32_t codepoint);

/*
 * Returns display width of UTF-8 char at offset. Convenience function combining
 * decode and width lookup. Returns 0 for null or invalid offset.
 */
int utf8_charwidth(const char *text, int length, int offset);

/*
 * Checks if codepoint is zero-width (combining marks, format chars, ZWJ).
 * Returns 1 if zero-width, 0 otherwise.
 */
int utf8_is_zerowidth(uint32_t codepoint);

/*
 * Checks if codepoint is double-width (CJK, emoji). Returns 1 if double-width,
 * 0 otherwise.
 */
int utf8_is_wide(uint32_t codepoint);

/*
 * Finds byte offset to truncate string at max display width. Wide chars
 * exceeding limit are excluded. Returns length if string fits. Returns 0
 * for null.
 */
int utf8_truncate(const char *text, int length, int max_cols);

/* ============================================================================
 * Grapheme String Layer: Length Functions
 * ============================================================================
 */

/*
 * Returns the number of grapheme clusters in the string.
 * Equivalent to strlen() but counts graphemes instead of bytes.
 * Returns 0 for NULL or empty input.
 */
size_t gstrlen(const char *s, size_t byte_len);

/*
 * Returns the number of grapheme clusters, up to max_graphemes.
 * Useful for truncation checks without scanning the entire string.
 * Returns 0 for NULL or empty input.
 */
size_t gstrnlen(const char *s, size_t byte_len, size_t max_graphemes);

/*
 * Calculates total display width in columns. CJK/emoji count as 2, most chars
 * as 1, combining marks as 0. Returns 0 for null.
 */
size_t gstrwidth(const char *s, size_t byte_len);

/* ============================================================================
 * Grapheme String Layer: Indexing Functions
 * ============================================================================
 */

/*
 * Returns byte offset of the Nth grapheme cluster (0-indexed).
 * Returns byte_len if n >= grapheme count.
 * Returns 0 for NULL input.
 */
size_t gstroff(const char *s, size_t byte_len, size_t grapheme_n);

/*
 * Returns pointer to the Nth grapheme cluster (0-indexed).
 * If out_len is not NULL, stores the byte length of the grapheme.
 * Returns NULL if n >= grapheme count or input is NULL.
 */
const char *gstrat(const char *s, size_t byte_len, size_t grapheme_n,
                   size_t *out_len);

/* ============================================================================
 * Grapheme String Layer: Comparison Functions
 * ============================================================================
 */

/*
 * Compares two strings grapheme-by-grapheme.
 * Returns <0 if a < b, 0 if equal, >0 if a > b.
 * Comparison is byte-exact (no normalization).
 */
int gstrcmp(const char *a, size_t a_len, const char *b, size_t b_len);

/*
 * Compares first n graphemes of two strings.
 * Returns <0 if a < b, 0 if equal, >0 if a > b.
 */
int gstrncmp(const char *a, size_t a_len, const char *b, size_t b_len,
             size_t n);

/*
 * Case-insensitive comparison (ASCII only).
 * Non-ASCII characters are compared byte-exact.
 * Returns <0 if a < b, 0 if equal, >0 if a > b.
 */
int gstrcasecmp(const char *a, size_t a_len, const char *b, size_t b_len);

/*
 * Case-insensitive comparison of first n graphemes (ASCII only).
 * Non-ASCII characters are compared byte-exact.
 * Returns <0 if a < b, 0 if equal, >0 if a > b.
 */
int gstrncasecmp(const char *a, size_t a_len, const char *b, size_t b_len,
                 size_t n);

/* ============================================================================
 * Grapheme String Layer: Prefix/Suffix Functions
 * ============================================================================
 */

/*
 * Checks if string starts with prefix.
 * Comparison is grapheme-by-grapheme.
 * Returns 1 if yes, 0 if no.
 */
int gstrstartswith(const char *s, size_t s_len, const char *prefix,
                   size_t prefix_len);

/*
 * Checks if string ends with suffix.
 * Comparison is grapheme-by-grapheme.
 * Returns 1 if yes, 0 if no.
 */
int gstrendswith(const char *s, size_t s_len, const char *suffix,
                 size_t suffix_len);

/* ============================================================================
 * Grapheme String Layer: Search Functions
 * ============================================================================
 */

/*
 * Finds first occurrence of grapheme in string.
 * The grapheme parameter must be a complete grapheme cluster.
 * Returns pointer to match, or NULL if not found.
 */
const char *gstrchr(const char *s, size_t len, const char *grapheme,
                    size_t g_len);

/*
 * Finds last occurrence of grapheme in string.
 * Returns pointer to match, or NULL if not found.
 */
const char *gstrrchr(const char *s, size_t len, const char *grapheme,
                     size_t g_len);

/*
 * Finds first occurrence of needle substring in haystack.
 * Both strings are treated as sequences of graphemes.
 * Match must align on grapheme boundaries.
 * Returns pointer to match, or NULL if not found.
 */
const char *gstrstr(const char *haystack, size_t h_len, const char *needle,
                    size_t n_len);

/*
 * Finds last occurrence of needle substring in haystack.
 * Both strings are treated as sequences of graphemes.
 * Match must align on grapheme boundaries.
 * Returns pointer to match, or NULL if not found.
 */
const char *gstrrstr(const char *haystack, size_t h_len, const char *needle,
                     size_t n_len);

/*
 * Case-insensitive substring search (ASCII only).
 * Non-ASCII characters are compared byte-exact.
 * Returns pointer to match, or NULL if not found.
 */
const char *gstrcasestr(const char *haystack, size_t h_len, const char *needle,
                        size_t n_len);

/*
 * Counts non-overlapping occurrences of needle in string.
 * Returns 0 if needle is empty or not found.
 */
size_t gstrcount(const char *s, size_t len, const char *needle, size_t n_len);

/* ============================================================================
 * Grapheme String Layer: Span Functions
 * ============================================================================
 */

/*
 * Returns count of leading graphemes that are in accept set.
 * The accept set is a string of graphemes to match.
 */
size_t gstrspn(const char *s, size_t len, const char *accept, size_t a_len);

/*
 * Returns count of leading graphemes that are NOT in reject set.
 */
size_t gstrcspn(const char *s, size_t len, const char *reject, size_t r_len);

/*
 * Finds first grapheme in s that is in accept set.
 * Returns pointer to matching grapheme, or NULL if none found.
 */
const char *gstrpbrk(const char *s, size_t len, const char *accept,
                     size_t a_len);

/* ============================================================================
 * Grapheme String Layer: Extraction Functions
 * ============================================================================
 */

/*
 * Extracts a range of graphemes into dst buffer.
 * Copies graphemes [start_grapheme, start_grapheme + count).
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrsub(char *dst, size_t dst_size, const char *src, size_t src_len,
               size_t start_grapheme, size_t count);

/* ============================================================================
 * Grapheme String Layer: Copy Functions
 * ============================================================================
 */

/*
 * Copies all graphemes from src to dst.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrcpy(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Copies up to n graphemes from src to dst.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrncpy(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t n);

/* ============================================================================
 * Grapheme String Layer: Concatenation Functions
 * ============================================================================
 */

/*
 * Appends src to dst. dst must be null-terminated.
 * Returns total bytes in dst (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrcat(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Appends up to n graphemes from src to dst.
 * Returns total bytes in dst (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrncat(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t n);

/* ============================================================================
 * Grapheme String Layer: Allocation Functions
 * ============================================================================
 */

/*
 * Allocates and copies entire string.
 * Returns malloc'd buffer containing the string.
 * Caller must free() the returned pointer.
 * Returns NULL on allocation failure or NULL input.
 */
char *gstrdup(const char *s, size_t len);

/*
 * Allocates and copies first n graphemes.
 * Returns malloc'd buffer containing up to n graphemes.
 * Caller must free() the returned pointer.
 * Returns NULL on allocation failure or NULL input.
 */
char *gstrndup(const char *s, size_t len, size_t n);

/* ============================================================================
 * Grapheme String Layer: Tokenization Functions
 * ============================================================================
 */

/*
 * Stateless tokenization (strsep-style).
 * Updates *stringp and *lenp to point past the token and delimiter.
 * Returns pointer to token start, or NULL if no more tokens.
 * Stores token length in *tok_len if not NULL.
 */
const char *gstrsep(const char **stringp, size_t *lenp, const char *delim,
                    size_t d_len, size_t *tok_len);

/* ============================================================================
 * Grapheme String Layer: Trimming Functions
 * ============================================================================
 */

/*
 * Trims leading ASCII whitespace (space, tab, newline, CR, VT, FF).
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrltrim(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Trims trailing ASCII whitespace (space, tab, newline, CR, VT, FF).
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrrtrim(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Trims both leading and trailing ASCII whitespace.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrtrim(char *dst, size_t dst_size, const char *src, size_t src_len);

/* ============================================================================
 * Grapheme String Layer: Transformation Functions
 * ============================================================================
 */

/*
 * Reverses string by graphemes.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrrev(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Replaces all occurrences of old with new.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 * Truncates at complete grapheme boundary if buffer is too small.
 */
size_t gstrreplace(char *dst, size_t dst_size, const char *src, size_t src_len,
                   const char *old, size_t old_len, const char *new_str,
                   size_t new_len);

/* ============================================================================
 * Grapheme String Layer: Case Conversion Functions
 * ============================================================================
 */

/*
 * Converts ASCII characters to lowercase.
 * Non-ASCII characters are copied unchanged.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrlower(char *dst, size_t dst_size, const char *src, size_t src_len);

/*
 * Converts ASCII characters to uppercase.
 * Non-ASCII characters are copied unchanged.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrupper(char *dst, size_t dst_size, const char *src, size_t src_len);

/* ============================================================================
 * Grapheme String Layer: Display/Truncation Functions
 * ============================================================================
 */

/*
 * Truncates to max_graphemes and appends ellipsis.
 * If string has <= max_graphemes, copies unchanged.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrellipsis(char *dst, size_t dst_size, const char *src, size_t src_len,
                    size_t max_graphemes, const char *ellipsis,
                    size_t ellipsis_len);

/* ============================================================================
 * Grapheme String Layer: Fill/Padding Functions
 * ============================================================================
 */

/*
 * Fills buffer with repeated grapheme.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrfill(char *dst, size_t dst_size, const char *grapheme, size_t g_len,
                size_t count);

/*
 * Pads string to width with grapheme (left padding).
 * If string >= width graphemes, copies unchanged.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrlpad(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t width, const char *pad, size_t pad_len);

/*
 * Pads string to width with grapheme (right padding).
 * If string >= width graphemes, copies unchanged.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrrpad(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t width, const char *pad, size_t pad_len);

/*
 * Pads string to width with grapheme (center padding).
 * If string >= width graphemes, copies unchanged.
 * Extra padding goes on the right if width - src_len is odd.
 * Returns bytes written (excluding null terminator).
 * Always null-terminates if dst_size > 0.
 */
size_t gstrpad(char *dst, size_t dst_size, const char *src, size_t src_len,
               size_t width, const char *pad, size_t pad_len);

#ifdef __cplusplus
}
#endif

#endif /* GSTR_H */
