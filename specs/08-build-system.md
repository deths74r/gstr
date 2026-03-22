# gstr Build System Improvements Specification

**Project:** gstr -- grapheme-based string library for C  
**Date:** 2026-03-21  
**Status:** Draft  
**Scope:** Build system, CI, packaging, and cross-language binding maintenance

---

## 1. Makefile Fixes

### 1.1 Allow Compiler Override

**Current state:** Line 4 of `Makefile` hard-codes `CC = clang`. This uses simple assignment (`=`), which prevents users and CI systems from overriding the compiler via environment variable or command-line argument.

**Required change:** Replace `CC = clang` with `CC ?= clang`. The `?=` operator sets `CC` only if it is not already defined in the environment, allowing `CC=gcc make` or export-based overrides to work. The default remains `clang` for developers who do not specify a compiler.

### 1.2 DESTDIR Support for Staged Installs

**Current state:** The `install` target writes directly to `$(PREFIX)/include`, which defaults to `/usr/local/include`. There is no `DESTDIR` variable. This breaks packaging workflows (dpkg-buildpackage, rpmbuild, `DESTDIR=... make install`) that need to install into a staging root without touching the live filesystem.

**Required change:** Prefix all install destination paths with `$(DESTDIR)`:

- `install -d $(DESTDIR)$(INCLUDEDIR)` 
- `install -m 644 $(HEADER) $(DESTDIR)$(INCLUDEDIR)/gstr.h`
- `rm -f $(DESTDIR)$(INCLUDEDIR)/gstr.h` in the `uninstall` target

`DESTDIR` should default to empty (i.e., `DESTDIR ?=` or simply left undefined) so that bare `make install` continues to work as before.

The pkg-config file install (Section 2) must also respect `DESTDIR`.

### 1.3 Separate Build and Run Targets for Tests

**Current state:** The `test` target both compiles and immediately runs the test binary in a single recipe. This is incompatible with cross-compilation workflows where the binary is built on the host but executed on a different target (or under an emulator like QEMU).

**Required change:** Split into three targets:

- **`build-test`**: Compiles `test/test_gstr.c` into `test/test_gstr`. Compiles `test/test_grapheme_walk.c` into `test/test_grapheme_walk`. Does not execute anything. These are file targets depending on their source files and `$(HEADER)`.
- **`run-test`**: Depends on `build-test`. Executes `./test/test_gstr` and `./test/test_grapheme_walk`. This is a `.PHONY` target.
- **`test`**: An alias for `run-test` (preserves backward compatibility). This is a `.PHONY` target.

The `all` target should depend on `build-test` rather than `test`, so that `make` alone compiles but does not automatically run tests. Running tests should be an explicit `make test` or `make run-test` step.

### 1.4 Clean Up WAL/SHM Database Files

**Current state:** The `clean` target only removes `test/test_gstr`. The repository root shows stale SQLite WAL and SHM files (`unicode16-eaw.db-shm`, `unicode16-eaw.db-wal`, `unicode16-emoji.db-shm`, `unicode16-emoji.db-wal`) and there are cached files under `scripts/.unicode_cache/`. These are not cleaned.

**Required change:** Extend the `clean` target to remove:

- `test/test_gstr`
- `test/test_grapheme_walk`
- `*.db-shm` and `*.db-wal` in the repository root
- `scripts/.unicode_cache/` directory contents (or the directory itself)

### 1.5 .gitignore Updates

**Current state:** The `.gitignore` lists `test/test_gstr`, `test/test_gstr_stress`, `test/test_new_functions_stress`, `test/test_single`, and `*.db` but is missing entries for several untracked artifacts visible in `git status`.

**Required additions:**

- `gstr-hare/test` -- the compiled Hare test binary
- `test/test_grapheme_walk` -- the grapheme walk test binary
- `scripts/.unicode_cache/` -- the Unicode data download cache
- `*.db-shm` and `*.db-wal` -- SQLite WAL-mode sidecar files

