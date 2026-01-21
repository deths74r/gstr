/*
 * gstr.c - Grapheme string library implementation
 *
 * Builds on utflite's grapheme iteration to provide string.h-like operations
 * that work on grapheme clusters instead of bytes.
 */

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

  /* Ensure we don't overflow dst */
  if (bytes_to_copy >= dst_size)
    bytes_to_copy = dst_size - 1;

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
