/****************************************************************************
 *
 * ftconfig.h
 *
 *   UNIX-specific configuration file (specification only).
 *
 * Copyright (C) 1996-2020 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


  /**************************************************************************
   *
   * This header file contains a number of macro definitions that are used by
   * the rest of the engine.  Most of the macros here are automatically
   * determined at compile time, and you should not need to change it to port
   * FreeType, except to compile the library with a non-ANSI compiler.
   *
   * Note however that if some specific modifications are needed, we advise
   * you to place a modified copy in your build directory.
   *
   * The build directory is usually `builds/<system>`, and contains
   * system-specific files that are always included first when building the
   * library.
   *
   */

#ifndef __FTCONFIG_H__
#define __FTCONFIG_H__
#define FTCONFIG_H_

#include <ft2build.h>
#if FREETYPE_VERSION >= 2010003L

#include FT_CONFIG_OPTIONS_H
#include FT_CONFIG_STANDARD_LIBRARY_H

#include <freetype/config/integer-types.h>
#include <freetype/config/public-macros.h>
#include <freetype/config/mac-support.h>

#else
/* Previous porting */
#undef FT_CONFIG_OPTIONS_H
#define FT_CONFIG_OPTIONS_H <freetype/config/ftoption.h>
#undef FT_CONFIG_CONFIG_H
#define FT_CONFIG_CONFIG_H <freetype/config/ftconfig.h>
#undef FT_CONFIG_MODULES_H
#define FT_CONFIG_MODULES_H <freetype/config/ftmodule.h>
#include <freetype/config/ftoption.h>
#ifdef FT_CONFIG_STANDARD_LIBRARY_H
#include FT_CONFIG_STANDARD_LIBRARY_H
#else
#include <freetype/config/ftstdlib.h>
#endif


FT_BEGIN_HEADER


  /**************************************************************************
   *
   *              PLATFORM-SPECIFIC CONFIGURATION MACROS
   *
   * These macros can be toggled to suit a specific system.  The current ones
   * are defaults used to compile FreeType in an ANSI C environment (16bit
   * compilers are also supported).  Copy this file to your own
   * `builds/<system>` directory, and edit it to port the engine.
   *
   */

  /* There are systems (like the Texas Instruments 'C54x) where a `char`  */
  /* has 16~bits.  ANSI~C says that `sizeof(char)` is always~1.  Since an */
  /* `int` has 16~bits also for this system, `sizeof(int)` gives~1 which  */
  /* is probably unexpected.                                              */
  /*                                                                      */
  /* `CHAR_BIT` (defined in `limits.h`) gives the number of bits in a     */
  /* `char` type.                                                         */

#ifndef FT_CHAR_BIT
#define FT_CHAR_BIT  CHAR_BIT
#endif


  /* Following cpp computation of the bit length of `int` and `long` */
  /* is copied from default `include/freetype/config/ftconfig.h`.    */
  /* If any improvement is required for this file, it should be      */
  /* applied to the original header file for the builders that do    */
  /* not use configure script.                                       */

  /* The size of an `int` type. */
#if                                 FT_UINT_MAX == 0xFFFFUL
#define FT_SIZEOF_INT  ( 16 / FT_CHAR_BIT )
#elif                               FT_UINT_MAX == 0xFFFFFFFFUL
#define FT_SIZEOF_INT  ( 32 / FT_CHAR_BIT )
#elif FT_UINT_MAX > 0xFFFFFFFFUL && FT_UINT_MAX == 0xFFFFFFFFFFFFFFFFUL
#define FT_SIZEOF_INT  ( 64 / FT_CHAR_BIT )
#else
#error "Unsupported size of `int' type!"
#endif

  /* The size of a `long` type.  A five-byte `long` (as used e.g. on the */
  /* DM642) is recognized but avoided.                                   */
#if                                  FT_ULONG_MAX == 0xFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 32 / FT_CHAR_BIT )
#elif FT_ULONG_MAX > 0xFFFFFFFFUL && FT_ULONG_MAX == 0xFFFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 32 / FT_CHAR_BIT )
#elif FT_ULONG_MAX > 0xFFFFFFFFUL && FT_ULONG_MAX == 0xFFFFFFFFFFFFFFFFUL
#define FT_SIZEOF_LONG  ( 64 / FT_CHAR_BIT )
#else
#error "Unsupported size of `long' type!"
#endif

  /* `FT_UNUSED` indicates that a given parameter is not used --   */
  /* this is only used to get rid of unpleasant compiler warnings. */
