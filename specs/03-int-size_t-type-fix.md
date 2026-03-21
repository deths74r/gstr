# Specification: Resolving the `int`/`size_t` Type Duality in gstr.h

## 1. Problem Statement

The gstr library has a type split at the boundary between its two layers:

- **UTF-8 Layer** (lines 1168-1529): All functions use `int` for byte offsets and lengths. This includes `utf8_decode`, `utf8_encode`, `utf8_next`, `utf8_prev`, `utf8_next_grapheme`, `utf8_prev_grapheme`, `utf8_valid`, `utf8_cpcount`, `utf8_truncate`, and `utf8_charwidth`.

- **Grapheme String Layer** (lines 1625-3691): All public functions use `size_t` for byte lengths in their signatures, but internally declare `int` local variables for byte offsets and cast back and forth at every call to the UTF-8 layer.

This creates three categories of defect:

### 1.1. Truncation on 64-bit Platforms

On LP64 and LLP64 systems (all modern 64-bit targets), `size_t` is 8 bytes and `int` is 4 bytes. Every `(int)byte_len` cast silently truncates strings longer than 2,147,483,647 bytes. There are **68 such casts** in gstr.h. A 2.5 GB text file processed through `gstrlen()` would wrap `byte_len` to a negative value inside `utf8_next_grapheme`, causing the `offset >= length` guard to return immediately, producing a count of 0 or 1.

### 1.2. Sign Mismatch in Comparisons

Internal `int offset` variables are compared against `size_t byte_len` values via casts like `(size_t)offset < byte_len`. There are approximately **99 such upward casts**. If `offset` ever becomes negative (e.g., due to overflow of an intermediate `offset + bytes` computation on a large string), the cast to `size_t` wraps it to a large positive value, causing unbounded reads past the buffer.

### 1.3. Heap Allocation with `int` Width

In `gstrrev()` (line 2812):
```c
int *offsets = malloc((grapheme_count + 1) * sizeof(int));
```
This allocates an array of `int` to store byte offsets. For strings exceeding 2 GB, offsets overflow their 32-bit storage, producing corrupted reverse output. Even for smaller strings, this misrepresents the semantic type of the stored values.

### 1.4. Impact on 16-bit Platforms

On platforms where `int` is 16 bits, the UTF-8 layer limits strings to 32,767 bytes. While uncommon for general computing, this matters for embedded applications where gstr's header-only design is most attractive.

### 1.5. Scope of Contamination

The `(int)` cast pattern propagates through all internal helpers: `gstr_grapheme_in_set`, `gstr_grapheme_width`, and into every public gstr function. Every function that calls `utf8_next_grapheme` or `utf8_decode` must cast its `size_t` parameters down to `int`, and every result must be cast back up.

## 2. Proposed Type Strategy

### 2.1. Candidates

| Type | Width | Signed | Pros | Cons |
|------|-------|--------|------|------|
| `size_t` | pointer-width | no | Matches public API, matches `strlen`, `memcpy`, standard idiom for byte lengths | Cannot represent error values as negative; comparison with 0 requires care for underflow |
| `int32_t` | 32-bit fixed | yes | Simple, portable, no width surprises | Still truncates on 64-bit for buffers > 2 GB; non-idiomatic for C string lengths |
| `ptrdiff_t` | pointer-width | yes | Signed, pointer-width, can hold error sentinels | Non-standard for lengths; unfamiliar to most C developers; `ptrdiff_t` semantically means "difference between pointers" |
| `int` (status quo) | platform-dependent | yes | Simple arithmetic, negative-as-error idiom | Root cause of the bug; 32-bit on 64-bit platforms; 16-bit on embedded |

### 2.2. Recommendation: `size_t` Throughout

Adopt `size_t` for all byte offset and byte length parameters and return values across both layers. Rationale:

1. **The public API already uses `size_t`.** The gstr layer functions take `size_t byte_len` and return `size_t`. Aligning the UTF-8 layer eliminates all 68 downward casts and all 99 upward casts.

