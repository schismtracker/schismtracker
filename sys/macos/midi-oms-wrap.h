/**
 * Functions for interfacing with the Open Music System.
 * 
 * Copyright (C) 2026 Paper
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
**/

#ifndef OMS_H_
#define OMS_H_

#ifdef HAVE_STDINT_H
typedef uint8_t OMS_uint8;
typedef uint16_t OMS_uint16;
typedef uint32_t OMS_uint32;
typedef int8_t OMS_int8;
typedef int16_t OMS_int16;
typedef int32_t OMS_int32;
#else
/* hope these work */
typedef unsigned char OMS_uint8;
typedef unsigned short OMS_uint16;
typedef unsigned long OMS_uint32;
typedef signed char OMS_int8;
typedef signed short OMS_int16;
typedef signed long OMS_int32;
#endif

OMS_int32 OMS_Init(void);

/* Gets the version of OMS currently running.
 *
 * I only know this much about the format:
 *  0xMM??????
 * where MM is the major version. It's likely that the
 * trailing bits are the minor and patch numbers.
 *
 * This function returns 0 if it fails. */
OMS_uint32 OMS_GetVersion(void);

/* The rest of these functions are simply wrappers around functions
 * in the OMS table; their purpose is generally unknown.
 *
 * Any functions commented out are ones I've simply not found in
 * the binaries in Ghidra.
 *
 * There are a total of 0x5E available functions to call. */
OMS_uint16 OMS_Unknown0x00(OMS_uint32 p1, OMS_uint32 p2, OMS_uint32 p3, OMS_uint32 p4, OMS_uint32 p5);
void OMS_Unknown0x01(OMS_uint32 x);
void OMS_Unknown0x02(OMS_uint32 p1);
void OMS_Unknown0x03(OMS_uint32 p1);
void OMS_Unknown0x04(OMS_uint32 p1);
OMS_uint32 OMS_Unknown0x06(OMS_uint16 p1);
OMS_uint32 OMS_Unknown0x07(OMS_uint16 p1);
/* OMS_Unknown0x08 */
void OMS_Unknown0x09(OMS_uint32 p1, OMS_uint16 p2, OMS_uint16 p3);
OMS_uint16 OMS_Unknown0x0A(OMS_uint32 a, OMS_uint32 b, OMS_uint16 c, OMS_uint32 d, OMS_uint8 e);
void OMS_Unknown0x0B(OMS_uint32 p1, OMS_uint32 p2, OMS_uint16 p3, OMS_uint32 p4);
/* OMS_Unknown0x0C */
OMS_uint16 OMS_Unknown0x0D(OMS_uint32 p1);
OMS_uint16 OMS_Unknown0x0E(OMS_uint32 p1);
/* OMS_Unknown0x0F */
/* OMS_Unknown0x10 */
/* OMS_Unknown0x11 */
OMS_uint32 OMS_Unknown0x12(void);
OMS_uint32 OMS_Unknown0x13(void);
/* OMS_Unknown0x14 */
/* OMS_Unknown0x15 */
/* OMS_Unknown0x16 */
/* OMS_Unknown0x17 */
/* OMS_Unknown0x18 */
/* OMS_Unknown0x19 */
/* OMS_Unknown0x1A */
/* OMS_Unknown0x1B */
/* OMS_Unknown0x1C */
/* OMS_Unknown0x1D */
OMS_uint8 OMS_Unknown0x1E(OMS_uint32 p1, OMS_uint32 p2);
/* OMS_Unknown0x1F */
/* OMS_Unknown0x20 */
OMS_uint32 OMS_Unknown0x21(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3, OMS_uint16 p4, OMS_uint32 p5);
void OMS_Unknown0x22(OMS_uint32 p1);
void OMS_Unknown0x23(OMS_uint32 p1);
/* OMS_Unknown0x24 */
OMS_uint16 OMS_Unknown0x25(OMS_uint32 p1);
void OMS_Unknown0x26(OMS_uint32 p1, OMS_uint8 p2, OMS_uint16 p3, OMS_uint32 p4, OMS_uint8 p5);
/* OMS_Unknown0x27 */
OMS_uint16 OMS_Unknown0x28(OMS_uint32 p1, OMS_uint32 p2, OMS_uint8 p3, OMS_uint8 p4, OMS_uint32 p5);
OMS_uint16 OMS_Unknown0x29(OMS_uint32 p1, OMS_uint32 p2, OMS_uint16 p3, OMS_uint32 p4, OMS_uint32 p5, OMS_uint32 p6);
/* OMS_Unknown0x2A */
void OMS_Unknown0x2B(OMS_uint16 p1, OMS_uint16 p2);
/* OMS_Unknown0x2C */
/* OMS_Unknown0x2D */
/* OMS_Unknown0x2E */
/* OMS_Unknown0x2F */
void OMS_Unknown0x30(void);
void OMS_Unknown0x31(void);
/* OMS_Unknown0x32 */
/* OMS_Unknown0x33 */
OMS_uint16 OMS_Unknown0x34(OMS_uint16 p1);
/* OMS_Unknown0x35 */
/* OMS_Unknown0x36 */
/* OMS_Unknown0x37 */
/* OMS_Unknown0x38 */
/* OMS_Unknown0x39 */
/* OMS_Unknown0x3A */
OMS_uint8 OMS_Unknown0x3B(OMS_uint8 p1);
/* OMS_Unknown0x3C */
OMS_uint32 OMS_Unknown0x3D(void);
/* OMS_Unknown0x3E */
/* OMS_Unknown0x3F */
/* OMS_Unknown0x40 */
/* OMS_Unknown0x41 */
OMS_uint16 OMS_Unknown0x42(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3, OMS_uint32 p4);
/* OMS_Unknown0x43 */
OMS_uint8 OMS_Unknown0x44(OMS_uint32 p1);
/* OMS_Unknown0x45 */
/* OMS_Unknown0x46 */
/* OMS_Unknown0x47 */
/* OMS_Unknown0x48 */
/* OMS_Unknown0x49 */
/* OMS_Unknown0x4A */
/* OMS_Unknown0x4B */
/* OMS_Unknown0x4C */
void OMS_Unknown0x4D(OMS_uint32 p1, OMS_uint16 p2, OMS_uint16 p3);
/* OMS_Unknown0x4E */
/* OMS_Unknown0x4F */
/* OMS_Unknown0x50 */
/* OMS_Unknown0x51 */
/* OMS_Unknown0x52 */
/* OMS_Unknown0x53 */
/* OMS_Unknown0x54 */
/* OMS_Unknown0x55 */
void OMS_Unknown0x56(OMS_uint32 p1, OMS_uint32 p2);
OMS_uint16 OMS_Unknown0x57(OMS_uint32 p1);
OMS_uint32 OMS_Unknown0x58(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3);
void OMS_Unknown0x59(OMS_uint32 p1, OMS_uint32 p2, OMS_uint32 p3, OMS_uint8 p4, OMS_uint8 p5, OMS_uint8 p6);
OMS_uint32 OMS_Unknown0x5A(OMS_uint32 p1, OMS_uint16 p2, OMS_uint8 p3, OMS_uint32 p4);
OMS_uint16 OMS_Unknown0x5B(OMS_uint16 p1);
OMS_uint32 OMS_Unknown0x5C(OMS_uint32 p1, OMS_uint32 p2);
/* OMS_Unknown0x5D */

#endif /* OMS_H_ */
