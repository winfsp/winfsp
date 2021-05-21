/**
 * @file tlib/injection.c
 *
 * @copyright 2014-2021 Bill Zissimopoulos
 */

#include <tlib/injection.h>
#include <tlib/callstack.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#undef NDEBUG
#include <assert.h>

#define NBUCKETS                        256
struct injection_cond_s
{
    struct injection_cond_s *cnext;
    char *sym;
    unsigned trigger, count;
};
struct injection_entry_s
{
    struct injection_entry_s *hnext;
    char *name;
    struct injection_cond_s *clist;
};
struct injection_htab_s
{
    struct injection_entry_s **buckets;
};

static inline size_t hash_chars(const char *s, size_t length)
{
    /* djb2: see http://www.cse.yorku.ca/~oz/hash.html */
    size_t h = 5381;
    for (const char *t = s + length; t > s; ++s)
        h = 33 * h + *s;
    return h;
}
static struct injection_htab_s *injection_htab()
{
    static struct injection_htab_s *htab;
    if (0 == htab)
    {
        htab = calloc(1, sizeof *htab);
        assert(0 != htab);
        htab->buckets = calloc(NBUCKETS, sizeof(struct injection_entry_s *));
    }
    return htab;
}
static struct injection_entry_s *injection_lookup(const char *name)
{
    struct injection_htab_s *htab = injection_htab();
    size_t i = hash_chars(name, strlen(name)) & (NBUCKETS - 1);
    for (struct injection_entry_s *entry = htab->buckets[i]; entry; entry = entry->hnext)
        if (0 == strcmp(entry->name, name))
            return entry;
    return 0;
}
static struct injection_entry_s *injection_insert(const char *name)
{
    struct injection_htab_s *htab = injection_htab();
    size_t i = hash_chars(name, strlen(name)) & (NBUCKETS - 1);
    struct injection_entry_s *entry = calloc(1, sizeof *entry);
    assert(0 != entry);
    entry->name = strdup(name);
    entry->hnext = htab->buckets[i];
    htab->buckets[i] = entry;
    return entry;
}
static struct injection_cond_s *injection_cond_get(struct injection_entry_s *entry, const char **syms)
{
    struct injection_cond_s *deinjection_centry = 0;
    for (struct injection_cond_s *centry = entry->clist; centry; centry = centry->cnext)
        if ('*' == centry->sym[0] && '\0' == centry->sym[1])
            deinjection_centry = centry;
        else
        {
            for (const char *sym; 0 != (sym = *syms); syms++)
                if (0 == strcmp(centry->sym, sym))
                    return centry;
        }
    return deinjection_centry;
}
static void injection_cond_set(struct injection_entry_s *entry, const char *sym, unsigned trigger)
{
    for (struct injection_cond_s *centry = entry->clist; centry; centry = centry->cnext)
        if (0 == strcmp(centry->sym, sym))
        {
            centry->trigger = trigger;
            return;
        }
    struct injection_cond_s *centry = calloc(1, sizeof *centry);
    assert(0 != centry);
    centry->sym = strdup(sym);
    centry->trigger = trigger;
    centry->cnext = entry->clist;
    entry->clist = centry;
}
static void injection_cond_remove(struct injection_entry_s *entry, const char *sym)
{
    struct injection_cond_s **p = &entry->clist;
    for (; *p; p = &(*p)->cnext)
        if (0 == strcmp((*p)->sym, sym))
            break;
    if (*p) /* did we find the condition? */
    {
        struct injection_cond_s *q = *p;
        *p = q->cnext;
        free(q->sym);
        free(q);
    }
}

void *tlib_injection(const char *name)
{
    struct injection_entry_s *entry = injection_lookup(name);
    if (0 == entry)
        entry = injection_insert(name);
    return entry;
}
int tlib_injection_trace(void *injection)
{
    if (0 == ((struct injection_entry_s *)injection)->clist)
        return 0;
    struct tlib_callstack_s stack;
    tlib_callstack(2, TLIB_MAX_SYMRET, &stack);
    struct injection_cond_s *centry = injection_cond_get(injection, stack.syms);
    if (0 == centry)
        return 0;
    return centry->count++ == centry->trigger || ~0 == centry->trigger;
}
void tlib_injection_enable(const char *name, const char *sym, unsigned trigger)
{
    struct injection_entry_s *entry = tlib_injection(name);
    injection_cond_set(entry, sym, trigger);
}
void tlib_injection_disable(const char *name, const char *sym)
{
    struct injection_entry_s *entry = tlib_injection(name);
    injection_cond_remove(entry, sym);
}
