# MC/DC Testing Specification for `is_grapheme_break` in gstr

## 1. Goal

Modified Condition/Decision Coverage (MC/DC) is a structural coverage criterion that requires, for every condition C in every decision D:

1. Every condition has taken both TRUE and FALSE values.
2. Every decision has taken both TRUE and FALSE outcomes.
3. Each condition is shown to **independently affect** the decision's outcome. This means: for each condition, there exist two test cases that differ only in the value of that condition while all other conditions in the same decision hold fixed, and the decision outcome flips.

The function under test is `is_grapheme_break` at `/home/edward/repos/gstr/include/gstr.h` lines 1091-1161. It implements UAX #29 grapheme cluster break rules via a chain of if-statements, each encoding one or more Unicode break rules (GB3 through GB999). The function takes six parameters:

| Parameter | Type | Description |
|-----------|------|-------------|
| `prev_prop` | `enum gcb_property` | GCB property of the preceding codepoint |
| `curr_prop` | `enum gcb_property` | GCB property of the current codepoint |
| `ri_count` | `int` | Count of consecutive Regional Indicator codepoints seen |
| `in_ext_pict` | `int` | Whether an Extended_Pictographic codepoint precedes in the cluster |
| `curr_cp` | `uint32_t` | The actual codepoint value of the current character |
| `incb_state` | `int` | Indic Conjunct Break state machine value (0, 1, or 2) |

Additionally, the state machine in `utf8_next_grapheme` (lines 1362-1416) manages three state variables (`ri_count`, `in_ext_pict`, `incb_state`) across iterations. MC/DC for the full grapheme-walking logic must cover both the per-boundary decisions in `is_grapheme_break` and the state transitions that feed into them.

The objective is to produce a test suite where every condition in every compound decision within `is_grapheme_break` has a pair of test cases demonstrating its independent effect on the return value. For simple (single-condition) decisions, condition coverage and decision coverage coincide. For compound decisions (GB4, GB5, GB6, GB7, GB8, GB9, GB11, GB12/GB13), explicit MC/DC pairs are required.

---

## 2. Condition Inventory

### 2.1 Decision Map

Each if-statement in `is_grapheme_break` is a **decision**. Each atomic comparison within is a **condition**. The following table enumerates them. Conditions are labeled `Cn` within each rule for cross-referencing in the test matrix.

| Rule | Line(s) | Decision Expression | Conditions | Return |
|------|---------|-------------------|------------|--------|
| GB3 | 1096 | `prev_prop == GCB_CR && curr_prop == GCB_LF` | C1: `prev_prop == GCB_CR`; C2: `curr_prop == GCB_LF` | 0 (no break) |
| GB4 | 1101 | `prev_prop == GCB_CONTROL \|\| prev_prop == GCB_CR \|\| prev_prop == GCB_LF` | C1: `prev_prop == GCB_CONTROL`; C2: `prev_prop == GCB_CR`; C3: `prev_prop == GCB_LF` | 1 (break) |
| GB5 | 1106 | `curr_prop == GCB_CONTROL \|\| curr_prop == GCB_CR \|\| curr_prop == GCB_LF` | C1: `curr_prop == GCB_CONTROL`; C2: `curr_prop == GCB_CR`; C3: `curr_prop == GCB_LF` | 1 (break) |
| GB6 | 1111-1112 | `prev_prop == GCB_L && (curr_prop == GCB_L \|\| curr_prop == GCB_V \|\| curr_prop == GCB_LV \|\| curr_prop == GCB_LVT)` | C1: `prev_prop == GCB_L`; C2: `curr_prop == GCB_L`; C3: `curr_prop == GCB_V`; C4: `curr_prop == GCB_LV`; C5: `curr_prop == GCB_LVT` | 0 (no break) |
| GB7 | 1117-1118 | `(prev_prop == GCB_LV \|\| prev_prop == GCB_V) && (curr_prop == GCB_V \|\| curr_prop == GCB_T)` | C1: `prev_prop == GCB_LV`; C2: `prev_prop == GCB_V`; C3: `curr_prop == GCB_V`; C4: `curr_prop == GCB_T` | 0 (no break) |
| GB8 | 1123 | `(prev_prop == GCB_LVT \|\| prev_prop == GCB_T) && curr_prop == GCB_T` | C1: `prev_prop == GCB_LVT`; C2: `prev_prop == GCB_T`; C3: `curr_prop == GCB_T` | 0 (no break) |
| GB9 | 1128 | `curr_prop == GCB_EXTEND \|\| curr_prop == GCB_ZWJ` | C1: `curr_prop == GCB_EXTEND`; C2: `curr_prop == GCB_ZWJ` | 0 (no break) |
| GB9a | 1133 | `curr_prop == GCB_SPACING_MARK` | C1: `curr_prop == GCB_SPACING_MARK` | 0 (no break) |
| GB9b | 1138 | `prev_prop == GCB_PREPEND` | C1: `prev_prop == GCB_PREPEND` | 0 (no break) |
| GB9c | 1143 | `incb_state == 2 && is_incb_consonant(curr_cp)` | C1: `incb_state == 2`; C2: `is_incb_consonant(curr_cp)` | 0 (no break) |
| GB11 | 1148-1149 | `in_ext_pict && prev_prop == GCB_ZWJ && is_extended_pictographic(curr_cp)` | C1: `in_ext_pict`; C2: `prev_prop == GCB_ZWJ`; C3: `is_extended_pictographic(curr_cp)` | 0 (no break) |
| GB12/13 | 1154-1156 | `prev_prop == GCB_REGIONAL_INDICATOR && curr_prop == GCB_REGIONAL_INDICATOR` (then `(ri_count % 2) == 0` determines outcome) | C1: `prev_prop == GCB_REGIONAL_INDICATOR`; C2: `curr_prop == GCB_REGIONAL_INDICATOR`; C3: `(ri_count % 2) == 0` | `(ri_count % 2) == 0` |
| GB999 | 1160 | (unconditional fallthrough) | None | 1 (break) |

**Total atomic conditions: 30** across 12 decisions (including the compound return expression in GB12/13).

### 2.2 Critical Observations for MC/DC

**Short-circuit evaluation**: C compilers use short-circuit `&&` and `||`. When a left operand of `&&` is false, the right is not evaluated; when a left operand of `||` is true, the right is not evaluated. MC/DC pairs must account for this -- to test a right-hand condition independently, the left-hand condition(s) must be held in the state that allows evaluation to reach the right-hand condition.

**Rule ordering matters**: `is_grapheme_break` is a sequential if-else chain. A later rule is only reached if all prior rules' decisions were FALSE. This means that to test GB9c, for example, the inputs must not trigger GB3-GB9b. The MC/DC pairs must be constructed with awareness of the rule precedence.

**GB4 complication for GB3**: The GB3 decision (CR + LF = no break) is checked before GB4 (CR/LF/Control = break). When `prev_prop == GCB_CR` and `curr_prop != GCB_LF`, control falls through GB3 to GB4 where `prev_prop == GCB_CR` triggers a break. This is correct per the Unicode spec but means the "GB3 C1=true, C2=false" case is actually caught by GB4, not by GB3's false path. MC/DC must account for this: the decision outcome of GB3 as a whole is what matters, not that GB3 explicitly returns on its false path.

