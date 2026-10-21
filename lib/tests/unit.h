/**
 * @file    unit.h
 * @brief   Minimal unit-test framework (header only, PC side).
 *
 *   TEST(name) { CHECK(x == 1); CHECK_EQ_INT(a, b); CHECK_EQ_STR(s, "ok"); }
 *   int main(void) { RUN(name); return unit_summary(); }
 */
#ifndef UNIT_H
#define UNIT_H

#include <stdio.h>
#include <string.h>

extern int unit_checks;
extern int unit_failures;

#define TEST(name)  static void test_##name(void)

#define RUN(name)                                  \
    do {                                           \
        const int before_ = unit_failures;         \
        test_##name();                             \
        printf("  %-28s %s\n", #name,              \
               (unit_failures == before_) ? "ok" : "FAILED"); \
    } while (0)

#define CHECK(cond)                                                     \
    do {                                                                \
        unit_checks++;                                                  \
        if (!(cond)) {                                                  \
            unit_failures++;                                            \
            printf("    %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
        }                                                               \
    } while (0)

#define CHECK_EQ_INT(actual, expected)                                  \
    do {                                                                \
        const long long a_ = (long long)(actual);                       \
        const long long e_ = (long long)(expected);                     \
        unit_checks++;                                                  \
        if (a_ != e_) {                                                 \
            unit_failures++;                                            \
            printf("    %s:%d: %s == %lld (0x%llx), expected %lld (0x%llx)\n", \
                   __FILE__, __LINE__, #actual, a_, (unsigned long long)a_, \
                   e_, (unsigned long long)e_);                         \
        }                                                               \
    } while (0)

#define CHECK_NEAR_INT(actual, expected, tol)                           \
    do {                                                                \
        const long long a_ = (long long)(actual);                       \
        const long long e_ = (long long)(expected);                     \
        unit_checks++;                                                  \
        if ((a_ - e_ > (tol)) || (e_ - a_ > (tol))) {                   \
            unit_failures++;                                            \
            printf("    %s:%d: %s == %lld, expected %lld +- %d\n",      \
                   __FILE__, __LINE__, #actual, a_, e_, (int)(tol));    \
        }                                                               \
    } while (0)

#define CHECK_EQ_STR(actual, expected)                                  \
    do {                                                                \
        const char *a_ = (actual);                                      \
        const char *e_ = (expected);                                    \
        unit_checks++;                                                  \
        if (strcmp(a_, e_) != 0) {                                      \
            unit_failures++;                                            \
            printf("    %s:%d: %s == \"%s\", expected \"%s\"\n",        \
                   __FILE__, __LINE__, #actual, a_, e_);                \
        }                                                               \
    } while (0)

static inline int unit_summary(void)
{
    printf("\n%d checks, %d failures\n", unit_checks, unit_failures);
    return (unit_failures == 0) ? 0 : 1;
}

#endif /* UNIT_H */
