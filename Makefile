# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>

# Default to clang unless the user overrides CC. Plain 'CC ?= clang' never
# fires under GNU make because CC has a built-in default of 'cc'.
ifeq ($(origin CC),default)
CC = clang
endif
ifeq ($(origin CXX),default)
CXX = clang++
endif
CFLAGS = -Wall -Wextra -pedantic -std=c23 -O3
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c23 -g -O0

PREFIX = /usr/local
INCLUDEDIR = $(PREFIX)/include
PKGCONFIGDIR = $(PREFIX)/lib/pkgconfig

INCDIR = include
TESTDIR = test
HEADER = $(INCDIR)/gstr.h

# ============================================================================
# Git-based versioning
# ============================================================================
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
GIT_TAG := $(shell git describe --tags --exact-match HEAD 2>/dev/null)
GIT_BRANCH := $(shell git branch --show-current 2>/dev/null)
LAST_TAG := $(shell git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
COMMITS_SINCE := $(shell git rev-list $(LAST_TAG)..HEAD --count 2>/dev/null || echo "0")

ifneq ($(GIT_TAG),)
    VERSION := $(patsubst v%,%,$(GIT_TAG))
else ifeq ($(GIT_BRANCH),main)
    VERSION := $(patsubst v%,%,$(LAST_TAG))+dev.$(COMMITS_SINCE)
else ifneq ($(GIT_BRANCH),)
    BRANCH_SUFFIX := $(shell echo "$(GIT_BRANCH)" | tr '/' '-' | cut -c1-30)
    VERSION := $(patsubst v%,%,$(LAST_TAG))+$(BRANCH_SUFFIX)
else
    VERSION := 0.0.0+unknown
endif

BUILD_TIME := $(shell date +%Y%m%d-%H%M%S)
BUILD_ID := $(GIT_HASH)-$(BUILD_TIME)
VERSION_FLAGS = -DGSTR_VERSION=\"$(VERSION)\" -DGSTR_BUILD_ID=\"$(BUILD_ID)\"

.PHONY: all clean install uninstall test run-test version

all: build-test

# ============================================================================
# Test
# ============================================================================
$(TESTDIR)/test_gstr: $(TESTDIR)/test_gstr.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< -o $@

$(TESTDIR)/test_grapheme_walk: $(TESTDIR)/test_grapheme_walk.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< -o $@

$(TESTDIR)/test_utf8_layer: $(TESTDIR)/test_utf8_layer.c $(TESTDIR)/test_macros.h $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) -I$(TESTDIR) $< -o $@

$(TESTDIR)/test_edge_cases: $(TESTDIR)/test_edge_cases.c $(TESTDIR)/test_macros.h $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) -I$(TESTDIR) $< -o $@

$(TESTDIR)/test_mcdc_grapheme_break: $(TESTDIR)/test_mcdc_grapheme_break.c $(TESTDIR)/test_macros.h $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) -I$(TESTDIR) $< -o $@

$(TESTDIR)/test_unicode_punct: $(TESTDIR)/test_unicode_punct.c $(TESTDIR)/test_macros.h $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) -I$(TESTDIR) $< -o $@

$(TESTDIR)/test_gstr_stress: $(TESTDIR)/test_gstr_stress.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $< -o $@

$(TESTDIR)/test_new_functions_stress: $(TESTDIR)/test_new_functions_stress.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) $< -o $@

# Built with the optimized flags: the >2 GB boundary tests are too slow at
# -O0, and this keeps at least one optimized build in the default target.
$(TESTDIR)/test_type_boundary: $(TESTDIR)/test_type_boundary.c $(TESTDIR)/test_macros.h $(HEADER)
	$(CC) $(CFLAGS) -I$(INCDIR) -I$(TESTDIR) $< -o $@

$(TESTDIR)/test_conformance: $(TESTDIR)/test_conformance.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) $< -o $@

build-test: $(TESTDIR)/test_gstr $(TESTDIR)/test_grapheme_walk $(TESTDIR)/test_utf8_layer $(TESTDIR)/test_edge_cases $(TESTDIR)/test_mcdc_grapheme_break $(TESTDIR)/test_unicode_punct $(TESTDIR)/test_gstr_stress $(TESTDIR)/test_new_functions_stress $(TESTDIR)/test_type_boundary $(TESTDIR)/test_conformance

run-test: build-test
	./$(TESTDIR)/test_gstr
	./$(TESTDIR)/test_grapheme_walk
	./$(TESTDIR)/test_utf8_layer
	./$(TESTDIR)/test_edge_cases
	./$(TESTDIR)/test_mcdc_grapheme_break
	./$(TESTDIR)/test_unicode_punct
	./$(TESTDIR)/test_gstr_stress
	./$(TESTDIR)/test_new_functions_stress
	./$(TESTDIR)/test_type_boundary
	$(MAKE) test-conformance

