# C Coding Standards

All code must follow these rules. Non-compliant code must be refactored.

Write idiomatic C — code that experienced C programmers read without friction.
Favor clarity over ceremony: short names in tight scopes, standard library
functions over hand-rolled equivalents, flat control flow, and comments that
explain *why*, not *what*. If it looks like Java or C++ squeezed into `.c`
files, rewrite it.

## Naming

Names should be as short as clarity allows. In a three-line loop body, `p` is
clearer than `current_pointer`. In a 200-line function (which you should
probably split), a longer name earns its keep. Match the name length to the
scope.

| Category | Format | Examples |
|----------|--------|----------|
| Constants, macros | `SCREAMING_SNAKE_CASE` | `TAB_WIDTH`, `CURSOR_MOVE` |
| Functions | `module_verb` or `module_verb_noun` | `abuf_write`, `row_destroy` |
| Variables, structs, enums | `snake_case` | `cx`, `nlines`, `editor_highlight` |

### Conventional Short Names

Standard C idioms are always fine regardless of scope: `buf`, `len`, `num`,
`tmp`, `fp`, `fd`, `ptr`, `str`, `sz`, `cnt`, `ret`, `err`, `ctx`, `dst`,
`src`, `pos`, `cur`, `prev`, `next`.

Single-letter names are fine when idiomatic:

| Name | Conventional meaning |
|------|---------------------|
| `i`, `j`, `k` | Loop counters |
| `p`, `q` | Pointers walking data |
| `c` | A single character |
| `n` | A count or size |
| `s` | A string |

Outside these conventions, single-letter names need a very small scope to stay
clear. If the variable lives longer than ~10 lines, give it a real name.

### Function Names: Module-Verb

Functions follow the pattern `module_verb` or `module_verb_noun` when
disambiguation is needed. Keep the module prefix short — abbreviate to
whatever is unambiguous within the project. `append_buffer` becomes `abuf`,
`editor_row` becomes `erow` or `row`, and so on. Once a prefix is established,
use it consistently everywhere.

Don't repeat the module concept in the verb or object.

| Wrong | Right | Why |
|-------|-------|-----|
| `abuf_append` | `abuf_write` | Redundant — "append" restates the module name |
| `abuf_free` | `abuf_destroy` | "destroy" shows cleanup intent |
| `abuf_append_bg` | `abuf_write_bg_color` | Spell out the noun when it adds clarity |
| `editor_row_insert_row` | `erow_insert` | Module prefix already implies "row" |

### No Typedefs for Structs or Enums

Always use explicit `struct name` and `enum name`. Typedefs hide what things
are. This follows Linux kernel style.

```c
/* Correct */
struct buffer {
	struct line    *lines;
	uint32_t        nlines;
};

enum buf_state {
	BUF_EMPTY,
	BUF_MODIFIED,
	BUF_SAVED,
};

struct buffer *buf_create(void);
enum buf_state buf_get_state(struct buffer *buf);

/* Wrong */
typedef struct { ... } Buffer;
typedef enum { ... } BufferState;
```

Typedef exceptions: opaque/semantic types (`uint32_t`, `size_t`, `pid_t`),
architecture-varying types, forward declarations for circular references.

### Constant and Macro Names

Apply the same brevity principle as functions. Use standard abbreviations
and short prefixes — think POSIX and kernel style: `BUFSIZ`, `EINVAL`,
`O_CREAT`, `PAGE_SIZE`. If the reader has to squint past five underscored
words to find the value, the name is too long.

| Verbose | Idiomatic | Why |
|---------|-----------|-----|
| `STATUS_MESSAGE_TIMEOUT_SECONDS` | `STATUS_MSG_TIMEOUT` | Drop redundant unit when the type or context implies it |
| `CURSOR_POSITION_BUFFER_SIZE` | `CURSOR_BUFSZ` | `BUFSZ` is a well-known C idiom |
| `FILE_PERMISSION_DEFAULT` | `FILE_PERM_DEFAULT` | `PERM` is universally understood |
| `ESCAPE_CLEAR_SCREEN` | `ESC_CLEAR_SCREEN` | `ESC` is the standard abbreviation |
| `TAB_STOP_WIDTH` | `TAB_WIDTH` | "stop" adds nothing |

### Prohibited Patterns

- `g_` prefixes, `_t` suffixes on custom types, Hungarian notation
- Cryptic single-character or meaningless names for global objects: `E` →
  `editor`, `S` → `screen`
- Magic numbers — numeric literals whose meaning isn't obvious from context
  must be named constants. Small integers in arithmetic (-1, 0, 1, 2), ASCII
  offsets, and powers of two in bit operations are typically fine inline.
- Hardcoded strings — terminal escape sequences, format strings, etc. must be
  constants