---

## 2. pkg-config Support

### 2.1 Template File: `gstr.pc.in`

Create a `gstr.pc.in` template at the repository root with the following exact content:

```
prefix=@PREFIX@
includedir=${prefix}/include

Name: gstr
Description: Grapheme-based string library for C (header-only)
URL: https://github.com/edwardJedmonds/gstr
Version: @VERSION@
Cflags: -I${includedir}
```

**Notes:**
- No `Libs:` line is needed because gstr is header-only with `static inline` functions.
- `@PREFIX@` and `@VERSION@` are placeholders replaced by `sed` during the build.

### 2.2 Makefile Generation

Add a `PKGCONFIGDIR` variable and a file target:

- `PKGCONFIGDIR = $(PREFIX)/lib/pkgconfig`
- The target `gstr.pc` depends on `gstr.pc.in` and runs `sed` to substitute `@PREFIX@` with `$(PREFIX)` and `@VERSION@` with `$(VERSION)`.
- The generated `gstr.pc` should be listed in `.gitignore`.

### 2.3 Install Location

Extend the `install` target to:

1. Create `$(DESTDIR)$(PKGCONFIGDIR)` with `install -d`.
2. Install the generated `gstr.pc` into that directory with mode 644.

Extend the `uninstall` target to remove `$(DESTDIR)$(PKGCONFIGDIR)/gstr.pc`.

Extend the `clean` target to remove the generated `gstr.pc`.

---

## 3. CI Configuration

### 3.1 GitHub Actions Workflow

Create `.github/workflows/ci.yml` with the following matrix and jobs.

### 3.2 Build Matrix

| OS | Compiler | C Standard | Notes |
|----|----------|------------|-------|
| `ubuntu-latest` | `gcc` | `-std=c99` | Verifies C99 compatibility claim from README |
| `ubuntu-latest` | `gcc` | `-std=c17` | Primary development standard per CODING_STANDARDS.md |
| `ubuntu-latest` | `clang` | `-std=c99` | |
| `ubuntu-latest` | `clang` | `-std=c17` | |
| `macos-latest` | Apple Clang (default `cc`) | `-std=c99` | Tests macOS/Xcode toolchain |
| `macos-latest` | Apple Clang (default `cc`) | `-std=c17` | |
| `macos-latest` | `gcc` (via Homebrew) | `-std=c17` | GCC on ARM64 macOS |
| `windows-latest` | MSVC (`cl.exe`) | `/std:c17` | Header-only; feasible via `Developer Command Prompt` setup action |

**MSVC feasibility note:** Since gstr.h is header-only and uses only standard C with `static inline`, MSVC compilation should work. The header uses `__cplusplus` guards and standard `<stddef.h>`/`<stdint.h>`. However, MSVC's C99 support has historically lagged. The CI job should be allowed to fail initially (`continue-on-error: true`) until any MSVC-specific issues are catalogued and fixed. Key risks: MSVC may warn on `static inline` in headers, and older MSVC versions lack full C99 support. Use `/std:c17` which implies C11+ features.

### 3.3 Job Steps (per matrix entry)

1. **Checkout** the repository.
2. **Set compiler** via `CC` environment variable.
3. **Override CFLAGS_DEBUG** to use the matrix C standard (e.g., `CFLAGS_DEBUG="-Wall -Wextra -pedantic -std=c99 -g -O0"`).
4. **Build tests:** `make build-test CC=$CC CFLAGS_DEBUG="..."` (requires Section 1.3 to land first).
5. **Run tests:** `make run-test`.
6. **Install test:** `make install DESTDIR=$(pwd)/_staging` followed by verifying the header and pkg-config file exist in the staging directory.

### 3.4 Coverage Reporting

Add a separate job (not part of the compiler matrix) that:

1. Builds with GCC on Ubuntu using `CFLAGS_DEBUG="-std=c17 -g -O0 --coverage -fprofile-arcs -ftest-branches"`.
2. Runs the test suite.
3. Runs `gcov test/test_gstr.c` to generate `.gcov` files.
4. Runs `lcov --capture --directory . --output-file coverage.info` and `lcov --remove coverage.info '/usr/*' --output-file coverage.info` to filter system headers.
5. Uploads the `coverage.info` artifact or posts to a coverage service (Codecov or Coveralls via their respective GitHub Actions).

**Important:** Because gstr.h is header-only with `static inline` functions, the coverage data will be attributed to `gstr.h` lines as inlined into the test translation unit. This is correct and expected. The `gcov` output will show which `static inline` functions and branches in `gstr.h` were exercised.

### 3.5 Unicode GraphemeBreakTest Conformance Suite

Add a dedicated CI job that:

1. Downloads `GraphemeBreakTest.txt` from `https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakTest.txt` (matching the Unicode 17.0 tables in gstr.h).
2. Builds and runs a conformance test program that parses each line of `GraphemeBreakTest.txt`, feeds the codepoint sequence to `utf8_next_grapheme()`, and verifies the grapheme breaks match the expected positions.
3. This conformance test program needs to be written (it does not currently exist as a standalone tool in the repository). The `test/test_grapheme_walk.c` file is a diagnostic/debug tool, not a conformance runner. A new `test/test_conformance.c` should be created that:
   - Reads `GraphemeBreakTest.txt` from stdin or a file path argument.
   - Parses the `÷` (break) and `×` (no-break) markers per the Unicode test format.
   - Encodes each codepoint sequence to UTF-8.
   - Calls `utf8_next_grapheme()` iteratively and checks that breaks occur at exactly the positions marked with `÷`.
   - Reports pass/fail counts and exits non-zero on any failure.
4. The CI job should cache the downloaded `GraphemeBreakTest.txt` between runs.
5. Add a Makefile target `test-conformance` that builds and runs this program.

---

## 4. Static Assertions

### 4.1 Minimum `int` Size

**Context:** The low-level `utf8_*` functions use `int` for byte offsets and lengths (e.g., `utf8_decode` takes `int length`, `utf8_next_grapheme` takes `int length` and `int offset`). The high-level `gstr*` functions use `size_t`. On any platform where `sizeof(int) < 4`, the `int`-based functions would overflow on strings longer than 32,767 bytes (with 16-bit int).

**Required assertion:** Add the following near the top of `gstr.h`, after the `#include` directives and before the first function or table definition:

```c
/* gstr requires int to be at least 32 bits for byte offset arithmetic. */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(int) >= 4, "gstr requires sizeof(int) >= 4");
#elif defined(__cplusplus) && __cplusplus >= 201103L
static_assert(sizeof(int) >= 4, "gstr requires sizeof(int) >= 4");
#endif
```

**Note:** C99 has no `_Static_assert`, so the guard on `__STDC_VERSION__ >= 201112L` (C11) is necessary. For C99 builds, consider a fallback using a negative-size array trick:

```c
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L
typedef char gstr_assert_int_at_least_32bit[sizeof(int) >= 4 ? 1 : -1];
#endif
```

This causes a compile error with a somewhat readable message if `int` is too small.

### 4.2 Unicode Version Tracking Macro

**Current state:** There is no `GSTR_UNICODE_VERSION` macro. The Unicode version (17.0) is documented only in code comments above the table arrays (e.g., "Zero-width character ranges (Unicode 17.0)").

**Required addition:** Define a macro near the version block at the top of `gstr.h`:

```c
/* Unicode standard version used to generate the character property tables. */
#define GSTR_UNICODE_VERSION "17.0.0"
#define GSTR_UNICODE_VERSION_MAJOR 17
#define GSTR_UNICODE_VERSION_MINOR 0
#define GSTR_UNICODE_VERSION_UPDATE 0
```