#ifndef FT_UNUSED
#define FT_UNUSED( arg )  ( (void) (arg) )
#endif


  /**************************************************************************
   *
   *                    AUTOMATIC CONFIGURATION MACROS
   *
   * These macros are computed from the ones defined above.  Don't touch
   * their definition, unless you know precisely what you are doing.  No
   * porter should need to mess with them.
   *
   */


  /**************************************************************************
   *
   * Mac support
   *
   *   This is the only necessary change, so it is defined here instead
   *   providing a new configuration file.
   */
#if defined( __APPLE__ ) || ( defined( __MWERKS__ ) && defined( macintosh ) )
  /* No Carbon frameworks for 64bit 10.4.x.                         */
  /* `AvailabilityMacros.h` is available since Mac OS X 10.2,       */
  /* so guess the system version by maximum errno before inclusion. */
#include <errno.h>
#ifdef ECANCELED /* defined since 10.2 */
#include "AvailabilityMacros.h"
#endif
#if defined( __LP64__ ) && \
    ( MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4 )
#undef FT_MACINTOSH
#endif

#elif defined( __SC__ ) || defined( __MRC__ )
  /* Classic MacOS compilers */
#include "ConditionalMacros.h"
#if TARGET_OS_MAC
#define FT_MACINTOSH 1
#endif

#endif


  /* Fix compiler warning with sgi compiler. */
#if defined( __sgi ) && !defined( __GNUC__ )
#if defined( _COMPILER_VERSION ) && ( _COMPILER_VERSION >= 730 )
#pragma set woff 3505
#endif
#endif


  /**************************************************************************
   *
   * @section:
   *   basic_types
   *
   */


  /**************************************************************************
   *
   * @type:
   *   FT_Int16
   *
   * @description:
   *   A typedef for a 16bit signed integer type.
   */
  typedef signed short  FT_Int16;


  /**************************************************************************
   *
   * @type:
   *   FT_UInt16
   *
   * @description:
   *   A typedef for a 16bit unsigned integer type.
   */
  typedef unsigned short  FT_UInt16;

  /* */


  /* this #if 0 ... #endif clause is for documentation purposes */
#if 0

  /**************************************************************************
   *
   * @type:
   *   FT_Int32
   *
   * @description:
   *   A typedef for a 32bit signed integer type.  The size depends on the
   *   configuration.
   */
  typedef signed XXX  FT_Int32;


  /**************************************************************************
   *
   * @type:
   *   FT_UInt32
   *
   *   A typedef for a 32bit unsigned integer type.  The size depends on the
   *   configuration.
   */
  typedef unsigned XXX  FT_UInt32;


  /**************************************************************************
   *
   * @type:
   *   FT_Int64
   *
   *   A typedef for a 64bit signed integer type.  The size depends on the
   *   configuration.  Only defined if there is real 64bit support;
   *   otherwise, it gets emulated with a structure (if necessary).
   */
  typedef signed XXX  FT_Int64;


  /**************************************************************************
   *
   * @type:
   *   FT_UInt64
   *
   *   A typedef for a 64bit unsigned integer type.  The size depends on the
   *   configuration.  Only defined if there is real 64bit support;
   *   otherwise, it gets emulated with a structure (if necessary).
   */
  typedef unsigned XXX  FT_UInt64;

  /* */

#endif

#if FT_SIZEOF_INT == 4

  typedef signed int      FT_Int32;
  typedef unsigned int    FT_UInt32;

#elif FT_SIZEOF_LONG == 4

  typedef signed long     FT_Int32;
  typedef unsigned long   FT_UInt32;

#else
#error "no 32bit type found -- please check your configuration files"
#endif


  /* look up an integer type that is at least 32~bits */
#if FT_SIZEOF_INT >= 4

  typedef int            FT_Fast;
  typedef unsigned int   FT_UFast;

#elif FT_SIZEOF_LONG >= 4

  typedef long           FT_Fast;
  typedef unsigned long  FT_UFast;

#endif


  /* determine whether we have a 64-bit `int` type for platforms without */
  /* Autoconf                                                            */
#if FT_SIZEOF_LONG == 8

  /* `FT_LONG64` must be defined if a 64-bit type is available */
