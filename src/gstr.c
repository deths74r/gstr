/*
 * gstr.c - Grapheme string library implementation
 *
 * Builds on utflite's grapheme iteration to provide string.h-like operations
 * that work on grapheme clusters instead of bytes.
 */

#include <stdlib.h>
#include <string.h>
#include <utflite/gstr.h>
#include <utflite/utflite.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================
 */

/*
 * Compares a single grapheme at position a with grapheme at position b.
 * Returns <0, 0, or >0 like memcmp.
 */
static int gstr_cmp_grapheme(const char *a, size_t a_len, const char *b,
                             size_t b_len) {
  size_t min_len = a_len < b_len ? a_len : b_len;
  int result = memcmp(a, b, min_len);
  if (result != 0)
    return result;
  /* Same prefix, shorter string is "less" */
  if (a_len < b_len)
    return -1;
  if (a_len > b_len)
    return 1;
  return 0;
}

/*
 * Converts ASCII uppercase to lowercase for case-insensitive comparison.
 */
static char gstr_ascii_lower(char c) {
  if (c >= 'A' && c <= 'Z')
    return (char)(c + ('a' - 'A'));
  return c;
}

/*
 * Case-insensitive comparison of two byte sequences (ASCII only).
 */
static int gstr_cmp_grapheme_icase(const char *a, size_t a_len, const char *b,
                                   size_t b_len) {
  size_t min_len = a_len < b_len ? a_len : b_len;
  for (size_t i = 0; i < min_len; i++) {
    char ca = gstr_ascii_lower(a[i]);
    char cb = gstr_ascii_lower(b[i]);
    if (ca != cb)
      return (unsigned char)ca - (unsigned char)cb;
  }
  if (a_len < b_len)
    return -1;
  if (a_len > b_len)
    return 1;
  return 0;
}

/*
 * Checks if grapheme g (of length g_len) is in the set of graphemes.
 * The set is a string where each grapheme is a potential match.
 */
