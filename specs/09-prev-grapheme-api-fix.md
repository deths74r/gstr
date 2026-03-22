# Specification: Add `length` Parameter to `utf8_prev_grapheme`

## 1. Problem Statement

### API Asymmetry

The UTF-8 grapheme navigation API has an inconsistent signature pair:

```c
int utf8_next_grapheme(const char *text, int length, int offset);  // 3 params
int utf8_prev_grapheme(const char *text, int offset);              // 2 params
```

The same asymmetry exists at the codepoint level:

```c
int utf8_next(const char *text, int length, int offset);  // 3 params
int utf8_prev(const char *text, int offset);               // 2 params
```

Both `utf8_prev` and `utf8_prev_grapheme` lack a `length` parameter, but only the grapheme-level function is the subject of this specification because its internal forward-walk makes the omission actively dangerous rather than merely inconsistent.

### Conflation of `offset` as Both Position and Length

On line 1446 of `gstr.h`, inside `utf8_prev_grapheme`, the forward-walk verification calls:

```c
int next = utf8_next_grapheme(text, offset, curr);
```

Here, `offset` (the caller's current position) is passed as the `length` parameter to `utf8_next_grapheme`. This is intentional -- it limits the forward scan to only the text before the current position. However, it means `utf8_prev_grapheme` has no knowledge of the actual string length. If a caller passes an `offset` beyond the true string length, every `utf8_decode` call inside the forward walk will read out of bounds, because `utf8_next_grapheme` trusts its `length` parameter as an upper bound on valid memory.

### No Bounds Validation Possible

Without a `length` parameter, `utf8_prev_grapheme` cannot validate that `offset <= length`. A caller who passes `offset = 500` into a 100-byte buffer gets silent out-of-bounds reads from both:

1. The backward scan via `utf8_prev` (which reads `text[offset - 1]` and backward).
2. The forward walk via `utf8_next_grapheme` (which reads up to `text[offset - 1]`).

## 2. Proposed Signature Change

### New Signature

```c
static inline int utf8_prev_grapheme(const char *text, int length, int offset);
```

### Semantics of `length`

`length` is the total byte length of the valid memory region `text[0..length-1]`. It serves two purposes:

1. **Input validation**: The function shall clamp `offset` to `length` if `offset > length`. This prevents out-of-bounds reads when callers pass stale or incorrect offsets.

2. **Forward-walk correctness**: The internal call to `utf8_next_grapheme` on line 1446 should continue to use `offset` (not `length`) as the length bound, because the forward walk must stop at the current position to identify the preceding grapheme boundary. The `length` parameter does not change the algorithm's logic; it only gates entry.

### Revised Implementation Sketch (Behavioral, Not Code)

The guard clause changes from:

- `if (!text || offset <= 0) return 0;`

to:

- `if (!text || length <= 0 || offset <= 0) return 0;`
- `if (offset > length) offset = length;`

The rest of the function body remains unchanged. The forward-walk call `utf8_next_grapheme(text, offset, curr)` continues to use `offset` as the scan boundary, which is now guaranteed to be within `[0, length]`.

### Companion Change to `utf8_prev`

For consistency, `utf8_prev` should also gain a `length` parameter:

```c
static inline int utf8_prev(const char *text, int length, int offset);
```

This is a secondary change. The `utf8_prev` function only reads backward (at most 4 bytes before `offset`), so the safety risk is lower, but the API consistency argument applies. All internal calls to `utf8_prev` within `utf8_prev_grapheme` must be updated to forward the `length` parameter. `utf8_prev` should clamp `offset` to `length` before scanning backward.

## 3. GRAPHEME_MAX_BACKTRACK Limit

### Current Value

`GRAPHEME_MAX_BACKTRACK` is defined as `128` (line 328 of `gstr.h`). It limits the backward scan in `utf8_prev_grapheme` to at most 128 codepoints before `offset`.

### Purpose

UAX #29 grapheme break rules are context-dependent. To determine whether a break exists before position P, one must know the grapheme break properties of codepoints preceding P. The backward scan finds a "safe starting point" from which a forward walk can re-derive the correct grapheme boundaries.

### Sufficiency Analysis

**128 codepoints is sufficient for all conformant Unicode text.** The longest grapheme clusters in practice are:

- **Flag sequences**: 2 Regional Indicator codepoints (4 bytes each = 8 bytes total).
- **ZWJ emoji sequences**: The longest standardized ZWJ sequence (family emoji) is 7 codepoints (~28 bytes). Hypothetical non-standardized sequences could be longer, but they are bounded by the ZWJ rule (GB9c) which only prevents breaks between `\p{Extended_Pictographic} ZWJ \p{Extended_Pictographic}`.
- **Indic conjuncts** (InCB rule, GB9c): A consonant followed by alternating linker+consonant pairs, with interleaved Extend characters. In theory, this can chain indefinitely: `C (Extend* Linker Extend* C)*`. However, real Indic text rarely exceeds 5-10 codepoints per conjunct.
- **Combining mark sequences**: A base character followed by combining marks (GCB=Extend). UAX #29 does not break between a base and its combining marks. Pathological strings can have hundreds of combining marks on a single base (the "zalgo text" pattern).

**Pathological case**: A string of 200 combining marks (U+0300 COMBINING GRAVE ACCENT, 2 bytes each) following a single base character forms a single grapheme cluster of 201 codepoints. With `GRAPHEME_MAX_BACKTRACK = 128`, if `offset` points to the 200th combining mark, the backward scan only reaches 128 codepoints back -- landing in the middle of the cluster, not at the base character. The forward walk from that mid-cluster position will find the wrong boundary.

### Behavior When the Limit Is Hit

When the backtrack limit is exhausted, the function starts its forward walk from `scan_start`, which may be mid-grapheme. The forward walk using `utf8_next_grapheme` will see a continuation byte or combining mark at `scan_start` and treat it as a grapheme boundary (since `utf8_next_grapheme` always advances at least one codepoint). This produces an **incorrect result**: it returns a position that is mid-grapheme-cluster rather than at a true grapheme boundary.

### Recommendation

**Do not increase the limit.** 128 is a pragmatic tradeoff. Pathological strings with >128 combining marks are:

1. Not found in natural text of any language.
2. Often classified as security concerns (homograph attacks, rendering exploits).
3. Typically rejected or truncated by text processing systems.

**Do document the limit.** The function's doc comment should state that results are undefined for grapheme clusters exceeding 128 codepoints. Callers handling untrusted input should validate combining mark runs separately.

**Do not make it configurable.** A compile-time constant is appropriate. Runtime configurability adds API complexity for a case that does not arise in practice. If a future Unicode version introduces a mechanism that creates longer clusters, the constant can be updated.

**Correct behavior specification when the limit is hit**: The function shall return the best-effort boundary found by forward-walking from the earliest reachable position. This may be incorrect for clusters exceeding `GRAPHEME_MAX_BACKTRACK` codepoints. This is explicitly documented as a known limitation, not a bug.

## 4. Correctness Verification

### Invariant

For any valid UTF-8 string `text` of length `length`, and any byte offset `pos` where `0 < pos <= length`:

```
utf8_prev_grapheme(text, length, pos) == last boundary before pos found by forward iteration
```

Formally: let `B = {b_0, b_1, ..., b_n}` be the set of grapheme boundaries produced by repeatedly calling `utf8_next_grapheme(text, length, b_i)` starting from `b_0 = 0`. Then `utf8_prev_grapheme(text, length, pos)` must return `max(b_i : b_i < pos)` for any `pos` in `(0, length]`, and must return `b_i` where `b_i == pos` if `pos` is itself a boundary and `pos > 0` (returning the boundary before it).

When `pos` is itself a grapheme boundary `b_k`, the function must return `b_{k-1}` (the previous boundary), not `b_k` itself.

### Test Strategy

**Property-based forward/backward consistency test:**

1. Generate or curate a corpus of test strings covering:
   - ASCII-only text.
   - Multibyte UTF-8 (CJK, Cyrillic, Arabic).
   - Emoji: simple, ZWJ sequences, flag sequences, keycap sequences.
   - Combining mark sequences of varying lengths (1, 10, 50, 127, 128, 129 marks).
   - Indic conjunct sequences (InCB consonant-linker chains).
   - CR+LF pairs.
   - Mixed scripts with interleaved combining marks.

2. For each test string:
   a. Build the complete boundary array `B[]` by forward-walking with `utf8_next_grapheme` from offset 0 to `length`.
   b. For every boundary `B[k]` where `k > 0`, assert that `utf8_prev_grapheme(text, length, B[k]) == B[k-1]`.
   c. For every non-boundary offset `p` (midpoints within clusters), assert that `utf8_prev_grapheme(text, length, p) == B[k]` where `B[k]` is the largest boundary less than `p`.

3. Additionally, for every boundary `B[k]`, assert that `utf8_prev_grapheme(text, length, B[k])` followed by `utf8_next_grapheme(text, length, result)` yields `B[k]`. That is, going back one grapheme and then forward one grapheme returns to the starting boundary.

**Edge case tests:**

- `offset = 0`: must return 0.
- `offset = length` (end of string): must return the start of the last grapheme cluster.
- `offset = 1` on a multibyte character (mid-codepoint): function behavior is technically undefined (callers should not pass mid-codepoint offsets), but should not crash. Document that passing mid-codepoint offsets produces unspecified results.
- `offset > length` (out-of-bounds): with the new signature, this is clamped to `length` and returns the start of the last grapheme.
- `text = NULL`: must return 0.
- `length = 0`: must return 0.
- Single-codepoint string: `utf8_prev_grapheme(text, length, length)` must return 0.

**Regression tests for GRAPHEME_MAX_BACKTRACK:**

- String of exactly 128 combining marks + 1 base: `utf8_prev_grapheme` from the end must return 0 (the base character).
- String of 129 combining marks + 1 base: `utf8_prev_grapheme` from the end will return a nonzero value (documenting the known limitation).

## 5. Affected Call Sites

### Direct Callers of `utf8_prev_grapheme`

There are **no internal callers** within `gstr.h`. The function is defined at line 1422 but is never called by any `gstr*`-layer function. All grapheme iteration in the public API uses `utf8_next_grapheme` exclusively (forward-only traversal).

The callers are all in language bindings:

| File | Line | Current Call | Required Change |
|------|------|--------------|-----------------|
| `gstr-zig/src/gstr.zig` | 856 | `c.c.utf8_prev_grapheme(s.ptr, @intCast(byte_offset))` | Add `@intCast(s.len)` as second argument: `c.c.utf8_prev_grapheme(s.ptr, @intCast(s.len), @intCast(byte_offset))` |
| `gstr-zig/src/c.zig` | 46 | `pub const utf8_prev_grapheme = c.utf8_prev_grapheme;` | No change needed (re-export picks up new signature automatically via C import) |
| `gstr-hare/gstr/grapheme.ha` | 226 | `prev_grapheme(bytes: []u8, off: size)` | This is a pure-Hare reimplementation, not an FFI call. Add `length` parameter or derive it from `len(bytes)` (Hare slices carry length). The Hare function already has implicit access to length via `len(bytes)` and calls `next_grapheme(bytes, curr)` which also uses `len(bytes)`. Signature should change to match for API consistency, or keep the slice-based API since Hare slices are inherently bounds-checked. **Recommended**: no signature change for Hare (slices are safe), but add a `scan_start` clamp: `if (scan_start + remaining_bytes > len(bytes))` as a defensive measure. |

### Indirect Effects

The `README.md` documents the function signature at line 759:

```
| `utf8_prev_grapheme(text, offset)` | Byte offset of previous grapheme cluster boundary. |
```

This must be updated to:

```
| `utf8_prev_grapheme(text, length, offset)` | Byte offset of previous grapheme cluster boundary. |
```

### Internal Call Within `utf8_prev_grapheme` Itself

Line 1427 calls `utf8_prev(text, offset)` and lines 1435 call `utf8_prev(text, scan_start)`. If `utf8_prev` is also updated to take a `length` parameter (recommended for consistency), these calls must pass `length` through. However, if `utf8_prev` is left unchanged, no changes are needed here.

## 6. Backward Compatibility

### Scope of the Break

This is an **API-breaking change at the `utf8_*` layer only**. The `gstr*` public API layer (all functions prefixed with `gstr`) is **unaffected** because none of them call `utf8_prev_grapheme`.

The `utf8_*` layer is documented in the README as a lower-level API. It is a header-only library (`gstr.h`), so there is no ABI concern -- users recompile from source.

### Affected Consumers

1. **Zig bindings** (`gstr-zig/`): Maintained in-tree. Update simultaneously with the header change.
2. **Hare bindings** (`gstr-hare/`): Pure reimplementation, not FFI. Update for API consistency but not strictly required for safety (Hare slices carry length).
3. **Direct C users of `utf8_prev_grapheme`**: Any external code calling this function will fail to compile after the change (missing argument), which is the desired behavior -- a compile error is far better than silent memory unsafety.

### Migration Strategy

1. **No deprecation period.** The function has no internal callers in the `gstr*` layer, and all in-tree bindings are updated atomically. External users of the `utf8_*` layer are expected to be few (it is explicitly documented as a lower-level API).

2. **Compile-time breakage is the migration mechanism.** The added parameter causes a compile error at every call site. The fix at each site is mechanical: insert the string's known length as the second argument.

3. **Semantic versioning**: This change requires a minor version bump (assuming pre-1.0) or a major version bump (if post-1.0), since it breaks a public API signature.

4. **Changelog entry**: Document the signature change, the rationale (bounds safety), and the one-line fix required at each call site.

### Optional: Symmetric Fix for `utf8_prev`

The same asymmetry exists at the codepoint level: `utf8_next(text, length, offset)` vs `utf8_prev(text, offset)`. Fixing both in the same change is recommended to avoid a second API break later. `utf8_prev` is called at lines 1427, 1435 within `utf8_prev_grapheme`, plus potentially in external code. The migration is identical: add `length` as the second parameter, producing a compile error at every call site that must be mechanically fixed.