#define FT_LONG64
#define FT_INT64   long
#define FT_UINT64  unsigned long

  /* we handle the LLP64 scheme separately for GCC and clang, */
  /* suppressing the `long long` warning                      */
#elif ( FT_SIZEOF_LONG == 4 )       && \
      defined( HAVE_LONG_LONG_INT ) && \
      defined( __GNUC__ )
#pragma GCC diagnostic ignored "-Wlong-long"
#define FT_LONG64
#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

  /**************************************************************************
   *
   * A 64-bit data type may create compilation problems if you compile in
   * strict ANSI mode.  To avoid them, we disable other 64-bit data types if
   * `__STDC__` is defined.  You can however ignore this rule by defining the
   * `FT_CONFIG_OPTION_FORCE_INT64` configuration macro.
   */
#elif !defined( __STDC__ ) || defined( FT_CONFIG_OPTION_FORCE_INT64 )

#if defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L

#define FT_LONG64
#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#elif defined( _MSC_VER ) && _MSC_VER >= 900 /* Visual C++ (and Intel C++) */

  /* this compiler provides the `__int64` type */
#define FT_LONG64
#define FT_INT64   __int64
#define FT_UINT64  unsigned __int64

#elif defined( __BORLANDC__ )  /* Borland C++ */

  /* XXXX: We should probably check the value of `__BORLANDC__` in order */
  /*       to test the compiler version.                                 */

  /* this compiler provides the `__int64` type */
#define FT_LONG64
#define FT_INT64   __int64
#define FT_UINT64  unsigned __int64

#elif defined( __WATCOMC__ )   /* Watcom C++ */

  /* Watcom doesn't provide 64-bit data types */

#elif defined( __MWERKS__ )    /* Metrowerks CodeWarrior */

#define FT_LONG64
#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#elif defined( __GNUC__ )

  /* GCC provides the `long long` type */
#define FT_LONG64
#define FT_INT64   long long int
#define FT_UINT64  unsigned long long int

#endif /* __STDC_VERSION__ >= 199901L */

#endif /* FT_SIZEOF_LONG == 8 */

#ifdef FT_LONG64
  typedef FT_INT64   FT_Int64;
  typedef FT_UINT64  FT_UInt64;
#endif

#ifdef _WIN64
  /* only 64bit Windows uses the LLP64 data model, i.e., */
  /* 32bit integers, 64bit pointers                      */
#define FT_UINT_TO_POINTER( x ) (void*)(unsigned __int64)(x)
#else
#define FT_UINT_TO_POINTER( x ) (void*)(unsigned long)(x)
#endif


  /**************************************************************************
   *
   * miscellaneous
   *
   */


#define FT_BEGIN_STMNT  do {
#define FT_END_STMNT    } while ( 0 )
#define FT_DUMMY_STMNT  FT_BEGIN_STMNT FT_END_STMNT


  /* `typeof` condition taken from gnulib's `intprops.h` header file */
#if ( ( defined( __GNUC__ ) && __GNUC__ >= 2 )                       || \
      ( defined( __IBMC__ ) && __IBMC__ >= 1210 &&                      \
        defined( __IBM__TYPEOF__ ) )                                 || \
      ( defined( __SUNPRO_C ) && __SUNPRO_C >= 0x5110 && !__STDC__ ) )
#define FT_TYPEOF( type )  ( __typeof__ ( type ) )
#else
#define FT_TYPEOF( type )  /* empty */
#endif


#ifndef  FT_CONFIG_OPTION_NO_ASSEMBLER
  /* Provide assembler fragments for performance-critical functions. */
  /* These must be defined `static __inline__' with GCC.             */

#if defined( __CC_ARM ) || defined( __ARMCC__ )  /* RVCT */

#define FT_MULFIX_ASSEMBLER  FT_MulFix_arm

  /* documentation is in freetype.h */

  static __inline FT_Int32
  FT_MulFix_arm( FT_Int32  a,
                 FT_Int32  b )
  {
    register FT_Int32  t, t2;


    __asm
    {
      smull t2, t,  b,  a           /* (lo=t2,hi=t) = a*b */
      mov   a,  t,  asr #31         /* a   = (hi >> 31) */
      add   a,  a,  #0x8000         /* a  += 0x8000 */
      adds  t2, t2, a               /* t2 += a */
      adc   t,  t,  #0              /* t  += carry */
      mov   a,  t2, lsr #16         /* a   = t2 >> 16 */
      orr   a,  a,  t,  lsl #16     /* a  |= t << 16 */
    }
    return a;
  }