static int gstr_grapheme_in_set(const char *g, size_t g_len, const char *set,
                                size_t set_len) {
  int offset = 0;
  while ((size_t)offset < set_len) {
    int next = utflite_next_grapheme(set, (int)set_len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (grapheme_len == g_len && memcmp(g, set + offset, g_len) == 0) {
      return 1;
    }
    offset = next;
  }
  return 0;
}

/* ============================================================================
 * Length Functions
 * ============================================================================
 */

size_t gstrlen(const char *s, size_t byte_len) {
  if (!s || byte_len == 0)
    return 0;

  size_t count = 0;
  int offset = 0;

  while ((size_t)offset < byte_len) {
    offset = utflite_next_grapheme(s, (int)byte_len, offset);
    count++;
  }

  return count;
}

size_t gstrnlen(const char *s, size_t byte_len, size_t max_graphemes) {
  if (!s || byte_len == 0 || max_graphemes == 0)
    return 0;

  size_t count = 0;
  int offset = 0;

  while ((size_t)offset < byte_len && count < max_graphemes) {
    offset = utflite_next_grapheme(s, (int)byte_len, offset);
    count++;
  }

  return count;
}

/* ============================================================================
 * Indexing Functions
 * ============================================================================
 */

size_t gstroff(const char *s, size_t byte_len, size_t grapheme_n) {
  if (!s)
    return 0;

  int offset = 0;
  size_t count = 0;

  while ((size_t)offset < byte_len && count < grapheme_n) {
    offset = utflite_next_grapheme(s, (int)byte_len, offset);
    count++;
  }

  return (size_t)offset;
}

const char *gstrat(const char *s, size_t byte_len, size_t grapheme_n,
                   size_t *out_len) {
  if (!s || byte_len == 0)
    return NULL;

  int offset = 0;
  size_t count = 0;

  /* Find the start of the Nth grapheme */
  while ((size_t)offset < byte_len && count < grapheme_n) {
    offset = utflite_next_grapheme(s, (int)byte_len, offset);
    count++;
  }

  if ((size_t)offset >= byte_len)
    return NULL;

  /* Find the end of this grapheme */
  int next = utflite_next_grapheme(s, (int)byte_len, offset);

  if (out_len)
    *out_len = (size_t)(next - offset);

  return s + offset;
}

/* ============================================================================
 * Comparison Functions
 * ============================================================================
 */

int gstrcmp(const char *a, size_t a_len, const char *b, size_t b_len) {
  if (!a && !b)
    return 0;
  if (!a)
    return -1;
  if (!b)
    return 1;

  int a_off = 0;
  int b_off = 0;

  while ((size_t)a_off < a_len && (size_t)b_off < b_len) {
    int a_next = utflite_next_grapheme(a, (int)a_len, a_off);
    int b_next = utflite_next_grapheme(b, (int)b_len, b_off);

    size_t a_glen = (size_t)(a_next - a_off);
    size_t b_glen = (size_t)(b_next - b_off);

    int cmp = gstr_cmp_grapheme(a + a_off, a_glen, b + b_off, b_glen);
    if (cmp != 0)
      return cmp;

    a_off = a_next;
    b_off = b_next;
  }

  /* If we've exhausted both strings, they're equal */
  if ((size_t)a_off >= a_len && (size_t)b_off >= b_len)
    return 0;

  /* Shorter string is "less" */
  if ((size_t)a_off >= a_len)
    return -1;
  return 1;
}

int gstrncmp(const char *a, size_t a_len, const char *b, size_t b_len,
             size_t n) {
  if (n == 0)
    return 0;
  if (!a && !b)
    return 0;
  if (!a)
    return -1;
  if (!b)
    return 1;

  int a_off = 0;
  int b_off = 0;
  size_t count = 0;

  while ((size_t)a_off < a_len && (size_t)b_off < b_len && count < n) {
    int a_next = utflite_next_grapheme(a, (int)a_len, a_off);
    int b_next = utflite_next_grapheme(b, (int)b_len, b_off);

    size_t a_glen = (size_t)(a_next - a_off);
    size_t b_glen = (size_t)(b_next - b_off);

    int cmp = gstr_cmp_grapheme(a + a_off, a_glen, b + b_off, b_glen);
    if (cmp != 0)
      return cmp;

    a_off = a_next;
    b_off = b_next;
    count++;
  }

  if (count == n)
    return 0;

  /* Didn't reach n graphemes - one string is shorter */
  if ((size_t)a_off >= a_len && (size_t)b_off >= b_len)
    return 0;
  if ((size_t)a_off >= a_len)
    return -1;
  return 1;
}

int gstrcasecmp(const char *a, size_t a_len, const char *b, size_t b_len) {
  if (!a && !b)
    return 0;
  if (!a)
    return -1;
  if (!b)
    return 1;

  int a_off = 0;
  int b_off = 0;

  while ((size_t)a_off < a_len && (size_t)b_off < b_len) {
    int a_next = utflite_next_grapheme(a, (int)a_len, a_off);
    int b_next = utflite_next_grapheme(b, (int)b_len, b_off);

    size_t a_glen = (size_t)(a_next - a_off);
    size_t b_glen = (size_t)(b_next - b_off);

    int cmp = gstr_cmp_grapheme_icase(a + a_off, a_glen, b + b_off, b_glen);
    if (cmp != 0)
      return cmp;

    a_off = a_next;
    b_off = b_next;
  }

  if ((size_t)a_off >= a_len && (size_t)b_off >= b_len)
    return 0;
  if ((size_t)a_off >= a_len)
    return -1;
  return 1;
}

/* ============================================================================
 * Search Functions
 * ============================================================================
 */

const char *gstrchr(const char *s, size_t len, const char *grapheme,
                    size_t g_len) {
  if (!s || len == 0 || !grapheme || g_len == 0)
    return NULL;

  int offset = 0;

  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(s, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (grapheme_len == g_len && memcmp(s + offset, grapheme, g_len) == 0) {
      return s + offset;
    }

    offset = next;
  }

  return NULL;
}

const char *gstrrchr(const char *s, size_t len, const char *grapheme,
                     size_t g_len) {
  if (!s || len == 0 || !grapheme || g_len == 0)
    return NULL;

  const char *last_match = NULL;
  int offset = 0;

  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(s, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (grapheme_len == g_len && memcmp(s + offset, grapheme, g_len) == 0) {
      last_match = s + offset;
    }

    offset = next;
  }

  return last_match;
}

const char *gstrstr(const char *haystack, size_t h_len, const char *needle,
                    size_t n_len) {
  if (!haystack || !needle)
    return NULL;
  if (n_len == 0)
    return haystack;
  if (h_len == 0)
    return NULL;

  /* Count graphemes in needle */
  size_t needle_graphemes = gstrlen(needle, n_len);
  if (needle_graphemes == 0)
    return haystack;

  int h_off = 0;

  while ((size_t)h_off < h_len) {
    /* Check if needle matches starting at this position */
    int h_pos = h_off;
    int n_pos = 0;
    int match = 1;
    size_t matched_graphemes = 0;

    while (matched_graphemes < needle_graphemes) {
      if ((size_t)h_pos >= h_len) {
        match = 0;
        break;
      }

      int h_next = utflite_next_grapheme(haystack, (int)h_len, h_pos);
      int n_next = utflite_next_grapheme(needle, (int)n_len, n_pos);

      size_t h_glen = (size_t)(h_next - h_pos);
      size_t n_glen = (size_t)(n_next - n_pos);

      if (h_glen != n_glen ||
          memcmp(haystack + h_pos, needle + n_pos, h_glen) != 0) {
        match = 0;
        break;
      }

      h_pos = h_next;
      n_pos = n_next;
      matched_graphemes++;
    }

    if (match)
      return haystack + h_off;

    /* Move to next grapheme in haystack */
    h_off = utflite_next_grapheme(haystack, (int)h_len, h_off);
  }

  return NULL;
}

/* ============================================================================
 * Span Functions
 * ============================================================================
 */

size_t gstrspn(const char *s, size_t len, const char *accept, size_t a_len) {
  if (!s || len == 0 || !accept || a_len == 0)
    return 0;

  size_t count = 0;
  int offset = 0;

  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(s, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (!gstr_grapheme_in_set(s + offset, grapheme_len, accept, a_len)) {
      break;
    }

    count++;
    offset = next;
  }

  return count;
}

size_t gstrcspn(const char *s, size_t len, const char *reject, size_t r_len) {
  if (!s || len == 0)
    return 0;
  if (!reject || r_len == 0)
    return gstrlen(s, len);

  size_t count = 0;
  int offset = 0;

  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(s, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (gstr_grapheme_in_set(s + offset, grapheme_len, reject, r_len)) {
      break;
    }

    count++;
    offset = next;
  }

  return count;
}

const char *gstrpbrk(const char *s, size_t len, const char *accept,
                     size_t a_len) {
  if (!s || len == 0 || !accept || a_len == 0)
    return NULL;

  int offset = 0;

  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(s, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);

    if (gstr_grapheme_in_set(s + offset, grapheme_len, accept, a_len)) {
      return s + offset;
    }

    offset = next;
  }

  return NULL;
}

