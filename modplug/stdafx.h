/*
 * This source code is public domain.
 *
 * Authors: Rani Assaf <rani@magic.metawire.com>,
 *          Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/

#ifndef _STDAFX_H_
#define _STDAFX_H_

#include "headers.h"

#ifdef MSC_VER

#pragma warning (disable:4201)
#pragma warning (disable:4514)
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <stdio.h>

inline void ProcessPlugins(int n) {}

#else

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef signed char CHAR;
typedef unsigned char UCHAR;
typedef unsigned char* PUCHAR;
typedef unsigned short USHORT;
typedef unsigned int ULONG;
typedef unsigned int UINT;
typedef unsigned int DWORD;
typedef int LONG;
typedef unsigned short WORD;
typedef unsigned char BYTE;
typedef unsigned char * LPBYTE;
#ifdef __cplusplus
typedef bool BOOL;
#endif
typedef char * LPSTR;
typedef void *  LPVOID;
typedef int * LPLONG;
typedef unsigned int * LPDWORD;
typedef unsigned short * LPWORD;
typedef const char * LPCSTR;
typedef long long LONGLONG;
typedef void * PVOID;
typedef void VOID;


#define NO_AGC
#define LPCTSTR LPCSTR
#define lstrcpyn strncpy
#define lstrcpy strcpy
#define lstrcmp strcmp
#define WAVE_FORMAT_PCM 1
//#define ENABLE_EQ

#define  GHND   0

#ifdef __cplusplus

#define GlobalAllocPtr(ign,size) calloc(1,size)
#define ProcessPlugins(z) /* noop */
#define GlobalFreePtr(p) free((void *)(p))

#define strnicmp(a,b,c)		strncasecmp(a,b,c)
#define wsprintf			sprintf
#endif

#ifndef FALSE
#define FALSE	false
#endif

#ifndef TRUE
#define TRUE	true
#endif

#endif // MSC_VER

#endif



