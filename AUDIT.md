<!--
SPDX-License-Identifier: MIT
Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
-->

# Repository Audit — 2026-07-08

Full three-pass audit (wiring, doc-vs-code, YAGNI) plus build-hygiene and
correctness review, run as a multi-agent adversarial workflow: 7 finder
lenses, every finding independently re-derived by two hostile verifiers,
a completeness critic, and 3 follow-up sweeps (107 agents total). 45
findings confirmed, 0 refuted, 3 disputed. The headline bugs were
reproduced at runtime with probe programs compiled against the header.

Status legend: `[ ]` open · `[x]` fixed · `[d]` deferred (with reason)

---

## A. Shipped bugs (runtime-confirmed)

- [x] **1. Finish the int→size_t migration — it silently failed.** (critical)
  **Fixed 2026-07-09.** All residue sites converted (plus an 11th site the
  audit missed: `gstrwtrunc` tracked a byte offset in `int last_valid`).
  Header now compiles with **zero** -Wconversion/-Wsign-conversion
  warnings (was 59), meeting spec 03's acceptance gate. Boundary tests
  added in `test/test_type_boundary.c`; the four fast spec 03 §8.2 tests
  run in the default `make test`. **Documented compromise:** the five
  >2 GB regression tests (gstrendswith/gstrsub/gstrrev/gstrstr/
  gstrreplace past INT_MAX) each grapheme-walk ~2^31 positions (measured
  1-5 min each at -O3, >2 GB RAM), so they are gated behind
  `make test-boundary-full` with a loud SKIP notice in every default run.
  Revisit trigger: run test-boundary-full before every release, and wire
  it into CI (item 24) when CI exists. All five passed on 2026-07-09.
  The v3.0.0 migration script (`scripts/fix_int_size_t.py`) used
  indentation-anchored regexes that didn't match the real nesting depth, so
  its substitutions no-opped and 9 functions still truncate offsets through
  `int`. Commit 69f7a90's claim "Remove all 68 downward (int)byte_len casts"
  is false. On strings/offsets past INT_MAX (2 GB):
  - `gstrendswith` returns 0 for a genuinely matching suffix (`include/gstr.h:2275`)
  - `gstrsub` returns an empty string instead of a truncated copy (`gstr.h:2663-2675`)
  - `gstrstr`/`gstrrstr`/`gstrcasestr` never find a needle past 2 GB (`gstr.h:2381`, `:2439`, `:2496`)
  - `gstrreplace` passes `(int)remaining`/`(int)before_len` as a `size_t`
    length — a negative int sign-extends to a near-SIZE_MAX length,
    enabling out-of-bounds reads (`gstr.h:3135`, `:3160`)
  - same residue in `gstrrev` (`:3088-3090`) and `gstrellipsis` (`:3343`)

  Fix: convert the remaining `int` locals / delete the remaining casts at
  gstr.h:2234-2235, 2275, 2282-2283, 2381-2382, 2439-2440, 2496-2497,
  2663-2675, 3088-3090, 3135, 3160, 3343. Add the spec 03 §8.2 boundary
  tests (byte_len = INT_MAX+1 on a short buffer — no 2 GB allocation
  needed). Spec 03's other unmet acceptance gate: zero
  -Wconversion/-Wsign-conversion warnings (59 at audit time).

- [x] **2. Emoji width regression from PR #1 (4cc5757).** (minor, real)
  **Fixed 2026-07-09.** `gstr_grapheme_width` now tracks
  Extended_Pictographic across the whole cluster (also corrects the same
  first-codepoint assumption in the VS16 rule). Regression test:
  `grapheme_width_prepend_zwj_emoji` in test_gstr.c.
  `gstr_grapheme_width` tests only the cluster's *first* codepoint for
  Extended_Pictographic (`gstr.h:3654`), so a Prepend-led emoji ZWJ cluster
  (e.g. U+0600 + 👨‍👩) went from 2 to 4 columns. Fix: track "saw an
  Extended_Pictographic anywhere in the cluster" during the scan loop
  (gstr.h:3624-3649) and use that in the width-2 condition.

