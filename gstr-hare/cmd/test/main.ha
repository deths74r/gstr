// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
//
// Test program for gstr Hare implementation

use fmt;
use gstr;

export fn main() void = {
	fmt::println("gstr-hare test suite")!;
	fmt::println("====================")!;
	fmt::println("")!;
	
	let passed: size = 0;
	let failed: size = 0;
	
	// Test basic length
	fmt::println("Testing gstrlen...")!;
	
	if (test_len("", 0)) passed += 1 else failed += 1;
	if (test_len("hello", 5)) passed += 1 else failed += 1;
	if (test_len("café", 4)) passed += 1 else failed += 1;
	if (test_len("世界", 2)) passed += 1 else failed += 1;
	if (test_len("👋", 1)) passed += 1 else failed += 1;
	if (test_len("👋🏽", 1)) passed += 1 else failed += 1;  // Wave + skin tone
	if (test_len("🇨🇦", 1)) passed += 1 else failed += 1;  // Flag
	if (test_len("👨‍👩‍👧", 1)) passed += 1 else failed += 1;  // Family
	
	// Test width
	fmt::println("")!;
	fmt::println("Testing gstrwidth...")!;
	
	if (test_width("hello", 5)) passed += 1 else failed += 1;
	if (test_width("世界", 4)) passed += 1 else failed += 1;
	if (test_width("👋", 2)) passed += 1 else failed += 1;
	if (test_width("A", 1)) passed += 1 else failed += 1;
	
	// Test at
	fmt::println("")!;
	fmt::println("Testing gstrat...")!;
	
	if (test_at("hello", 0, "h")) passed += 1 else failed += 1;
	if (test_at("hello", 4, "o")) passed += 1 else failed += 1;
	if (test_at("café", 3, "é")) passed += 1 else failed += 1;
	if (test_at("世界", 1, "界")) passed += 1 else failed += 1;
	
	// Test comparison
	fmt::println("")!;
	fmt::println("Testing gstrcmp...")!;
	
	if (test_cmp("abc", "abc", 0)) passed += 1 else failed += 1;
	if (test_cmp("abc", "abd", -1)) passed += 1 else failed += 1;
	if (test_cmp("abd", "abc", 1)) passed += 1 else failed += 1;
	if (test_cmp("ab", "abc", -1)) passed += 1 else failed += 1;
	if (test_cmp("abc", "ab", 1)) passed += 1 else failed += 1;
	
	// Test startswith/endswith
	fmt::println("")!;
	fmt::println("Testing gstrstartswith/gstrendswith...")!;
	
	if (test_startswith("hello world", "hello", true)) passed += 1 else failed += 1;
	if (test_startswith("hello world", "world", false)) passed += 1 else failed += 1;
	if (test_endswith("hello world", "world", true)) passed += 1 else failed += 1;
	if (test_endswith("hello world", "hello", false)) passed += 1 else failed += 1;
	
	// Test chr
	fmt::println("")!;
	fmt::println("Testing gstrchr...")!;
	
	if (test_chr("hello", "l", 2)) passed += 1 else failed += 1;
	if (test_chr("hello", "x", void)) passed += 1 else failed += 1;
	if (test_chr("世界hello", "h", 2)) passed += 1 else failed += 1;
	
	// Test iterator
	fmt::println("")!;
	fmt::println("Testing iterator...")!;
	
	if (test_iter("abc", ["a", "b", "c"])) passed += 1 else failed += 1;
	if (test_iter("世界", ["世", "界"])) passed += 1 else failed += 1;
	
	// Summary
	fmt::println("")!;
	fmt::println("====================")!;
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
		fmt::printfln("  PASS: gstrlen(\"{}\") = {}", s, result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrlen(\"{}\") = {}, expected {}", s, result, expected)!;
	return false;
};

fn test_width(s: str, expected: size) bool = {
	const result = gstr::gstrwidth(s);
	if (result == expected) {
		fmt::printfln("  PASS: gstrwidth(\"{}\") = {}", s, result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrwidth(\"{}\") = {}, expected {}", s, result, expected)!;
	return false;
};

fn test_at(s: str, n: size, expected: str) bool = {
	match (gstr::gstrat(s, n)) {
	case let result: str =>
		if (result == expected) {
			fmt::printfln("  PASS: gstrat(\"{}\", {}) = \"{}\"", s, n, result)!;
			return true;
		};
		fmt::printfln("  FAIL: gstrat(\"{}\", {}) = \"{}\", expected \"{}\"", 
		              s, n, result, expected)!;
		return false;
	case void =>
		fmt::printfln("  FAIL: gstrat(\"{}\", {}) = void, expected \"{}\"", 
		              s, n, expected)!;
		return false;
	};
};

fn test_cmp(a: str, b: str, expected: i8) bool = {
	const result = gstr::gstrcmp(a, b);
	// Normalize to -1, 0, 1
	const normalized: i8 = if (result < 0) -1 else if (result > 0) 1 else 0;
	if (normalized == expected) {
		fmt::printfln("  PASS: gstrcmp(\"{}\", \"{}\") = {}", a, b, normalized)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrcmp(\"{}\", \"{}\") = {}, expected {}", 
	              a, b, normalized, expected)!;
	return false;
};

fn test_startswith(s: str, prefix: str, expected: bool) bool = {
	const result = gstr::gstrstartswith(s, prefix);
	if (result == expected) {
		fmt::printfln("  PASS: gstrstartswith(\"{}\", \"{}\") = {}", s, prefix, result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrstartswith(\"{}\", \"{}\") = {}, expected {}", 
	              s, prefix, result, expected)!;
	return false;
};

fn test_endswith(s: str, suffix: str, expected: bool) bool = {
	const result = gstr::gstrendswith(s, suffix);
	if (result == expected) {
		fmt::printfln("  PASS: gstrendswith(\"{}\", \"{}\") = {}", s, suffix, result)!;
		return true;
	};
	fmt::printfln("  FAIL: gstrendswith(\"{}\", \"{}\") = {}, expected {}", 
	              s, suffix, result, expected)!;
	return false;
};

fn test_chr(s: str, g: str, expected: (size | void)) bool = {
	const result = gstr::gstrchr(s, g);
	match (expected) {
	case let e: size =>
		match (result) {
		case let r: size =>
			if (r == e) {
				fmt::printfln("  PASS: gstrchr(\"{}\", \"{}\") = {}", s, g, r)!;
				return true;
			};
			fmt::printfln("  FAIL: gstrchr(\"{}\", \"{}\") = {}, expected {}", 
			              s, g, r, e)!;
			return false;
		case void =>
			fmt::printfln("  FAIL: gstrchr(\"{}\", \"{}\") = void, expected {}", 
			              s, g, e)!;
			return false;
		};
	case void =>
		match (result) {
		case size =>
			fmt::printfln("  FAIL: gstrchr(\"{}\", \"{}\") found, expected void", s, g)!;
			return false;
		case void =>
			fmt::printfln("  PASS: gstrchr(\"{}\", \"{}\") = void", s, g)!;
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
				fmt::printfln("  PASS: iter(\"{}\") produced {} graphemes", s, idx)!;
				return true;
			};
			fmt::printfln("  FAIL: iter(\"{}\") ended at {}, expected {}", 
			              s, idx, len(expected))!;
			return false;
		case let gs: str =>
			if (idx >= len(expected)) {
				fmt::printfln("  FAIL: iter(\"{}\") extra grapheme at {}", s, idx)!;
				return false;
			};
			if (gs != expected[idx]) {
				fmt::printfln("  FAIL: iter(\"{}\")[{}] = \"{}\", expected \"{}\"", 
				              s, idx, gs, expected[idx])!;
				return false;
			};
			idx += 1;
		};
	};
};