---

## 3. Test Case Matrix

### Notation

- **Direct tests** call `is_grapheme_break` directly with explicit parameter values. This is possible because the function is `static` in the header and thus available in the test translation unit.
- **Integration tests** use `utf8_next_grapheme` / `gstrlen` with crafted UTF-8 strings, exercising the state machine that feeds `is_grapheme_break`.
- For each MC/DC pair, we label the test as `RuleXX_Cn_T` (condition n is TRUE) and `RuleXX_Cn_F` (condition n is FALSE), with all other conditions held constant.

### 3.1 GB3: `prev_prop == GCB_CR && curr_prop == GCB_LF`

| Test ID | C1 (prev==CR) | C2 (curr==LF) | Decision | Return | Concrete Input |
|---------|:---:|:---:|:---:|:---:|---|
| GB3_C1_T | T | T | T | 0 | prev=U+000D (CR), curr=U+000A (LF) |
| GB3_C1_F | F | T | F | (falls to GB5, returns 1) | prev=U+0061 ('a'), curr=U+000A (LF) |
| GB3_C2_T | T | T | T | 0 | (same as GB3_C1_T) |
| GB3_C2_F | T | F | F | (falls to GB4, returns 1) | prev=U+000D (CR), curr=U+0061 ('a') |

MC/DC pairs for independent effect:
- **C1**: {GB3_C1_T, GB3_C1_F} -- hold C2=T(LF), toggle C1. Decision flips from 0 to 1.
- **C2**: {GB3_C2_T, GB3_C2_F} -- hold C1=T(CR), toggle C2. Decision flips from 0 to 1.

### 3.2 GB4: `prev_prop == GCB_CONTROL || prev_prop == GCB_CR || prev_prop == GCB_LF`

Note: GB4 is only reached when GB3 is false. When `prev_prop == GCB_CR`, GB3 is false only when `curr_prop != GCB_LF`. When `prev_prop == GCB_LF`, GB3 is false (since GB3 checks prev==CR). So GB4's CR and LF branches are reachable.

| Test ID | C1 (CONTROL) | C2 (CR) | C3 (LF) | Decision | Return | Concrete Input |
|---------|:---:|:---:|:---:|:---:|:---:|---|
| GB4_C1_T | T | F | F | T | 1 | prev=U+0000 (NUL, GCB_CONTROL), curr=U+0061 |
| GB4_C1_F | F | F | F | F | (falls through) | prev=U+0061 (GCB_OTHER), curr=U+0061 |
| GB4_C2_T | F | T | F | T | 1 | prev=U+000D (CR), curr=U+0061 (not LF, so GB3 skipped) |
| GB4_C2_F | F | F | F | F | (falls through) | prev=U+0061, curr=U+0061 |
| GB4_C3_T | F | F | T | T | 1 | prev=U+000A (LF), curr=U+0061 |
| GB4_C3_F | F | F | F | F | (falls through) | prev=U+0061, curr=U+0061 |

MC/DC pairs:
- **C1**: {GB4_C1_T, GB4_C1_F} -- hold C2=F, C3=F. Toggle CONTROL. Decision flips.
- **C2**: {GB4_C2_T, GB4_C2_F} -- hold C1=F, C3=F. Toggle CR. Decision flips.
- **C3**: {GB4_C3_T, GB4_C3_F} -- hold C1=F, C2=F. Toggle LF. Decision flips.

### 3.3 GB5: `curr_prop == GCB_CONTROL || curr_prop == GCB_CR || curr_prop == GCB_LF`

Only reached when GB3 and GB4 are both false, meaning `prev_prop` is not CONTROL/CR/LF.

| Test ID | C1 (CONTROL) | C2 (CR) | C3 (LF) | Decision | Return | Concrete Input |
|---------|:---:|:---:|:---:|:---:|:---:|---|
| GB5_C1_T | T | F | F | T | 1 | prev=U+0061, curr=U+0000 (NUL) |
| GB5_C1_F | F | F | F | F | (falls through) | prev=U+0061, curr=U+0061 |
| GB5_C2_T | F | T | F | T | 1 | prev=U+0061, curr=U+000D (CR) |
| GB5_C2_F | F | F | F | F | (falls through) | prev=U+0061, curr=U+0061 |
| GB5_C3_T | F | F | T | T | 1 | prev=U+0061, curr=U+000A (LF) |
| GB5_C3_F | F | F | F | F | (falls through) | prev=U+0061, curr=U+0061 |

MC/DC pairs: Same structure as GB4 but on `curr_prop`.

### 3.4 GB6: `prev_prop == GCB_L && (curr_prop == GCB_L || curr_prop == GCB_V || curr_prop == GCB_LV || curr_prop == GCB_LVT)`

The outer `&&` has two operands: C1 (prev==L) and the inner disjunction (C2||C3||C4||C5). To reach GB6, GB3-GB5 must all be false. Since GCB_L is not CONTROL/CR/LF, using prev=L passes GB4. For curr, using Hangul jamo types (L/V/LV/LVT/T) passes GB5 since none are CONTROL/CR/LF.

Representative codepoints:
- GCB_L: U+1100 (Hangul Choseong Kiyeok)
- GCB_V: U+1161 (Hangul Jungseong A)
- GCB_T: U+11A8 (Hangul Jongseong Kiyeok)
- GCB_LV: U+AC00 (Hangul syllable GA, computed: (0xAC00-0xAC00) % 28 == 0)
- GCB_LVT: U+AC01 (Hangul syllable GAG, (0xAC01-0xAC00) % 28 == 1)

| Test ID | C1 (prev==L) | C2 (curr==L) | C3 (curr==V) | C4 (curr==LV) | C5 (curr==LVT) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| GB6_C1_T_C2_T | T | T | F | F | F | T | 0 |
| GB6_C1_F | F | T | F | F | F | F | (GB999=1) |
| GB6_C1_T_C3_T | T | F | T | F | F | T | 0 |
| GB6_C1_T_C4_T | T | F | F | T | F | T | 0 |
| GB6_C1_T_C5_T | T | F | F | F | T | T | 0 |
| GB6_inner_allF | T | F | F | F | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB6_C1_T_C2_T, GB6_C1_F} -- prev=L vs prev=OTHER, curr=L both times.
- **C2**: {GB6_C1_T_C2_T, GB6_inner_allF with curr=T} -- hold prev=L. curr=L(T) vs curr=T(F). Inner disjunction flips.
- **C3**: {GB6_C1_T_C3_T, GB6_inner_allF with curr=T} -- hold prev=L. curr=V(T) vs curr=T(F).
- **C4**: {GB6_C1_T_C4_T, GB6_inner_allF with curr=T} -- hold prev=L. curr=LV(T) vs curr=T(F).
- **C5**: {GB6_C1_T_C5_T, GB6_inner_allF with curr=T} -- hold prev=L. curr=LVT(T) vs curr=T(F).

Concrete inputs for inner-false: prev=U+1100 (L), curr=U+11A8 (T). This falls through GB6 and is caught by GB999 (break), returning 1.