- [x] **3. `gstrrev` truncation semantics changed and the change was masked.** (major)
  **Fixed 2026-07-09.** "oll" semantics restored and documented in a doc
  comment. The rewrite segments **forward** (adversarial review caught
  that any `utf8_prev_grapheme`-based walk mis-pairs regional-indicator
  runs longer than the 128-codepoint backtrack window — probe-confirmed
  torn flags — a defect the shipped code also had in full-fit mode).
  test_gstr.c reverted to "oll"; stress suite's empty-needle expectation
  fixed to `haystack` per spec 07 §3. Trade-off: truncation with a tiny
  dst is now O(src_len) instead of O(dst); correctness over speed.
  Commit e465d3a's malloc-free rewrite made truncation reverse the source
  *prefix* that fits ("hello" into 4 bytes → "leh") instead of returning the
  head of the full reversal ("oll") as spec 07 §4 mandates and the pre-v2
  code shipped. The same commit edited `test/test_gstr.c:1054` to expect the
  new behavior; the (unwired) stress suite still asserts "oll" and fails.
  Also from the same rot cluster: `gstrrstr("")` — spec 07 §3 says empty
  needle returns `haystack` (strstr convention); the implementation is
  correct and the stress-suite assertion (`hay+5`) is the stale side.
  Fix: restore "oll" semantics (walk backward from src_len copying while it
  fits), document the truncation contract in a doc comment, revert
  test_gstr.c to "oll", fix the stress suite's empty-needle expectation.

## B. Test infrastructure (currently unable to catch failures)

- [x] **4. `RUN` macro counts failures as passes.** Fixed 2026-07-09
  in both copies (snapshot `tests_failed` before the call).
  (`test/test_macros.h:19`, duplicated in `test/test_gstr.c:19`.) ASSERT
  failure `return`s from the test function, then RUN unconditionally prints
  ` PASS` and increments `tests_passed` — a failing test prints "FAIL …
  PASS" and lands in both totals. Exit codes are correct; summaries of all
  five RUN-based suites are wrong. Fix: snapshot `tests_failed` before the
  call; only print PASS / increment when unchanged.

- [x] **5. `test_grapheme_walk` can never fail the build.** Fixed
  2026-07-09: every detection site increments a global counter; both
  exit paths return nonzero when it is set.
  Wired into `make test` (Makefile:70) but both exit paths
  (`test_grapheme_walk.c:216`, `:466`) unconditionally `return 0` even
  after printing `*** BUG` / `*** MISMATCH`. Fix: count detections and
  return nonzero (spec 10 alternatively allows dropping it from make test).

- [x] **6. Two git-tracked suites are wired into nothing.** Fixed
  2026-07-09: both wired into build-test/run-test/clean; their two stale
  assertions resolved with item 3 (454 assertions now run and pass).
  `test_gstr_stress.c` + `test_new_functions_stress.c` (454 assertions):
  no Makefile rule builds, runs, or cleans them, while specs 01/04/10 call
  them living suites. When actually run, test_new_functions_stress fails
  2/119 (the two stale contracts in item 3). Fix: add build/run/clean rules;
  the two failures resolve with item 3.

- [x] **7. MC/DC suite never reaches rule GB9b.** Fixed 2026-07-09:
  false-path cases rewritten to GCB_OTHER + 7 new pair cases (40→47
  tests). gcov-verified: every line of is_grapheme_break executes and
  every branch is taken at least once.
  (`test/test_mcdc_grapheme_break.c:146`.) The tests use GCB_EXTEND where
  the spec's matrix requires GCB_OTHER, so GB9 intercepts every false-path
  case before the rule under test; gcov shows GB9b's body never executes
  and 7 branch outcomes never taken. Deleting GB9b from the header would
  still pass every wired suite. Fix: add spec 01 Appendix A false-path
  cases (GB9b_F, GB9c_C2_F, GB11_C2_F/C3_F, GB12_C2_F, GB7_rightF, GB8_C3F).