/* ============================================================================
 * Extraction Functions
 * ============================================================================
 */

size_t gstrsub(char *dst, size_t dst_size, const char *src, size_t src_len,
               size_t start_grapheme, size_t count) {
  if (!dst || dst_size == 0)
    return 0;

  dst[0] = '\0';

  if (!src || src_len == 0 || count == 0)
    return 0;

  /* Find start position */
  size_t start_offset = gstroff(src, src_len, start_grapheme);
  if (start_offset >= src_len)
    return 0;

  /* Find end position */
  int offset = (int)start_offset;
  size_t graphemes_copied = 0;

  while ((size_t)offset < src_len && graphemes_copied < count) {
    offset = utflite_next_grapheme(src, (int)src_len, offset);
    graphemes_copied++;
  }

  size_t bytes_to_copy = (size_t)offset - start_offset;

  /* Ensure we don't overflow dst - find last complete grapheme that fits */
  if (bytes_to_copy >= dst_size) {
    int fit_offset = (int)start_offset;
    int last_complete = (int)start_offset;

    while ((size_t)fit_offset < start_offset + bytes_to_copy) {
      int next = utflite_next_grapheme(src, (int)src_len, fit_offset);
      if ((size_t)(next - (int)start_offset) < dst_size) {
        last_complete = next;
      } else {
        break;
      }
      fit_offset = next;
    }
    bytes_to_copy = (size_t)(last_complete - (int)start_offset);
  }

  memcpy(dst, src + start_offset, bytes_to_copy);
  dst[bytes_to_copy] = '\0';

  return bytes_to_copy;
}