2. **Matches C standard library convention.** `strlen`, `memcpy`, `memcmp`, `malloc` all use `size_t`. Users expect byte lengths to be `size_t`.

3. **Pointer-width on all platforms.** On 64-bit systems, `size_t` is 64-bit; on 32-bit, 32-bit; on 16-bit, 16-bit. It always matches the addressable range.

4. **The "negative return for error" pattern is unnecessary.** The existing UTF-8 functions use positive values or boundary values (returning `length` for "at end"), not negative sentinels. `utf8_encode` returns 0 on error. `utf8_decode` returns 0 on null input. `utf8_cpwidth` returns -1 for control characters, but this is a display width property, not a byte offset. `utf8_prev` returns 0 for "at start". None of these require signed byte offsets.

5. **`utf8_cpwidth` is the only exception.** It returns -1 for control characters. This function should remain `int` since it returns a display property (column width), not a byte offset.

### 2.3. Error Signaling After the Change

- `utf8_decode`: Returns 0 bytes consumed on null/empty input. No change needed.
- `utf8_encode`: Returns 0 on invalid codepoint. No change needed.
- `utf8_next`: Returns `length` when at end. No change needed.
- `utf8_prev`: Returns 0 when at start. No change needed.
- `utf8_next_grapheme`: Returns `length` when at end. No change needed.
- `utf8_prev_grapheme`: Returns 0 when at start. No change needed.
- `utf8_valid`: Returns int (boolean). No change needed; `error_offset` out-parameter changes to `size_t *`.

## 3. Functions to Change

### 3.1. UTF-8 Layer Functions (Signature Changes)

**`utf8_decode`**
```
Before: static inline int  utf8_decode(const char *bytes, int length, uint32_t *codepoint)
After:  static inline size_t utf8_decode(const char *bytes, size_t length, uint32_t *codepoint)
```

**`utf8_encode`**
```
Before: static inline int utf8_encode(uint32_t codepoint, char *buffer)
After:  static inline size_t utf8_encode(uint32_t codepoint, char *buffer)
```

**`utf8_charwidth`**
```
Before: static inline int utf8_charwidth(const char *text, int length, int offset)
After:  static inline int utf8_charwidth(const char *text, size_t length, size_t offset)
```
Note: Return type stays `int` because it returns column width (can be -1 for control chars).

**`utf8_next`**
```
Before: static inline int utf8_next(const char *text, int length, int offset)
After:  static inline size_t utf8_next(const char *text, size_t length, size_t offset)
```

**`utf8_prev`**
```
Before: static inline int utf8_prev(const char *text, int offset)
After:  static inline size_t utf8_prev(const char *text, size_t offset)
```

**`utf8_next_grapheme`**
```
Before: static inline int utf8_next_grapheme(const char *text, int length, int offset)
After:  static inline size_t utf8_next_grapheme(const char *text, size_t length, size_t offset)
```

**`utf8_prev_grapheme`**
```
Before: static inline int utf8_prev_grapheme(const char *text, int offset)
After:  static inline size_t utf8_prev_grapheme(const char *text, size_t offset)
```

**`utf8_valid`**
```
Before: static inline int utf8_valid(const char *text, int length, int *error_offset)
After:  static inline int utf8_valid(const char *text, size_t length, size_t *error_offset)
```
Note: Return type stays `int` (boolean). The `error_offset` out-parameter changes to `size_t *`.

**`utf8_cpcount`**
```
Before: static inline int utf8_cpcount(const char *text, int length)
After:  static inline size_t utf8_cpcount(const char *text, size_t length)
```

**`utf8_truncate`**
```
Before: static inline int utf8_truncate(const char *text, int length, int max_cols)
After:  static inline size_t utf8_truncate(const char *text, size_t length, size_t max_cols)
```

### 3.2. Functions with No Signature Change

