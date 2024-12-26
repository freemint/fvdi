/*
 * fVDI utility functions
 *
 * Copyright 1997-2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include "stdio.h"
#include "stdarg.h"

#include "os.h"
#include "relocate.h"
#include "stdlib.h"
#include "utility.h"
#include "globals.h"
#include "string.h"
#include "nf_ops.h"
#include "function.h"

/* referenced in simple.s */
void DRIVER_EXPORT event(long id_type, long data);

#ifndef SuperToUser
# define SuperToUser(ptr) Super(ptr)
#endif


/*
 * Global variables
 */

Access real_access = {
    {
        copymem,
        next_line,
        skip_space,
        get_token,
        equal,
        length,
        copy,
        cat,
        numeric,
        atol,
        error,
        fmalloc,
        free,
        kputs,
        ltoa,
        get_cookie,
        set_cookie,
        fixup_font,
        unpack_font,
        insert_font,
        get_size,
        allocate_block,
        free_block,
        cache_flush,
        misc,
        event
    },
    {
        /* vars: nowhere initialized or accessed? */
        0, 0
    }
};
Access *access = &real_access;

typedef struct _Circle {
    struct _Circle *prev;
    struct _Circle *next;
    long size;
} Circle;

static Circle *mblocks = 0;

long cpu = 0;

long mint = 0;
long magic = 0;

static long nf_print_id = 0;

long pid_addr = 0;        /* Copied into 'pid' when fVDI is installed */
long *pid = 0;
short mxalloc = 0;

char *block_chain = 0;

static struct nf_ops _nf_ops = { _nf_get_id, _nf_call, { 0, 0, 0 } };
static struct nf_ops *nf_ops;




static long nf_get_id(const char *feature_name)
{
    long id = 0;
    
    if (nf_ops != NULL)
    {
        id = NF_GET_ID(nf_ops, feature_name);
    }
    return id;
}


/*
 * Turn string (max four characters) into a long
 */
long str2long(const char *text)
{
    long v;

    v = 0;
    while (*text)
    {
        v <<= 8;
        v += (unsigned char) *text++;
    }

    return v;
}


long get_l(long addr)
{
    return *(long *)addr;
}


/*
 * Get a long value after switching to supervisor mode
 */
long get_protected_l(long addr)
{
    long oldstack, v;

    oldstack = (long)Super(0L);
    v = *(long *)addr;
    SuperToUser((void *)oldstack);

    return v;
}


void set_l(long addr, long value)
{
    *(long *)addr = value;
}


/*
 * Set a long value after switching to supervisor mode
 */
void set_protected_l(long addr, long value)
{
    long oldstack;

    oldstack = (long)Super(0L);
    *(long *)addr = value;
    SuperToUser((void *)oldstack);
}


/*
 * Returns the value of a cookie, or -1
 */
long DRIVER_EXPORT get_cookie(const char *cname, long super)
{
    long ptr, value, cname_l;
    long (*get)(long addr);

    cname_l = str2long(cname);

    get = super ? get_l : get_protected_l;

    ptr = get(0x5a0);

    value = -1;
    if (ptr)
    {
        long v = get(ptr);

        while (v && (v != cname_l))
        {
            ptr += 8;
            v = get(ptr);
        }
        if (v == cname_l)
            value = get(ptr + 4);
    }

    return value;
}


/*
 * Follows an XBRA chain and removes a link if found.
 * Returns 0 if XBRA id not found.
 */
long remove_xbra(long vector, const char *name)
{
    long link, *addr, name_l, xbra_l;

    link = vector;
    addr = (long *) get_protected_l(link);    /* Probably an exception vector */
    xbra_l = str2long("XBRA");
    name_l = str2long(name);

    while ((addr[-3] == xbra_l) && (addr[-2] != name_l))
    {
        link = (long) &addr[-1];
        addr = (long *) addr[-1];
    }
    if (addr[-3] != xbra_l)
        return 0;

    set_protected_l(link, addr[-1]);    /* Might well be an exception vector */

    return 1;
}


/*
 * Set a cookie value. Replace if there already is one.
 * Create/expand jar if needed.
 * Returns != 0 if an old cookie was replaced.
 */
long DRIVER_EXPORT set_cookie(const char *name, long value)
{
    long *addr, *old_addr;
    long name_l;
    int count;

    count = 0;
    name_l = str2long(name);
    addr = old_addr = (long *) get_protected_l(0x5a0);    /* _p_cookies */

    if (addr)
    {
        while (*addr && (*addr != name_l))
        {
            count++;
            addr += 2;
        }
        if (*addr == name_l)
        {
            addr[1] = value;
            return 1;
        }

        /* Must make sure there is room for the final count!  [010109] */

        if (count != addr[1] - 1)
        {
            addr[2] = 0;
            addr[3] = addr[1];
            addr[0] = name_l;
            addr[1] = value;
            return 0;
        }
    }

    addr = malloc((count + 8) * 8);

    copymem(old_addr, addr, count * 8);

    addr[count * 2 + 0] = name_l;
    addr[count * 2 + 1] = value;
    addr[count * 2 + 2] = 0;
    addr[count * 2 + 3] = count + 8;

    set_protected_l(0x5a0, (long) addr);    /* _p_cookies */

    return 0;
}


