/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_conformance.c - Unicode GraphemeBreakTest.txt conformance runner.
 *
 * Parses the official Unicode grapheme-break test vectors and checks that
 * utf8_next_grapheme() places boundaries at exactly the positions the
 * Unicode Consortium specifies. This is the spec-based (black-box)
 * baseline that complements the white-box MC/DC suite.
 *
 * Usage: test_conformance [path-to-GraphemeBreakTest.txt]
 *        (defaults to test/GraphemeBreakTest.txt; also reads stdin if the
 *         path is "-")
 *
 * Line format (Unicode UAX #29):
 *   ÷ 0020 ÷ 0020 ÷   # comment
 * where ÷ (U+00F7) marks a break and × (U+00D7) marks no-break, between
 * hex codepoints. The leading/trailing markers (string start/end) are
 * always breaks and are not checked; only the interior markers matter.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gstr.h>

#define MAX_CPS 64
#define DIV "\xC3\xB7" /* U+00F7 DIVISION SIGN — break */
#define MUL "\xC3\x97" /* U+00D7 MULTIPLICATION SIGN — no-break */

/* Parse one test line into codepoints[] and break_before[] (1 if the
 * marker preceding codepoint i is a break). Returns the codepoint count,
 * or -1 if the line is a comment/blank/unparseable. */
static int parse_line(char *line, uint32_t *codepoints, int *break_before)
{
	char *hash = strchr(line, '#');
	if (hash)
		*hash = '\0';

	int cp_count = 0;
	int pending_break = -1; /* marker seen but not yet attached to a cp */

	for (char *tok = strtok(line, " \t\r\n"); tok;
	     tok = strtok(NULL, " \t\r\n")) {
		if (strcmp(tok, DIV) == 0) {
			pending_break = 1;
		} else if (strcmp(tok, MUL) == 0) {
			pending_break = 0;
		} else {
			/* Hex codepoint. A marker must have preceded it. */
			if (pending_break < 0)
				return -1;
			if (cp_count >= MAX_CPS)
				return -1;
			codepoints[cp_count] = (uint32_t)strtoul(tok, NULL, 16);
			break_before[cp_count] = pending_break;
			cp_count++;
			pending_break = -1;
		}
	}
	return cp_count > 0 ? cp_count : -1;
}

int main(int argc, char **argv)
{
	const char *path = (argc > 1) ? argv[1] : "test/GraphemeBreakTest.txt";
	FILE *f = (strcmp(path, "-") == 0) ? stdin : fopen(path, "r");
	if (!f) {
		fprintf(stderr, "conformance: cannot open %s\n", path);
		return 2;
	}

	printf("\nUnicode GraphemeBreakTest Conformance (gstr targets Unicode %s)\n",
	       GSTR_UNICODE_VERSION);
	printf("======================================================\n");

	char line[4096];
	int lineno = 0;
	int tested = 0, passed = 0, failed = 0;
	int version_checked = 0;

	while (fgets(line, sizeof(line), f)) {
		lineno++;

		/* Version banner: first comment line names the file's Unicode version. */
		if (!version_checked && line[0] == '#' &&
		    strstr(line, "GraphemeBreakTest-")) {
			version_checked = 1;
			if (!strstr(line, GSTR_UNICODE_VERSION)) {
				printf("  [warn] test file is not Unicode %s: %s",
				       GSTR_UNICODE_VERSION, line);
			}
		}

		uint32_t cps[MAX_CPS];
		int brk[MAX_CPS];
		char parse_buf[4096];
		memcpy(parse_buf, line, sizeof(parse_buf));
		parse_buf[sizeof(parse_buf) - 1] = '\0';

		int n = parse_line(parse_buf, cps, brk);
		if (n < 0)
			continue;

		/* Encode codepoints to UTF-8, recording the byte offset of each. */
		char text[MAX_CPS * 4 + 1];
		size_t byteoff[MAX_CPS + 1];
		size_t len = 0;
		int encode_ok = 1;
		for (int i = 0; i < n; i++) {
			byteoff[i] = len;
			size_t w = utf8_encode(cps[i], text + len);
			if (w == 0) {
				encode_ok =
					0; /* surrogate or out-of-range — skip the vector */
				break;
			}
			len += w;
		}
		if (!encode_ok)
			continue;
		byteoff[n] = len;

		/* Expected interior breaks: byte offsets of codepoints i (i >= 1)
     * whose preceding marker is a break. */
		size_t expected[MAX_CPS];
		int exp_count = 0;
		for (int i = 1; i < n; i++) {
			if (brk[i])
				expected[exp_count++] = byteoff[i];
		}

		/* Actual interior breaks from the segmenter. */
		size_t actual[MAX_CPS];
		int act_count = 0;
		size_t off = 0;
		while (off < len) {
			size_t next = utf8_next_grapheme(text, len, off);
			if (next <= off) /* non-advance guard */
				break;
			if (next < len)
				actual[act_count++] = next;
			off = next;
		}

		tested++;
		int match = (exp_count == act_count);
		for (int i = 0; match && i < exp_count; i++)
			if (expected[i] != actual[i])
				match = 0;

		if (match) {
			passed++;
		} else {
			failed++;
			if (failed <= 20) {
				printf("  FAIL line %d: expected breaks {",
				       lineno);
				for (int i = 0; i < exp_count; i++)
					printf("%s%zu", i ? "," : "",
					       expected[i]);
				printf("} got {");
				for (int i = 0; i < act_count; i++)
					printf("%s%zu", i ? "," : "",
					       actual[i]);
				printf("}\n");
			}
		}
	}

	if (f != stdin)
		fclose(f);

	printf("------------------------------------------------------\n");
	printf("Conformance: %d/%d passed, %d failed\n", passed, tested,
	       failed);
	printf("------------------------------------------------------\n");
	return failed > 0 ? 1 : 0;
}
