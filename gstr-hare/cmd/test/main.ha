// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
//
// Comprehensive test program for gstr Hare implementation

use fmt;
use gstr;

export fn main() void = {
	fmt::println("gstr-hare comprehensive test suite")!;
	fmt::println("===================================")!;
	fmt::println("")!;
	
	let passed: size = 0;
	let failed: size = 0;
	
	// =========================================================================
	// Basic Length Tests
	// =========================================================================
	fmt::println("=== Basic Length (gstrlen) ===")!;
	
	if (test_len("", 0)) passed += 1 else failed += 1;
	if (test_len("a", 1)) passed += 1 else failed += 1;
	if (test_len("hello", 5)) passed += 1 else failed += 1;
	if (test_len("Hello World", 11)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Latin Extended (2 bytes, width 1)
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Latin Extended ===")!;
	
	if (test_len("café", 4)) passed += 1 else failed += 1;  // precomposed é
	if (test_len("naïve", 5)) passed += 1 else failed += 1;
	if (test_len("résumé", 6)) passed += 1 else failed += 1;
	
	// Decomposed vs Precomposed
	// é precomposed (U+00E9) = 1 grapheme
	if (test_len("\xC3\xA9", 1)) passed += 1 else failed += 1;
	// e + combining acute (U+0065 U+0301) = 1 grapheme (combines)
	if (test_len("e\xCC\x81", 1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// CJK Characters (3 bytes, width 2 each)
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== CJK Characters ===")!;
	
	if (test_len("中", 1)) passed += 1 else failed += 1;
	if (test_len("中文", 2)) passed += 1 else failed += 1;
	if (test_len("日本語", 3)) passed += 1 else failed += 1;
	if (test_len("世界", 2)) passed += 1 else failed += 1;
	
	if (test_width("中", 2)) passed += 1 else failed += 1;
	if (test_width("中文", 4)) passed += 1 else failed += 1;
	if (test_width("日本語", 6)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Hangul
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Hangul ===")!;
	
	// Precomposed Hangul syllables
	if (test_len("한글", 2)) passed += 1 else failed += 1;
	if (test_len("가", 1)) passed += 1 else failed += 1;  // precomposed GA
	if (test_width("한글", 4)) passed += 1 else failed += 1;
	
	// Hangul Jamo (should cluster: L + V = 1 grapheme)
	// ㄱ (U+1100) + ㅏ (U+1161) = 가
	if (test_len("\xE1\x84\x80\xE1\x85\xA1", 1)) passed += 1 else failed += 1;
	
	// L + V + T = 1 grapheme: ㄱ + ㅏ + ㄴ = 간
	if (test_len("\xE1\x84\x80\xE1\x85\xA1\xE1\x86\xAB", 1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Simple Emoji (4 bytes, width 2)
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Simple Emoji ===")!;
	
	if (test_len("😀", 1)) passed += 1 else failed += 1;
	if (test_len("🎉", 1)) passed += 1 else failed += 1;
	if (test_len("👋", 1)) passed += 1 else failed += 1;
	if (test_width("😀", 2)) passed += 1 else failed += 1;
	
	// Heart with variation selector (2 codepoints, 1 grapheme)
	// ❤️ = ❤ (U+2764) + VS16 (U+FE0F)
	if (test_len("\xE2\x9D\xA4\xEF\xB8\x8F", 1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Flags (Regional Indicators)
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Flags ===")!;
	
	// 🇨🇦 Canada = Regional Indicator C + A
	if (test_len("🇨🇦", 1)) passed += 1 else failed += 1;
	if (test_len("🇺🇸", 1)) passed += 1 else failed += 1;
	if (test_len("🇯🇵", 1)) passed += 1 else failed += 1;
	if (test_len("🇬🇧", 1)) passed += 1 else failed += 1;
	if (test_len("🇫🇷", 1)) passed += 1 else failed += 1;
	
	// Multiple flags
	if (test_len("🇨🇦🇺🇸", 2)) passed += 1 else failed += 1;
	if (test_len("🇨🇦🇺🇸🇯🇵", 3)) passed += 1 else failed += 1;
	
	if (test_width("🇨🇦", 2)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Skin Tone Modifiers
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Skin Tone Modifiers ===")!;
	
	// 👋🏻 light skin
	if (test_len("👋🏻", 1)) passed += 1 else failed += 1;
	// 👋🏽 medium skin
	if (test_len("👋🏽", 1)) passed += 1 else failed += 1;
	// 👋🏿 dark skin
	if (test_len("👋🏿", 1)) passed += 1 else failed += 1;
	
	// All five skin tones on same base
	if (test_len("👋🏻👋🏼👋🏽👋🏾👋🏿", 5)) passed += 1 else failed += 1;
	
	// =========================================================================
	// ZWJ Sequences
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== ZWJ Sequences ===")!;
	
	// 👨‍👩‍👧 family (man + ZWJ + woman + ZWJ + girl)
	if (test_len("👨‍👩‍👧", 1)) passed += 1 else failed += 1;
	
	// 👨‍👩‍👧‍👦 family of four
	if (test_len("👨‍👩‍👧‍👦", 1)) passed += 1 else failed += 1;
	
	// 👨‍💻 man technologist
	if (test_len("👨‍💻", 1)) passed += 1 else failed += 1;
	
	// 👩‍🔬 woman scientist
	if (test_len("👩‍🔬", 1)) passed += 1 else failed += 1;
	
	// 🏳️‍🌈 rainbow flag
	if (test_len("🏳️‍🌈", 1)) passed += 1 else failed += 1;
	
	// 🏳️‍⚧️ transgender flag
	if (test_len("🏳️‍⚧️", 1)) passed += 1 else failed += 1;
	
	// ZWJ sequence with skin tone: 👨🏽‍💻 man technologist medium skin
	if (test_len("👨🏽‍💻", 1)) passed += 1 else failed += 1;
	
	if (test_width("👨‍👩‍👧", 2)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Combining Marks
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Combining Marks ===")!;
	
	// a + combining acute (U+0301)
	if (test_len("a\xCC\x81", 1)) passed += 1 else failed += 1;
	
	// o + combining diaeresis (U+0308)
	if (test_len("o\xCC\x88", 1)) passed += 1 else failed += 1;
	
	// s + dot below (U+0323) + dot above (U+0307) = ṩ
	if (test_len("s\xCC\xA3\xCC\x87", 1)) passed += 1 else failed += 1;
	
	// Multiple combining marks
	if (test_len("a\xCC\x81\xCC\x82\xCC\x83", 1)) passed += 1 else failed += 1;  // a + acute + circumflex + tilde
	
	// =========================================================================
	// Zero-Width Characters
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Zero-Width Characters ===")!;
	
	// Zero-width space (U+200B)
	if (test_len("\xE2\x80\x8B", 1)) passed += 1 else failed += 1;
	if (test_width("\xE2\x80\x8B", 0)) passed += 1 else failed += 1;
	
	// Word joiner (U+2060)
	if (test_len("\xE2\x81\xA0", 1)) passed += 1 else failed += 1;
	if (test_width("\xE2\x81\xA0", 0)) passed += 1 else failed += 1;
	
	// ZWJ alone (U+200D)
	if (test_len("\xE2\x80\x8D", 1)) passed += 1 else failed += 1;
	if (test_width("\xE2\x80\x8D", 0)) passed += 1 else failed += 1;
	
	// ZWNJ alone (U+200C)
	if (test_len("\xE2\x80\x8C", 1)) passed += 1 else failed += 1;
	if (test_width("\xE2\x80\x8C", 0)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Keycap Sequences
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Keycap Sequences ===")!;
	
	// 1️⃣ = 1 + VS16 + combining enclosing keycap
	if (test_len("1️⃣", 1)) passed += 1 else failed += 1;
	if (test_len("2️⃣", 1)) passed += 1 else failed += 1;
	if (test_len("#️⃣", 1)) passed += 1 else failed += 1;
	if (test_len("*️⃣", 1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Mixed Strings
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Mixed Strings ===")!;
	
	// Hi🇨🇦中👨‍👩‍👧!
	if (test_len("Hi🇨🇦中👨‍👩‍👧!", 6)) passed += 1 else failed += 1;
	// Width: H(1) i(1) 🇨🇦(2) 中(2) 👨‍👩‍👧(2) !(1) = 9
	if (test_width("Hi🇨🇦中👨‍👩‍👧!", 9)) passed += 1 else failed += 1;
	
	// A中🇯🇵👨‍💻é
	if (test_len("A中🇯🇵👨‍💻é", 5)) passed += 1 else failed += 1;
	// Width: A(1) 中(2) 🇯🇵(2) 👨‍💻(2) é(1) = 8
	if (test_width("A中🇯🇵👨‍💻é", 8)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Edge Cases
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Edge Cases ===")!;
	
	// Fullwidth space (U+3000) - width 2
	if (test_len("\xE3\x80\x80", 1)) passed += 1 else failed += 1;
	if (test_width("\xE3\x80\x80", 2)) passed += 1 else failed += 1;
	
	// Only combining marks (orphaned) - should still count as graphemes
	if (test_len("\xCC\x81", 1)) passed += 1 else failed += 1;  // lone combining acute
	
	// Very long combining sequence
	if (test_len("a\xCC\x81\xCC\x82\xCC\x83\xCC\x84\xCC\x85", 1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// gstrat Tests
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Character Access (gstrat) ===")!;
	
	if (test_at("hello", 0, "h")) passed += 1 else failed += 1;
	if (test_at("hello", 4, "o")) passed += 1 else failed += 1;
	if (test_at("café", 3, "é")) passed += 1 else failed += 1;
	if (test_at("世界", 0, "世")) passed += 1 else failed += 1;
	if (test_at("世界", 1, "界")) passed += 1 else failed += 1;
	
	// Emoji access
	if (test_at("👋🏽", 0, "👋🏽")) passed += 1 else failed += 1;
	if (test_at("🇨🇦🇺🇸", 0, "🇨🇦")) passed += 1 else failed += 1;
	if (test_at("🇨🇦🇺🇸", 1, "🇺🇸")) passed += 1 else failed += 1;
	
	// Family
	if (test_at("A👨‍👩‍👧B", 0, "A")) passed += 1 else failed += 1;
	if (test_at("A👨‍👩‍👧B", 1, "👨‍👩‍👧")) passed += 1 else failed += 1;
	if (test_at("A👨‍👩‍👧B", 2, "B")) passed += 1 else failed += 1;
	
	// Out of bounds
	if (test_at_void("hello", 10)) passed += 1 else failed += 1;
	if (test_at_void("", 0)) passed += 1 else failed += 1;
	
	// =========================================================================
	// gstrcmp Tests
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Comparison (gstrcmp) ===")!;
	
	if (test_cmp("abc", "abc", 0)) passed += 1 else failed += 1;
	if (test_cmp("abc", "abd", -1)) passed += 1 else failed += 1;
	if (test_cmp("abd", "abc", 1)) passed += 1 else failed += 1;
	if (test_cmp("ab", "abc", -1)) passed += 1 else failed += 1;
	if (test_cmp("abc", "ab", 1)) passed += 1 else failed += 1;
	if (test_cmp("", "", 0)) passed += 1 else failed += 1;
	if (test_cmp("a", "", 1)) passed += 1 else failed += 1;
	if (test_cmp("", "a", -1)) passed += 1 else failed += 1;
	
	// Unicode comparison
	if (test_cmp("世界", "世界", 0)) passed += 1 else failed += 1;
	if (test_cmp("👋", "👋", 0)) passed += 1 else failed += 1;
	if (test_cmp("🇨🇦", "🇺🇸", -1)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Iterator Tests
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Iterator ===")!;
	
	if (test_iter("abc", ["a", "b", "c"])) passed += 1 else failed += 1;
	if (test_iter("世界", ["世", "界"])) passed += 1 else failed += 1;
	if (test_iter("👋🏽", ["👋🏽"])) passed += 1 else failed += 1;
	if (test_iter("🇨🇦🇺🇸", ["🇨🇦", "🇺🇸"])) passed += 1 else failed += 1;
	if (test_iter("👨‍👩‍👧", ["👨‍👩‍👧"])) passed += 1 else failed += 1;
	if (test_iter("", [])) passed += 1 else failed += 1;
	
	// Mixed
	if (test_iter("Hi中!", ["H", "i", "中", "!"])) passed += 1 else failed += 1;
	
	// =========================================================================
	// Substring Tests
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Substrings ===")!;
	
	if (test_startswith("hello world", "hello", true)) passed += 1 else failed += 1;
	if (test_startswith("hello world", "world", false)) passed += 1 else failed += 1;
	if (test_startswith("🇨🇦Canada", "🇨🇦", true)) passed += 1 else failed += 1;
	if (test_startswith("", "", true)) passed += 1 else failed += 1;
	if (test_startswith("a", "", true)) passed += 1 else failed += 1;
	
	if (test_endswith("hello world", "world", true)) passed += 1 else failed += 1;
	if (test_endswith("hello world", "hello", false)) passed += 1 else failed += 1;
	if (test_endswith("Canada🇨🇦", "🇨🇦", true)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Search Tests
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Search (gstrchr) ===")!;
	
	if (test_chr("hello", "l", 2)) passed += 1 else failed += 1;
	if (test_chr("hello", "x", void)) passed += 1 else failed += 1;
	if (test_chr("世界hello", "h", 2)) passed += 1 else failed += 1;
	if (test_chr("🇨🇦🇺🇸🇯🇵", "🇺🇸", 1)) passed += 1 else failed += 1;
	if (test_chr("👨‍👩‍👧family", "f", 1)) passed += 1 else failed += 1;
	if (test_chr("", "a", void)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Width regression: EAW split (ExtPict symbols should be width 1, not 2)
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== Width Regression (EAW Split) ===")!;
	
	// These are Extended_Pictographic but NOT East_Asian_Width W/F
	// They should be width 1, not width 2
	if (test_width("♠", 1)) passed += 1 else failed += 1;
	if (test_width("♣", 1)) passed += 1 else failed += 1;
	if (test_width("♥", 1)) passed += 1 else failed += 1;
	if (test_width("♦", 1)) passed += 1 else failed += 1;
	if (test_width("★", 1)) passed += 1 else failed += 1;
	// These ARE East_Asian_Width W/F — should be width 2
	if (test_width("😀", 2)) passed += 1 else failed += 1;
	if (test_width("日", 2)) passed += 1 else failed += 1;
	
	// =========================================================================
	// ZWJ regression: ExtPict should join, non-ExtPict should not
	// =========================================================================
	fmt::println("")!;
	fmt::println("=== ZWJ Regression (ExtPict Split) ===")!;
	
	// ♠ (U+2660) IS Extended_Pictographic — ZWJ should join
	if (test_len("♠\u200D♠", 1)) passed += 1 else failed += 1;
	// 中 (U+4E2D) is NOT Extended_Pictographic — ZWJ should not join
	if (test_len("中\u200D中", 2)) passed += 1 else failed += 1;
	// ★ (U+2605) is NOT Extended_Pictographic — ZWJ should not join
	if (test_len("★\u200D★", 2)) passed += 1 else failed += 1;
	
	// =========================================================================
	// Summary
	// =========================================================================
	fmt::println("")!;
	fmt::println("===================================")!;
	fmt::printfln("Passed: {}", passed)!;
	fmt::printfln("Failed: {}", failed)!;
	
	if (failed > 0) {
		fmt::println("SOME TESTS FAILED")!;
	} else {
		fmt::println("ALL TESTS PASSED")!;
	};
};

fn test_len(s: str, expected: size) bool = {
	const result = gstr::gstrlen(s);
	if (result == expected) {
		fmt::printfln("  PASS: gstrlen({}) = {}", repr(s), result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrlen({}) = {}, expected {}", repr(s), result, expected)!;
	return false;
};

fn test_width(s: str, expected: size) bool = {
	const result = gstr::gstrwidth(s);
	if (result == expected) {
		fmt::printfln("  PASS: gstrwidth({}) = {}", repr(s), result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrwidth({}) = {}, expected {}", repr(s), result, expected)!;
	return false;
};

fn test_at(s: str, n: size, expected: str) bool = {
	match (gstr::gstrat(s, n)) {
	case let result: str =>
		if (result == expected) {
			fmt::printfln("  PASS: gstrat({}, {}) = {}", repr(s), n, repr(result))!;
			return true;
		};
		fmt::printfln("  FAIL: gstrat({}, {}) = {}, expected {}", 
		              repr(s), n, repr(result), repr(expected))!;
		return false;
	case void =>
		fmt::printfln("  FAIL: gstrat({}, {}) = void, expected {}", 
		              repr(s), n, repr(expected))!;
		return false;
	};
};

fn test_at_void(s: str, n: size) bool = {
	match (gstr::gstrat(s, n)) {
	case str =>
		fmt::printfln("  FAIL: gstrat({}, {}) should be void", repr(s), n)!;
		return false;
	case void =>
		fmt::printfln("  PASS: gstrat({}, {}) = void", repr(s), n)!;
		return true;
	};
};

fn test_cmp(a: str, b: str, expected: i8) bool = {
	const result = gstr::gstrcmp(a, b);
	const normalized: i8 = if (result < 0) -1 else if (result > 0) 1 else 0;
	if (normalized == expected) {
		fmt::printfln("  PASS: gstrcmp({}, {}) = {}", repr(a), repr(b), normalized)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrcmp({}, {}) = {}, expected {}", 
	              repr(a), repr(b), normalized, expected)!;
	return false;
};

fn test_startswith(s: str, prefix: str, expected: bool) bool = {
	const result = gstr::gstrstartswith(s, prefix);
	if (result == expected) {
		fmt::printfln("  PASS: gstrstartswith({}, {}) = {}", repr(s), repr(prefix), result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrstartswith({}, {}) = {}, expected {}", 
	              repr(s), repr(prefix), result, expected)!;
	return false;
};

fn test_endswith(s: str, suffix: str, expected: bool) bool = {
	const result = gstr::gstrendswith(s, suffix);
	if (result == expected) {
		fmt::printfln("  PASS: gstrendswith({}, {}) = {}", repr(s), repr(suffix), result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrendswith({}, {}) = {}, expected {}", 
	              repr(s), repr(suffix), result, expected)!;
	return false;
};

fn test_chr(s: str, g: str, expected: (size | void)) bool = {
	const result = gstr::gstrchr(s, g);
	match (expected) {
	case let e: size =>
		match (result) {
		case let r: size =>
			if (r == e) {
				fmt::printfln("  PASS: gstrchr({}, {}) = {}", repr(s), repr(g), r)!;
				return true;
			};
			fmt::printfln("  FAIL: gstrchr({}, {}) = {}, expected {}", 
			              repr(s), repr(g), r, e)!;
			return false;
		case void =>
			fmt::printfln("  FAIL: gstrchr({}, {}) = void, expected {}", 
			              repr(s), repr(g), e)!;
			return false;
		};
	case void =>
		match (result) {
		case size =>
			fmt::printfln("  FAIL: gstrchr({}, {}) found, expected void", repr(s), repr(g))!;
			return false;
		case void =>
			fmt::printfln("  PASS: gstrchr({}, {}) = void", repr(s), repr(g))!;
			return true;
		};
	};
};

fn test_iter(s: str, expected: []str) bool = {
	let it = gstr::iter(s);
	let idx: size = 0;
	
	for (true) {
		const g = gstr::next(&it);
		match (g) {
		case void =>
			if (idx == len(expected)) {
				fmt::printfln("  PASS: iter({}) produced {} graphemes", repr(s), idx)!;
				return true;
			};
			fmt::printfln("  FAIL: iter({}) ended at {}, expected {}", 
			              repr(s), idx, len(expected))!;
			return false;
		case let gs: str =>
			if (idx >= len(expected)) {
				fmt::printfln("  FAIL: iter({}) extra grapheme at {}", repr(s), idx)!;
				return false;
			};
			if (gs != expected[idx]) {
				fmt::printfln("  FAIL: iter({})[{}] = {}, expected {}", 
				              repr(s), idx, repr(gs), repr(expected[idx]))!;
				return false;
			};
			idx += 1;
		};
	};
};

// Helper to create a display representation of a string
fn repr(s: str) str = {
	// For now, just return the string with quotes
	// A proper repr would escape non-printable characters
	return s;
};