All `gstr*` public functions already use `size_t` in their signatures. Their signatures remain unchanged:
`gstrlen`, `gstrnlen`, `gstrwidth`, `gstroff`, `gstrat`, `gstrcmp`, `gstrncmp`, `gstrcasecmp`, `gstrncasecmp`, `gstrstartswith`, `gstrendswith`, `gstrchr`, `gstrrchr`, `gstrstr`, `gstrrstr`, `gstrcasestr`, `gstrcount`, `gstrspn`, `gstrcspn`, `gstrpbrk`, `gstrsub`, `gstrcpy`, `gstrncpy`, `gstrcat`, `gstrncat`, `gstrdup`, `gstrndup`, `gstrsep`, `gstrltrim`, `gstrrtrim`, `gstrtrim`, `gstrrev`, `gstrreplace`, `gstrlower`, `gstrupper`, `gstrellipsis`, `gstrfill`, `gstrlpad`, `gstrrpad`, `gstrpad`, `gstr_grapheme_width`, `gstrwtrunc`, `gstrwlpad`, `gstrwrpad`, `gstrwpad`.

### 3.3. Functions with No Change at All

- `utf8_cpwidth` - Returns column width as `int` (can be -1). Takes `uint32_t codepoint`. Unchanged.
- `utf8_is_zerowidth` - Takes `uint32_t`, returns `int` (boolean). Unchanged.
- `utf8_is_wide` - Takes `uint32_t`, returns `int` (boolean). Unchanged.
- `gstr_cmp_grapheme` - Already uses `size_t`. Unchanged.
- `gstr_cmp_grapheme_icase` - Already uses `size_t`. Unchanged.
- `gstr_ascii_lower`, `gstr_ascii_upper` - Operate on single chars. Unchanged.
- `gstr_is_whitespace` - Already uses `size_t`. Unchanged.
- `is_grapheme_break` - Operates on codepoint properties. Unchanged.

### 3.4. Internal Helper Signature Changes

**`gstr_grapheme_in_set`**
```
Before: static int gstr_grapheme_in_set(const char *g, size_t g_len, const char *set, size_t set_len)
         [uses int offset internally]
After:  [same signature, internal int offset -> size_t offset]
```

**`gstr_grapheme_width`**
```
Before: static inline size_t gstr_grapheme_width(const char *s, size_t byte_len, int offset, int next)
After:  static inline size_t gstr_grapheme_width(const char *s, size_t byte_len, size_t offset, size_t next)
```

## 4. Internal Changes

### 4.1. Local Variable Type Changes

Approximately **30 local variable declarations** of the form `int offset = 0` change to `size_t offset = 0` within gstr layer functions. All other `int` local variables that hold byte offsets change similarly:

- `int offset` -> `size_t offset` (30 occurrences across gstr functions)
- `int next` -> `size_t next` (used to hold `utf8_next_grapheme` return values)
- `int a_off`, `int b_off` -> `size_t a_off`, `size_t b_off` (in comparison functions)
- `int s_off`, `int suf_off`, `int p_off` -> `size_t` equivalents (in startswith/endswith)
- `int h_off`, `int h_pos`, `int n_pos` -> `size_t` equivalents (in search functions)
- `int fit_offset`, `int last_complete`, `int last_valid` -> `size_t` equivalents (in truncation paths)
- `int start`, `int end` -> `size_t start`, `size_t end` (in `gstrrev`)

### 4.2. Cast Elimination

All **68 downward casts** `(int)byte_len`, `(int)src_len`, `(int)s_len`, etc. are deleted. All **99 upward casts** `(size_t)offset`, `(size_t)(next - offset)`, etc. are deleted. The types naturally match without casting.

### 4.3. Comparison Adjustments

Signed comparisons like `offset >= length` in the UTF-8 layer become unsigned comparisons. Since both operands will be `size_t`, the semantics are preserved. The `offset < 0` guard in `utf8_next_grapheme` (line 1363) must be removed (or converted to a `text == NULL` check), since `size_t` cannot be negative.