- [x] **8. `gstr_grapheme_width` has zero direct tests.** Fixed
  2026-07-09: eight direct tests in test_gstr.c, including the item 2
  regression case and the spec 05 gstrwidth-agreement check.
  Recommended by header docs; spec 05 (§209) requires width tests to call
  both `gstrwidth()` and `gstr_grapheme_width()` to prove agreement; spec
  10 names it the #1 untested function. Fix: add direct tests, including
  the item 2 regression case.

## C. README claims that are wrong

- [x] **9. "Freestanding: ✅" is false** (README.md:46). The header
  unconditionally includes `<stdlib.h>`/`<string.h>` and calls
  malloc/strlen/memcpy with no guards; it won't preprocess on a
  freestanding toolchain. Change to ❌ or build a real freestanding mode.
- [x] **10. Family emoji is 5 codepoints, not 7** (README.md:646);
  `utf8_cpcount` returns 5.
- [x] **11. `gstrellipsis` example output is "Hello W…", not "Hello…"**
  (README.md:438) — the ellipsis budget is counted in graphemes, not bytes.
- [x] **12. "Your First Program" passes len 14 for an 18-byte string**
  (README.md:71), silently dropping the emoji it showcases.
- [x] **13. Stale counts and a bad example**: "340 tests across 4 suites"
  (README.md:1099) is 6 suites/~430; the "GOOD" iteration example
  (README.md:831) still uses `int` offsets — the exact bug class of item 1.
  Also info-level: "46 functions" heading lists 44 (README.md:788);
  "~110KB source" is 126 KB (README.md:1156).

## D. Dead code to delete

- [x] **14. `--hare` output mode in `scripts/gen_unicode_tables.py`**
  (lines 18-20, 471-486, 489-517, 524-528, 588-589). Its only consumer
  (gstr-hare/) was deleted in cd98de4; no fork or external consumer exists.
- [x] **15. `scripts/fix_int_size_t.py`** — spent one-shot whose broken
  regexes caused item 1; its patterns can no longer match the header.