/*
 * Initialize an internal memory pool.
 * Returns zero on error.
 */
long initialize_pool(long size, long n)
{
    char *addr, *ptr;

    if ((size <= 0) || (n <= 0))
        return 0;

    if ((addr = malloc(size * n)) == NULL)
        return 0;

    block_size = size;
    ptr = 0;
    for (n = n - 1; n >= 0; n--)
    {
        block_chain = addr;
        *(char **) addr = ptr;
        ptr = addr;
        addr += size;
    }

    return 1;
}


/*
 * Allocate a block from the internal memory pool.
 */
char *DRIVER_EXPORT allocate_block(long size)
{
    char *addr;

    if ((size > block_size) || !block_chain)
        return 0;

    addr = block_chain;
    block_chain = *(char **) addr;
    *(long *) addr = block_size;    /* Make size info available */

    return addr;
}


/*
 * Free a block and return it to the internal memory pool.
 */
void DRIVER_EXPORT free_block(void *addr)
{
    *(char **) addr = block_chain;
    block_chain = addr;
}


/*
 * Fetch some interesting cookies
 */
static void check_cookies(void)
{
    long addr;

    cpu = get_cookie("_CPU", 0);
    if (cpu == -1)
        cpu = 0;
    if ((addr = get_cookie("MiNT", 0)) != -1)
        mint = (long)addr;
    if ((addr = get_cookie("MagX", 0)) != -1)
        magic = (long)addr;
    if ((addr = get_cookie("__NF", 0)) != -1 && addr != 0)
    {
        if (((NatFeatCookie *) addr)->magic == 0x20021021L)
        {
            _nf_ops.get_id = ((NatFeatCookie *) addr)->nfGetID;
            _nf_ops.call = ((NatFeatCookie *) addr)->nfCall;
            nf_ops = &_nf_ops;
        }
    }
#ifndef __mcoldfire__
    if (Supexec(_nf_detect))
        nf_ops = &_nf_ops;
#endif
    nf_print_id = nf_get_id(NF_ID_STDERR);
    /*
     * default to NatFeats logging when detected.
     * Can be overridden in config file
     */
    if (nf_print_id)
        debug_out = -1;
}



#include "string/memcpy.h"
#include "string/memmove.h"

#ifndef USE_LIBKERN
size_t strlen(const char *s)
{
    const char *p = s;

    while (*p++)
        ;

    return (size_t) (p - s) - 1;
}


long strcmp(const char *s1, const char *s2)
{
    char c1;

    do
    {
        if ((c1 = *s1++) == 0)
        {
            s2++;
            break;
        }
    } while (c1 == *s2++);

    return (long) (c1 - s2[-1]);
}


long strncmp(const char *s1, const char *s2, size_t n)
{
    char c1;
    long ns;     /* size_t can't be negative */

    ns = n;
    for (ns--; ns >= 0; ns--)
    {
        if ((c1 = *s1++) == 0)
        {
            s2++;
            break;
        }
        if (c1 != *s2++)
            break;
    }

    if (ns < 0)
        return 0L;

    return (long) (c1 - s2[-1]);
}


char *strstr(const char *s, const char *wanted)
{
    register const char *scan;
    register size_t len;
    register char firstc;

    if (!*s)
    {
        if (*wanted)
            return NULL;
        else
            return (char *) (s);
    } else if (!*wanted)
    {
        return (char *) (s);
    }

    /*
     * The odd placement of the two tests is so "" is findable.
     * Also, we inline the first char for speed.
     * The ++ on scan has been moved down for optimization.
     */
    firstc = *wanted;
    len = strlen(wanted);
    for (scan = s; *scan != firstc || strncmp(scan, wanted, len) != 0;)
        if (*scan++ == '\0')
            return NULL;
    return (char *) (scan);
}


long memcmp(const void *s1, const void *s2, size_t n)
{
    const char *s1c, *s2c;
    long ns;     /* size_t can't be negative */

    ns = n;
    s1c = (const char *)s1;
    s2c = (const char *)s2;
    for (ns--; ns >= 0; ns--)
    {
        if (*s1c++ != *s2c++)
            return (long)(s1c[-1] - s2c[-1]);
    }

    return 0L;
}
#endif