### 4.4. `utf8_prev` Underflow Protection

The current `utf8_prev` uses `offset <= 0` as the base case. With `size_t`, the check becomes `offset == 0`. The backward scan `int pos = offset - 1` becomes `size_t pos = offset - 1`, which is safe because we already checked `offset == 0`. The `offset > 4 ? offset - 4 : 0` limit calculation is already underflow-safe in the unsigned domain.

### 4.5. `utf8_prev_grapheme` Adjustments

Uses `offset <= 0` guard, changes to `offset == 0`. Internal `scan_start > 0` and `prev == scan_start` comparisons remain valid with `size_t`.

### 4.6. `gstrrev` Heap Allocation

```
Before: int *offsets = malloc((grapheme_count + 1) * sizeof(int));
After:  size_t *offsets = malloc((grapheme_count + 1) * sizeof(size_t));
```

### 4.7. `gstrendswith` Internal Cast

Line 2015: `int s_off = (int)s_offset;` becomes `size_t s_off = s_offset;` (no cast needed).

### 4.8. Arithmetic in `utf8_decode`

The internal `sequence_length` variable (1-4) and loop variable `i` can remain `int` since they represent sequence structure, not buffer offsets. However, the `length` parameter comparison `length < sequence_length` becomes `size_t < int`. This should use an explicit cast: `length < (size_t)sequence_length`, or change `sequence_length` to `size_t` as well for clarity.

## 5. Binary Search Changes

### 5.1. `unicode_range_contains`

```
Before:
static int unicode_range_contains(uint32_t codepoint,
                                  const struct unicode_range *ranges,
                                  int count) {
  int low = 0;
  int high = count - 1;

After:
static int unicode_range_contains(uint32_t codepoint,
                                  const struct unicode_range *ranges,
                                  size_t count) {
  size_t low = 0;
  size_t high = count;  /* Use half-open [low, high) to avoid underflow */
```

The current binary search uses `int high = count - 1` with `low <= high`. With unsigned `size_t`, when `count` is 0, `count - 1` wraps to `SIZE_MAX`. The loop must be restructured to use a half-open interval `[low, high)`:

```c
static int unicode_range_contains(uint32_t codepoint,
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
}
```

### 5.2. `get_gcb`

Same pattern. Internal `int low`, `int high` become `size_t low`, `size_t high` with half-open interval.

### 5.3. `is_incb_linker`

Same pattern. Internal binary search over `INCB_LINKERS[]` array.

### 5.4. Count Macros

The `_COUNT` macros (e.g., `ZERO_WIDTH_COUNT`, `EAW_WIDE_COUNT`, `GCB_RANGE_COUNT`, `INCB_LINKER_COUNT`, `INCB_CONSONANT_COUNT`, `EXTENDED_PICTOGRAPHIC_COUNT`) already evaluate to `size_t` because `sizeof` returns `size_t`. No change needed, but they now align naturally with the `size_t count` parameter.

## 6. Impact on Language Bindings

### 6.1. gstr-zig (C wrapper via `@cImport`)

**Nature**: Thin FFI wrapper. The Zig binding in `/home/edward/repos/gstr/gstr-zig/src/c.zig` uses `@cImport` to import `gstr.h` directly. Zig's `@intCast` calls in `/home/edward/repos/gstr/gstr-zig/src/gstr.zig` (lines 77-80, 92-95) currently cast between `usize` and C `int` when calling `utf8_next_grapheme`.

**Required changes**:
- After the fix, `utf8_next_grapheme` takes and returns `size_t`, which maps to `usize` in Zig. The `@intCast` calls in `GraphemeIterator.next()` and `peek()` can be simplified or removed entirely.
- The `@cImport` re-export in `c.zig` requires no code change; it automatically picks up the new signatures.
- The idiomatic wrapper functions in `gstr.zig` that pass `s.len` (which is `usize`) will no longer need narrowing casts to C `int`.
- The Zig binding also has a copy of `gstr.h` at `/home/edward/repos/gstr/gstr-zig/include/gstr.h` that must be updated in sync.

