/*
 * C structure expression creation and printing functions
 *
 * Copyright 1997-2002, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "misc.h"
#include "expr.h"
#include "list.h"
#include "memory.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#undef TEST
#define ET(text)     if (opt & 0x01) printf("expr.c: %s\n", text)
#define ET1(text, arg)     if (opt & 0x01) printf("expr.c: %s %s\n", text, arg)




static Expression new(void)
{
    Expression exprcell;

    exprcell = (Expression)memalloc(sizeof(struct _Expr));
    if (!exprcell)
        exit(1);
    return exprcell;
}


Expression mkid(Identifier name, int count)
{
    Expression expr = new();

    ET1("mkid", name ? name->string : "");
    expr->type = _Idexpr;
    expr->info.id.name = name;
    expr->info.id.count = count;
    return expr;
}


static void printid(Expression expr)
{
    if (!expr->info.id.name)
        printf("<anonymous>;\n");
    else if (expr->info.id.count >= 0)
        printf("%s[%d];\n", expr->info.id.name->string, expr->info.id.count);
    else
        printf("%s;\n", expr->info.id.name->string);
}


Expression mktypedef(Expression expr)
{
    ET("mktypedef");
    expr->type = _Typedefexpr;
    return expr;
}


Expression mkstruct(Identifier name, List defs)
{
    Expression expr = new();

    ET("mkstruct");
    expr->type = _Structexpr;
    expr->info.strct.name = name;
    expr->info.strct.defs = defs;
    return expr;
}


Expression mkunion(Identifier name, List defs)
{
    Expression expr = new();

    ET("mkunion");
    expr->type = _Unionexpr;
    expr->info.unjon.name = name;
    expr->info.unjon.defs = defs;
    return expr;
}


Expression mklist(Expression type)
{
    Expression expr = new();

    ET("list");
    expr->type = _Listexpr;
    expr->info.list.type = type;
    return expr;
}


Expression mktype(int n, Identifier name, Expression type)
{
    Expression expr = new();

    ET("mktype");
    expr->type = _Typeexpr;
    expr->info.def.sort = n;
    expr->info.def.name = name;
    expr->info.def.type = type;
    return expr;
}


Expression mkvar(Expression type, Expression id)
{
    Expression expr = new();

    ET1("var", (id && id->type == _Idexpr && id->info.id.name) ? id->info.id.name->string : "");
    expr->type = _Varexpr;
    expr->info.var.id = id;
    expr->info.var.type = type;
    return expr;
}


static void printstruct(Expression expr)
{
    if (expr->info.strct.name)
        printf("struct %s {\n", expr->info.strct.name->string);
    else
        printf("struct {\n");
    printdefs(expr->info.strct.defs);
    printf("} ");
}


static void printunion(Expression expr)
{
    if (expr->info.unjon.name)
        printf("union %s {\n", expr->info.unjon.name->string);
    else
        printf("union {\n");
    printdefs(expr->info.unjon.defs);
    printf("} ");
}


static void printtype(Expression expr)
{
    switch (expr->info.def.sort)
    {
    case _Void:
        printf("void ");
        break;
    case _Char:
        printf("char ");
        break;
    case _Short:
        printf("short ");
        break;
    case _Int:
        printf("int ");
        break;
    case _Long:
        printf("long ");
        break;
    case _Struct:
        printf("struct %s ", expr->info.def.name->string);
        break;
    case _Union:
        printf("union %s ", expr->info.def.name->string);
        break;
    case _Uniondef:
        printunion(expr->info.def.type);
        break;
    case _Typedef:
        printf("%s ", expr->info.def.name->string);
        break;
    case _Pointer:
        printtype(expr->info.def.type);
        printf("* ");
        break;
    default:
        break;
    }
}


static void printtypedef(Expression expr)
{
    printf("typedef\n");
    printtype(expr->info.var.type);
    printid(expr->info.var.id);
}


static void printvar(Expression expr)
{
    printtype(expr->info.var.type);
    printid(expr->info.var.id);
}


static void printlist(Expression expr)
{
    printf("(");
    printtype(expr->info.list.type);
    printf(")\n");
}


void printdefs(List defs)
{
    Listelement ptr;

    ptr = FIRST(defs);
    while (ptr)
    {
        switch (EXPR(ptr)->type)
        {
        case _Typedefexpr:
            printtypedef(EXPR(ptr));
            break;
        case _Varexpr:
            printvar(EXPR(ptr));
            break;
        case _Structexpr:
            printstruct(EXPR(ptr));
            break;
        case _Listexpr:
            printlist(EXPR(ptr));
            break;
        default:
            break;
        }
        NEXT(ptr);
    }
}