void DRIVER_EXPORT copy(const char *src, char *dest)
{
    while ((*dest++ = *src++) != 0)
        ;
}


#ifndef USE_LIBKERN
char *strcpy(char *dest, const char *src)
{
    copy(src, dest);

    return dest;
}


char *strncpy(char *dest, const char *src, size_t n)
{
    char c1, *d;
    long ns;

    d = dest;
    ns = n;
    for (ns--; ns >= 0; ns--)
    {
        c1 = *src++;
        *dest++ = c1;
        if (!c1)
            break;
    }
    for (ns--; ns >= 0; ns--)
        *dest++ = 0;

    return d;
}


char *strdup(const char *s)
{
    char *d;

    if ((d = (char *) malloc(strlen(s) + 1)) != NULL)
        strcpy(d, s);

    return d;
}
#endif


void DRIVER_EXPORT cat(const char *src, char *dest)
{
    while (*dest++)
        ;
    copy(src, dest - 1);
}


#ifndef USE_LIBKERN
char *strcat(char *dest, const char *src)
{
    cat(src, dest);

    return dest;
}


char *strchr(const char *s, long c)
{
    char ch, c1;

    if (!c)
        return (char *)s + strlen(s);

    c1 = c;
    while ((ch = *s++) != 0)
    {
        if (ch == c1)
            return (char *)s - 1;
    }

    return 0;
}


char *strrchr(const char *s, long c)
{
    char *found, ch, c1;

    if (!c)
        return (char *)s + strlen(s);

    c1 = c;
    found = 0;
    while ((ch = *s++) != 0)
    {
        if (ch == c1)
            found = (char *)s;
    }

    if (found)
        return found - 1;

    return 0;
}


void *memchr(const void *s, long c, size_t n)
{
    char ch, c1;
    char *m;
    long ns;

    m = (char *)s;
    c1 = c;
    ns = n;
    for (ns--; ns >= 0; ns--)
    {
        ch = *m++;
        if (ch == c1)
            return m - 1;
    }

    return 0;
}
#endif


long DRIVER_EXPORT length(const char *text)
{
#if __GNUC__ >= 12
	/* gcc >= 12 will generate implicit reference to strlen with the code below */
	return _mint_strlen(text);
#else
    int n;

    n = 0;
    while (*text++)
        n++;

    return n;
#endif
}


long DRIVER_EXPORT numeric(long ch)
{
    if ((ch >= '0') && (ch <= '9'))
        return 1;

    return 0;
}


int check_base(char ch, int base)
{
    if (numeric(ch) && (ch < '0' + base))
        return ch - '0';
    if ((ch >= 'a') && (ch <= 'z'))
        ch -= 'a' - 'A';
    if ((ch >= 'A') && (ch < 'A' + base - 10))
        return ch - 'A' + 10;

    return -1;
}


#ifndef USE_LIBKERN
int isdigit(int c)
{
    return (int) numeric(c);
}


int isxdigit(int c)
{
    return check_base(c, 16) >= 0;
}


int isalnum(int c)
{
    return check_base(c, 36) >= 0;   /* Base 36 has 0-9, A-Z */
}
#endif


int isspace(int c)
{
    switch (c)
    {
    case ' ':
    case '\f':
    case '\n':
    case '\r':
    case '\t':
    case '\v':
        return 1;
    }

    return 0;
}


#include "stdlib/atol.h"
#include "stdlib/ltoa.h"
#include "stdlib/ultoa.h"
#include "string/memset.h"


#ifndef USE_LIBKERN
#include "stdio/printf.h"


void qsort(void *base, long nmemb, long size, int (*compar) (const void *, const void *))
{
    static long incs[16] = { 1391376L, 463792L, 198768L, 86961L, 33936L, 13776L,
                             4592, 1968, 861, 336, 112, 48, 21, 7, 3, 1 };
    long i, j, k, h, j_size, h_size;
    short n;
    char buf[16], *v, *p1, *p2, *cbase;

    v = buf;
    if (size > (long)sizeof(buf))
    {
        v = malloc(size);
        if (!v)       /* Can't sort? */
            return;
    }

    cbase = (char *)base;
    for (k = 0; k < 16; k++)
    {
        h = incs[k];
        h_size = h * size;
        for (i = h; i < nmemb; i++)
        {
            j = i;
            j_size = j * size;
            p1 = v;
            p2 = cbase + j_size;
            for (n = size - 1; n >= 0; n--)
                *p1++ = *p2++;
            while ((j >= h) && (compar(v, cbase + j_size - h_size) < 0))
            {
                p1 = cbase + j_size;
                p2 = p1 - h_size;
                for (n = size - 1; n >= 0; n--)
                    *p1++ = *p2++;
                j -= h;
                j_size -= h_size;
            }
            p1 = cbase + j_size;
            p2 = v;
            for (n = size - 1; n >= 0; n--)
                *p1++ = *p2++;
        }
    }

    if (size > (long)sizeof(buf))
        free(v);
}
#endif