/* ============================================================================
 * Copy Functions
 * ============================================================================
 */

size_t gstrcpy(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;

  dst[0] = '\0';

  if (!src || src_len == 0)
    return 0;

  size_t bytes_to_copy = src_len;
  if (bytes_to_copy >= dst_size)
    bytes_to_copy = dst_size - 1;

  /* Find last complete grapheme that fits */
  int offset = 0;
  int last_complete = 0;

  while ((size_t)offset < bytes_to_copy) {
    int next = utflite_next_grapheme(src, (int)src_len, offset);
    if ((size_t)next <= bytes_to_copy) {
      last_complete = next;
    } else {
      break;
    }
    offset = next;
  }

  memcpy(dst, src, (size_t)last_complete);
  dst[last_complete] = '\0';

  return (size_t)last_complete;
}

size_t gstrncpy(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t n) {
  if (!dst || dst_size == 0)
    return 0;

  dst[0] = '\0';

  if (!src || src_len == 0 || n == 0)
    return 0;

  int offset = 0;
  size_t count = 0;

  /* Find the end of the nth grapheme or end of string */
  while ((size_t)offset < src_len && count < n) {
    offset = utflite_next_grapheme(src, (int)src_len, offset);
    count++;
  }

  size_t bytes_to_copy = (size_t)offset;
  if (bytes_to_copy >= dst_size)
    bytes_to_copy = dst_size - 1;

  /* Find last complete grapheme that fits in dst */
  int fit_offset = 0;
  int last_complete = 0;

  while ((size_t)fit_offset < bytes_to_copy) {
    int next = utflite_next_grapheme(src, (int)src_len, fit_offset);
    if ((size_t)next <= bytes_to_copy) {
      last_complete = next;
    } else {
      break;
    }
    fit_offset = next;
  }

  memcpy(dst, src, (size_t)last_complete);
  dst[last_complete] = '\0';

  return (size_t)last_complete;
}

/* ============================================================================
 * Concatenation Functions
 * ============================================================================
 */

size_t gstrcat(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;

  size_t dst_len = strlen(dst);
  if (dst_len >= dst_size - 1)
    return dst_len;

  if (!src || src_len == 0)
    return dst_len;

  size_t remaining = dst_size - dst_len - 1;
  size_t bytes_to_copy = src_len;
  if (bytes_to_copy > remaining)
    bytes_to_copy = remaining;

  /* Find last complete grapheme that fits */
  int offset = 0;
  int last_complete = 0;

  while ((size_t)offset < bytes_to_copy) {
    int next = utflite_next_grapheme(src, (int)src_len, offset);
    if ((size_t)next <= bytes_to_copy) {
      last_complete = next;
    } else {
      break;
    }
    offset = next;
  }

  memcpy(dst + dst_len, src, (size_t)last_complete);
  dst[dst_len + (size_t)last_complete] = '\0';

  return dst_len + (size_t)last_complete;
}