### 3.5 GB7: `(prev_prop == GCB_LV || prev_prop == GCB_V) && (curr_prop == GCB_V || curr_prop == GCB_T)`

To reach GB7, GB6 must be false. Using prev=LV or prev=V (not L) ensures GB6's `prev==L` is false. Using curr=V or curr=T avoids GB5.

| Test ID | C1 (prev==LV) | C2 (prev==V) | C3 (curr==V) | C4 (curr==T) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|:---:|:---:|
| GB7_C1T_C3T | T | F | T | F | T | 0 |
| GB7_C2T_C3T | F | T | T | F | T | 0 |
| GB7_C1T_C4T | T | F | F | T | T | 0 |
| GB7_C2T_C4T | F | T | F | T | T | 0 |
| GB7_leftF | F | F | T | F | F | (falls through) |
| GB7_rightF | T | F | F | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB7_C1T_C3T, GB7_leftF with prev=OTHER curr=V} -- toggle prev between LV and OTHER. Hold curr=V.
- **C2**: {GB7_C2T_C3T, GB7_leftF with prev=OTHER curr=V} -- toggle prev between V and OTHER. Hold curr=V.
- **C3**: {GB7_C1T_C3T, GB7_rightF} -- hold prev=LV. Toggle curr between V and OTHER.
- **C4**: {GB7_C1T_C4T, GB7_rightF} -- hold prev=LV. Toggle curr between T and OTHER.

Concrete:
- prev LV = U+AC00, prev V = U+1161, prev OTHER = U+0061
- curr V = U+1161, curr T = U+11A8, curr OTHER = U+0061

### 3.6 GB8: `(prev_prop == GCB_LVT || prev_prop == GCB_T) && curr_prop == GCB_T`

To reach GB8, GB7 must be false. Using prev=LVT ensures GB7's `(prev==LV||prev==V)` is false. Using prev=T with curr=T: GB7 has `(prev==LV||prev==V)` which is false for T, so GB7 is skipped.

| Test ID | C1 (prev==LVT) | C2 (prev==T) | C3 (curr==T) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|:---:|
| GB8_C1T_C3T | T | F | T | T | 0 |
| GB8_C2T_C3T | F | T | T | T | 0 |
| GB8_leftF | F | F | T | F | (falls through) |
| GB8_C3F | T | F | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB8_C1T_C3T, GB8_leftF} -- prev=LVT vs prev=OTHER, hold curr=T.
- **C2**: {GB8_C2T_C3T, GB8_leftF} -- prev=T vs prev=OTHER, hold curr=T.
- **C3**: {GB8_C1T_C3T, GB8_C3F} -- hold prev=LVT, toggle curr between T and OTHER.

Concrete:
- prev LVT = U+AC01, prev T = U+11A8, prev OTHER = U+0061
- curr T = U+11A8, curr OTHER = U+0061

### 3.7 GB9: `curr_prop == GCB_EXTEND || curr_prop == GCB_ZWJ`

To reach GB9, GB3-GB8 must all be false. Using prev=OTHER, curr=EXTEND or ZWJ works since EXTEND and ZWJ are not CONTROL/CR/LF.

| Test ID | C1 (curr==EXTEND) | C2 (curr==ZWJ) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|
| GB9_C1_T | T | F | T | 0 |
| GB9_C2_T | F | T | T | 0 |
| GB9_allF | F | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB9_C1_T, GB9_allF} -- toggle curr between EXTEND and OTHER. Hold prev=OTHER.
- **C2**: {GB9_C2_T, GB9_allF} -- toggle curr between ZWJ and OTHER. Hold prev=OTHER.

Concrete:
- curr EXTEND = U+0300 (Combining Grave Accent, GCB=Extend)
- curr ZWJ = U+200D (Zero Width Joiner)
- curr OTHER = U+0061

### 3.8 GB9a: `curr_prop == GCB_SPACING_MARK`

Single condition. To reach GB9a, curr must not be EXTEND or ZWJ (GB9 would catch those).

| Test ID | C1 (curr==SPACING_MARK) | Decision | Return |
|---------|:---:|:---:|:---:|
| GB9a_T | T | T | 0 |
| GB9a_F | F | F | (falls through) |

MC/DC pair: {GB9a_T, GB9a_F}.

Concrete:
- curr SPACING_MARK = U+0903 (Devanagari Visarga, GCB=SpacingMark)
- prev = U+0061 (OTHER), which ensures GB3-GB9 don't fire

### 3.9 GB9b: `prev_prop == GCB_PREPEND`

Single condition. To reach GB9b, the previous rules must not match. Using prev=PREPEND and curr=OTHER: GB3-GB9a don't fire (PREPEND is not CR/LF/CONTROL/L/LV/LVT/V/T, and curr=OTHER is not CR/LF/CONTROL/EXTEND/ZWJ/SPACING_MARK).

| Test ID | C1 (prev==PREPEND) | Decision | Return |
|---------|:---:|:---:|:---:|
| GB9b_T | T | T | 0 |
| GB9b_F | F | F | (falls through) |

MC/DC pair: {GB9b_T, GB9b_F}.

Concrete:
- prev PREPEND = U+0600 (Arabic Number Sign, GCB=Prepend)
- curr = U+0061 (OTHER)

### 3.10 GB9c: `incb_state == 2 && is_incb_consonant(curr_cp)`

To reach GB9c, GB3-GB9b must all be false. This means prev_prop must not be PREPEND and curr_prop must not be EXTEND/ZWJ/SPACING_MARK. Additionally, `incb_state` must be set by the state machine before this call.

| Test ID | C1 (incb_state==2) | C2 (is_incb_consonant) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|
| GB9c_both_T | T | T | T | 0 |
| GB9c_C1_F | F (0 or 1) | T | F | (falls through) |
| GB9c_C2_F | T | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB9c_both_T, GB9c_C1_F} -- hold curr_cp as InCB consonant. Toggle incb_state between 2 and 0.
- **C2**: {GB9c_both_T, GB9c_C2_F} -- hold incb_state=2. Toggle curr_cp between InCB consonant and non-consonant.

Concrete:
- InCB Consonant: U+0915 (Devanagari KA)
- InCB Linker: U+094D (Devanagari Virama)
- Non-consonant for C2_F: U+0061 ('a')
- For direct call: set `incb_state=2`, `curr_cp=0x0915`, `prev_prop=GCB_OTHER`, `curr_prop=GCB_OTHER`
- For integration: The sequence U+0915, U+094D, U+0915 should produce incb_state=2 when the second U+0915 is reached. The virama (U+094D) has GCB=Extend, so GB9 catches it and does not break, and the state machine sets `incb_state=2` when it sees the linker at state>=1. Then the third character (U+0915) is evaluated with incb_state=2.

### 3.11 GB11: `in_ext_pict && prev_prop == GCB_ZWJ && is_extended_pictographic(curr_cp)`

Three-condition AND. To reach GB11, GB3-GB9c must all be false. This means:
- `prev_prop` must be `GCB_ZWJ` (not PREPEND, not CR/LF/CONTROL, not L/V/T/LV/LVT)
- `curr_prop` should be OTHER (ExtPict codepoints have GCB=OTHER) -- not EXTEND/ZWJ/SPACING_MARK, so GB9/GB9a don't fire
- `incb_state` must not be 2, or `curr_cp` must not be InCB consonant (else GB9c fires)

