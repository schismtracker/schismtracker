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
typedef const BYTE * LPCBYTE;
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


//#define ENABLE_EQ

#ifndef FALSE
#define FALSE	false
#endif

#ifndef TRUE
#define TRUE	true
#endif

#endif /* _STDAFX_H_ */