```c
/* Correct */
#define STATUS_MSG_TIMEOUT 5
#define FILE_PERM_DEFAULT 0644
#define ESC_CLEAR_SCREEN "\x1b[2J"
#define CURSOR_BUFSZ 32

if (time(NULL) - timestamp < STATUS_MSG_TIMEOUT)
char buf[CURSOR_BUFSZ];

/* Wrong */
if (time(NULL) - timestamp < 5)
char buf[32];
```

## Idioms

These patterns are the backbone of well-written C. Use them.

### Prefer the Standard Library

Never hand-roll what `<string.h>`, `<stdlib.h>`, or `<stdio.h>` already
provide. Use `strchr` instead of scanning a string in a loop. Use `memcpy`
instead of byte-by-byte assignment. Use `snprintf` instead of manual buffer
assembly. The standard library is well-tested and readers recognize calls to it
instantly.

```c
/* Idiomatic — leverage the standard library */
char *p = strchr(s, ':');
if (p)
	*p++ = '\0';

/* Non-idiomatic — reinventing strchr */
int i = 0;
while (s[i] != '\0') {
	if (s[i] == ':') {
		s[i] = '\0';
		break;
	}
	i++;
}
```

### Pointer Idioms

Walk pointers instead of indexing when iterating linearly through data. This is
natural C — the language was designed around pointers.

```c
/* Idiomatic */
for (char *p = s; *p; p++) {
	if (*p == '\t')
		col += TAB_WIDTH - (col % TAB_WIDTH);
	else
		col++;
}

/* Less idiomatic */
for (int i = 0; i < strlen(s); i++) {
	if (s[i] == '\t')
		col += TAB_WIDTH - (col % TAB_WIDTH);
	else
		col++;
}
```

Use array indexing when you need random access or the index itself is
meaningful (e.g., line numbers).

### Error Handling: goto cleanup

Use `goto` for centralized cleanup. This keeps the happy path at the left
margin and avoids deeply nested `if` chains. Every C programmer recognizes this
pattern.

```c
int file_save(struct editor_state *ed)
{
	int fd = -1;
	char *buf = NULL;
	int ret = -1;

	buf = rows_to_string(ed, &len);
	if (!buf)
		goto cleanup;

	fd = open(ed->filename, O_WRONLY | O_CREAT | O_TRUNC,
	          FILE_PERM_DEFAULT);
	if (fd < 0)
		goto cleanup;

	if (write(fd, buf, len) != len)
		goto cleanup;

	ret = 0;

cleanup:
	if (fd >= 0)
		close(fd);
	free(buf);
	return ret;
}
```

### Return Early

Check preconditions at the top and bail out. Don't wrap the whole function body
in an `if`.

```c
/* Idiomatic */
void row_update_render(struct editor_row *row)
{
	if (!row || !row->chars)
		return;

	/* ... actual work at the top indentation level ... */
}

/* Non-idiomatic */
void row_update_render(struct editor_row *row)
{
	if (row != NULL) {
		if (row->chars != NULL) {
			/* ... deeply indented work ... */
		}
	}
}
```

### Compact Expressions

Use C's expression-rich syntax where experienced programmers expect it.
Assignment-in-condition, comma operators in `for` headers, and the ternary
operator are all idiomatic when they avoid unnecessary verbosity.

```c
/* Idiomatic */
int c;
while ((c = getchar()) != EOF && c != '\n')
	/* process c */;

const char *label = (count == 1) ? "line" : "lines";

/* Overly verbose */
int c = getchar();
while (c != EOF) {
	if (c == '\n')
		break;
	/* process c */
	c = getchar();
}

const char *label;
if (count == 1)
	label = "line";
else
	label = "lines";
```

### Zero Means Success

Functions that can fail return 0 on success and -1 (or a negative error code)
on failure, matching POSIX convention. Return the result directly, not through
an out-parameter, unless the function needs to return something else.

### Allocate, Check, Use, Free

Every `malloc` is followed by a NULL check. Every allocation has a clear owner
responsible for `free`. Pair them visibly — if you can't quickly find the
matching `free`, the code needs restructuring.

## Formatting

### C Standard

All code must be C23 compliant (`-std=c23`). Prefer standard library functions
over platform-specific alternatives.

### Indentation

Tabs only, displayed as 8 characters. Never spaces. If code is indented more
than 3 levels, refactor — deep nesting is a sign of a function doing too much.

### Brace Style

Function opening braces go on a **new line**. Control structure braces go on
the **same line**.

Single-statement bodies can omit braces when the statement fits on one line.

```c
int main(void)
{
	return 0;
}

void process_data(int n)
{
	for (int i = 0; i < n; i++) {
		if (condition)
			do_thing(i);
	}
}
```

### Pointer Declaration

Asterisk attaches to the variable name: `char *buf`, not `char* buf`.

### Function Signatures