size_t gstrncat(char *dst, size_t dst_size, const char *src, size_t src_len,
                size_t n) {
  if (!dst || dst_size == 0)
    return 0;

  size_t dst_len = strlen(dst);
  if (dst_len >= dst_size - 1)
    return dst_len;

  if (!src || src_len == 0 || n == 0)
    return dst_len;

  /* Find the end of the nth grapheme or end of string */
  int offset = 0;
  size_t count = 0;

  while ((size_t)offset < src_len && count < n) {
    offset = utflite_next_grapheme(src, (int)src_len, offset);
    count++;
  }

  size_t bytes_to_append = (size_t)offset;
  size_t remaining = dst_size - dst_len - 1;
  if (bytes_to_append > remaining)
    bytes_to_append = remaining;

  /* Find last complete grapheme that fits */
  int fit_offset = 0;
  int last_complete = 0;

  while ((size_t)fit_offset < bytes_to_append) {
    int next = utflite_next_grapheme(src, (int)src_len, fit_offset);
    if ((size_t)next <= bytes_to_append) {
      last_complete = next;
    } else {
      break;
    }
    fit_offset = next;
  }

  memcpy(dst + dst_len, src, (size_t)last_complete);
  dst[dst_len + (size_t)last_complete] = '\0';

  return dst_len + (size_t)last_complete;
}
/* ============================================================================
 * Additional Comparison Functions
 * ============================================================================
 */
int gstrncasecmp(const char *a, size_t a_len, const char *b, size_t b_len,
                 size_t n) {
  if (n == 0)
    return 0;
  if (!a && !b)
    return 0;
  if (!a)
    return -1;
  if (!b)
    return 1;
  int a_off = 0;
  int b_off = 0;
  size_t count = 0;
  while ((size_t)a_off < a_len && (size_t)b_off < b_len && count < n) {
    int a_next = utflite_next_grapheme(a, (int)a_len, a_off);
    int b_next = utflite_next_grapheme(b, (int)b_len, b_off);
    size_t a_glen = (size_t)(a_next - a_off);
    size_t b_glen = (size_t)(b_next - b_off);
    int cmp = gstr_cmp_grapheme_icase(a + a_off, a_glen, b + b_off, b_glen);
    if (cmp != 0)
      return cmp;
    a_off = a_next;
    b_off = b_next;
    count++;
  }
  if (count == n)
    return 0;
  /* Didn't reach n graphemes - one string is shorter */
  if ((size_t)a_off >= a_len && (size_t)b_off >= b_len)
    return 0;
  if ((size_t)a_off >= a_len)
    return -1;
  return 1;
}
/* ============================================================================
 * Allocation Functions
 * ============================================================================
 */
char *gstrdup(const char *s, size_t len) {
  if (!s)
    return NULL;
  char *result = malloc(len + 1);
  if (!result)
    return NULL;
  memcpy(result, s, len);
  result[len] = '\0';
  return result;
}
char *gstrndup(const char *s, size_t len, size_t n) {
  if (!s)
    return NULL;
  if (n == 0)
    return gstrdup("", 0);
  /* Find byte offset after n graphemes */
  int offset = 0;
  size_t count = 0;
  while ((size_t)offset < len && count < n) {
    offset = utflite_next_grapheme(s, (int)len, offset);
    count++;
  }
  return gstrdup(s, (size_t)offset);
}
/* ============================================================================
 * Additional Search Functions
 * ============================================================================
 */
const char *gstrrstr(const char *haystack, size_t h_len, const char *needle,
                     size_t n_len) {
  if (!haystack || !needle)
    return NULL;
  if (n_len == 0)
    return haystack + h_len; /* Point to end for empty needle */
  if (h_len == 0)
    return NULL;
  const char *last_match = NULL;
  size_t needle_graphemes = gstrlen(needle, n_len);
  if (needle_graphemes == 0)
    return haystack + h_len;
  int h_off = 0;
  while ((size_t)h_off < h_len) {
    /* Check if needle matches starting at this position */
    int h_pos = h_off;
    int n_pos = 0;
    int match = 1;
    size_t matched_graphemes = 0;
    while (matched_graphemes < needle_graphemes) {
      if ((size_t)h_pos >= h_len) {
        match = 0;
        break;
      }
      int h_next = utflite_next_grapheme(haystack, (int)h_len, h_pos);
      int n_next = utflite_next_grapheme(needle, (int)n_len, n_pos);
      size_t h_glen = (size_t)(h_next - h_pos);
      size_t n_glen = (size_t)(n_next - n_pos);
      if (h_glen != n_glen ||
          memcmp(haystack + h_pos, needle + n_pos, h_glen) != 0) {
        match = 0;
        break;
      }
      h_pos = h_next;
      n_pos = n_next;
      matched_graphemes++;
    }
    if (match)
      last_match = haystack + h_off;
    /* Move to next grapheme in haystack */
    h_off = utflite_next_grapheme(haystack, (int)h_len, h_off);
  }
  return last_match;
}
/*
 * Helper: checks if a grapheme is ASCII whitespace.
 * Also handles CRLF (\r\n) which is a single 2-byte grapheme per UAX #29.
 */