**Purpose:** This allows:
- Runtime version queries (`printf("Unicode %s\n", GSTR_UNICODE_VERSION)`).
- Compile-time version checks in downstream code (`#if GSTR_UNICODE_VERSION_MAJOR >= 17`).
- The conformance test (Section 3.5) to automatically select the correct `GraphemeBreakTest.txt` URL.
- The `scripts/gen_unicode_tables.py` script to update these macros when regenerating tables for a new Unicode version.

---

## 5. Language Binding Sync

### 5.1 Current State

| Binding | Header Strategy | Notes |
|---------|----------------|-------|
| **gstr-zig** | Symlink: `gstr-zig/include/gstr.h -> ../../include/gstr.h` | Pure Zig wrapper (`gstr.zig`) calls into C header via `@cImport`. Build uses `addIncludePath`. |
| **gstr-go** | Pure Go reimplementation (`gstr.go`, 19KB) | No C dependency. Tables are duplicated in Go. |
| **gstr-hare** | Pure Hare reimplementation (`gstr/` module with `.ha` files) | No C dependency. Tables are duplicated in Hare. |

### 5.2 Zig Binding (Symlink -- No Change Needed)

The Zig binding already uses a relative symlink, which is the correct approach. The symlink ensures `gstr-zig` always sees the canonical header without any copy or build step. This works because:

- Zig's build system follows symlinks.
- The relative path `../../include/gstr.h` is stable within the monorepo layout.
- No action needed for Zig.

**Risk:** If gstr-zig is ever published as a standalone Zig package (outside this monorepo), the symlink will break. At that point, the Zig `build.zig.zon` would need to fetch the header as a dependency. This is a future concern, not a current one.

### 5.3 Go and Hare Bindings (Table Sync)

The Go and Hare bindings are pure reimplementations that duplicate the Unicode property tables. They do not use the C header at all. This means:

- When `scripts/gen_unicode_tables.py` regenerates the C tables (e.g., for a new Unicode version), the Go and Hare tables must be updated independently.
- The recent commit `5965e75` ("Update language bindings for Unicode table fixes, add regression tests") confirms this is already a manual process.

**Recommended approach -- Makefile validation target:**

Add a `check-bindings` target to the Makefile that verifies the binding tables are consistent with the C header. This target should:

1. Extract the Unicode version from `GSTR_UNICODE_VERSION` in `gstr.h` (requires Section 4.2 to land first).
2. Check that each binding's table files contain comments or constants referencing the same Unicode version.
3. For the Go binding: verify that the number of ranges in key tables (e.g., `ZERO_WIDTH_RANGES`, `EAW_WIDE_RANGES`, `EXTENDED_PICTOGRAPHIC_RANGES`) matches between C and Go.
4. For the Hare binding: same range-count check against `tables.ha`, `tables_gcb.ha`, and `tables_props.ha`.
5. Exit non-zero with a clear message if any mismatch is detected.

This target should run in CI (Section 3) as a separate job to catch drift early.

**Not recommended:**

- Auto-generating Go/Hare tables from the C header: The Go and Hare implementations use native data structures and idioms that differ from the C `struct unicode_range` arrays. Auto-generation would require language-specific code generators with no clear benefit over the existing Python script approach.
- Symlinks for Go/Hare: These are pure reimplementations; there is no shared file to symlink.

---

## 6. Version Management

### 6.1 Current Mechanism

The Makefile implements git-based version derivation (lines 17--37):

| Git State | VERSION Result | Example |
|-----------|---------------|---------|
| Exactly on a tag | Tag without `v` prefix | `0.9.0` |
| On `main`, commits since last tag | `<last-tag>+dev.<count>` | `0.9.0+dev.3` |
| On a feature branch | `<last-tag>+<branch-name>` | `0.9.0+fix-tables` |
| No git (tarball) | `0.0.0+unknown` | `0.0.0+unknown` |

