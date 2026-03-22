# SPDX-License-Identifier: MIT
# Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>

CC ?= clang
CFLAGS = -Wall -Wextra -pedantic -std=c17 -O3
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c17 -g -O0

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

build-test: $(TESTDIR)/test_gstr $(TESTDIR)/test_grapheme_walk $(TESTDIR)/test_utf8_layer $(TESTDIR)/test_edge_cases $(TESTDIR)/test_mcdc_grapheme_break $(TESTDIR)/test_unicode_punct

run-test: build-test
	./$(TESTDIR)/test_gstr
	./$(TESTDIR)/test_grapheme_walk
	./$(TESTDIR)/test_utf8_layer
	./$(TESTDIR)/test_edge_cases
	./$(TESTDIR)/test_mcdc_grapheme_break
	./$(TESTDIR)/test_unicode_punct

test: run-test

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
gstr.pc: gstr.pc.in
	sed -e 's|@PREFIX@|$(PREFIX)|g' -e 's|@VERSION@|$(VERSION)|g' $< > $@

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
