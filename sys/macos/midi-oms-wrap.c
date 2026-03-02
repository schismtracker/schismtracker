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

#include "midi-oms-wrap.h"

#include "MixedMode.h"
#include "Gestalt.h"
#include "DriverServices.h"

#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static UniversalProcPtr OMS_Internal_GetGestaltPtr(void)
{
	OSErr err;
	OMS_uint32 x;

	err = Gestalt(0x204F4D53 /* " OMS" */, &x);
	if (err != noErr)
		return NULL;

	return (UniversalProcPtr)x;
}

/* `selector` is some kind of selector
 * `xtra` is... some extra data ???
 *
 * this function returns 0xFFFFFFFF if the selector is not supported. */
static OMS_uint32 OMS_Internal_CallGestaltPtr(UniversalProcPtr omsptr, OMS_uint16 selector, OMS_uint32 xtra)
{
	return CallUniversalProc(omsptr, kThinkCStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode),
		selector, xtra);
}

static UniversalProcPtr OMS_Internal_FuncTable[0x5E];

OMS_int32 OMS_Init(void)
{
	OMS_uint32 functable;
	UniversalProcPtr omsptr;

	omsptr = OMS_Internal_GetGestaltPtr();
	if (!omsptr)
		return -1;

	functable = OMS_Internal_CallGestaltPtr(omsptr, 4, 0);
	if (functable != 0xFFFFFFFF) {
		BlockCopy((const void *)functable, OMS_Internal_FuncTable, sizeof(OMS_Internal_FuncTable));
	} else {
		OMS_uint32 i;

		functable = OMS_Internal_CallGestaltPtr(omsptr, 1, 0);
		if (!functable)
			return -1;

		/* `functable` points to an array??? */
		for (i = 0; i < ARRAY_SIZE(OMS_Internal_FuncTable); i++, functable += 4)
			OMS_Internal_FuncTable[i] = (UniversalProcPtr)functable;
	}

	return 0;
}

OMS_uint32 OMS_GetVersion(void)
{
	UniversalProcPtr omsptr;

	omsptr = OMS_Internal_GetGestaltPtr();
	if (!omsptr)
		return 0;

	return OMS_Internal_CallGestaltPtr(omsptr, 0, 0);
}

/* 0x0X --------------------------------------------------------------------------- */

OMS_uint16 OMS_Unknown0x00(OMS_uint32 p1, OMS_uint32 p2, OMS_uint32 p3, OMS_uint32 p4, OMS_uint32 p5)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x00],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode) | STACK_ROUTINE_PARAMETER(3, kFourByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode) | STACK_ROUTINE_PARAMETER(5, kFourByteCode)
	, p1, p2, p3, p4, p5);
}

void OMS_Unknown0x01(OMS_uint32 x)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x01], kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode), x);
}

void OMS_Unknown0x02(OMS_uint32 p1)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x02],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

void OMS_Unknown0x03(OMS_uint32 p1)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x03],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

void OMS_Unknown0x04(OMS_uint32 p1)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x04],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

OMS_uint32 OMS_Unknown0x06(OMS_uint16 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x06],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode)
	, p1);
}

OMS_uint32 OMS_Unknown0x07(OMS_uint16 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x07],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode)
	, p1);
}

void OMS_Unknown0x09(OMS_uint32 p1, OMS_uint16 p2, OMS_uint16 p3)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x09],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kTwoByteCode) | STACK_ROUTINE_PARAMETER(3, kTwoByteCode)
	, p1, p2, p3);
}

OMS_uint16 OMS_Unknown0x0A(OMS_uint32 a, OMS_uint32 b, OMS_uint16 c, OMS_uint32 d, OMS_uint8 e)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x0A],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode)
			| STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode)
			| STACK_ROUTINE_PARAMETER(3, kTwoByteCode)  | STACK_ROUTINE_PARAMETER(4, kFourByteCode)
			| STACK_ROUTINE_PARAMETER(5, kOneByteCode), a, b, c, d, e);
}

void OMS_Unknown0x0B(OMS_uint32 p1, OMS_uint32 p2, OMS_uint16 p3, OMS_uint32 p4)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x0B],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode) | STACK_ROUTINE_PARAMETER(3, kTwoByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode)
	, p1, p2, p3, p4);
}

OMS_uint16 OMS_Unknown0x0D(OMS_uint32 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x0D],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

OMS_uint16 OMS_Unknown0x0E(OMS_uint32 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x0E],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

/* 0x1X --------------------------------------------------------------------------- */

OMS_uint32 OMS_Unknown0x12(void)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x12],
		kPascalStackBased | RESULT_SIZE(kFourByteCode)
	);
}

OMS_uint32 OMS_Unknown0x13(void)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x13],
		kPascalStackBased | RESULT_SIZE(kFourByteCode)
	);
}

OMS_uint8 OMS_Unknown0x1E(OMS_uint32 p1, OMS_uint32 p2)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x1E],
		kPascalStackBased | RESULT_SIZE(kOneByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode)
	, p1, p2);
}

/* 0x2X --------------------------------------------------------------------------- */

OMS_uint32 OMS_Unknown0x21(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3, OMS_uint16 p4, OMS_uint32 p5)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x21],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kOneByteCode) | STACK_ROUTINE_PARAMETER(3, kFourByteCode) | STACK_ROUTINE_PARAMETER(4, kTwoByteCode) | STACK_ROUTINE_PARAMETER(5, kFourByteCode)
	, p1, p2, p3, p4, p5);
}

void OMS_Unknown0x22(OMS_uint32 p1)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x22],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

