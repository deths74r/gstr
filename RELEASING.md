<!-- SPDX-License-Identifier: MIT -->
# Releasing gstr

The version string that ends up in `pkg-config` (`gstr.pc`) and in the
`GSTR_VERSION` macro comes from **one source of truth: the committed
`VERSION` file**. A git checkout adds a `+dev.N` (on `main`) or branch
suffix on top; a release tarball or a vendored copy of `gstr.h` has no
`.git`, so it reports exactly what `VERSION` says. This is why the file
must be tracked and kept correct.

## Cutting a release

For a release `X.Y.Z`:

1. **Bump `VERSION`** to `X.Y.Z` (no leading `v`, single line).
2. **Bump the `GSTR_VERSION` fallback** in `include/gstr.h` to match
   (`#define GSTR_VERSION "X.Y.Z"`). This is the version a vendored,
   unstamped header reports, so it must equal `VERSION`.
3. Refresh `test/GraphemeBreakTest.txt` and `GSTR_UNICODE_VERSION` if the
   Unicode data version changed (independent of the library version).
4. `make test` — this runs `check-version`, which **fails the build** if
   the header fallback disagrees with `VERSION`. Fix any mismatch now.
5. Commit, then tag: `git tag vX.Y.Z`.
6. `make check-version` once more on the tagged commit: it now also asserts
   the tag name (minus the leading `v`) equals `VERSION`. Pushing the tag
   runs the full CI suite, including this gate and the `tarball` job that
   verifies a `git archive` tarball stamps a real version.

## Version-number policy (semver)

`X` (major) covers source-incompatible changes to the header or to the
**supported build configuration** — e.g. `4.0.0` was cut when the project
moved to C23 and dropped the tested C99-support guarantee. `Y` (minor) is
additive API; `Z` (patch) is fixes. The Unicode data version tracked by
`GSTR_UNICODE_VERSION` is orthogonal and does not drive the library
version.

## Why a VERSION file and not `git describe` alone

`git describe` only works inside a git checkout. Release tarballs (GitHub's
auto-generated ones use `git archive`, which includes only tracked files)
and copies of `gstr.h` dropped into another tree have no git metadata, and
would otherwise stamp a `0.0.0+unknown` placeholder. The `VERSION` file is
the one input guaranteed to travel with every distribution form.
