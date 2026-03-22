/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025 Edward J Edmonds <edward.edmonds@gmail.com>
 *
 * test_macros.h - Shared test framework macros for gstr test suite
 */

#ifndef TEST_MACROS_H
#define TEST_MACROS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN(name)                                                              \
  do {                                                                         \
    printf("  %-50s", #name);                                                  \
    test_##name();                                                             \
    printf(" PASS\n");                                                         \
    tests_passed++;                                                            \
  } while (0)

#define ASSERT(cond)                                                           \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf(" FAIL\n    Assertion failed: %s\n    at %s:%d\n", #cond,         \
             __FILE__, __LINE__);                                              \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ(a, b)                                                        \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      printf(" FAIL\n    Expected %d, got %d\n    at %s:%d\n", (int)(b),       \
             (int)(a), __FILE__, __LINE__);                                    \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_SIZE(a, b)                                                   \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      printf(" FAIL\n    Expected %zu, got %zu\n    at %s:%d\n", (size_t)(b),  \
             (size_t)(a), __FILE__, __LINE__);                                 \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_EQ_U32(a, b)                                                    \
  do {                                                                         \
    if ((a) != (b)) {                                                          \
      printf(" FAIL\n    Expected 0x%X, got 0x%X\n    at %s:%d\n",            \
             (unsigned)(b), (unsigned)(a), __FILE__, __LINE__);                \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_STR_EQ(a, b)                                                    \
  do {                                                                         \
    if (strcmp((a), (b)) != 0) {                                               \
      printf(" FAIL\n    Expected \"%s\", got \"%s\"\n    at %s:%d\n", (b),    \
             (a), __FILE__, __LINE__);                                         \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NULL(ptr)                                                       \
  do {                                                                         \
    if ((ptr) != NULL) {                                                       \
      printf(" FAIL\n    Expected NULL, got non-NULL\n    at %s:%d\n",         \
             __FILE__, __LINE__);                                              \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define ASSERT_NOT_NULL(ptr)                                                   \
  do {                                                                         \
    if ((ptr) == NULL) {                                                       \
      printf(" FAIL\n    Expected non-NULL, got NULL\n    at %s:%d\n",         \
             __FILE__, __LINE__);                                              \
      tests_failed++;                                                          \
      return;                                                                  \
    }                                                                          \
  } while (0)

#endif /* TEST_MACROS_H */
