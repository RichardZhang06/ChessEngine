#pragma once
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);            \
      g_failures++;                                                          \
    }                                                                        \
  } while (0)
#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                              \
    auto _a = (a);                                                                                  \
    auto _b = (b);                                                                                  \
    if (!(_a == _b)) {                                                                              \
      std::printf("FAIL %s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b);                            \
      g_failures++;                                                                                 \
    }                                                                                               \
  } while (0)
#define TEST_MAIN_END() \
  do { std::printf(g_failures ? "%d FAILURES\n" : "all passed\n", g_failures); return g_failures ? 1 : 0; } while (0)