static int gstr_is_whitespace(const char *g, size_t g_len) {
  /* Handle CR LF as whitespace (single grapheme per UAX #29) */
  if (g_len == 2 && g[0] == '\r' && g[1] == '\n')
    return 1;
  if (g_len != 1)
    return 0;
  char c = g[0];
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
         c == '\f';
}
const char *gstrcasestr(const char *haystack, size_t h_len, const char *needle,
                        size_t n_len) {
  if (!haystack || !needle)
    return NULL;
  if (n_len == 0)
    return haystack;
  if (h_len == 0)
    return NULL;
  size_t needle_graphemes = gstrlen(needle, n_len);
  if (needle_graphemes == 0)
    return haystack;
  int h_off = 0;
  while ((size_t)h_off < h_len) {
    /* Check if needle matches starting at this position (case-insensitive) */
    int h_pos = h_off;
    int n_pos = 0;
    int match = 1;
    size_t matched_graphemes = 0;
    while (matched_graphemes < needle_graphemes) {
      if ((size_t)h_pos >= h_len) {
        match = 0;
        break;
      }
      int h_next = utflite_next_grapheme(haystack, (int)h_len, h_pos);
      int n_next = utflite_next_grapheme(needle, (int)n_len, n_pos);
      size_t h_glen = (size_t)(h_next - h_pos);
      size_t n_glen = (size_t)(n_next - n_pos);
      if (gstr_cmp_grapheme_icase(haystack + h_pos, h_glen, needle + n_pos,
                                  n_glen) != 0) {
        match = 0;
        break;
      }
      h_pos = h_next;
      n_pos = n_next;
      matched_graphemes++;
    }
    if (match)
      return haystack + h_off;
    h_off = utflite_next_grapheme(haystack, (int)h_len, h_off);
  }
  return NULL;
}
size_t gstrcount(const char *s, size_t len, const char *needle, size_t n_len) {
  if (!s || len == 0 || !needle || n_len == 0)
    return 0;
  size_t count = 0;
  const char *pos = s;
  size_t remaining = len;
  while (remaining > 0) {
    const char *found = gstrstr(pos, remaining, needle, n_len);
    if (!found)
      break;
    count++;
    /* Move past the match */
    size_t skip = (size_t)(found - pos) + n_len;
    if (skip > remaining)
      break;
    pos = found + n_len;
    remaining = len - (size_t)(pos - s);
  }
  return count;
}
/* ============================================================================
 * Tokenization Functions
 * ============================================================================
 */