void OMS_Unknown0x23(OMS_uint32 p1)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x23],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

OMS_uint16 OMS_Unknown0x25(OMS_uint32 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x25],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

void OMS_Unknown0x26(OMS_uint32 p1, OMS_uint8 p2, OMS_uint16 p3, OMS_uint32 p4, OMS_uint8 p5)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x26],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kOneByteCode) | STACK_ROUTINE_PARAMETER(3, kTwoByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode) | STACK_ROUTINE_PARAMETER(5, kOneByteCode)
	, p1, p2, p3, p4, p5);
}

OMS_uint16 OMS_Unknown0x28(OMS_uint32 p1, OMS_uint32 p2, OMS_uint8 p3, OMS_uint8 p4, OMS_uint32 p5)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x28],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode) | STACK_ROUTINE_PARAMETER(3, kOneByteCode) | STACK_ROUTINE_PARAMETER(4, kOneByteCode) | STACK_ROUTINE_PARAMETER(5, kFourByteCode)
	, p1, p2, p3, p4, p5);
}

OMS_uint16 OMS_Unknown0x29(OMS_uint32 p1, OMS_uint32 p2, OMS_uint16 p3, OMS_uint32 p4, OMS_uint32 p5, OMS_uint32 p6)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x29],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode) | STACK_ROUTINE_PARAMETER(3, kTwoByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode) | STACK_ROUTINE_PARAMETER(5, kFourByteCode) | STACK_ROUTINE_PARAMETER(6, kFourByteCode)
	, p1, p2, p3, p4, p5, p6);
}

void OMS_Unknown0x2B(OMS_uint16 p1, OMS_uint16 p2)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x2B],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode) | STACK_ROUTINE_PARAMETER(2, kTwoByteCode)
	, p1, p2);
}

/* 0x3X --------------------------------------------------------------------------- */

void OMS_Unknown0x30(void)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x30], 0);
}

void OMS_Unknown0x31(void)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x31], 0);
}

OMS_uint16 OMS_Unknown0x34(OMS_uint16 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x34],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode)
	, p1);
}

OMS_uint8 OMS_Unknown0x3B(OMS_uint8 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x3B],
		kPascalStackBased | RESULT_SIZE(kOneByteCode) | STACK_ROUTINE_PARAMETER(1, kOneByteCode)
	, p1);
}

OMS_uint32 OMS_Unknown0x3D(void)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x3D],
		kPascalStackBased | RESULT_SIZE(kFourByteCode)
	);
}

/* 0x4X --------------------------------------------------------------------------- */

OMS_uint16 OMS_Unknown0x42(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3, OMS_uint32 p4)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x42],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kOneByteCode) | STACK_ROUTINE_PARAMETER(3, kFourByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode)
	, p1, p2, p3, p4);
}

OMS_uint8 OMS_Unknown0x44(OMS_uint32 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x44],
		kPascalStackBased | RESULT_SIZE(kOneByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

void OMS_Unknown0x4D(OMS_uint32 p1, OMS_uint16 p2, OMS_uint16 p3)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x4D],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kTwoByteCode) | STACK_ROUTINE_PARAMETER(3, kTwoByteCode)
	, p1, p2, p3);
}

/* 0x5X --------------------------------------------------------------------------- */

void OMS_Unknown0x56(OMS_uint32 p1, OMS_uint32 p2)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x56],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode)
	, p1, p2);
}

OMS_uint16 OMS_Unknown0x57(OMS_uint32 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x57],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode)
	, p1);
}

OMS_uint32 OMS_Unknown0x58(OMS_uint32 p1, OMS_uint8 p2, OMS_uint32 p3)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x58],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kOneByteCode) | STACK_ROUTINE_PARAMETER(3, kFourByteCode)
	, p1, p2, p3);
}

void OMS_Unknown0x59(OMS_uint32 p1, OMS_uint32 p2, OMS_uint32 p3, OMS_uint8 p4, OMS_uint8 p5, OMS_uint8 p6)
{
	CallUniversalProc(OMS_Internal_FuncTable[0x59],
		kPascalStackBased | RESULT_SIZE(kNoByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode) | STACK_ROUTINE_PARAMETER(3, kFourByteCode) | STACK_ROUTINE_PARAMETER(4, kOneByteCode) | STACK_ROUTINE_PARAMETER(5, kOneByteCode) | STACK_ROUTINE_PARAMETER(6, kOneByteCode)
	, p1, p2, p3, p4, p5, p6);
}

OMS_uint32 OMS_Unknown0x5A(OMS_uint32 p1, OMS_uint16 p2, OMS_uint8 p3, OMS_uint32 p4)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x5A],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kTwoByteCode) | STACK_ROUTINE_PARAMETER(3, kOneByteCode) | STACK_ROUTINE_PARAMETER(4, kFourByteCode)
	, p1, p2, p3, p4);
}

OMS_uint16 OMS_Unknown0x5B(OMS_uint16 p1)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x5B],
		kPascalStackBased | RESULT_SIZE(kTwoByteCode) | STACK_ROUTINE_PARAMETER(1, kTwoByteCode)
	, p1);
}

OMS_uint32 OMS_Unknown0x5C(OMS_uint32 p1, OMS_uint32 p2)
{
	return CallUniversalProc(OMS_Internal_FuncTable[0x5C],
		kPascalStackBased | RESULT_SIZE(kFourByteCode) | STACK_ROUTINE_PARAMETER(1, kFourByteCode) | STACK_ROUTINE_PARAMETER(2, kFourByteCode)
	, p1, p2);
}

