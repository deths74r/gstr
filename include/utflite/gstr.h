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

#include "utflite.h"
#include <stddef.h>

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
 * Length Functions
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

/* ============================================================================
 * Indexing Functions
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
 * Comparison Functions
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

/* ============================================================================
 * Search Functions
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

/* ============================================================================
 * Span Functions
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
 * Extraction Functions
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
 * Copy Functions
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
 * Concatenation Functions
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

#ifdef __cplusplus
}
#endif

#endif /* GSTR_H */