#endif /* __CC_ARM || __ARMCC__ */


#ifdef __GNUC__

#if defined( __arm__ )                                 && \
    ( !defined( __thumb__ ) || defined( __thumb2__ ) ) && \
    !( defined( __CC_ARM ) || defined( __ARMCC__ ) )

#define FT_MULFIX_ASSEMBLER  FT_MulFix_arm

  /* documentation is in freetype.h */

  static __inline__ FT_Int32
  FT_MulFix_arm( FT_Int32  a,
                 FT_Int32  b )
  {
    register FT_Int32  t, t2;


    __asm__ __volatile__ (
      "smull  %1, %2, %4, %3\n\t"       /* (lo=%1,hi=%2) = a*b */
      "mov    %0, %2, asr #31\n\t"      /* %0  = (hi >> 31) */
#ifdef __clang__
      "add.w  %0, %0, #0x8000\n\t"      /* %0 += 0x8000 */
#else
      "add    %0, %0, #0x8000\n\t"      /* %0 += 0x8000 */
#endif
      "adds   %1, %1, %0\n\t"           /* %1 += %0 */
      "adc    %2, %2, #0\n\t"           /* %2 += carry */
      "mov    %0, %1, lsr #16\n\t"      /* %0  = %1 >> 16 */
      "orr    %0, %0, %2, lsl #16\n\t"  /* %0 |= %2 << 16 */
      : "=r"(a), "=&r"(t2), "=&r"(t)
      : "r"(a), "r"(b)
      : "cc" );
    return a;
  }

#endif /* __arm__                      && */
       /* ( __thumb2__ || !__thumb__ ) && */
       /* !( __CC_ARM || __ARMCC__ )      */


#if defined( __i386__ )

#define FT_MULFIX_ASSEMBLER  FT_MulFix_i386

  /* documentation is in freetype.h */

  static __inline__ FT_Int32
  FT_MulFix_i386( FT_Int32  a,
                  FT_Int32  b )
  {
    register FT_Int32  result;


    __asm__ __volatile__ (
      "imul  %%edx\n"
      "movl  %%edx, %%ecx\n"
      "sarl  $31, %%ecx\n"
      "addl  $0x8000, %%ecx\n"
      "addl  %%ecx, %%eax\n"
      "adcl  $0, %%edx\n"
      "shrl  $16, %%eax\n"
      "shll  $16, %%edx\n"
      "addl  %%edx, %%eax\n"
      : "=a"(result), "=d"(b)
      : "a"(a), "d"(b)
      : "%ecx", "cc" );
    return result;
  }

#endif /* i386 */

#endif /* __GNUC__ */


#ifdef _MSC_VER /* Visual C++ */

#ifdef _M_IX86

#define FT_MULFIX_ASSEMBLER  FT_MulFix_i386

  /* documentation is in freetype.h */

  static __inline FT_Int32
  FT_MulFix_i386( FT_Int32  a,
                  FT_Int32  b )
  {
    register FT_Int32  result;

    __asm
    {
      mov eax, a
      mov edx, b
      imul edx
      mov ecx, edx
      sar ecx, 31
      add ecx, 8000h
      add eax, ecx
      adc edx, 0
      shr eax, 16
      shl edx, 16
      add eax, edx
      mov result, eax
    }
    return result;
  }

#endif /* _M_IX86 */

#endif /* _MSC_VER */


#if defined( __GNUC__ ) && defined( __x86_64__ )

#define FT_MULFIX_ASSEMBLER  FT_MulFix_x86_64

  static __inline__ FT_Int32
  FT_MulFix_x86_64( FT_Int32  a,
                    FT_Int32  b )
  {
#if 1
    /* Technically not an assembly fragment, but GCC does a really good */
    /* job at inlining it and generating good machine code for it.      */
    __extension__ long long  ret, tmp;


    ret  = __extension__ (long long)a * b;
    tmp  = ret >> 63;
    ret += 0x8000 + tmp;

    return (FT_Int32)( ret >> 16 );
#else

    /* For some reason, GCC 4.6 on Ubuntu 12.04 generates invalid machine  */
    /* code from the lines below.  The main issue is that `wide_a' is not  */
    /* properly initialized by sign-extending `a'.  Instead, the generated */
    /* machine code assumes that the register that contains `a' on input   */
    /* can be used directly as a 64-bit value, which is wrong most of the  */
    /* time.                                                               */
    long long  wide_a = (long long)a;
    long long  wide_b = (long long)b;
    long long  result;


    __asm__ __volatile__ (
      "imul %2, %1\n"
      "mov %1, %0\n"
      "sar $63, %0\n"
      "lea 0x8000(%1, %0), %0\n"
      "sar $16, %0\n"
      : "=&r"(result), "=&r"(wide_a)
      : "r"(wide_b)
      : "cc" );

    return (FT_Int32)result;
#endif
  }