| Test ID | C1 (in_ext_pict) | C2 (prev==ZWJ) | C3 (is_ext_pict) | Decision | Return |
|---------|:---:|:---:|:---:|:---:|:---:|
| GB11_all_T | T | T | T | T | 0 |
| GB11_C1_F | F | T | T | F | (falls through) |
| GB11_C2_F | T | F | T | F | (falls through; but if prev!=ZWJ, GB9 might catch ZWJ as curr; need careful construction) |
| GB11_C3_F | T | T | F | F | (falls through) |

MC/DC pairs:
- **C1**: {GB11_all_T, GB11_C1_F} -- hold prev=ZWJ, curr=ExtPict. Toggle in_ext_pict.
- **C2**: {GB11_all_T, GB11_C2_F} -- hold in_ext_pict=1, curr=ExtPict. Toggle prev between ZWJ and OTHER.
- **C3**: {GB11_all_T, GB11_C3_F} -- hold in_ext_pict=1, prev=ZWJ. Toggle curr between ExtPict and non-ExtPict.

Concrete:
- ExtPict codepoint: U+1F600 (Grinning Face, GCB=OTHER, ExtPict=Yes)
- Non-ExtPict: U+0061 ('a', GCB=OTHER, ExtPict=No)
- ZWJ: U+200D
- For C1 toggle (integration): `U+1F600 U+200D U+1F600` (in_ext_pict=1) vs `U+0061 U+200D U+1F600` (in_ext_pict=0 because 'a' is not ExtPict and the state machine resets in_ext_pict when it sees a non-Extend/non-ZWJ character that is not ExtPict)
- For C2 toggle (direct): `is_grapheme_break(GCB_ZWJ, GCB_OTHER, 0, 1, 0x1F600, 0)` vs `is_grapheme_break(GCB_OTHER, GCB_OTHER, 0, 1, 0x1F600, 0)`. Note: when prev_prop=GCB_OTHER, we must ensure GB3-GB9c don't fire for OTHER+OTHER. They don't: none of the early rules match OTHER+OTHER.
- For C3 toggle (direct): `is_grapheme_break(GCB_ZWJ, GCB_OTHER, 0, 1, 0x1F600, 0)` returns 0 vs `is_grapheme_break(GCB_ZWJ, GCB_OTHER, 0, 1, 0x0061, 0)` returns 1 (GB999).

Integration sequences:
- **C1=T**: `U+1F600 U+FE0F U+200D U+1F600` -- emoji, extend (variation selector), ZWJ, emoji. The state machine sees ExtPict, then Extend (preserves in_ext_pict), then ZWJ (preserves in_ext_pict), then at the final emoji: in_ext_pict=1, prev=ZWJ, curr=ExtPict. GB11 fires. 1 grapheme.
- **C1=F**: `U+0061 U+200D U+1F600` -- 'a', ZWJ, emoji. State machine: in_ext_pict=0 (a is not ExtPict). At boundary ZWJ|emoji: in_ext_pict=0, prev=ZWJ, is_ext_pict(curr)=true. GB11 does not fire. GB999 fires. But actually GB9 fires first at a|ZWJ (ZWJ is GCB_ZWJ, so GB9 catches it: no break). Then at ZWJ|emoji: prev_prop=ZWJ. GB9c check: incb_state=0, no. GB11: in_ext_pict=0, so false. GB12: no. GB999: break. Result: 2 graphemes ["a\u200D", "emoji"].
- **C3=F**: `U+1F600 U+200D U+0061` -- emoji, ZWJ, 'a'. At ZWJ|a: in_ext_pict=1, prev=ZWJ, is_ext_pict('a')=false. GB11 false. GB999: break. Result: 2 graphemes.

### 3.12 GB12/GB13: `prev_prop == GCB_REGIONAL_INDICATOR && curr_prop == GCB_REGIONAL_INDICATOR` then `return (ri_count % 2) == 0`

This rule has a guarding two-condition AND and a separate return expression. The guard determines whether the RI-specific logic is reached. The return expression then determines break vs no-break. We treat this as three conditions for MC/DC purposes.

To reach GB12/13, GB3-GB11 must all be false. RI codepoints have GCB=REGIONAL_INDICATOR, which is not CR/LF/CONTROL/L/V/T/LV/LVT/EXTEND/ZWJ/SPACING_MARK/PREPEND, so GB3-GB9b don't fire. GB9c won't fire because RI codepoints are not InCB consonants. GB11 won't fire because RI is not ZWJ. Safe.

| Test ID | C1 (prev==RI) | C2 (curr==RI) | C3 (ri_count%2==0) | Guard | Return |
|---------|:---:|:---:|:---:|:---:|:---:|
| GB12_guard_T_break | T | T | T (even, e.g. 0) | T | 1 (break) |
| GB12_guard_T_nobreak | T | T | F (odd, e.g. 1) | T | 0 (no break) |
| GB12_C1_F | F | T | -- | F | (falls to GB999=1) |
| GB12_C2_F | T | F | -- | F | (falls to GB999=1) |

MC/DC pairs:
- **C1**: {GB12_guard_T_nobreak (prev=RI, curr=RI, ri_count=1), GB12_C1_F (prev=OTHER, curr=RI, ri_count=1)}. When C1=T, guard passes, return 0. When C1=F, guard fails, fall to GB999 return 1. Outcome flips.
- **C2**: {GB12_guard_T_nobreak, GB12_C2_F (prev=RI, curr=OTHER, ri_count=1)}. Same logic.
- **C3**: {GB12_guard_T_break (ri_count=0), GB12_guard_T_nobreak (ri_count=1)}. Hold prev=RI, curr=RI. Toggle ri_count parity. Return flips between 0 and 1.

Concrete:
- RI: U+1F1E6 (Regional Indicator Symbol Letter A)
- RI2: U+1F1E8 (Regional Indicator Symbol Letter C)
- Integration for C3 toggle:
  - Two RI codepoints: `U+1F1E6 U+1F1E8` -- ri_count starts at 1 (odd). At boundary: no break. 1 grapheme (flag AC).
  - Three RI codepoints: `U+1F1E6 U+1F1E8 U+1F1E6` -- after first pair (no break, ri_count becomes 2). At RI2|RI3: ri_count=2 (even). Break. Result: 2 graphemes.
  - Four RI codepoints: `U+1F1E6 U+1F1E8 U+1F1E6 U+1F1E8` -- ri_count goes 1->2(break)->1->... Wait, the state machine resets on break since `utf8_next_grapheme` returns and starts fresh for the next grapheme. So: grapheme 1 = RI+RI (pair), grapheme 2 = RI+RI (pair). 2 graphemes.

### 3.13 GB999: Default fallthrough

GB999 is not a decision -- it is the unconditional `return 1`. It is reached when no prior rule matches. Any pair of adjacent codepoints that fail all rules will hit GB999. Example: two consecutive ASCII letters `U+0061 U+0062`. Both are GCB_OTHER. No rule matches. Return 1.

This case must appear in the test suite to confirm the default path is exercised, though there are no conditions requiring MC/DC pairs.