### 6.2. gstr-go (CGo wrapper)

**Nature**: CGo wrapper with shim functions. The Go binding defines C shim functions in a comment block (lines 26-163 of `gstr.go`).

**Required changes**:
- `gstr_shim_utf8_valid` (line 157): Change `int byte_len` to `size_t byte_len` and `int *error_offset` to `size_t *error_offset`. The Go side changes from `C.int` to `C.size_t`.
- `gstr_shim_utf8_next_grapheme` (line 161): Change `int byte_len, int offset` to `size_t byte_len, size_t offset` and return type from `int` to `size_t`. The Go side changes from `C.int` to `C.size_t`.
- The `GraphemeIterator.Next()` method (line 665) currently uses `C.int` casts; these change to `C.size_t`.
- `Valid()` and `ValidIndex()` functions change their C-side types similarly.
- All other shim functions already use `size_t` for byte lengths and are unaffected.

### 6.3. gstr-hare (Pure Reimplementation)

**Nature**: Complete reimplementation in Hare. The Hare binding does not call C code. It uses Hare's `size` type (equivalent to `size_t`) for all byte offsets throughout.

**Required changes**: None. The Hare binding already uses the correct types. It serves as a reference implementation of what the C library's type usage should look like.

## 7. Backward Compatibility

### 7.1. This Is a Breaking API Change

The UTF-8 layer functions change their signatures. Any code that:
- Stores a function pointer to `utf8_next_grapheme` or similar
- Passes an `int *` to `utf8_valid` for `error_offset`
- Relies on the `int` return type for sign-testing

...will fail to compile or silently break.

### 7.2. Scope of Impact

The library is header-only (`static inline`) and pre-1.0 (version `0.0.0+dev`). The UTF-8 layer functions are documented as lower-level building blocks. The gstr layer signatures are **unchanged**, so users who only use `gstrlen`, `gstrcmp`, etc. see no API change.

### 7.3. Mitigation Strategy

1. **Version bump**: Increment the minor version (0.1.0 or similar) to signal the breaking change.
2. **Changelog entry**: Document that UTF-8 layer functions now use `size_t` for byte offsets/lengths.
3. **No compatibility shim**: Given the pre-1.0 status and the fact that this is a correctness fix, no `#ifdef`-based backward compatibility layer is warranted.
4. **Compile-time detection**: Users who assigned `utf8_next_grapheme` results to `int` variables will get compiler warnings (with `-Wsign-conversion`) or errors (with `-Werror`), guiding them to update.

## 8. Testing

### 8.1. Existing Tests

The test suite in `/home/edward/repos/gstr/test/test_gstr.c` exercises the public API with typical-size strings. All existing tests must continue to pass after the change. The `ASSERT_EQ` macro (line 37) casts to `int` for display; `ASSERT_EQ_SIZE` (line 47) uses `%zu`/`size_t` and is already correct for the new return types.

### 8.2. New Tests Required

**Type boundary test**: Verify that `utf8_next_grapheme` and `gstrlen` work correctly with `byte_len` values near `INT_MAX` (2,147,483,647). This does not require allocating 2 GB; it can be tested by passing a `byte_len` larger than the actual buffer and verifying the function respects the buffer content:

```
Test: utf8_next_grapheme with byte_len = (size_t)INT_MAX + 1
      on a short ASCII string should still find the correct boundary.
```

**Zero-count binary search test**: Verify that `unicode_range_contains` with `count = 0` returns 0 (testing the half-open interval restructuring does not regress).

**Compile-time width test**: A test that compiles with:
```c
_Static_assert(sizeof(size_t) >= sizeof(int), "size_t must be at least as wide as int");
```