#endif /* __GNUC__ && __x86_64__ */

#endif /* !FT_CONFIG_OPTION_NO_ASSEMBLER */


#ifdef FT_CONFIG_OPTION_INLINE_MULFIX
#ifdef FT_MULFIX_ASSEMBLER
#define FT_MULFIX_INLINED  FT_MULFIX_ASSEMBLER
#endif
#endif


  /* Use `FT_LOCAL` and `FT_LOCAL_DEF` to declare and define,            */
  /* respectively, a function that gets used only within the scope of a  */
  /* module.  Normally, both the header and source code files for such a */
  /* function are within a single module directory.                      */
  /*                                                                     */
  /* Intra-module arrays should be tagged with `FT_LOCAL_ARRAY` and      */
  /* `FT_LOCAL_ARRAY_DEF`.                                               */
  /*                                                                     */
#ifdef FT_MAKE_OPTION_SINGLE_OBJECT

#define FT_LOCAL( x )      static  x
#define FT_LOCAL_DEF( x )  static  x

#else

#ifdef __cplusplus
#define FT_LOCAL( x )      extern "C"  x
#define FT_LOCAL_DEF( x )  extern "C"  x
#else
#define FT_LOCAL( x )      extern  x
#define FT_LOCAL_DEF( x )  x
#endif

#endif /* FT_MAKE_OPTION_SINGLE_OBJECT */

#define FT_LOCAL_ARRAY( x )      extern const  x
#define FT_LOCAL_ARRAY_DEF( x )  const  x


  /* Use `FT_BASE` and `FT_BASE_DEF` to declare and define, respectively, */
  /* functions that are used in more than a single module.  In the        */
  /* current setup this implies that the declaration is in a header file  */
  /* in the `include/freetype/internal` directory, and the function body  */
  /* is in a file in `src/base`.                                          */
  /*                                                                      */
#ifndef FT_BASE

#ifdef __cplusplus
#define FT_BASE( x )  extern "C"  x
#else
#define FT_BASE( x )  extern  x
#endif

#endif /* !FT_BASE */


#ifndef FT_BASE_DEF

#ifdef __cplusplus
#define FT_BASE_DEF( x )  x
#else
#define FT_BASE_DEF( x )  x
#endif

#endif /* !FT_BASE_DEF */


  /* When compiling FreeType as a DLL or DSO with hidden visibility    */
  /* some systems/compilers need a special attribute in front OR after */
  /* the return type of function declarations.                         */
  /*                                                                   */
  /* Two macros are used within the FreeType source code to define     */
  /* exported library functions: `FT_EXPORT` and `FT_EXPORT_DEF`.      */
  /*                                                                   */
  /* - `FT_EXPORT( return_type )`                                      */
  /*                                                                   */
  /*   is used in a function declaration, as in                        */
  /*                                                                   */
  /*   ```                                                             */
  /*     FT_EXPORT( FT_Error )                                         */
  /*     FT_Init_FreeType( FT_Library*  alibrary );                    */
  /*   ```                                                             */
  /*                                                                   */
  /* - `FT_EXPORT_DEF( return_type )`                                  */
  /*                                                                   */
  /*   is used in a function definition, as in                         */
  /*                                                                   */
  /*   ```                                                             */
  /*     FT_EXPORT_DEF( FT_Error )                                     */
  /*     FT_Init_FreeType( FT_Library*  alibrary )                     */
  /*     {                                                             */
  /*       ... some code ...                                           */
  /*       return FT_Err_Ok;                                           */
  /*     }                                                             */
  /*   ```                                                             */
  /*                                                                   */
  /* You can provide your own implementation of `FT_EXPORT` and        */
  /* `FT_EXPORT_DEF` here if you want.                                 */
  /*                                                                   */
  /* To export a variable, use `FT_EXPORT_VAR`.                        */
  /*                                                                   */