Concrete: `is_grapheme_break(GCB_OTHER, GCB_OTHER, 0, 0, 0x0062, 0)` returns 1.

---

## 4. State Coverage

The state machine in `utf8_next_grapheme` (lines 1390-1408) maintains three state variables that are passed to `is_grapheme_break`. MC/DC of `is_grapheme_break` alone is necessary but not sufficient -- we must also cover the state transitions that produce each combination of state variable values.

### 4.1 `ri_count` State Machine

```
ri_count starts at 1 if first codepoint is RI, else 0.
On each non-breaking boundary:
  if curr_prop == RI:        ri_count++
  elif curr_prop != EXTEND && curr_prop != ZWJ:  ri_count = 0
  (else: ri_count unchanged -- Extend/ZWJ preserve it)
```

Required transition sequences:

| Sequence | ri_count values at each boundary | Tests |
|----------|--------------------------------|-------|
| RI, RI | 1 -> no break (odd) | Pair forms flag; 1 grapheme |
| RI, RI, RI | 1 -> no break; 2 -> break (even) | Two graphemes: [RI,RI] [RI] |
| RI, RI, RI, RI | 1 -> no break; 2 -> break; 1 -> no break | Two graphemes: [RI,RI] [RI,RI] |
| RI, Extend, RI | 1 -> (Extend: ri_count stays 1) -> no break | Extend between RI pair preserves count |
| RI, OTHER, RI | 1 -> (OTHER: ri_count resets to 0) -> break? | Actually at RI|OTHER: RI is prev, OTHER is curr. GB12 guard: prev=RI, curr=OTHER -> false. Falls to GB999: break. New grapheme starts with OTHER. |

Concrete:
- `U+1F1FA U+1F1F8` (US flag): 1 grapheme
- `U+1F1FA U+1F1F8 U+1F1EC` (US + orphan G): 2 graphemes
- `U+1F1FA U+1F1F8 U+1F1EC U+1F1E7` (US + GB flags): 2 graphemes

### 4.2 `in_ext_pict` State Machine

```
in_ext_pict starts at 1 if first codepoint is ExtPict, else 0.
On each non-breaking boundary:
  if is_extended_pictographic(curr_cp):   in_ext_pict = 1
  elif curr_prop != EXTEND && curr_prop != ZWJ:  in_ext_pict = 0
  (else: in_ext_pict unchanged -- Extend/ZWJ preserve it)
```

Required transition sequences:

| Sequence | in_ext_pict at each step | Purpose |
|----------|------------------------|---------|
| ExtPict, Extend, ZWJ, ExtPict | 1 -> 1 (Extend preserves) -> 1 (ZWJ preserves) -> GB11 fires | Normal ZWJ emoji sequence |
| ExtPict, ZWJ, ExtPict | 1 -> 1 (ZWJ preserves) -> GB11 fires | Minimal GB11 trigger |
| ExtPict, OTHER, ZWJ, ExtPict | 1 -> 0 (OTHER resets) -> 0 (ZWJ preserves 0) -> GB11 does NOT fire | Intervening non-Extend/ZWJ breaks the chain |
| OTHER, ZWJ, ExtPict | 0 -> 0 (ZWJ preserves 0) -> GB11 does NOT fire | No preceding ExtPict |
| ExtPict, Extend, Extend, ZWJ, ExtPict | 1 -> 1 -> 1 -> 1 -> GB11 fires | Multiple extends between ExtPict and ZWJ |

Concrete:
- `U+1F468 U+200D U+1F469` (man ZWJ woman): 1 grapheme (GB11)
- `U+1F468 U+0061 U+200D U+1F469`: 'a' resets in_ext_pict. After a|ZWJ (GB9 no break): in_ext_pict=0. At ZWJ|woman: GB11 fails (in_ext_pict=0). 3 graphemes: [man] [a+ZWJ] [woman]. Actually: man|a: GB999 break. a|ZWJ: GB9 no break. ZWJ|woman: in_ext_pict=0 (reset by 'a'), GB11 false, GB999 break. 3 graphemes.
- `U+0061 U+200D U+1F600`: in_ext_pict stays 0. GB11 does not fire. 2 graphemes.

### 4.3 `incb_state` State Machine

```
incb_state starts at 1 if first codepoint is InCB Consonant, else 0.
On each non-breaking boundary:
  if is_incb_consonant(curr_cp):               incb_state = 1
  elif is_incb_linker(curr_cp) && incb_state >= 1:  incb_state = 2
  elif curr_prop != EXTEND && curr_prop != ZWJ:      incb_state = 0
  (else: incb_state unchanged -- Extend/ZWJ preserve it)
```

State transitions: 0 -> 1 (consonant), 1 -> 2 (linker), 2 -> check at next consonant (GB9c fires). Also: 1 -> 0 (non-Extend/ZWJ/non-linker), 2 -> 0 (non-Extend/ZWJ/non-consonant), 2 -> 1 (consonant resets to 1).

Required transition sequences:

| Sequence | incb_state path | GB9c fires? |
|----------|----------------|-------------|
| Consonant, Linker, Consonant | 1 -> 2 (linker) -> GB9c fires (state=2, curr=consonant) | Yes: no break |
| Consonant, Linker, Extend, Consonant | 1 -> 2 -> 2 (Extend preserves) -> GB9c fires | Yes: Extend between linker and consonant |
| Consonant, Linker, OTHER | 1 -> 2 -> 0 (OTHER resets) | No: state reset |
| Consonant, OTHER, Linker, Consonant | 1 -> 0 (OTHER resets) -> 0 (linker but state=0, so no advance) -> ... | No: state was reset before linker |
| OTHER, Linker, Consonant | 0 -> 0 (linker but state=0 < 1) -> ... | No: never reached state 1 |
| Consonant, Linker, Linker, Consonant | 1 -> 2 -> 2 (linker at state>=1, stays 2) -> GB9c fires | Yes: double linker |

Important: The Virama (U+094D) has GCB=Extend. So at Consonant|Virama, GB9 fires (curr=EXTEND, no break). The state machine then sets incb_state=2 (linker at state>=1). At Virama|Consonant, prev_prop=EXTEND, curr_prop=OTHER (consonant has GCB=OTHER typically -- but we should verify). If the consonant is GCB_OTHER, GB9c checks: incb_state=2, is_incb_consonant(curr)=true. Returns 0.

Actually, we must verify that Devanagari consonants have GCB=Extend or GCB=OTHER. Let me note: U+0915 (Devanagari KA) has GCB=Other in Unicode. U+094D (Devanagari Virama) has GCB=Extend. So the sequence U+0915 U+094D U+0915:
1. Start: prev=U+0915 (GCB_OTHER), incb_state=1 (is consonant)
2. Boundary 0915|094D: prev=OTHER, curr=EXTEND. GB9 fires: no break. State: incb_state -> is_incb_linker(094D)=true, incb_state>=1 -> incb_state=2.
3. Boundary 094D|0915: prev=EXTEND, curr=OTHER. GB3-GB9b all false (EXTEND is not PREPEND). GB9c: incb_state=2, is_incb_consonant(0915)=true -> return 0 (no break).
Result: 1 grapheme for the entire conjunct.