**`utf8_prev` boundary test**: Verify `utf8_prev(text, 0)` returns 0 and does not underflow.

**`gstrrev` large offset test**: Verify `gstrrev` correctly stores and retrieves offsets larger than `INT_MAX` (if feasible on the test platform; otherwise assert the offset array element size is `sizeof(size_t)`).

### 8.3. Compiler Warning Verification

After the change, compile with `-Wall -Wextra -Wsign-conversion -Wconversion` and verify zero warnings from gstr.h. This is the single most important validation: the current codebase cannot pass `-Wsign-conversion` due to the casts.

## 9. Static Assertions

Add the following compile-time checks near the top of gstr.h (after the includes):

```c
/* Verify size_t is large enough for all byte offset operations */
_Static_assert(sizeof(size_t) >= sizeof(int),
    "gstr requires size_t to be at least as wide as int");

/* Verify size_t can represent the maximum UTF-8 sequence length */
_Static_assert((size_t)4 == 4,
    "gstr requires size_t to represent small constants");

/* Verify uint32_t is exactly 32 bits (for Unicode codepoint storage) */
_Static_assert(sizeof(uint32_t) == 4,
    "gstr requires uint32_t to be exactly 4 bytes");
```

For C89/C90 compatibility (if needed), guard with:
```c
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
/* _Static_assert available */
#elif defined(__cplusplus) && __cplusplus >= 201103L
/* static_assert available */
#endif
```

## 10. Migration Plan

### Phase 1: Binary Search Functions (Lowest Risk)

Change `unicode_range_contains`, `get_gcb`, and `is_incb_linker` to use `size_t count` parameter and half-open `[low, high)` interval. These are purely internal and have no API surface. Run existing tests.

### Phase 2: UTF-8 Leaf Functions (Low Risk)

Change `utf8_decode` and `utf8_encode` to use `size_t`. These are called by other UTF-8 functions but have simple, self-contained logic. Their return values (bytes consumed/written, range 0-4) fit in `size_t` trivially. Run existing tests.

### Phase 3: UTF-8 Navigation Functions (Medium Risk)

Change `utf8_next`, `utf8_prev`, `utf8_next_grapheme`, `utf8_prev_grapheme` to use `size_t`. This is the critical change that eliminates the type boundary. Carefully handle:
- Removal of `offset < 0` checks (replace with documentation that `size_t` cannot be negative)
- `utf8_prev` backward scan logic with unsigned arithmetic
- `utf8_prev_grapheme` scan-start calculation

Run existing tests, then add the new boundary tests.

### Phase 4: UTF-8 Utility Functions (Low Risk)

Change `utf8_valid`, `utf8_cpcount`, `utf8_truncate`, `utf8_charwidth`. Run tests.

### Phase 5: Gstr Layer Internal Cleanup (High Volume, Low Risk Per Change)

With the UTF-8 layer now using `size_t`, systematically:
1. Change all `int offset = 0` to `size_t offset = 0` in gstr functions
2. Delete all `(int)byte_len` casts
3. Delete all `(size_t)offset` casts
4. Change `gstr_grapheme_width` parameters from `int` to `size_t`
5. Change `gstrrev`'s `int *offsets` to `size_t *offsets`
6. Change `gstrendswith`'s `int s_off = (int)s_offset` to direct assignment

Run full test suite after each function group.

### Phase 6: Add Static Assertions and Compiler Flags

Add `_Static_assert` checks. Enable `-Wsign-conversion` in the build system. Verify clean compilation.

### Phase 7: Update Language Bindings

1. Copy updated `gstr.h` to `gstr-zig/include/gstr.h`
2. Simplify Zig `@intCast` calls in `gstr.zig`
3. Update Go shim function signatures in `gstr.go`
4. Hare binding: no changes needed
5. Run each binding's test suite

### Phase 8: Documentation and Version

1. Bump version number
2. Document the breaking change in UTF-8 layer function signatures
3. Note that gstr layer signatures are unchanged