void *fmalloc(long size, long type)
{
    Circle *new;
    long bp;
    long *ppid = pid;

    if (ppid)
    {
        /* Pretend to be fVDI if possible */
        bp = *ppid;
        *ppid = basepage;
    }
    if (mint | magic)
    {
        if (!(type & 0xfff8))  /* Simple type? */
            type |= 0x4030;     /* Keep around, supervisor accessible */
        new = (Circle *)Mxalloc(size + sizeof(Circle), (int)type);
    } else
    {
        type &= 3;
        if (mxalloc)      /* Alternative if possible */
            new = (Circle *) Mxalloc(size + sizeof(Circle), (int)type);
        else
            new = (Circle *) Malloc(size + sizeof(Circle));
    }
    if (ppid)
    {
        *ppid = bp;
    }

    if ((long) new > 0)
    {
        if ((debug > 2) && !(silentx[0] & 0x01))
        {
            PRINTF(("Allocation at $%08lx, %ld bytes\n", (long) new, size));
        }
        if (memlink)
        {
            if (mblocks)
            {
                new->prev = mblocks->prev;
                new->next = mblocks;
                ((Circle *)((long)mblocks->prev & ~1))->next = new;
                mblocks->prev = (Circle *)((long)new | 1);
            } else
            {
                mblocks = new;
                new->prev = (Circle *)((long)new | 1);
                new->next = new;
            }
        }
        new->size = size + sizeof(Circle);
        *(long *) &new[1] = size;
        return (void *) &new[1];
    } else
    {
        kprintf("fVDI: fatal: M%salloc(%ld=$%lx, $%lx) failed\n", mint | magic | mxalloc ? "x" : "", size, size, type);
        return new;
    }
}