# Unicode GraphemeBreakTest.txt conformance gate (spec 01 §7, spec 08 §3.5).
# The test vectors are committed at test/GraphemeBreakTest.txt so this runs
# offline; refresh the file when bumping GSTR_UNICODE_VERSION.
test-conformance: $(TESTDIR)/test_conformance
	./$(TESTDIR)/test_conformance $(TESTDIR)/GraphemeBreakTest.txt

.PHONY: test-conformance

# Compile-only gates: strict C23 conformance (the project standard, per
# CODING_STANDARDS.md), C++ interop (the header ships extern "C" guards and
# 4cc5757 fixed real C++ builds), and spec 03's zero
# -Wconversion/-Wsign-conversion acceptance criterion for the header.
check-compat: $(HEADER)
	echo '#include <gstr.h>' | $(CC) -x c -std=c23 -pedantic -Wall -Wextra -Werror -I$(INCDIR) -fsyntax-only -
	echo '#include <gstr.h>' | $(CXX) -x c++ -std=c++17 -Wall -Wextra -Werror -I$(INCDIR) -fsyntax-only -
	echo '#include <gstr.h>' | $(CC) -x c -std=c23 -Wconversion -Wsign-conversion -Werror -I$(INCDIR) -fsyntax-only -
	@echo "check-compat: C23, C++17, and -Wconversion gates passed"

.PHONY: check-compat

# ============================================================================
# Formatting (clang-format per .clang-format / CODING_STANDARDS.md)
# ============================================================================
# Pin the version in CI so results are reproducible (pip install
# clang-format==22.1.5); override CLANG_FORMAT to point at that binary.
CLANG_FORMAT ?= clang-format
FORMAT_FILES = $(HEADER) $(wildcard $(TESTDIR)/*.c) $(wildcard $(TESTDIR)/*.h) $(wildcard tools/*.c)

format:
	$(CLANG_FORMAT) -i --style=file $(FORMAT_FILES)

format-check:
	$(CLANG_FORMAT) --dry-run -Werror --style=file $(FORMAT_FILES)

.PHONY: format format-check

test: check-compat run-test

# Includes the >2 GB boundary tests (offsets past INT_MAX in
# gstrendswith/gstrsub/gstrrev/gstrstr/gstrreplace). Each grapheme-walks
# ~2^31 positions and takes 1-5 minutes at -O3, and the run needs >2 GB
# RAM, so the default test target skips them. Run this before releases.
test-boundary-full: $(TESTDIR)/test_type_boundary
	GSTR_FULL_BOUNDARY=1 ./$(TESTDIR)/test_type_boundary

.PHONY: test-boundary-full

# ============================================================================
# Tools
# ============================================================================
tools/cursor_walk: tools/cursor_walk.c $(HEADER)
	$(CC) $(CFLAGS_DEBUG) -I$(INCDIR) $< -o $@

cursor-walk: tools/cursor_walk

.PHONY: cursor-walk

# ============================================================================
# pkg-config
# ============================================================================
# FORCE so a change in VERSION or PREFIX (both computed, not files)
# regenerates the file instead of installing a stale one.
gstr.pc: gstr.pc.in FORCE
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

FORCE:
.PHONY: FORCE

# ============================================================================
# Install/Uninstall (header-only library)
# ============================================================================
install: $(HEADER) gstr.pc
	install -d $(DESTDIR)$(INCLUDEDIR)
	install -m 644 $(HEADER) $(DESTDIR)$(INCLUDEDIR)/gstr.h
	install -d $(DESTDIR)$(PKGCONFIGDIR)
	install -m 644 gstr.pc $(DESTDIR)$(PKGCONFIGDIR)/gstr.pc

uninstall:
	rm -f $(DESTDIR)$(INCLUDEDIR)/gstr.h
	rm -f $(DESTDIR)$(PKGCONFIGDIR)/gstr.pc

# ============================================================================
# Clean
# ============================================================================
clean:
	rm -f $(TESTDIR)/test_gstr
	rm -f $(TESTDIR)/test_grapheme_walk
	rm -f $(TESTDIR)/test_utf8_layer
	rm -f $(TESTDIR)/test_edge_cases
	rm -f $(TESTDIR)/test_mcdc_grapheme_break
	rm -f $(TESTDIR)/test_unicode_punct
	rm -f $(TESTDIR)/test_gstr_stress
	rm -f $(TESTDIR)/test_new_functions_stress
	rm -f $(TESTDIR)/test_type_boundary
	rm -f $(TESTDIR)/test_conformance
	rm -f tools/cursor_walk
	rm -f *.db-shm *.db-wal
	rm -rf scripts/.unicode_cache/
	rm -f gstr.pc

# ============================================================================
# Version info
# ============================================================================
version:
	@echo "Version: $(VERSION)"
	@echo "Build ID: $(BUILD_ID)"