#ifndef FT_EXPORT

#ifdef FT2_BUILD_LIBRARY

#if defined( _WIN32 ) && defined( DLL_EXPORT )
#define FT_EXPORT( x )  __declspec( dllexport )  x
#elif defined( __GNUC__ ) && __GNUC__ >= 4
#define FT_EXPORT( x )  __attribute__(( visibility( "default" ) ))  x
#elif defined( __SUNPRO_C ) && __SUNPRO_C >= 0x550
#define FT_EXPORT( x )  __global  x
#elif defined( __cplusplus )
#define FT_EXPORT( x )  extern "C"  x
#else
#define FT_EXPORT( x )  extern  x
#endif

#else

#if defined( _WIN32 ) && defined( DLL_IMPORT )
#define FT_EXPORT( x )  __declspec( dllimport )  x
#elif defined( __cplusplus )
#define FT_EXPORT( x )  extern "C"  x
#else
#define FT_EXPORT( x )  extern  x
#endif

#endif

#endif /* !FT_EXPORT */


#ifndef FT_EXPORT_DEF

#ifdef __cplusplus
#define FT_EXPORT_DEF( x )  extern "C"  x
#else
#define FT_EXPORT_DEF( x )  extern  x
#endif

#endif /* !FT_EXPORT_DEF */


#ifndef FT_EXPORT_VAR

#ifdef __cplusplus
#define FT_EXPORT_VAR( x )  extern "C"  x
#else
#define FT_EXPORT_VAR( x )  extern  x
#endif

#endif /* !FT_EXPORT_VAR */


  /* The following macros are needed to compile the library with a   */
  /* C++ compiler and with 16bit compilers.                          */
  /*                                                                 */

  /* This is special.  Within C++, you must specify `extern "C"` for */
  /* functions which are used via function pointers, and you also    */
  /* must do that for structures which contain function pointers to  */
  /* assure C linkage -- it's not possible to have (local) anonymous */
  /* functions which are accessed by (global) function pointers.     */
  /*                                                                 */
  /*                                                                 */
  /* FT_CALLBACK_DEF is used to _define_ a callback function,        */
  /* located in the same source code file as the structure that uses */
  /* it.                                                             */
  /*                                                                 */
  /* FT_BASE_CALLBACK and FT_BASE_CALLBACK_DEF are used to declare   */
  /* and define a callback function, respectively, in a similar way  */
  /* as FT_BASE and FT_BASE_DEF work.                                */
  /*                                                                 */
  /* FT_CALLBACK_TABLE is used to _declare_ a constant variable that */
  /* contains pointers to callback functions.                        */
  /*                                                                 */
  /* FT_CALLBACK_TABLE_DEF is used to _define_ a constant variable   */
  /* that contains pointers to callback functions.                   */
  /*                                                                 */
  /*                                                                 */
  /* Some 16bit compilers have to redefine these macros to insert    */
  /* the infamous `_cdecl` or `__fastcall` declarations.             */
  /*                                                                 */
#ifndef FT_CALLBACK_DEF
#ifdef __cplusplus
#define FT_CALLBACK_DEF( x )  extern "C"  x
#else
#define FT_CALLBACK_DEF( x )  static  x
#endif
#endif /* FT_CALLBACK_DEF */

#ifndef FT_BASE_CALLBACK
#ifdef __cplusplus
#define FT_BASE_CALLBACK( x )      extern "C"  x
#define FT_BASE_CALLBACK_DEF( x )  extern "C"  x
#else
#define FT_BASE_CALLBACK( x )      extern  x
#define FT_BASE_CALLBACK_DEF( x )  x
#endif
#endif /* FT_BASE_CALLBACK */

#ifndef FT_CALLBACK_TABLE
#ifdef __cplusplus
#define FT_CALLBACK_TABLE      extern "C"
#define FT_CALLBACK_TABLE_DEF  extern "C"
#else
#define FT_CALLBACK_TABLE      extern
#define FT_CALLBACK_TABLE_DEF  /* nothing */
#endif
#endif /* FT_CALLBACK_TABLE */


FT_END_HEADER

#endif

#endif /* __FTCONFIG_H__ */


/* END */
