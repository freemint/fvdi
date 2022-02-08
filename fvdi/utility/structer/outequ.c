/*
 * Assembly equ output functions
 *
 * Copyright 1997-2002, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#define GIVE_UP_ON_ERROR

#include "misc.h"
#include "expr.h"
#include "list.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#undef TEST
#define OT(text)     if (opt & 0x04) printf("outequ.c: %s\n", text)

static const char *outer[10];
static int count;

#ifdef GIVE_UP_ON_ERROR
#  define EXIT(x) exit(x)
#else
#  define EXIT(x)
#endif


#define ALIGN(pos) (((pos) + 1) & ~1)


static int list_to_equ(char *base, List defs, int pos, List all_defs, int *size);
static int ulist_to_equ(char *base, List defs, int pos, List all_defs, int *size);


static void push(const char *name)
{
    if (count >= (int)(sizeof(outer) / sizeof(outer[0])))
    {
        fprintf(stderr, "structure nested too deeply\n");
        EXIT(1);
	}
    outer[count] = name;
    count++;
}

static void pop(void)
{
    assert(count > 0);
    count--;
}


static void out(const char *name, int pos)
{
    int i;

    for (i = 0; i < count; i++)
    {
        if (outer[i])
            printf("%s_", outer[i]);
    }
    printf("%s\t=\t%d\n", name, pos);
}


static int compare(const char *name, Identifier struct_id)
{
    if (!struct_id)
        return 0;
    return !strcmp(name, struct_id->string);
}


static Expression locate_typedef(char *name, List all_defs)
{
    Listelement ptr;
    Expression cur_expr;
    Expression type_expr;
    Expression expr;
    int type;

    ptr = FIRST(all_defs);
    while (ptr)
    {
        cur_expr = EXPR(ptr);
        if (cur_expr->type == _Typedefexpr)
        {
            if (!cur_expr->info.var.id)
            {
                fprintf(stderr, "No id!\n");
                EXIT(1);
            } else if (compare(name, cur_expr->info.var.id->info.id.name))
            {
                return cur_expr;
            }
        }
        NEXT(ptr);
    }
    return NULL;
}


static int expr_to_equ(Expression cur_expr, int pos, List all_defs, int *size)
{
    Expression type_expr, expr, id;
    int type;
    char *name, *typedef_name;
    int oldpos;
    int count;
    int dummy;

    id = cur_expr->info.var.id;
    count = id->info.id.count;
    type_expr = cur_expr->info.var.type;
    if (type_expr->type != _Typeexpr)
    {
        fprintf(stderr, "Unsupported construction (B)!\n");
        EXIT(1);
    }
    if (cur_expr->info.var.id->info.id.name)
        name = cur_expr->info.var.id->info.id.name->string;
    else
        name = 0;

    switch (type_expr->info.def.sort)
    {
    case _Char:
#if 0
        if (count > 0)
            pos = ALIGN(pos);
#endif
        out(name, pos);
        *size = 1;
        break;

    case _Short:
        pos = ALIGN(pos);
        out(name, pos);
        *size = 2;
        break;

    case _Int:
        pos = ALIGN(pos);
        out(name, pos);
        *size = 4;
        break;

    case _Long:
        pos = ALIGN(pos);
        out(name, pos);
        *size = 4;
        break;

    case _Structdef:
        expr = type_expr->info.def.type;
        if (expr->type != _Structexpr)
        {
            fprintf(stderr, "Bad type (E)!\n");
            EXIT(1);
        }
        pos = ALIGN(pos);
        out(name, pos);
        dummy = list_to_equ(name, expr->info.strct.defs, pos, all_defs, size);
        *size = ALIGN(*size);
        break;

    case _Struct:
        fprintf(stderr, "Not yet supported (C)!\n");
        EXIT(1);
        break;

    case _Uniondef:
        expr = type_expr->info.def.type;
        if (expr->type != _Unionexpr)
        {
            fprintf(stderr, "Bad type (E)!\n");
            EXIT(1);
        }
        pos = ALIGN(pos);
        if (name)
            out(name, pos);
        dummy = ulist_to_equ(name, expr->info.unjon.defs, pos, all_defs, size);
        *size = ALIGN(*size);
        break;

    case _Union:
        fprintf(stderr, "Not yet supported (C)!\n");
        EXIT(1);
        break;

    case _Typedef:
        if ((expr = locate_typedef(type_expr->info.def.name->string, all_defs)))
        {
            /* Below is an ugly hack! */
            typedef_name = expr->info.var.id->info.id.name->string;
            expr->info.var.id->info.id.name->string = name;
            dummy = expr_to_equ(expr, pos, all_defs, size);
            if (pos + *size != dummy)
                pos++;
            expr->info.var.id->info.id.name->string = typedef_name;
        } else
        {
            fprintf(stderr, "Unknown typedef!\n");
            EXIT(1);
        }
        break;

    case _Pointer:
        pos = ALIGN(pos);
        out(name, pos);
        *size = 4;
        break;

    default:
        fprintf(stderr, "Bad type (%d)!\n", type_expr->info.def.sort);
        EXIT(1);
        break;
    }

    if (count >= 0)
    {
        /* Used to be >, 981114 */
        pos += *size * count;
        *size *= count;
    } else
    {
        pos += *size;
    }

    return pos;
}