const char *gstrsep(const char **stringp, size_t *lenp, const char *delim,
                    size_t d_len, size_t *tok_len) {
  if (!stringp || !*stringp || !lenp)
    return NULL;

  /* Exhausted - set stringp to NULL to signal end */
  if (*lenp == 0) {
    *stringp = NULL;
    return NULL;
  }
  const char *start = *stringp;
  size_t len = *lenp;
  /* Handle empty delimiter - return entire string */
  if (!delim || d_len == 0) {
    if (tok_len)
      *tok_len = len;
    *stringp = NULL;
    *lenp = 0;
    return start;
  }
  /* Find first delimiter */
  int offset = 0;
  while ((size_t)offset < len) {
    int next = utflite_next_grapheme(start, (int)len, offset);
    size_t grapheme_len = (size_t)(next - offset);
    /* Check if this grapheme is in the delimiter set */
    if (gstr_grapheme_in_set(start + offset, grapheme_len, delim, d_len)) {
      /* Found delimiter - return token before it */
      if (tok_len)
        *tok_len = (size_t)offset;
      *stringp = start + next;
      *lenp = len - (size_t)next;
      return start;
    }
    offset = next;
  }
  /* No delimiter found - return entire remaining string */
  if (tok_len)
    *tok_len = len;
  *stringp = NULL;
  *lenp = 0;
  return start;
}
/* ============================================================================
 * Trimming Functions
 * ============================================================================
 */
size_t gstrltrim(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;
  dst[0] = '\0';
  if (!src || src_len == 0)
    return 0;
  /* Find first non-whitespace grapheme */
  int offset = 0;
  while ((size_t)offset < src_len) {
    int next = utflite_next_grapheme(src, (int)src_len, offset);
    size_t grapheme_len = (size_t)(next - offset);
    if (!gstr_is_whitespace(src + offset, grapheme_len)) {
      break;
    }
    offset = next;
  }
  /* Copy from first non-whitespace to end */
  size_t remaining = src_len - (size_t)offset;
  return gstrcpy(dst, dst_size, src + offset, remaining);
}
size_t gstrrtrim(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;
  dst[0] = '\0';
  if (!src || src_len == 0)
    return 0;
  /* Build list of grapheme end offsets to find the last non-whitespace */
  int last_non_ws_end = 0;
  int offset = 0;
  while ((size_t)offset < src_len) {
    int next = utflite_next_grapheme(src, (int)src_len, offset);
    size_t grapheme_len = (size_t)(next - offset);
    if (!gstr_is_whitespace(src + offset, grapheme_len)) {
      last_non_ws_end = next;
    }
    offset = next;
  }
  /* Copy from start to end of last non-whitespace */
  return gstrcpy(dst, dst_size, src, (size_t)last_non_ws_end);
}
size_t gstrtrim(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;
  dst[0] = '\0';
  if (!src || src_len == 0)
    return 0;
  /* Find first non-whitespace grapheme */
  int start_offset = 0;
  while ((size_t)start_offset < src_len) {
    int next = utflite_next_grapheme(src, (int)src_len, start_offset);
    size_t grapheme_len = (size_t)(next - start_offset);
    if (!gstr_is_whitespace(src + start_offset, grapheme_len)) {
      break;
    }
    start_offset = next;
  }
  /* All whitespace? */
  if ((size_t)start_offset >= src_len)
    return 0;
  /* Find last non-whitespace grapheme end */
  int last_non_ws_end = start_offset;
  int offset = start_offset;
  while ((size_t)offset < src_len) {
    int next = utflite_next_grapheme(src, (int)src_len, offset);
    size_t grapheme_len = (size_t)(next - offset);
    if (!gstr_is_whitespace(src + offset, grapheme_len)) {
      last_non_ws_end = next;
    }
    offset = next;
  }
  /* Copy the trimmed portion */
  size_t trimmed_len = (size_t)last_non_ws_end - (size_t)start_offset;
  return gstrcpy(dst, dst_size, src + start_offset, trimmed_len);
}
/* ============================================================================
 * Transformation Functions
 * ============================================================================
 */
