/*
 * utflite.h - Lightweight UTF-8 handling library
 *
 * Unicode 17.0 compliant UTF-8 encoding/decoding with character width support.
 * Zero external dependencies beyond standard C.
 *
 * Usage:
 *   #include <utflite/utflite.h>
 *
 * Or use the single-header version:
 *   #define UTFLITE_IMPLEMENTATION
 *   #include "utflite.h"
 */

#ifndef UTFLITE_H
#define UTFLITE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Library version: major.minor.patch */
#define UTFLITE_VERSION_MAJOR 1
#define UTFLITE_VERSION_MINOR 5
#define UTFLITE_VERSION_PATCH 3
#define UTFLITE_VERSION_STRING "1.5.3"

#define UTFLITE_UNICODE_VERSION 170 /* Unicode 17.0 */

/* Unicode replacement character (returned on decode errors) */
#define UTFLITE_REPLACEMENT_CHAR 0xFFFD

/* Maximum bytes needed to encode any Unicode codepoint */
#define UTFLITE_MAX_BYTES 4

/* ============================================================================
 * Core Encoding/Decoding
 * ============================================================================
 */

/*
 * Decodes UTF-8 bytes into a codepoint. The bytes parameter points to UTF-8
 * data, length specifies available bytes, and codepoint receives the decoded
 * value (U+FFFD on error). Returns bytes consumed (1-4), or 0 for null/empty.
 * Rejects overlong encodings, surrogates, and values above U+10FFFF.
 */
int utflite_decode(const char *bytes, int length, uint32_t *codepoint);

/*
 * Encodes a codepoint as UTF-8. The codepoint must be 0x0000-0x10FFFF, buffer
 * needs UTFLITE_MAX_BYTES space. Returns bytes written (1-4), or 0 for invalid
 * codepoints. Does not null-terminate.
 */
int utflite_encode(uint32_t codepoint, char *buffer);

/* ============================================================================
 * Character Width (Display Columns)
 * ============================================================================
 */

/*
 * Returns display width in terminal columns. Control chars return -1, combining
 * marks return 0, normal chars return 1, CJK/emoji return 2.
 */
int utflite_codepoint_width(uint32_t codepoint);

/*
 * Returns display width of UTF-8 char at offset. Convenience function combining
 * decode and width lookup. Returns 0 for null or invalid offset.
 */
int utflite_char_width(const char *text, int length, int offset);

/* ============================================================================
 * String Navigation
 * ============================================================================
 */

/*
 * Returns byte offset of next UTF-8 character. Advances past current char.
 * Returns length if at end.
 */
int utflite_next_char(const char *text, int length, int offset);

/*
 * Returns byte offset where previous UTF-8 character starts. Returns 0 if at
 * beginning.
 */
int utflite_prev_char(const char *text, int offset);

/* ============================================================================
 * Grapheme Cluster Navigation (UAX #29)
 * ============================================================================
 */

/*
 * Returns byte offset of next grapheme cluster boundary per UAX #29. Handles
 * emoji ZWJ sequences, combining marks, flags, and Hangul.
 */
int utflite_next_grapheme(const char *text, int length, int offset);

/*
 * Returns byte offset of previous grapheme cluster boundary per UAX #29.
 */
int utflite_prev_grapheme(const char *text, int offset);

/* ============================================================================
 * Utility Functions
 * ============================================================================
 */

/*
 * Validates UTF-8 encoding. Returns 1 if valid, 0 if invalid. Sets error_offset
 * (if provided) to first error position.
 */
int utflite_validate(const char *text, int length, int *error_offset);

/*
 * Counts Unicode codepoints (not bytes or graphemes). Returns 0 for null.
 */
int utflite_codepoint_count(const char *text, int length);

/*
 * Counts grapheme clusters (user-perceived characters) in a UTF-8 string. A
 * grapheme may contain multiple codepoints, such as an emoji with skin tone
 * or a letter with combining accents. Returns 0 for null or empty input.
 */
int utflite_grapheme_count(const char *text, int length);

/*
 * Calculates total display width in columns. CJK/emoji count as 2, most chars
 * as 1, combining marks as 0. Returns 0 for null.
 */
int utflite_string_width(const char *text, int length);

/*
 * Checks if codepoint is zero-width (combining marks, format chars, ZWJ).
 * Returns 1 if zero-width, 0 otherwise.
 */
int utflite_is_zero_width(uint32_t codepoint);

/*
 * Checks if codepoint is double-width (CJK, emoji). Returns 1 if double-width,
 * 0 otherwise.
 */
int utflite_is_wide(uint32_t codepoint);

/*
 * Finds byte offset to truncate string at max display width. Wide chars
 * exceeding limit are excluded. Returns length if string fits. Returns 0
 * for null.
 */
int utflite_truncate(const char *text, int length, int max_cols);

#ifdef __cplusplus
}
#endif

#endif /* UTFLITE_H */