`BUILD_ID` is `<short-hash>-<YYYYmmdd-HHMMSS>`, e.g., `5965e75-20260321-143022`.

Both are injected via `-D` flags into the compiler invocation for tests.

### 6.2 Header Fallbacks

The header defines fallback values (lines 34--40 of `gstr.h`):

```c
#ifndef GSTR_VERSION
#define GSTR_VERSION "0.0.0+dev"
#endif
#ifndef GSTR_BUILD_ID
#define GSTR_BUILD_ID "dev"
#endif
```

These activate when the header is used directly (copied into a project, no Makefile). This is correct for a header-only library.

### 6.3 Assessment

**What works well:**
- The git-based derivation is sound and follows semver-with-metadata conventions.
- The `#ifndef` guards in the header allow both build-system-driven and standalone usage.
- The `version` phony target provides easy inspection.

**Issues:**

1. **VERSION_FLAGS only applied to test builds.** The `VERSION_FLAGS` variable is used in the `test` target recipe but not in the `install` target. This is actually correct because the library is header-only -- the installed header uses the `#ifndef` fallback, and downstream projects can pass their own `-DGSTR_VERSION=...` if desired. However, this should be explicitly documented in the README's installation section.

2. **BUILD_ID includes a timestamp, causing non-reproducible builds.** Every compilation produces a different `BUILD_ID` even from the same source at the same commit. For test builds this is harmless, but if `VERSION_FLAGS` were ever applied to installed artifacts, it would break reproducible builds. The `BUILD_TIME` component should be removed from `BUILD_ID` for reproducibility, leaving just the git hash. The timestamp adds no value that the git hash does not already provide.

3. **No tag-based CI release automation.** There is no mechanism to trigger a release when a tag is pushed. Consider adding a GitHub Actions workflow (separate from CI) that triggers on `v*` tags and:
   - Runs the full test matrix.
   - Creates a GitHub Release with the tag name as the release title.
   - Attaches the header file as a release asset (convenient for users who download directly).
   - Publishes the changelog (if one exists).

4. **No version validation.** There is no check that a tagged version matches any version string inside the header or other files. The `check-bindings` target (Section 5.3) could be extended to also verify that the `GSTR_VERSION` fallback in the header is updated for release tags.

### 6.4 Recommended Release Process

1. Update the fallback `GSTR_VERSION` in `gstr.h` from `"0.0.0+dev"` to the release version (e.g., `"1.0.0"`).
2. Update `GSTR_UNICODE_VERSION` if tables have been regenerated.
3. Commit with message `release: vX.Y.Z`.
4. Tag with `git tag vX.Y.Z`.
5. Push tag. CI runs full test matrix plus conformance suite.
6. After CI passes, GitHub Actions (or manual process) creates the release.
7. After tagging, bump the fallback `GSTR_VERSION` back to `"X.Y.Z+dev"` in a follow-up commit to indicate the header is now post-release.

---

## Summary of Dependencies Between Sections

Several sections depend on others landing first:

| Change | Depends On |
|--------|-----------|
| Section 3 (CI build/run split) | Section 1.3 (`build-test` / `run-test` targets) |
| Section 3.5 (conformance test URL) | Section 4.2 (`GSTR_UNICODE_VERSION` macro) |
| Section 5.3 (`check-bindings` target) | Section 4.2 (`GSTR_UNICODE_VERSION` macro) |
| Section 2 (pkg-config install) | Section 1.2 (`DESTDIR` support) |

**Recommended implementation order:**

1. Makefile fixes (Section 1) -- no dependencies, unblocks everything else
2. Static assertions and Unicode version macro (Section 4) -- no dependencies
3. pkg-config support (Section 2) -- depends on Section 1.2
4. CI configuration (Section 3) -- depends on Section 1.3
5. Language binding sync validation (Section 5) -- depends on Section 4.2
6. Version management improvements (Section 6) -- can be done at any time