Concrete sequences:
- `U+0915 U+094D U+0915` (KA + Virama + KA): 1 grapheme (GB9c joins)
- `U+0915 U+094D U+0061` (KA + Virama + 'a'): GB9c check at Virama|a: incb_state=2, is_incb_consonant('a')=false. Falls through. GB999: break. 2 graphemes.
- `U+0061 U+094D U+0915` (a + Virama + KA): incb_state starts at 0 ('a' not consonant). At a|Virama: GB9 no break. State: is_incb_linker(094D)=true but incb_state=0 < 1, so incb_state stays 0 (falls to Extend check: 094D is GCB_EXTEND, so state unchanged at 0). At Virama|KA: incb_state=0. GB9c: false. GB999: break. 2 graphemes.

### 4.4 Cross-State Interactions

Test sequences where multiple state variables are active simultaneously:

| Scenario | Sequence | Expected behavior |
|----------|----------|-------------------|
| ExtPict with Extend, then InCB | `U+1F600 U+0300 U+0915 U+094D U+0915` | in_ext_pict set by emoji, then 0300 (Extend) preserves it. Then 0915 resets in_ext_pict=0 (not ExtPict, not Extend/ZWJ). incb_state=1. Normal InCB sequence follows. |
| RI followed by InCB | `U+1F1E6 U+1F1E8 U+0915 U+094D U+0915` | Flag grapheme + conjunct grapheme. ri_count resets when non-RI seen. |
| GB11 near GB9c | `U+1F600 U+200D U+0915` | in_ext_pict=1, prev=ZWJ at boundary. GB11: is_extended_pictographic(0x0915)=false. Falls through. GB9c: incb_state=0 (emoji is not consonant). GB999: break. |

---

## 5. Implementation Approach

### 5.1 File Structure

```
test/
  test_gstr.c                  # Existing functional tests (unchanged)
  test_grapheme_walk.c         # Existing debugging/walk tool (unchanged)
  test_gstr_stress.c           # Existing stress tests (unchanged)
  test_mcdc_grapheme_break.c   # NEW: MC/DC tests for is_grapheme_break
```

The new file `test_mcdc_grapheme_break.c` will contain two layers of tests:

1. **Direct-call tests**: Call `is_grapheme_break` directly with explicit parameter values. This gives precise control over all six parameters and allows isolating individual conditions without worrying about the state machine.

2. **Integration tests**: Call `utf8_next_grapheme` / `gstrlen` with UTF-8 byte sequences. These exercise the state machine and verify that the state variables (`ri_count`, `in_ext_pict`, `incb_state`) are correctly computed and passed to `is_grapheme_break`.

### 5.2 Test Macros

Reuse the existing macro framework from `test_gstr.c`:

```c
/* From existing pattern: */
#define TEST(name) static void test_##name(void)
#define RUN(name) ...
#define ASSERT(cond) ...
#define ASSERT_EQ(a, b) ...
```

Add MC/DC-specific macros:

```c
/* Assert that is_grapheme_break returns the expected value */
#define ASSERT_BREAK(prev_p, curr_p, ri, ext, cp, incb, expected) ...

/* Assert grapheme count for a UTF-8 string */
#define ASSERT_GRAPHEME_COUNT(str, expected_count) ...

/* Document an MC/DC pair: rule, condition, and the two test IDs */
#define MCDC_PAIR(rule, cond, test_true, test_false) ...
```

The `MCDC_PAIR` macro would be a documentation/logging macro that prints which MC/DC pair is being exercised, making test output self-documenting for coverage audits.

### 5.3 Test Organization

Group tests by rule, with each rule section containing:
1. A comment block listing the decision, conditions, and MC/DC pair plan.
2. Individual `TEST()` functions for each test vector.
3. A section in `main()` that runs them grouped under a header.

Example structure in `main()`:
```
MC/DC: GB3 (CR x LF)
MC/DC: GB4 (Control|CR|LF ÷)
MC/DC: GB5 (÷ Control|CR|LF)
...
MC/DC: State - ri_count transitions
MC/DC: State - in_ext_pict transitions
MC/DC: State - incb_state transitions
MC/DC: State - cross-state interactions
```

### 5.4 Coverage Verification

#### Build with coverage instrumentation

Add a Makefile target:

```makefile
CFLAGS_COV = -Wall -Wextra -pedantic -std=c17 -g -O0 \
             --coverage -fprofile-arcs -ftest-coverage

mcdc-coverage: $(TESTDIR)/test_mcdc_grapheme_break.c $(HEADER)
	$(CC) $(CFLAGS_COV) $(VERSION_FLAGS) -I$(INCDIR) $< \
	    -o $(TESTDIR)/test_mcdc_grapheme_break
	./$(TESTDIR)/test_mcdc_grapheme_break
	llvm-cov gcov -b -c $(TESTDIR)/test_mcdc_grapheme_break-gstr.h.gcda
```

Since `gstr.h` is a header-only library, the coverage data for `is_grapheme_break` will be in the `.gcda` file generated for the test translation unit. The `-b` flag to `gcov` reports branch coverage, which is the closest standard proxy for MC/DC (true MC/DC instrumentation requires `clang -fcs-profile-generate` or specialized tools).

#### Coverage targets

| Metric | Target | How to verify |
|--------|--------|---------------|
| Line coverage of `is_grapheme_break` | 100% | Every line 1091-1161 executed at least once |
| Branch coverage of `is_grapheme_break` | 100% | Every branch (true/false) of every if-statement taken |
| MC/DC condition pairs | All 30 conditions have documented pairs | Manual audit of test matrix vs test code |
| State transition coverage | All transitions in Section 4 exercised | Integration tests with assertions |

#### Clang Source-Based Coverage (preferred)

For more precise reporting:

```makefile
mcdc-coverage-clang: $(TESTDIR)/test_mcdc_grapheme_break.c $(HEADER)
	clang -fprofile-instr-generate -fcoverage-mapping \
	    $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< \
	    -o $(TESTDIR)/test_mcdc_grapheme_break
	LLVM_PROFILE_FILE=mcdc.profraw ./$(TESTDIR)/test_mcdc_grapheme_break
	llvm-profdata merge -sparse mcdc.profraw -o mcdc.profdata
	llvm-cov show $(TESTDIR)/test_mcdc_grapheme_break \
	    -instr-profile=mcdc.profdata \
	    -show-branches=count \
	    -show-mcdc \
	    -name=is_grapheme_break
```

Note: `llvm-cov show -show-mcdc` requires Clang 18+ which added native MC/DC instrumentation support via `-fcoverage-mcdc`. If available, add `-fcoverage-mcdc` to the compilation flags for hardware-level MC/DC tracking.

### 5.5 Pass/Fail Criteria

1. **All tests pass**: Zero test failures in `test_mcdc_grapheme_break`.
2. **100% line coverage** of `is_grapheme_break` (lines 1091-1161).
3. **100% branch coverage** of `is_grapheme_break` -- every if-statement has been evaluated to both true and false.
4. **MC/DC pair completeness**: For each of the 30 atomic conditions listed in Section 2, the test file contains an explicit pair of test cases demonstrating independent effect, with a comment citing the pair.
5. **State coverage completeness**: Every state transition listed in Section 4 is exercised by at least one integration test.
6. **No regressions**: The existing `test_gstr` and `test_gstr_stress` suites continue to pass.

