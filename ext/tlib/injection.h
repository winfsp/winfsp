/**
 * @file tlib/injection.h
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

/* NOTE: This header may usefully be included multiple times.
 * The TLIB_INJECT() macro will be redefined based on whether
 * TLIB_INJECTIONS_ENABLED is defined.
 */

#undef TLIB_INJECT
#if defined(TLIB_INJECTIONS_ENABLED)
#define TLIB_INJECT(name, stmt)         \
    do\
    {\
        static void *injection = 0;\
        if (0 == injection)\
            injection = tlib_injection(name);\
        if (tlib_injection_trace(injection))\
            stmt;\
    } while (0)
#else
#define TLIB_INJECT(name, stmt)         do {} while (0)
#endif

void *tlib_injection(const char *name);
int tlib_injection_trace(void *injection);
void tlib_injection_enable(const char *name, const char *sym, unsigned trigger);
void tlib_injection_disable(const char *name, const char *sym);