static short block_space[] = { 16, 48, 112, 240, 496, 1008, 2028, 4068, 8148, 16308 };
static char *block_free[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static Circle *block_used[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static short free_blocks[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
static short used_blocks[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static short allocated = 0;

#define ADDR_NOT_OK 0xfc000003


void allocate(long amount)
{
    const int sizes = (int)(sizeof(block_space) / sizeof(block_space[0]));
    char *buf;
    Circle *link, *last;
    int i;

    amount &= ~0x0fL;
    if (!amount)
        return;

    buf = fmalloc(amount * 1024, 3);
    if (!buf)
        return;

    if ((debug > 2) && !(silentx[0] & 0x02))
    {
        PRINTF(("       Malloc at $%08lx\n", (long) buf));
    }

    last = (Circle *)block_free[sizes - 1];
    for (i = 0; i < amount; i += 16)
    {
        link = (Circle *)&buf[i * 1024L];
        link->next = last;
        link->prev = 0;
        link->size = sizes - 1;
        last = link;
    }

    block_free[sizes - 1] = (char *)last;
    free_blocks[sizes - 1] += amount >> 4;
    allocated += amount >> 4;
}


#ifdef FVDI_DEBUG
static void memory_statistics(void)
{
    const int sizes = sizeof(block_space) / sizeof(block_space[0]);
    int n;

    PRINTF(("       %d: ", allocated));
    for (n = 0; n < sizes; n++)
    {
        PRINTF(("%d/%d ", used_blocks[n], free_blocks[n]));
    }
    PUTS("\n");
}


static void search_links(Circle *srch)
{
    int n;
    const int sizes = sizeof(block_space) / sizeof(block_space[0]);
    Circle *link, *next, *first, *found = 0;
    long dist, dist_min;
    int dist_n = 0;

    dist_min = 999999999;
    for (n = 0; n < sizes; n++)
    {
        link = (Circle *)block_free[n];
        while (link)
        {
            if (((long)link & ADDR_NOT_OK) ||
                ((unsigned int)(link->size & 0xffff) >=
                 sizeof(block_space) / sizeof(block_space[0])) ||
                 !(link->size >> 16))
                break;

            dist = (long)srch - (long)link;
            if ((dist > 0) && (dist < dist_min))
            {
                dist_min = dist;
                dist_n = n | 0x8000;
                found = link;
            }

            link = link->next;
        }

        link = first = block_used[n];
        while (link)
        {
            if (((long)link & ADDR_NOT_OK) ||
                ((unsigned int)(link->size & 0xffff) >=
                 sizeof(block_space) / sizeof(block_space[0])) ||
                 !(link->size >> 16))
                break;

            dist = (long)srch - (long)link;
            if ((dist > 0) && (dist < dist_min))
            {
                dist_min = dist;
                dist_n = n;
                found = link;
            }

            next = link->next;
            if ((next == first) || (next->prev != link))
                break;
            link = next;
        }
    }

    PRINTF(("At distance %ld: $%08lx(%d %s)\n", dist_min, (long) found, dist_n & 0x7fff, dist_n & 0x8000 ? "free" : "used"));
}


static void display_links(Circle *first)
{
    Circle *link, *last;
    int m = 0;

    PRINTF(("Links: $%08lx", (long) first));

    last = first;
    if (first)
    {
        link = first->next;
        while (link != first)
        {
            if (m++ > 1000)
                break;
            PRINTF(("->$%08lx", (long) link));
            if (link->prev != last)
            {
                PRINTF(("($%08lx)", (long) link->prev));
                break;
            }
            last = link;
            link = link->next;
            if ((long)link & ADDR_NOT_OK)
                break;
        }
    }
    PUTS("\n");
}


void check_memory(void)
{
    int m, n;
    const int sizes = sizeof(block_space) / sizeof(block_space[0]);
    Circle *link, *next, *first;
    int error, statistics;

    statistics = 0;
    for (n = 0; n < sizes; n++)
    {
        link = (Circle *)block_free[n];
        m = 0;
        error = 0;
        while (link)
        {
            m++;
            if ((long)link & ADDR_NOT_OK)
            {
                PUTS("Bad free list linkage at ");
                error = 1;
            }
            if (error)
            {
                PRINTF(("$%08lx(%d,%d)\n", (long) link, m, n));
                break;
            }

            link = link->next;
        }

        if (!error && (m != free_blocks[n]))
        {
            PRINTF(("Wrong number of free blocks (%d/%d)\n", m, free_blocks[n]));
            error = 1;
        }

        if (error && !statistics)
        {
            statistics = 1;
            memory_statistics();
        }

        link = first = block_used[n];
        m = 0;
        error = 0;
        while (link)
        {
            m++;
            if ((long)link & ADDR_NOT_OK)
            {
                PUTS("Bad used list link at ");
                error = 1;
            } else if ((unsigned int)(link->size & 0xffff) >=
                       sizeof(block_space) / sizeof(block_space[0]) ||
                       !(link->size >> 16))
            {
                PUTS("\n");
                search_links(link);
                PUTS("Bad used list size at ");
                error = 1;
            } else if ((long)link->next & ADDR_NOT_OK)
            {
                PUTS("\n");
                search_links(link);
                PUTS("Bad used list linkage at ");
                error = 1;
            }
            next = link->next;
            if (next->prev != link)
            {
                PUTS("\n");
                search_links(next);
                PRINTF(("Bad used list prev linkage $%08lx $%08lx ", (long) next, (long) next->prev));
                error = 1;
            }

            if (error)
            {
                PRINTF(("$%08lx (%d,%d)\n", (long) link, m, n));
                display_links(block_used[n]);
                break;
            }
            if (next == first)
                break;
            link = next;
        }

        if (!error && (m != used_blocks[n]))
        {
            PRINTF(("Wrong number of used blocks (%d:%d/%d)\n", n, m, used_blocks[n]));
            display_links(block_used[n]);
            error = 1;
        }

        if (error && !statistics)
        {
            statistics = 1;
            memory_statistics();
        }
    }
}
#endif /* FVDI_DEBUG */


#define OS_MARGIN 64
#define MIN_BLOCK 32
#define LINK_SIZE (sizeof(Circle))
#define MIN_KB_BLOCK
void *DRIVER_EXPORT malloc(size_t size)
{
    int m, n;
    const int sizes = (int)(sizeof(block_space) / sizeof(block_space[0]));
    char *block;
    Circle *link, *next;

    size += ext_malloc;

#ifdef FVDI_DEBUG
    if (check_mem)
        check_memory();
#endif

    if (old_malloc || (size > 16 * 1024 - OS_MARGIN - (long)LINK_SIZE))
        return fmalloc(size, 3);

    /* This will always break eventually thanks to the if-statement above */
    for (n = 0; n < sizes; n++)
    {
        if ((long)size <= block_space[n])
            break;
    }

    if ((debug > 2) && !(silentx[0] & 0x02))
    {
        PRINTF(("Alloc: Need block of size %d/%ld\n", n, size));
    }

    if (!block_free[n])
    {
        for (m = n + 1; m < sizes; m++)
        {
            if (block_free[m])
                break;
        }
        if (m >= sizes)
        {
            m = sizes - 1;
            block_free[m] = fmalloc(16 * 1024 - OS_MARGIN, 3);
            if (!block_free[m])
                return 0;
            if ((debug > 2) && !(silentx[0] & 0x02))
            {
                PRINTF(("       Malloc at $%08lx\n", (long) block_free[m]));
            }
            link = (Circle *)block_free[m];
            link->next = 0;
            link->prev = 0;
            link->size = m;
            free_blocks[m]++;
            allocated++;
        }
        for (; m > n; m--)
        {
            if ((debug > 2) && !(silentx[0] & 0x02))
            {
                PUTS("       Splitting\n");
            }
            block_free[m - 1] = block_free[m];
            link = (Circle *)block_free[m];
            block_free[m] = (char *)link->next;
            free_blocks[m]--;
            next = (Circle *)(block_free[m - 1] + block_space[m - 1] + LINK_SIZE);
            link->next = next;
            link->prev = 0;
            link->size = m - 1;
            free_blocks[m - 1]++;
            link = next;
            link->next = 0;
            link->prev = 0;
            link->size = m - 1;
            free_blocks[m - 1]++;
        }
    } else
    {
        if ((debug > 2) && !(silentx[0] & 0x02))
        {
            PUTS("       Available\n");
        }
    }

    block = block_free[n];
    block_free[n] = (char *)((Circle *)block)->next;

    if ((debug > 2) && !(silentx[0] & 0x02))
    {
        PRINTF(("       Allocating at $%08lx (next at $%08lx)\n", (long) block, (long) block_free[n]));
    }

    ((Circle *)block)->size = (((Circle *)block)->size & 0xffff) + (size << 16);

    if (1 || memlink)
    {
        Circle *new = (Circle *)block;

        if (block_used[n])
        {
            new->prev = block_used[n]->prev;
            new->next = block_used[n];
            block_used[n]->prev->next = new;
            block_used[n]->prev = new;
        } else
        {
            block_used[n] = new;
            new->prev = new;
            new->next = new;
        }
    }

    free_blocks[n]--;
    used_blocks[n]++;

#ifdef FVDI_DEBUG
    if ((debug > 2) && !(silentx[0] & 0x02))
    {
        memory_statistics();
    }
#endif

    *(long *)(block + sizeof(Circle)) = block_space[n];
    return block + sizeof(Circle);
}


void *realloc(void *addr, size_t new_size)
{
    Circle *current;
    size_t old_size;
    void *new;

    if (!addr)
        return malloc(new_size);
    if (!new_size)
    {
        free(addr);
        return 0;
    }

    new = malloc(new_size);
    if ((long)new <= 0)
        return 0;
    current = &((Circle *)addr)[-1];
    if ((long)current->prev & 1)
        old_size = current->size - sizeof(Circle);
    else
        old_size = current->size >> 16;

    copymem_aligned(addr, new, old_size < new_size ? old_size : new_size);
    free(addr);

    if ((debug > 2) && !(silentx[0] & 0x01))
    {
        PRINTF(("Reallocation from size %ld at $%08lx to %ld\n", old_size, (long) addr, new_size));
    }

    return new;
}


long free_size(void *addr)
{
    Circle *current;
    long size, ret;

    if (!addr)
        return 0;

    current = &((Circle *) addr)[-1];

    if (!((long) current->prev & 1))
    {
        size = current->size & 0xffff;
#if 1
        /*
         * FIXME:
         * memlink is forced here in free(). It is also forced in malloc().
         * But it is not forced in fmalloc(), so current->prev may be 0.
         * This can happen because real_access.funcs.malloc == fmalloc,
         * so drivers indirectly call fmalloc(), hence current->prev is set to 0
         * if nomemlink is used.
         */
        if (1 || memlink)
        {
            if (block_used[size] == current)
            {
                block_used[size] = current->next;
                if (current->next == current)
                    block_used[size] = NULL;
            }
            if (current->prev == 0)
            {
                access->funcs.puts("BUG!! current->prev == 0");
                access->funcs.puts("Please comment out nomemlink in fvdi.sys");
                for (;;) ;
            }
            current->prev->next = current->next;
            current->next->prev = current->prev;
        }
#endif
        current->next = (Circle *) block_free[size];
        block_free[size] = (char *) current;
        free_blocks[size]++;
        used_blocks[size]--;
        return 0;
    }

    if (memlink)
    {
#if 0
        current->prev->next = current->next;
#else
        ((Circle *) ((long) current->prev & ~1))->next = current->next;
#endif
        current->next->prev = current->prev;
    }
    size = current->size;

    if (pid)
    {                                   /* Pretend to be fVDI if possible */
        long bp = *pid;
        *pid = basepage;
        ret = Mfree(current);
        *pid = bp;
    } else
    {
        ret = Mfree(current);
    }

    if (ret)
        return ret;
    else
        return size;
}


void DRIVER_EXPORT free(void *addr)
{
    Circle *current;
    long size;

    if (!addr)
        return;

    current = &((Circle *) addr)[-1];

    if (!((long)current->prev & 1))
    {
        size = current->size & 0xffff;
        /*
         * FIXME:
         * memlink is forced here in free(). It is also forced in malloc().
         * But it is not forced in fmalloc(), so current->prev may be 0.
         * This can happen because real_access.funcs.malloc == fmalloc,
         * so drivers indirectly call fmalloc(), hence current->prev is set to 0
         * if nomemlink is used.
         */
        if (1 || memlink)
        {
            if (block_used[size] == current)
            {
                block_used[size] = current->next;
                if (current->next == current)
                    block_used[size] = NULL;
            }
            if (current->prev == 0)
            {
                access->funcs.puts("BUG!! current->prev == 0\n");
                access->funcs.puts("Please comment out nomemlink in fvdi.sys\n");
                for (;;) ;
            }
            current->prev->next = current->next;
            current->next->prev = current->prev;
        }
        current->next = (Circle *)block_free[size];
        block_free[size] = (char *)current;
        free_blocks[size]++;
        used_blocks[size]--;
        return;
    }

    if (memlink)
    {
#if 0
        current->prev->next = current->next;
#else
        ((Circle *)((long)current->prev & ~1))->next = current->next;
#endif
        current->next->prev = current->prev;
    }

    if (pid)
    {           /* Pretend to be fVDI if possible */
        long bp = *pid;
        *pid = basepage;
        Mfree(current);
        *pid = bp;
    } else
    {
        Mfree(current);
    }
}


long free_all(void)
{
    long oldstack, total, ret, err;

    if (!mblocks)
        return 0;

    oldstack = (long)Super(0L);

    err = 0;
    total = 0;
    while (mblocks->prev != mblocks)
    {
        /* Remove the last until there is no more */
        ret = free_size((void *)&mblocks->prev[1]);
        if (ret > 0)
            total += ret;
        else
            err = 1;
    }

    SuperToUser((void *)oldstack);

    if (err)
        total = -total;
    return total;
}


long DRIVER_EXPORT kputs(const char *text)
{
    if ((debug_out == -3) && debug_file)
    {
        int file;

        if (((file = (int)Fopen(debug_file, O_WRONLY)) < 0) ||
            (Fseek(0, file, SEEK_END) < 0) ||
            (Fwrite(file, strlen(text), text) < 0))
        {
            free(debug_file);
            debug_file = 0;
            debug_out = -2;
            kputs("Write to debug file failed!\n");
            kputs(text);
        }
        if (file >= 0)
            Fclose(file);
    } else if (debug_out == -2)
    {
        (void) Cconws(text);
        if (strchr(text, '\n'))
            Cconout(0x0d);
    } else if (debug_out == -1)
    {
        if (nf_print_id != 0)
        {
            nf_ops->call(nf_print_id | 0, text);
        } else
        {
            (void) Cconws(text);
            if (strchr(text, '\n'))
                Cconout(0x0d);
        }
    } else
    {
        while (*text)
        {
            if (*text == 0x0a)
                Bconout(debug_out, 0x0d);
            Bconout(debug_out, *text++);
        }
    }

    return 1;
}


long DRIVER_EXPORT equal(const char *str1, const char *str2)
{
    char ch1, ch2;

    do
    {
        ch1 = *str1++;
        ch2 = *str2++;
        if (ch1 != ch2)
        {
            if ((ch1 >= 'A') && (ch1 <= 'Z'))
            {
                if ((ch1 | 32) != ch2)
                    return 0;
            } else
                return 0;
        }
    } while (ch1 && ch2);

    return 1;
}


long DRIVER_EXPORT misc(long func, long par, const char *token)
{
    (void) token;
    switch ((int)func)
    {
    case 0:
        switch ((int)par)
        {
        case 0:
            return key_pressed;
        case 1:
            return debug;
        }
    }

    return 0;
}


void DRIVER_EXPORT error(const char *text1, const char *text2)
{
    kputs("fVDI: ");
    kputs(text1);
    if (text2)
        kputs(text2);
    kputs("\n");
    if (debug_out < 0)
    {
        (void) Cconws("fVDI: ");
        (void) Cconws(text1);
        if (text2)
            (void) Cconws(text2);
        (void) Cconws("\r\n");
    }
}


const char *DRIVER_EXPORT next_line(const char *ptr)
{
    while (1)
    {
        if (!*ptr)
            return 0;
        else if ((*ptr == 10) || (*ptr == 13))
            break;
        ptr++;
    }
    ptr++;
    while (1)
    {
        if (!*ptr)
            return 0;
        else if ((*ptr != 10) && (*ptr != 13))
            break;
        ptr++;
    }
    return ptr;
}


const char *DRIVER_EXPORT skip_space(const char *ptr)
{
    if (!ptr)
        return 0;
    while (1)
    {
        if (!*ptr)
            return 0;
        else if (*ptr <= ' ')
            ;
        else if (*ptr == '#')
        {
            if ((ptr = next_line(ptr)) == NULL)
                return 0;
            else
                continue;     /* Updating of ptr already done! */
        } else
            return ptr;
        ptr++;
    }
}


const char *skip_only_space(const char *ptr)
{
    if (!ptr)
        return 0;
    while ((*ptr == ' ') || (*ptr == '\t'))
    {
        ptr++;
    }

    return ptr;
}


const char *DRIVER_EXPORT get_token(const char *ptr, char *buf, long n)
{
    if (*ptr == '=')            /* Assignment token? */
        *buf++ = *ptr++;
    else if (*ptr != '\"')
    {
        /* Get ordinary token */
        while (--n)
        {
            if ((*ptr <= ' ') || (*ptr == '='))
                break;
            *buf++ = *ptr++;
        }
    } else
    {                    /* Get quoted token */
        ptr++;
        while (--n)
        {
            if (((*ptr < ' ') && (*ptr != '\t')) || (*ptr == '\"'))
                break;
            *buf++ = *ptr++;
        }
        if (*ptr == '\"')
            ptr++;
    }
    *buf = '\0';

    return ptr;
}


/*
 * Returns the size of a file
 */
long DRIVER_EXPORT get_size(const char *name)
{
    _DTA info, *old;
    long file_size;
    int error;

    old = Fgetdta();
    Fsetdta((void *)&info);
    error = Fsfirst(name, FA_RDONLY|FA_HIDDEN|FA_SYSTEM);
    Fsetdta(old);

    if (!error)
        file_size = info.dta_size;
    else
        file_size = -1;

    return file_size;
}


void DRIVER_EXPORT event(long id_type, long data)
{
    long xy;
    long *xyp;

    /* Really needs to do something about the id part */
    switch ((unsigned int)(id_type & 0xffff))
    {
    case 0:    /* Initialize */
        stand_alone = 1;
        break;
    case 1:    /* Relative mouse movement */
        if (!screen_wk->mouse.forced)
        {
            if (data)
            {
                data = ((screen_wk->mouse.position.x + (data >> 16)) << 16) | ((screen_wk->mouse.position.y + data) & 0xffff);
                data = vector_call(screen_wk->vector.motion, data);
                data = vector_call(screen_wk->vector.draw, data);
            }
        }
        break;
    case 2:    /* Absolute mouse movement */
        if (!screen_wk->mouse.forced)
        {
            xyp = (long *) &screen_wk->mouse.position.x;
            xy = *xyp;
            if (data != xy)
            {
                if (move_mouse)
                    vector_call(user_cur, data);
                data = vector_call(screen_wk->vector.motion, data);
                data = vector_call(screen_wk->vector.draw, data);
            }
        }
        break;
    case 3:    /* Button change */
        if (data != screen_wk->mouse.buttons)
        {
            screen_wk->mouse.buttons = data;
            data = (data << 16) | (data & 0xffff);
            data = vector_call(screen_wk->vector.button, data);
        }
        break;
    case 4:    /* Wheel movement */
        if (data)
        {
            data = vector_call(screen_wk->vector.wheel, data);
        }
        break;
    case 5:    /* Vertical blank */
        /* data = vector_call(screen_wk->vector.vblank, data); */
        break;
    case 6:    /* Forced absolute move */
        xyp = (long *) &screen_wk->mouse.position.x;
        xy = *xyp;
        if (data != xy)
        {
            screen_wk->mouse.forced = 1;
            data = vector_call(screen_wk->vector.motion, data);
            data = vector_call(screen_wk->vector.draw, data);
        }
        break;
    }
}


long init_utility(void)
{
    long tmp;

    check_cookies();

    tmp = (long) Mxalloc(10, 3);      /* Try allocating a little memory */
    if (tmp == -32)
        mxalloc = 0;
    else if (tmp < 0)
        return 0;                     /* Should not happen */
    else
    {
        mxalloc = 1;
        Mfree((void *)tmp);
    }

    tmp = get_protected_l(0x4f2);       /* _sysbase */
    if ((get_protected_l(tmp) & 0x0000ffff) < 0x0102)
        tmp = 0;
    else
    {
        tmp = get_protected_l(tmp + 40); /* p_run (ptr to current base page) */
        if (get_protected_l(tmp) != basepage)    /* Not what it should be? */
            tmp = 0;
    }

    if (!mint && !magic)     /* Probably a bad idea under multitasking */
        pid_addr = tmp;

    return 1;
}


#ifdef __GNUC__
#pragma GCC optimize "-O1"
#endif
void *DRIVER_EXPORT calloc(size_t nmemb, size_t size)
{
    void *p;
    size *= nmemb;
    p = malloc(size);
    if (p)
        memset(p, 0, size);
    return p;
}