---

## 6. Integration with Existing Tests

### 6.1 Relationship to `test_gstr.c`

The existing test file (`/home/edward/repos/gstr/test/test_gstr.c`) tests the public API surface: `gstrlen`, `gstroff`, `gstrat`, `gstrcmp`, `gstrstr`, etc. These are black-box functional tests. The MC/DC tests are white-box structural tests targeting `is_grapheme_break` internals. They are complementary:

- `test_gstr.c` verifies **what** the library does (correct grapheme counts, correct indexing).
- `test_mcdc_grapheme_break.c` verifies **how** `is_grapheme_break` reaches its decisions (every condition path).

### 6.2 Relationship to `test_grapheme_walk.c`

The walk test (`/home/edward/repos/gstr/test/test_grapheme_walk.c`) is a diagnostic tool that prints grapheme boundaries for manual inspection. It includes some assertions (ExtPict split tests) but is primarily exploratory. The MC/DC suite supersedes its coverage role but does not replace it -- the walk test remains useful for debugging.

### 6.3 Relationship to `test_gstr_stress.c`

The stress test exercises the library with large/adversarial inputs for robustness. MC/DC tests use minimal, targeted inputs. No overlap in purpose.

### 6.4 Makefile Integration

Add to the existing Makefile:

```makefile
test-mcdc: $(TESTDIR)/test_mcdc_grapheme_break.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< \
	    -o $(TESTDIR)/test_mcdc_grapheme_break
	./$(TESTDIR)/test_mcdc_grapheme_break

test-all: test test-mcdc
```

The `test` target continues to build and run `test_gstr.c`. The `test-mcdc` target builds and runs the MC/DC suite. A `test-all` target runs both.

### 6.5 No Modifications to Existing Files

The MC/DC test suite is a new file that `#include`s `<gstr.h>` directly. Since `is_grapheme_break` is a `static` function defined in the header, it is available in the test translation unit without any source modifications. The `enum gcb_property` values, `is_extended_pictographic`, `is_incb_consonant`, `is_incb_linker`, and `get_gcb` are all similarly available.

---

## 7. GraphemeBreakTest.txt Conformance

### 7.1 Purpose

The Unicode Consortium publishes `GraphemeBreakTest.txt` as part of each Unicode release (available at `https://www.unicode.org/Public/UCD/latest/ucd/auxiliary/GraphemeBreakTest.txt`). This file contains thousands of test vectors specifying exact grapheme cluster boundaries for various codepoint sequences. It serves as the ground-truth conformance suite.

### 7.2 Relationship to MC/DC

GraphemeBreakTest.txt provides **specification-based** (black-box) coverage. MC/DC provides **implementation-based** (white-box) coverage. They serve different purposes:

- GraphemeBreakTest.txt catches cases where the implementation disagrees with the spec.
- MC/DC catches cases where code paths are unreachable or untested.

Both are needed. The GraphemeBreakTest.txt suite should be the **baseline** that runs first. The MC/DC suite then provides assurance that all code paths within `is_grapheme_break` are exercised.

### 7.3 Integration Approach

Add a conformance test that parses `GraphemeBreakTest.txt` at runtime or compile time:

**Option A: Runtime parser** (recommended for maintainability)

Ship `GraphemeBreakTest.txt` in the `test/` directory (or download it as part of the build). Write a test function that:
1. Opens and parses the file line by line.
2. For each line, extracts the codepoint sequence and expected break/no-break markers.
3. Encodes the codepoints as UTF-8.
4. Walks the string with `utf8_next_grapheme` and compares the resulting boundaries against the expected ones.
5. Reports any mismatches with the line number from the test file.

**Option B: Code-generated test vectors**

A script (Python or shell) reads `GraphemeBreakTest.txt` and generates a C array of test structs:

```c
struct grapheme_break_test {
    uint32_t codepoints[MAX_CPS];
    int cp_count;
    int breaks[MAX_CPS];  /* 1 = break before this cp, 0 = no break */
};
```

This array is included in the test file. A loop iterates over it, encodes each sequence as UTF-8, and verifies boundaries.

**Option A is preferred** because it avoids regeneration when updating Unicode versions -- simply replace the text file.

### 7.4 Test File Format

Each non-comment line in `GraphemeBreakTest.txt` looks like:

```
÷ 0020 ÷ 0020 ÷	# ÷ [0.2] SPACE (Other) ÷ [999.0] SPACE (Other) ÷ [0.2]
÷ 000D × 000A ÷	# ÷ [0.2] <CARRIAGE RETURN (CR)> (CR) × [3.0] <LINE FEED (LF)> (LF) ÷ [0.2]
```

Where `÷` means break, `×` means no-break, and hex values are codepoints. The parser must:
1. Skip lines starting with `#`.
2. Split on whitespace, recognizing `÷` (UTF-8: `\xC3\xB7`) and `×` (`\xC3\x97`) as delimiters.
3. Parse hex codepoint values between delimiters.

### 7.5 Conformance as CI Gate

The conformance test should be a hard pass/fail gate. Any failure indicates a bug in the grapheme break implementation. The Makefile target:

```makefile
test-conformance: $(TESTDIR)/test_grapheme_conformance.c $(HEADER) $(TESTDIR)/GraphemeBreakTest.txt
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< \
	    -o $(TESTDIR)/test_grapheme_conformance
	./$(TESTDIR)/test_grapheme_conformance $(TESTDIR)/GraphemeBreakTest.txt
```

### 7.6 Version Tracking

The test should print which Unicode version's test file it is running against (parsed from the file header comment). The gstr library should document which Unicode version it targets. A mismatch should produce a warning, not a failure, since the library may intentionally lag behind the latest Unicode release.

---

## Appendix A: Complete MC/DC Pair Summary

