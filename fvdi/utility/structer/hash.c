/*
 * Hashing functions
 *
 * Copyright 1997-2002, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "hash.h"
#include "memory.h"

#define IFHT       if (opt & 0x08)

extern int opt;

static unsigned int hashsize = NHASH;
#if 0
static NTentry *nhash[NHASH];
#else
static NTentry **nhash = NULL;
#endif


#if 0
static double mod(double a, double b)
{
    return a - floor(a / b) * b;
}

static int slow_hash(const char *str, int len)
{
    double k, k2, c;
    int key, i;

    key = i = 0;
    k = pow(2.0, 15.0);
    k2 = pow((k - 1), 2.0);
    c = sqrt(k2 / k);

    while (*str != '\0')
        key = mod((key + ((unsigned) *str++) * pow(128.0, abs(4 - i++) % 4)), k);
    return (int) mod((key * key / c), (double) (hashsize + 1));
}
#else
static unsigned int fast_hash(const char *str, int len)
{
    unsigned int key;

    key = 0;
    while (len)
    {
        key = (key << 5) ^ *str++ ^ key;
        len--;
    }
    return key % hashsize;
}
#endif

#define hash(str, len) fast_hash(str, len)



static NTentry *find(const char *str, int len)
{
    NTentry *np;

    np = nhash[hash(str, len)];
    while (np && (len != np->len || strncmp(np->string, str, len) != 0))
        np = np->link;
    return np;
}


static char *copy(const char *str, int len)
{
    char *new;

    if ((new = (char *) malloc(len + 1)) == 0)
    {
        fprintf(stderr, "Can't allocate storage for new identifier!\n");
        exit(1);
    }
    strncpy(new, str, len);
    new[len] = '\0';
    return new;
}


NTentry *new_string(const char *str, int len)
{
    NTentry *new, *fnd;
    char *cpy;
    int t;

    if (!nhash)
    {
        IFHT printf("Allocating hash table\n");

        nhash = (NTentry **) malloc(hashsize * sizeof(NTentry *));
        if (!nhash)
        {
            fprintf(stderr, "Out of memory!\n");
            exit(1);
        }
        memset(nhash, 0, hashsize * sizeof(NTentry *));
    }
    if ((fnd = find(str, len)))
    {
        IFHT printf("Successful hash search\n");

        return fnd;
    } else
    {
        new = (NTentry *) memalloc(sizeof(NTentry));
        cpy = copy(str, len);
        new->string = cpy;
        new->len = len;
        t = hash(cpy, len);
        new->link = nhash[t];
        nhash[t] = new;
        return new;
    }
}


void hash_stat(int *total, int *used, int *maxno)
{
    int i, count;
    NTentry *ptr;

    *total = hashsize;
    *used = *maxno = 0;
    for (i = 0; i < hashsize; i++)
    {
        if (nhash[i])
        {
            (*used)++;
            count = 0;
            ptr = nhash[i];
            while (ptr)
            {
                count++;
                ptr = ptr->link;
            }
            if (count > *maxno)
                *maxno = count;
        }
    }
}


void init_hash(int size)
{
    if (size)
        hashsize = size;
}
