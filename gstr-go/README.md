# gstr-go

Go bindings for [gstr](../README.md) - a grapheme-aware string library implementing UAX #29 grapheme cluster segmentation.

## Overview

Go's `len()` returns bytes, `utf8.RuneCountInString()` returns codepoints, but neither gives graphemes - what users perceive as "characters".

```go
import "github.com/edwardedmonds/gstr-go"

len("👨‍👩‍👧")                       // 18 (bytes)
utf8.RuneCountInString("👨‍👩‍👧")    // 7 (codepoints)
gstr.Len("👨‍👩‍👧")                   // 1 (graphemes)
```

## Installation

```bash
go get github.com/edwardedmonds/gstr-go
```

Requires cgo and a C compiler. The bindings use `#cgo CFLAGS: -I${SRCDIR}/../include` to access `gstr.h` directly.

## API Reference

### Length & Width

```go
gstr.Len(s)            // grapheme count
gstr.NLen(s, max)      // count up to max graphemes
gstr.Width(s)          // terminal column width
```

### Indexing

```go
gstr.At(s, n)          // Nth grapheme (string)
gstr.Offset(s, n)      // byte offset of Nth grapheme
```

### Comparison

```go
gstr.Compare(a, b)     // -1, 0, or 1
gstr.CompareN(a, b, n) // compare first n graphemes
gstr.CaseCompare(a, b) // case-insensitive (ASCII)
```

### Prefix/Suffix

```go
gstr.HasPrefix(s, prefix)  // bool
gstr.HasSuffix(s, suffix)  // bool
```

### Search

```go
gstr.Index(s, grapheme)         // byte offset of first grapheme (-1 if not found)
gstr.LastIndex(s, grapheme)     // byte offset of last grapheme
gstr.Contains(s, needle)        // bool
gstr.IndexSubstring(s, needle)  // byte offset of substring
gstr.LastIndexSubstring(s, needle)
gstr.IndexFold(s, needle)       // case-insensitive substring search
gstr.Count(s, needle)           // count occurrences
```

### Extraction

```go
gstr.Sub(s, start, count)  // extract count graphemes from start
```

### Transform

```go
gstr.Reverse(s)                          // reverse by graphemes
gstr.Replace(s, old, new)               // replace all occurrences
gstr.ToLower(s)                          // ASCII lowercase
gstr.ToUpper(s)                          // ASCII uppercase
gstr.Ellipsis(s, maxGraphemes, suffix)   // truncate with suffix
```

### Trim

```go
gstr.TrimLeft(s)   // remove leading whitespace
gstr.TrimRight(s)  // remove trailing whitespace
gstr.Trim(s)       // remove both
```

### Padding

```go
gstr.PadLeft(s, width, pad)    // left-pad to grapheme width
gstr.PadRight(s, width, pad)   // right-pad to grapheme width
gstr.PadCenter(s, width, pad)  // center-pad to grapheme width
gstr.Truncate(s, maxWidth)     // truncate to terminal column width
```

### Validation

```go
gstr.Valid(s)        // bool - is valid UTF-8?
gstr.ValidIndex(s)   // byte offset of first invalid byte (-1 if valid)
```

### Iterator

```go
it := gstr.Graphemes(s)
for {
    g := it.Next()
    if g == "" {
        break
    }
    // process grapheme g
}
it.Reset()  // rewind to start
```

## Testing

```bash
cd gstr-go
go test -v
```

Tests cover: basic length/width, CJK, emoji (ZWJ sequences, skin tones, flags), combining marks, Korean Hangul, searching, comparison, prefix/suffix, padding, trimming, tokenization, EAW split regression (♠♣♥♦★ = width 1), and ZWJ join/non-join verification.

## Unicode Version

Unicode 17.0 property tables via the C header (`include/gstr.h`). The Go bindings use cgo to access the C implementation directly — no duplicate tables.

## License

MIT - see parent project for details.