| Rule | Cond | Pair A (condition TRUE, toggles outcome) | Pair B (condition FALSE, toggles outcome) | Minimum test count |
|------|------|----------------------------------------|------------------------------------------|--------------------|
| GB3 | C1 | prev=CR, curr=LF -> 0 | prev=OTHER, curr=LF -> 1 | 2 |
| GB3 | C2 | prev=CR, curr=LF -> 0 | prev=CR, curr=OTHER -> 1 | (shares tests) |
| GB4 | C1 | prev=CONTROL, curr=OTHER -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | 2 |
| GB4 | C2 | prev=CR, curr=OTHER -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | (shares false case) |
| GB4 | C3 | prev=LF, curr=OTHER -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | (shares false case) |
| GB5 | C1 | prev=OTHER, curr=CONTROL -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | 2 |
| GB5 | C2 | prev=OTHER, curr=CR -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | (shares false case) |
| GB5 | C3 | prev=OTHER, curr=LF -> 1 | prev=OTHER, curr=OTHER -> 1(GB999) | (shares false case) |
| GB6 | C1 | prev=L, curr=L -> 0 | prev=OTHER, curr=L -> 1 | 2 |
| GB6 | C2 | prev=L, curr=L -> 0 | prev=L, curr=T -> 1 | 2 |
| GB6 | C3 | prev=L, curr=V -> 0 | prev=L, curr=T -> 1 | (shares false) |
| GB6 | C4 | prev=L, curr=LV -> 0 | prev=L, curr=T -> 1 | (shares false) |
| GB6 | C5 | prev=L, curr=LVT -> 0 | prev=L, curr=T -> 1 | (shares false) |
| GB7 | C1 | prev=LV, curr=V -> 0 | prev=OTHER, curr=V -> 1 | 2 |
| GB7 | C2 | prev=V, curr=V -> 0 | prev=OTHER, curr=V -> 1 | (shares false) |
| GB7 | C3 | prev=LV, curr=V -> 0 | prev=LV, curr=OTHER -> 1 | 2 |
| GB7 | C4 | prev=LV, curr=T -> 0 | prev=LV, curr=OTHER -> 1 | (shares false) |
| GB8 | C1 | prev=LVT, curr=T -> 0 | prev=OTHER, curr=T -> 1 | 2 |
| GB8 | C2 | prev=T, curr=T -> 0 | prev=OTHER, curr=T -> 1 | (shares false) |
| GB8 | C3 | prev=LVT, curr=T -> 0 | prev=LVT, curr=OTHER -> 1 | 2 |
| GB9 | C1 | prev=OTHER, curr=EXTEND -> 0 | prev=OTHER, curr=OTHER -> 1 | 2 |
| GB9 | C2 | prev=OTHER, curr=ZWJ -> 0 | prev=OTHER, curr=OTHER -> 1 | (shares false) |
| GB9a | C1 | prev=OTHER, curr=SPACING_MARK -> 0 | prev=OTHER, curr=OTHER -> 1 | 2 |
| GB9b | C1 | prev=PREPEND, curr=OTHER -> 0 | prev=OTHER, curr=OTHER -> 1 | 2 |
| GB9c | C1 | incb_state=2, curr=consonant -> 0 | incb_state=0, curr=consonant -> 1 | 2 |
| GB9c | C2 | incb_state=2, curr=consonant -> 0 | incb_state=2, curr=non-consonant -> 1 | 2 |
| GB11 | C1 | ext=1, prev=ZWJ, curr=ExtPict -> 0 | ext=0, prev=ZWJ, curr=ExtPict -> 1 | 2 |
| GB11 | C2 | ext=1, prev=ZWJ, curr=ExtPict -> 0 | ext=1, prev=OTHER, curr=ExtPict -> 1 | 2 |
| GB11 | C3 | ext=1, prev=ZWJ, curr=ExtPict -> 0 | ext=1, prev=ZWJ, curr=non-ExtPict -> 1 | 2 |
| GB12 | C1 | prev=RI, curr=RI, ri=1 -> 0 | prev=OTHER, curr=RI, ri=1 -> 1 | 2 |
| GB12 | C2 | prev=RI, curr=RI, ri=1 -> 0 | prev=RI, curr=OTHER, ri=1 -> 1 | 2 |
| GB12 | C3 | prev=RI, curr=RI, ri=0 -> 1 | prev=RI, curr=RI, ri=1 -> 0 | 2 |
| GB999 | -- | (unconditional fallthrough) | -- | 1 |

**Note on GB4/GB5 MC/DC validity**: For GB4 and GB5, both the true and false cases of the overall decision produce `return 1` (GB4/GB5 return 1; the false path eventually reaches GB999 which also returns 1 when all other conditions are OTHER+OTHER). This means the decision outcome (break=1) does not change. Strictly speaking, MC/DC for a decision that always leads to the same program outcome is trivially satisfied. However, the decision itself has two outcomes (true=enters the if-body, false=falls through), and the function's return value from the if-body is 1 vs the return value from GB999 is also 1. For a rigorous MC/DC interpretation, we document that the decision is reached and both truth values are taken, even though the return value happens to be the same. The critical structural coverage is that all branches of the if-statement are taken.

**Total minimum direct-call test cases**: ~35 (with sharing of false-case tests across conditions in the same disjunction).

**Total minimum integration test cases**: ~15 (state machine sequences from Section 4).

**Grand total: approximately 50 test cases.**

---

## Appendix B: Codepoint Reference Table

| Symbol | Codepoint | UTF-8 Bytes | GCB Property | Notes |
|--------|-----------|-------------|-------------|-------|
| CR | U+000D | `\x0D` | GCB_CR | |
| LF | U+000A | `\x0A` | GCB_LF | |
| NUL | U+0000 | `\x00` | GCB_CONTROL | |
| BEL | U+0007 | `\x07` | GCB_CONTROL | |
| 'a' | U+0061 | `\x61` | GCB_OTHER | |
| Combining Grave | U+0300 | `\xCC\x80` | GCB_EXTEND | |
| Devanagari Visarga | U+0903 | `\xE0\xA4\x83` | GCB_SPACING_MARK | |
| ZWJ | U+200D | `\xE2\x80\x8D` | GCB_ZWJ | |
| Arabic Num Sign | U+0600 | `\xD8\x80` | GCB_PREPEND | |
| Hangul L (Kiyeok) | U+1100 | `\xE1\x84\x80` | GCB_L | |
| Hangul V (A) | U+1161 | `\xE1\x85\xA1` | GCB_V | |
| Hangul T (Kiyeok) | U+11A8 | `\xE1\x86\xA8` | GCB_T | |
| Hangul LV (GA) | U+AC00 | `\xEA\xB0\x80` | GCB_LV | (AC00-AC00)%28==0 |
| Hangul LVT (GAG) | U+AC01 | `\xEA\xB0\x81` | GCB_LVT | (AC01-AC00)%28==1 |
| RI Letter A | U+1F1E6 | `\xF0\x9F\x87\xA6` | GCB_REGIONAL_INDICATOR | |
| RI Letter C | U+1F1E8 | `\xF0\x9F\x87\xA8` | GCB_REGIONAL_INDICATOR | |
| RI Letter U | U+1F1FA | `\xF0\x9F\x87\xBA` | GCB_REGIONAL_INDICATOR | |
| RI Letter S | U+1F1F8 | `\xF0\x9F\x87\xB8` | GCB_REGIONAL_INDICATOR | |
| Grinning Face | U+1F600 | `\xF0\x9F\x98\x80` | GCB_OTHER | ExtPict=Yes |
| Man | U+1F468 | `\xF0\x9F\x91\xA8` | GCB_OTHER | ExtPict=Yes |
| Woman | U+1F469 | `\xF0\x9F\x91\xA9` | GCB_OTHER | ExtPict=Yes |
| Black Spade | U+2660 | `\xE2\x99\xA0` | GCB_OTHER | ExtPict=Yes |
| Black Star | U+2605 | `\xE2\x98\x85` | GCB_OTHER | ExtPict=No |
| Devanagari KA | U+0915 | `\xE0\xA4\x95` | GCB_OTHER | InCB Consonant |
| Devanagari Virama | U+094D | `\xE0\xA5\x8D` | GCB_EXTEND | InCB Linker |
| VS-16 | U+FE0F | `\xEF\xB8\x8F` | GCB_EXTEND | Variation Selector |