Prefer a single line. When a signature is too long to read comfortably, wrap at
parameter boundaries and align with the opening parenthesis.

```c
/* Preferred — fits on one line */
int row_cx_to_rx(struct editor_row *row, int cx);

/* Acceptable — wrapped at parameter boundary */
int row_cx_to_rx(struct editor_row *row,
                 int cx);
```

### Initializers

Struct arrays use compact single-line entries. Keyword arrays use flow-wrapped
formatting.

```c
struct editor_theme editor_themes[] = {
	{ .name = "Dark",  .background = "000000", .foreground = "FFFFFF" },
	{ .name = "Light", .background = "FFFFFF", .foreground = "000000" },
};

char *c_keywords[] = {
	"switch", "if", "while", "for", "break", "continue", "return",
	"else", "case", "struct", "union", "typedef", "enum", "class",
	"int|", "long|", "double|", "float|", "char|", "unsigned|",
	"signed|", "void|", NULL
};
```

### Macros

Single line unless multi-line continuation is truly necessary.

```c
/* Correct */
#define CURSOR_MOVE "\x1b[%d;%dH"

/* Wrong */
#define CURSOR_MOVE                                                   \
  "\x1b[%d;%dH"
```

### Section Banners

Preserve existing section banners exactly as written: `/*** Terminal ***/`,
`/*** Row Operations ***/`, etc.

## Comments

Assume the reader knows C. Don't explain the language — explain your
decisions. Good code is mostly self-documenting; comments fill in the parts
that code can't express: intent, tradeoffs, and non-obvious constraints.

### When to Comment

**Always comment:** functions (briefly — what it does and any non-obvious
behavior), structs (their role in the system), non-obvious logic, workarounds,
and anything where the *why* isn't obvious from the *what*.

**Don't comment:** obvious field names, trivial getters/setters, standard
patterns that any C programmer recognizes, or anything where the comment just
restates the code.

```c
/* Good — explains a non-obvious design choice */
/* We store both raw and rendered versions of each line so tab expansion
 * doesn't have to be recalculated on every screen refresh. */
struct editor_row {
	int idx;
	int size;
	int rsize;
	char *chars;
	char *render;
	unsigned char *hl;
	int hl_open_comment;
};

/* Bad — comments restate what the code already says */
struct editor_row {
	/* The index of the row. */
	int idx;
	/* The size of the row. */
	int size;
	/* The render size of the row. */
	int rsize;
};
```

### Placement

Always above the entity, never trailing.

```c
/* Correct */
/* Maximum number of characters per line */
#define MAX_LINE_LEN 1024

/* Wrong */
#define MAX_LINE_LEN 1024  // Maximum number of characters per line
```

### Format

Use `/* */` block comments. Brief `//` comments are acceptable for inline
notes.

### Tone

Conversational and direct. Write the way you'd explain something to another
programmer at a whiteboard. Vary your phrasing — don't start every comment with
"This function..." or "This field...".

```c
/* Good — natural, varied */
/* Checks whether the cursor has wandered past the end of the line. */
/* Converts a char index to a rendered column, accounting for tabs. */

/* Bad — robotic */
/* This function checks whether the cursor is inside the buffer bounds. */
/* This function converts a character index to its rendered column position. */
```

### Function Comments

Keep them brief. One to three lines covering what it does and anything
surprising. Don't write a paragraph when a sentence will do.

```c
/* Converts a character index in the raw line to its rendered column
 * position by expanding tabs. Does not modify any state. */
int row_cx_to_rx(struct editor_row *row, int cx);
```

## Review Checklist

- [ ] Tab indentation, 8-char display width
- [ ] K&R braces: new line for functions, same line for control structures
- [ ] Pointer asterisk on the variable name
- [ ] Function signatures on one line, or wrapped at parameter boundaries
- [ ] Macros on a single line
- [ ] Section banners preserved exactly
- [ ] `SCREAMING_SNAKE_CASE` for constants/macros, kept terse (`ESC_`, `BUFSZ`, `PERM`, etc.)
- [ ] `snake_case` for everything else
- [ ] `module_verb` or `module_verb_noun` function names with short module prefixes
- [ ] Module prefixes abbreviated and used consistently (`abuf_`, `row_`, `erow_`, etc.)
- [ ] Short names for short scopes; longer names for wider scopes
- [ ] Standard C idioms used (`buf`, `len`, `p`, `n`, `c`, etc.)
- [ ] No typedefs for structs or enums
- [ ] No magic numbers or hardcoded strings
- [ ] Standard library preferred over hand-rolled equivalents
- [ ] `goto cleanup` for multi-resource error handling
- [ ] Early returns for precondition checks
- [ ] Comments explain *why*, not *what*; no restating the obvious
- [ ] Every function and struct has a brief comment
- [ ] Comments are conversational with varied phrasing
- [ ] No trailing comments, no formal labeled sections
