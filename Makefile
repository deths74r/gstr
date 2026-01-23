CC = clang
CFLAGS = -Wall -Wextra -pedantic -std=c17 -O3
CFLAGS_DEBUG = -Wall -Wextra -pedantic -std=c17 -g -O0
AR = ar
ARFLAGS = rcs

PREFIX = /usr/local
INCLUDEDIR = $(PREFIX)/include
LIBDIR = $(PREFIX)/lib

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
TESTDIR = test
SINGLE_HEADER_DIR = single_include

# ============================================================================
# Git-based versioning
# ============================================================================
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo "nogit")
GIT_TAG := $(shell git describe --tags --exact-match HEAD 2>/dev/null)
GIT_BRANCH := $(shell git branch --show-current 2>/dev/null)
LAST_TAG := $(shell git describe --tags --abbrev=0 2>/dev/null || echo "v0.0.0")
COMMITS_SINCE := $(shell git rev-list $(LAST_TAG)..HEAD --count 2>/dev/null || echo "0")

# Version string logic
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

# Pass version to compiler
VERSION_FLAGS = -DGSTR_VERSION=\"$(VERSION)\" -DGSTR_BUILD_ID=\"$(BUILD_ID)\"

# ============================================================================
# Files (merged library - gstr.h includes utf8_* and gstr* APIs)
# ============================================================================
GSTR_SRC = $(SRCDIR)/gstr.c
GSTR_OBJ = $(BUILDDIR)/gstr.o
GSTR_HEADER = $(INCDIR)/gstr.h
GSTR_SINGLE = $(SINGLE_HEADER_DIR)/gstr.h

# Library
LIB = $(BUILDDIR)/libgstr.a

.PHONY: all clean install uninstall test test-single debug version

all: $(LIB) $(GSTR_SINGLE)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

# ============================================================================
# Library build
# ============================================================================
$(LIB): $(GSTR_OBJ)
	$(AR) $(ARFLAGS) $@ $^

$(GSTR_OBJ): $(GSTR_SRC) $(GSTR_HEADER) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(VERSION_FLAGS) -I$(INCDIR) -c $< -o $@

# ============================================================================
# Single-header generation
# ============================================================================
$(GSTR_SINGLE): $(GSTR_HEADER) $(GSTR_SRC)
	@mkdir -p $(SINGLE_HEADER_DIR)
	@echo "Generating single-header gstr.h..."
	@echo "/*" > $@
	@echo " * gstr.h - Single-header UTF-8 and grapheme string library" >> $@
	@echo " * Version: $(VERSION)" >> $@
	@echo " * Build: $(BUILD_ID)" >> $@
	@echo " *" >> $@
	@echo " * Usage:" >> $@
	@echo " *   #define GSTR_IMPLEMENTATION" >> $@
	@echo " *   #include \"gstr.h\"" >> $@
	@echo " */" >> $@
	@echo "" >> $@
	@echo "#ifndef GSTR_SINGLE_H" >> $@
	@echo "#define GSTR_SINGLE_H" >> $@
	@echo "" >> $@
	@echo "/* Version info (can be overridden at compile time) */" >> $@
	@echo "#ifndef GSTR_VERSION" >> $@
	@echo "#define GSTR_VERSION \"$(VERSION)\"" >> $@
	@echo "#endif" >> $@
	@echo "#ifndef GSTR_BUILD_ID" >> $@
	@echo "#define GSTR_BUILD_ID \"$(BUILD_ID)\"" >> $@
	@echo "#endif" >> $@
	@echo "" >> $@
	@# Include gstr header (skip header guard - we use our own)
	@sed -e '/#ifndef GSTR_H/d' \
		-e '/#define GSTR_H/d' \
		-e '/#endif \/\* GSTR_H \*\//d' \
		$(GSTR_HEADER) >> $@
	@echo "" >> $@
	@echo "#endif /* GSTR_SINGLE_H */" >> $@
	@echo "" >> $@
	@echo "#ifdef GSTR_IMPLEMENTATION" >> $@
	@echo "" >> $@
	@# Include gstr implementation (skip includes - they're in header)
	@sed -e '/#include <stdlib.h>/d' \
		-e '/#include <string.h>/d' \
		-e '/#include <gstr.h>/d' $(GSTR_SRC) >> $@
	@echo "" >> $@
	@echo "#endif /* GSTR_IMPLEMENTATION */" >> $@
	@echo "Generated $(GSTR_SINGLE) ($(VERSION))"

# ============================================================================
# Clean
# ============================================================================
clean:
	rm -rf $(BUILDDIR)
	rm -f $(TESTDIR)/test_gstr $(TESTDIR)/test_gstr_single

# ============================================================================
# Install/Uninstall
# ============================================================================
install: $(LIB) $(GSTR_HEADER) $(GSTR_SINGLE)
	install -d $(LIBDIR)
	install -m 644 $(GSTR_SINGLE) $(INCLUDEDIR)/gstr.h
	install -m 644 $(LIB) $(LIBDIR)/

uninstall:
	rm -f $(INCLUDEDIR)/gstr.h
	rm -f $(LIBDIR)/libgstr.a

# ============================================================================
# Tests
# ============================================================================
# Test gstr using static library
test: $(LIB) $(TESTDIR)/test_gstr.c
	$(CC) $(CFLAGS_DEBUG) $(VERSION_FLAGS) -I$(INCDIR) $(TESTDIR)/test_gstr.c -L$(BUILDDIR) -lgstr -o $(TESTDIR)/test_gstr
	./$(TESTDIR)/test_gstr

# Test gstr using single-header version
test-single: $(TESTDIR)/test_gstr.c $(GSTR_SINGLE)
	$(CC) $(CFLAGS_DEBUG) -DGSTR_SINGLE_HEADER -I$(SINGLE_HEADER_DIR) $(TESTDIR)/test_gstr.c -o $(TESTDIR)/test_gstr_single
	./$(TESTDIR)/test_gstr_single

# Run all tests
test-all: test test-single
	@echo "All tests passed!"

# Debug build
debug: CFLAGS = $(CFLAGS_DEBUG)
debug: clean $(LIB)

# Show version info
version:
	@echo "Version: $(VERSION)"
	@echo "Build ID: $(BUILD_ID)"
	@echo "Git hash: $(GIT_HASH)"
	@echo "Git tag: $(GIT_TAG)"
	@echo "Git branch: $(GIT_BRANCH)"
	@echo "Last tag: $(LAST_TAG)"
	@echo "Commits since: $(COMMITS_SINCE)"
