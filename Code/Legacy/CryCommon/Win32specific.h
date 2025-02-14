/*
 * Copyright (c) Contributors to the Open 3D Engine Project.
 * For complete copyright and license terms please see the LICENSE at the root of this distribution.
 *
 * SPDX-License-Identifier: Apache-2.0 OR MIT
 *
 */


// Description : Specific to Win32 declarations, inline functions etc.


#ifndef CRYINCLUDE_CRYCOMMON_WIN32SPECIFIC_H
#define CRYINCLUDE_CRYCOMMON_WIN32SPECIFIC_H
#pragma once


#define _CPU_X86
#define _CPU_SSE
#ifdef _DEBUG
#define ILINE _inline
#else
#define ILINE __forceinline
#endif

#define DEBUG_BREAK _asm { int 3 }
#define RC_EXECUTABLE "rc.exe"
#define DEPRECATED __declspec(deprecated)
#define TYPENAME(x) typeid(x).name()
#define SIZEOF_PTR 4

#ifndef _WIN32_WINNT
# define _WIN32_WINNT 0x501
#endif

//////////////////////////////////////////////////////////////////////////
// Standard includes.
//////////////////////////////////////////////////////////////////////////
#include <malloc.h>
#include <io.h>
#include <new.h>
#include <direct.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
//////////////////////////////////////////////////////////////////////////

// Special intrinsics
#include <intrin.h> // moved here to prevent assert from being redefined when included elsewhere


//////////////////////////////////////////////////////////////////////////
// Define platform independent types.
//////////////////////////////////////////////////////////////////////////
#include "BaseTypes.h"

#define THREADID_NULL -1

typedef unsigned char BYTE;
typedef unsigned int threadID;
typedef unsigned long DWORD;
typedef double        real;  // biggest float-type on this machine
typedef long          LONG;

#if defined(_WIN64)
typedef __int64 INT_PTR, * PINT_PTR;
typedef unsigned __int64 UINT_PTR, * PUINT_PTR;

typedef __int64 LONG_PTR, * PLONG_PTR;
typedef unsigned __int64 ULONG_PTR, * PULONG_PTR;

typedef ULONG_PTR DWORD_PTR, * PDWORD_PTR;
#else
typedef __w64 int INT_PTR, * PINT_PTR;
typedef __w64 unsigned int UINT_PTR, * PUINT_PTR;

typedef __w64 long LONG_PTR, * PLONG_PTR;
typedef __w64 unsigned long ULONG_PTR, * PULONG_PTR;

typedef ULONG_PTR DWORD_PTR, * PDWORD_PTR;
#endif

typedef void* THREAD_HANDLE;
typedef void* EVENT_HANDLE;

//////////////////////////////////////////////////////////////////////////
// Multi platform Hi resolution ticks function, should only be used for profiling.
//////////////////////////////////////////////////////////////////////////


int64 CryGetTicks();
int64 CryGetTicksPerSec();

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) { if (p) { delete (p); (p) = NULL; } \
}
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) { if (p) { delete [] (p); (p) = NULL; } \
}
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = NULL; } \
}
#endif

#define _MS_ALIGN(num) \
    AZ_PUSH_DISABLE_WARNING(4324, "-Wunknown-warning-option") \
    __declspec(align(num))

#define DEFINE_ALIGNED_DATA(type, name, alignment) _declspec(align(alignment)) type name;
#define DEFINE_ALIGNED_DATA_STATIC(type, name, alignment) static _declspec(align(alignment)) type name;
#define DEFINE_ALIGNED_DATA_CONST(type, name, alignment) const _declspec(align(alignment)) type name;

#ifndef FILE_ATTRIBUTE_NORMAL
    #define FILE_ATTRIBUTE_NORMAL 0x00000080
#endif

#define FP16_TERRAIN
#define TARGET_DEFAULT_ALIGN (0x4U)


#endif // CRYINCLUDE_CRYCOMMON_WIN32SPECIFIC_H
