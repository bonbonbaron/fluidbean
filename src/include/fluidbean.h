/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#ifndef _FLUIDSYNTH_PRIV_H
#define _FLUIDSYNTH_PRIV_H
/***************************************************************
 *
 *         BASIC TYPES
 */

#if defined(WITH_FLOAT)
typedef float fluid_real_t;
#else
typedef double fluid_real_t;
#endif


typedef enum {
	FLUID_OK = 0,
	FLUID_FAILED = -1
} fluid_status;


//socket disabled


#include "config.h"
#include "botox/data.h"
#include <glib.h>
#include "soundfont.h"

#if HAVE_STRING_H
#include <string.h>
#endif

#if HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_MATH_H
#include <math.h>
#endif

#if HAVE_STDARG_H
#include <stdarg.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if HAVE_LIMITS_H
#include <limits.h>
#endif

/** Integer types  */

#if 1 /**/
typedef S8  sint8;
typedef U8  uint8;
typedef S16 sint16;
typedef U16 uint16;
typedef S32 sint32;
typedef U32 uint32;
typedef signed long long sint64;
typedef unsigned long long uint64;

#if defined(__LP64__) || defined(_WIN64)
typedef uint64 uintptr;
#else
typedef uint32 uintptr;
#endif

#else /**/
#if defined(MINGW32)
/* Windows using MinGW32 */
typedef int8_t sint8;
typedef uint8_t uint8;
typedef int16_t sint16;
typedef uint16_t uint16;
typedef int32_t sint32;
typedef uint32_t uint32;
typedef int64_t sint64;
typedef uint64_t uint64;

#elif defined(_WIN32)
/* Windows */
typedef signed __int8 sint8;
typedef unsigned __int8 uint8;
typedef signed __int16 sint16;
typedef unsigned __int16 uint16;
typedef signed __int32 sint32;
typedef unsigned __int32 uint32;
typedef signed __int64 sint64;
typedef unsigned __int64 uint64;

#elif defined(MACOS9)
/* Macintosh */
typedef S8 sint8;
typedef S8 sint8;
typedef U8 uint8;
typedef S16 sint16;
typedef S16 sint16;
typedef U16 uint16;
typedef S32 sint32;
typedef S32 sint32;
typedef U32 uint32;
/* FIXME: needs to be verified */
typedef long long sint64;
typedef unsigned long long uint64;

#else
/* Linux & Darwin */
typedef int8_t sint8;
typedef u_int8_t uint8;
typedef int16_t sint16;
typedef u_int16_t uint16;
typedef int32_t sint32;
typedef u_int32_t uint32;
typedef int64_t sint64;
typedef u_int64_t uint64;

#endif
#endif /* */

#define fluid_return_if_fail(cond) \
if(cond) \
    ; \
else \
    return

#define fluid_return_val_if_fail(cond, val) \
 fluid_return_if_fail(cond) (val)

/** Atomic types  */
typedef int fluid_atomic_int_t;
typedef unsigned int fluid_atomic_uint_t;
typedef float fluid_atomic_float_t;
#if defined(_MSC_VER) && (_MSC_VER < 1800)
typedef __int64 fluid_long_long_t; // even on 32bit windows
#else
/**
 * A typedef for C99's type long long, which is at least 64-bit wide, as guaranteed by the C99.
 * @p __int64 will be used as replacement for VisualStudio 2010 and older.
 */
typedef long long fluid_long_long_t;
#endif

/***************************************************************
 *
 *       FORWARD DECLARATIONS
 */

/***************************************************************
 *
 *                      CONSTANTS
 */

#define FLUID_BUFSIZE                64

#ifndef PI
#define PI                          3.141592654
#endif

/***************************************************************
 *
 *                      SYSTEM INTERFACE
 */
//typedef FILE*  fluid_file;

#define FLUID_MALLOC(_n)             malloc(_n)
#define FLUID_REALLOC(_p,_n)         realloc(_p,_n)
#define FLUID_NEW(_t)                (_t*)malloc(sizeof(_t))
#define FLUID_ARRAY(_t,_n)           (_t*)malloc((_n)*sizeof(_t))
#define FLUID_FREE(_p)               free(_p)
#define FLUID_FOPEN(_f,_m)           fopen(_f,_m)
#define FLUID_FCLOSE(_f)             fclose(_f)
#define FLUID_FREAD(_p,_s,_n,_f)     fread(_p,_s,_n,_f)
#define FLUID_FSEEK(_f,_n,_set)      fseek(_f,_n,_set)
#define FLUID_FTELL(_f)              ftell(_f)
#define FLUID_MEMCPY(_dst,_src,_n)   memcpy(_dst,_src,_n)
#define FLUID_MEMSET(_s,_c,_n)       memset(_s,_c,_n)
#define FLUID_STRLEN(_s)             strlen(_s)
#define FLUID_STRCMP(_s,_t)          strcmp(_s,_t)
#define FLUID_STRNCMP(_s,_t,_n)      strncmp(_s,_t,_n)
#define FLUID_STRCPY(_dst,_src)      strcpy(_dst,_src)
#define FLUID_STRCHR(_s,_c)          strchr(_s,_c)
#define FLUID_STRDUP(s)              FLUID_STRCPY((S8*)FLUID_MALLOC(FLUID_STRLEN(s) + 1), s)
#define FLUID_SPRINTF                sprintf
#define FLUID_FPRINTF                fprintf

#define fluid_clip(_val, _min, _max) \
{ (_val) = ((_val) < (_min))? (_min) : (((_val) > (_max))? (_max) : (_val)); }

#if WITH_FTS
#define FLUID_PRINTF                 post
#define FLUID_FLUSH()
#else
#define FLUID_PRINTF                 printf
#define FLUID_FLUSH()                fflush(stdout)
#endif

#define FLUID_LOG                    fluid_log

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif


#define FLUID_ASSERT(a,b)
#define FLUID_ASSERT_P(a,b)

S8 *fluid_error (void);


/* Internationalization */
#define _(s) s


#endif /* _FLUIDSYNTH_PRIV_H */