- [x] **16. `cursor_walk` dead branches**: the 'p' toggle (`show_props`
  toggled at tools/cursor_walk.c:576, never read, advertised in help) and
  the unreachable empty-string/[SKIP] branches (:377-381, :589-593 — spec
  02's empty/NUL test entries were never added).
- [x] **17. Orphans**: `.gitignore` entries `test/test_single` and
  `.zig-cache/` removed; unreachable duplicated guard
  `if (needle_graphemes == 0)` second checks removed from gstrstr/
  gstrrstr/gstrcasestr. **Decision on the fixtures (not deleted):**
  `test/utf8_test_samples.txt` and `test/grapheme_test_strings.txt` are
  kept as manual diagnostic inputs for `test_grapheme_walk`'s file mode
  (`./test/test_grapheme_walk test/<fixture>.txt`). They intentionally
  contain U+FFFD/invalid-UTF-8 lines, so the tool flags "problems" on
  them by design — that's why they are not wired into `make test`.
  Delete them instead if manual-inspection data isn't worth keeping.
- [x] **18. Deleted-bindings debris in specs**: specs 03 (§6.1-6.3,
  checklist), 04 (:197), 09 (:209) direct maintainers to update
  gstr-zig/gstr-go/gstr-hare files that were deleted before the specs were
  implemented. Strike or mark historical.

## E. Build hygiene

- [ ] **19. `CFLAGS` (-O3) was consumed by no rule** (Makefile:5).
  *Partially fixed 2026-07-09:* the new test_type_boundary rule builds
  with $(CFLAGS), so one optimized build is now exercised per default
  test run. Still open: the other nine targets build only at -O0.
- [x] **20. `CC ?= clang` is a no-op** (Makefile:4) — GNU make's built-in
  `CC = cc` means the ?= never fires; builds silently use cc.
- [x] **21. No C++-mode or C99-mode compile check**, despite 4cc5757
  shipping a C++ fix and the README claiming C99.
- [ ] **22. `gstr.pc` staleness** (Makefile:90).
  *Partially fixed 2026-07-09:* gstr.pc now regenerates on every build
  via a FORCE prerequisite, so VERSION/PREFIX changes can no longer
  install a stale file. Still open: tarball (non-git) builds install
  `Version: 0.0.0+unknown` — needs a release-process decision (e.g. a
  VERSION file stamped at tag time).
- [x] **23. SPDX headers missing on 5 tracked files** (incl.
  scripts/gen_unicode_tables.py).

## F. Bigger decisions — implement or formally defer

- [x] **24. CI + Unicode conformance gate** (spec 08:114, spec 01 §7).
  **Fixed 2026-07-10.** `test/test_conformance.c` parses the official
  Unicode `GraphemeBreakTest.txt` (committed at `test/GraphemeBreakTest.txt`,
  Unicode 17.0.0, so the gate runs offline) and checks `utf8_next_grapheme`
  against every vector: **766/766 pass**. Wired into `make test` as a hard
  gate and exposed as `make test-conformance`. `.github/workflows/ci.yml`
  runs `make test` (all suites + the C99/C++17/-Wconversion compile gates +
  conformance) on ubuntu gcc/clang and macOS clang, verifies a staged
  `DESTDIR` install, runs `make test-boundary-full` off the PR path (the
  CI home for the item-1 compromise), and smoke-compiles under MSVC
  (continue-on-error, per spec 08's feasibility note). **Deferred:** the
  coverage-upload job (spec 08 §3.4) — needs a Codecov/Coveralls decision;
  gcov works locally today (see item 7). Refresh the committed test file
  when bumping `GSTR_UNICODE_VERSION`.
- [ ] **25. Never-implemented spec parts, currently reading as fact**:
  spec 06 Part B (Unicode case ops); spec 07 §1/§5/§6/§7/§9 (gstr_iter,
  endswith backward-walk, overlap docs + memmove, gstrwellipsis, namespace
  prefixing) — mark deferred in the specs; spec 02's interactive features
  ('g' goto key, char-name table, SIGWINCH, ESC timeout, cursor-walk-verify
  make target, and the props/GCB-rule-chain view whose dead toggle was
  removed 2026-07-09); spec 01's promised macros/coverage targets.
- [x] **26. CODING_STANDARDS.md is violated wholesale** by gstr.h.
  **Fixed 2026-07-10.** Replaced the standards doc with a rewritten one
  (V2) that resolves the substance — the old "no abbreviations" rule
  contradicted the public API itself (`gstrlen`, `n_len`); V2 blesses the
  concise, standard-library-first, pointer-idiom style the code actually
  uses. Then reconciled the remaining formatting gap Edward's way: **all
  C sources reformatted to V2's Linux-kernel style** (tabs, function
  braces on their own line) via a committed `.clang-format`, and the
  project **standard bumped C17 → C23** (the C99 compatibility claim +
  gate were retired; `check-compat` now gates C23 + C++17 interop +
  -Wconversion). The reformat is provably cosmetic: every C file is
  token-identical before/after (whitespace + macro-continuation joins
  only), and the full suite + 766/766 conformance pass unchanged. A
  `make format` / `make format-check` pair plus a pinned
  clang-format 22.1.5 CI `lint` job keep it from drifting again. The old
  doc is archived at `CODING_STANDARDS_V1.md`.

---

## Notes (info-level / disputed)

- `GSTR_UNICODE_VERSION_MINOR`/`_UPDATE` macros have no consumer and no
  user-facing doc (gstr.h:45).
- cursor_walk's `run_verify` implements 4 of spec 02's 5 checks (the
  backward-forward roundtrip was dropped).
- Spec 08:230 states in present tense that Zig/Go/Hare bindings exist
  in-tree (disputed only on whether it's misleading post-deletion).
- `gstrellipsis`'s `(int)ellipsis_len` cast (gstr.h:3343) is unreachable in
  practice (>2 GB ellipsis string) — fixed with item 1 regardless.
- Positive: `make` + `make test` pass clean; no binaries tracked in git;
  no RPM/COPR packaging exists yet (keep raw-byte diagnostic tools out of
  %check when it does — COPR aborts on invalid UTF-8 stdout).