size_t gstrrev(char *dst, size_t dst_size, const char *src, size_t src_len) {
  if (!dst || dst_size == 0)
    return 0;
  dst[0] = '\0';
  if (!src || src_len == 0)
    return 0;
  /* Count graphemes and collect their offsets */
  size_t grapheme_count = gstrlen(src, src_len);
  if (grapheme_count == 0)
    return 0;
  /* Allocate array to store grapheme start offsets */
  int *offsets = malloc((grapheme_count + 1) * sizeof(int));
  if (!offsets)
    return 0;
  /* Populate offsets */
  int offset = 0;
  size_t idx = 0;
  while ((size_t)offset < src_len && idx < grapheme_count) {
    offsets[idx++] = offset;
    offset = utflite_next_grapheme(src, (int)src_len, offset);
  }
  offsets[grapheme_count] = offset; /* End offset */
  /* Copy graphemes in reverse order */
  size_t written = 0;
  for (size_t i = grapheme_count; i > 0; i--) {
    int start = offsets[i - 1];
    int end = offsets[i];
    size_t glen = (size_t)(end - start);
    /* Check if this grapheme fits */
    if (written + glen >= dst_size) {
      break; /* Can't fit more complete graphemes */
    }
    memcpy(dst + written, src + start, glen);
    written += glen;
  }
  free(offsets);
  dst[written] = '\0';
  return written;
}
size_t gstrreplace(char *dst, size_t dst_size, const char *src, size_t src_len,
                   const char *old_str, size_t old_len, const char *new_str,
                   size_t new_len) {
  if (!dst || dst_size == 0)
    return 0;
  dst[0] = '\0';
  if (!src || src_len == 0)
    return 0;
  /* Handle empty old string - just copy src */
  if (!old_str || old_len == 0)
    return gstrcpy(dst, dst_size, src, src_len);
  size_t written = 0;
  const char *pos = src;
  size_t remaining = src_len;
  while (remaining > 0) {
    /* Find next occurrence of old string */
    const char *found = gstrstr(pos, remaining, old_str, old_len);
    if (!found) {
      /* No more matches - copy remaining */
      size_t to_copy = remaining;
      if (written + to_copy >= dst_size) {
        /* Find last complete grapheme that fits */
        int fit_offset = 0;
        int last_complete = 0;
        size_t max_bytes = dst_size - written - 1;
        while ((size_t)fit_offset < to_copy && (size_t)fit_offset < max_bytes) {
          int next = utflite_next_grapheme(pos, (int)remaining, fit_offset);
          if ((size_t)next <= max_bytes) {
            last_complete = next;
          } else {
            break;
          }
          fit_offset = next;
        }
        to_copy = (size_t)last_complete;
      }
      memcpy(dst + written, pos, to_copy);
      written += to_copy;
      break;
    }
    /* Copy segment before match */
    size_t before_len = (size_t)(found - pos);
    if (before_len > 0) {
      if (written + before_len >= dst_size) {
        /* Find last complete grapheme that fits */
        int fit_offset = 0;
        int last_complete = 0;
        size_t max_bytes = dst_size - written - 1;
        while ((size_t)fit_offset < before_len &&
               (size_t)fit_offset < max_bytes) {
          int next = utflite_next_grapheme(pos, (int)before_len, fit_offset);
          if ((size_t)next <= max_bytes) {
            last_complete = next;
          } else {
            break;
          }
          fit_offset = next;
        }
        memcpy(dst + written, pos, (size_t)last_complete);
        written += (size_t)last_complete;
        dst[written] = '\0';
        return written;
      }
      memcpy(dst + written, pos, before_len);
      written += before_len;
    }
    /* Copy replacement string */
    if (new_str && new_len > 0) {
      if (written + new_len >= dst_size) {
        /* Find last complete grapheme of replacement that fits */
        int fit_offset = 0;
        int last_complete = 0;
        size_t max_bytes = dst_size - written - 1;
        while ((size_t)fit_offset < new_len && (size_t)fit_offset < max_bytes) {
          int next = utflite_next_grapheme(new_str, (int)new_len, fit_offset);
          if ((size_t)next <= max_bytes) {
            last_complete = next;
          } else {
            break;
          }
          fit_offset = next;
        }
        memcpy(dst + written, new_str, (size_t)last_complete);
        written += (size_t)last_complete;
        dst[written] = '\0';
        return written;
      }
      memcpy(dst + written, new_str, new_len);
      written += new_len;
    }
    /* Move past the match */
    pos = found + old_len;
    remaining = src_len - (size_t)(pos - src);
  }
  dst[written] = '\0';
  return written;
}