static int list_to_equ(char *base, List defs, int pos, List all_defs, int *size)
{
    Listelement ptr;
    Expression cur_expr, type_expr, expr;
    int type;
    int oldpos;
    int dummy;

    oldpos = pos;
    push(base);
    ptr = FIRST(defs);
    while (ptr)
    {
        cur_expr = EXPR(ptr);
        if (cur_expr->type != _Varexpr)
        {
            fprintf(stderr, "Unsupported construction (A)!\n");
            EXIT(1);
        }
        pos = expr_to_equ(cur_expr, pos, all_defs, &dummy);
        NEXT(ptr);
    }
    pop();
    *size = pos - oldpos;
    return pos;
}


static int ulist_to_equ(char *base, List defs, int pos, List all_defs, int *size)
{
    Listelement ptr;
    Expression cur_expr, type_expr, expr;
    int type;
    int oldpos;
    int maxpos;
    int dummy;

    oldpos = pos;
    maxpos = pos;
    push(base);
    ptr = FIRST(defs);
    while (ptr)
    {
        cur_expr = EXPR(ptr);
        if (cur_expr->type != _Varexpr)
        {
            fprintf(stderr, "Unsupported construction (A)!\n");
            EXIT(1);
        }
        pos = expr_to_equ(cur_expr, pos, all_defs, &dummy);
        if (pos > maxpos)
            maxpos = pos;
        pos = oldpos;
        NEXT(ptr);
    }
    pop();
    *size = maxpos - oldpos;
    return maxpos;
}


void convert(char *name, List defs)
{
    Listelement ptr;
    Expression expr;
    int type;
    int size;
    char *equ_name;
    int dummy;
    int found;
    Expression id;

    count = 0;
    if ((equ_name = strchr(name, '=')))
    {
        *equ_name = '\0';
        equ_name++;
    } else
    {
        equ_name = name;
    }
    size = 0;

    ptr = FIRST(defs);
    found = 0;
    while (!found && ptr)
    {
        expr = EXPR(ptr);
        type = expr->type;
        switch (type)
        {
        case _Structexpr:
            if (compare(name, expr->info.strct.name))
            {
                size = list_to_equ(equ_name, expr->info.strct.defs, 0, defs, &dummy);
                found = 1;
            } else
            {
#if 0
                fprintf(stderr, "Mismatch: %s, %s\n", name, expr->info.strct.name->string);
#endif
            }
            break;

        case _Unionexpr:
            if (compare(name, expr->info.unjon.name))
            {
                size = ulist_to_equ(equ_name, expr->info.unjon.defs, 0, defs, &dummy);
                found = 1;
            } else
            {
#if 0
                fprintf(stderr, "Mismatch: %s, %s\n", name, expr->info.unjon.name->string);
#endif
            }
            break;

        case _Typedefexpr:
            if (compare(name, expr->info.var.id->info.id.name))
            {
                expr = expr->info.var.type;
                if ((expr->type == _Typeexpr) && (expr->info.def.sort == _Structdef))
                {
                    size = list_to_equ(equ_name, expr->info.def.type->info.strct.defs, 0, defs, &dummy);
                    found = 1;
                } else if ((expr->type == _Typeexpr) && (expr->info.def.sort == _Uniondef))
                {
                    size = ulist_to_equ(equ_name, expr->info.def.type->info.unjon.defs, 0, defs, &dummy);
                    found = 1;
                } else
                {
                    fprintf(stderr, "Not a struct typedef (%d)!\n", expr->type);
                    exit(1);
                }
            } else
            {
#if 0
                fprintf(stderr, "Mismatch: %s, %s\n", name, expr->info.var.id->info.id.name->string);
#endif
            }
            break;

        case _Varexpr:
            id = expr->info.var.id;
            if (compare(name, id->info.id.name))
            {
                expr = expr->info.var.type;
                if ((expr->type == _Typeexpr) && (expr->info.def.sort == _Structdef))
                {
                    size = list_to_equ(equ_name, expr->info.def.type->info.strct.defs, 0, defs, &dummy);
                    found = 1;
                } else
                {
                    fprintf(stderr, "Not a struct variable (%d)!\n", expr->type);
                    exit(1);
                }
            } else
            {
#if 0
                fprintf(stderr, "Mismatch: %s, %s\n", name, expr->info.var.id->info.id.name->string);
#endif
            }
            break;

        default:
            fprintf(stderr, "Expression type: %d\n", type);
            break;
        }
        NEXT(ptr);
    }
    assert(count == 0);
    if (size)
    {
        size = ALIGN(size);
        push(equ_name);
        out("struct_size", size);
        pop();
    }
}
