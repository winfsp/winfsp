/**
 * @file tlib/testsuite.h
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#ifndef TLIB_TESTSUITE_H_INCLUDED
#define TLIB_TESTSUITE_H_INCLUDED

#include <stdlib.h>

/**
 * Assert macro
 *
 * This macro works similarly to the Standard C assert macro, except for the following differences:
 *
 * <ul>
 * <li>The macro always checks the specified expression regardless of the NDEBUG macro.</li>
 * <li>The macro aborts the execution of the current test, but not necessarily the execution of the
 * whole testsuite.</li>
 * </ul>
 */
#define ASSERT(expr)\
    (!(expr) ? (tlib__assert(__func__, __FILE__, __LINE__, #expr), abort()) : (void)0)

/**
 * Register a test suite for execution.
 *
 * Test suites are simple functions with prototype <code>void testsuite()</code>. When executed
 * they register individual test cases for execution.
 */
#define TESTSUITE(fn)\
    do\
    {\
        void fn(void);\
        tlib_add_test_suite(#fn, fn);\
    } while (0)

/**
 * Register a test case for execution.
 *
 * Test cases are simple functions with prototype <code>void testcase()</code>.
 */
#define TEST(fn)\
    do\
    {\
        void fn(void);\
        tlib_add_test(#fn, fn);\
    } while (0)

/**
 * Register an optional test case for execution.
 *
 * Test cases are simple functions with prototype <code>void testcase()</code>.
 * Optional tests are not executed by default.
 */
#define TEST_OPT(fn)\
    do\
    {\
        void fn(void);\
        tlib_add_test_opt(#fn, fn);\
    } while (0)

void tlib_add_test_suite(const char *name, void (*fn)(void));
void tlib_add_test(const char *name, void (*fn)(void));
void tlib_add_test_opt(const char *name, void (*fn)(void));

/**
 * Printf function.
 *
 * Use this to produce output in the appropriate tlib file stream. This function uses the local
 * printf facilities and understands the same format strings.
 */
#if defined(__GNUC__)
void tlib_printf(const char *fmt, ...) __attribute__ ((format (printf, 1, 2)));
#else
void tlib_printf(const char *fmt, ...);
#endif
/**
 * Run tests.
 *
 * This function will first execute all registered test suites, thus giving them the chance to
 * register any test cases. It will then execute all registered test cases according to the
 * command line arguments passed in argv. The command line syntax is a follows:
 *
 * Usage: testprog [--list][[--tap][--no-abort][--repeat-forever] [[+-]TESTNAME...]
 *
 * <ul>
 * <li>--list - list tests only</li>
 * <li>--tap - produce output in TAP format</li>
 * <li>--no-abort - do not abort all tests when an ASSERT fails
 * (only the current test is aborted)</li>
 * <li>--repeat-forever - repeat tests forever</li>
 * </ul>
 *
 * By default all test cases are executed unless specific test cases are named. By default optional
 * test cases are not executed. To execute a specific test case specify its TESTNAME; if it is an
 * optional test case specify +TESTNAME. To excluse a test case specify -TESTNAME.
 *
 * TESTNAME may also contain a single asterisk at the end; for example, mytest* will match all test
 * cases that have names starting with "mytest".
 *
 * @param argc
 *     Argument count.
 * @param argv
 *     Argument vector.
 */
void tlib_run_tests(int argc, char *argv[]);

void tlib__assert(const char *func, const char *file, int line, const char *expr);

#endif
