/*
** Zabbix
** Copyright (C) 2001-2025 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#ifndef ZABBIX_TYPES_H
#define ZABBIX_TYPES_H

#include "sysinc.h"

#if defined(_WINDOWS)
#	define ZBX_THREAD_LOCAL __declspec(thread)
#else
/* for non windows build thread local storage is required only for agent2 */
#	if defined(ZBX_BUILD_AGENT2)
#		if defined(HAVE_THREAD_LOCAL) && (defined(__GNUC__) || defined(__clang__) || defined(__MINGW32__))
#			define ZBX_THREAD_LOCAL __thread
#		else
#			error "C compiler is not compatible with agent2 assembly"
#		endif
#	endif
#	if !defined(ZBX_THREAD_LOCAL)
#		define ZBX_THREAD_LOCAL
#	endif
#endif

#if defined(_WINDOWS) || defined(__MINGW32__)
#	define zbx_open(pathname, flags)	__zbx_open(pathname, flags | O_BINARY)
#	define PATH_SEPARATOR	'\\'
#else
#	define zbx_open(pathname, flags)	open(pathname, flags)
#	define PATH_SEPARATOR	'/'
#endif

#if defined(_WINDOWS)
#	include <strsafe.h>

#	ifndef __UINT64_C
#		define __UINT64_C(x)	x
#	endif

#	ifndef __INT64_C
#		define __INT64_C(x)	x
#	endif

#	define zbx_uint64_t	unsigned __int64
#	define ZBX_FS_UI64	"%I64u"
#	define ZBX_FS_UO64	"%I64o"
#	define ZBX_FS_UX64	"%I64x"

#	define zbx_int64_t	__int64
#	define ZBX_FS_I64	"%I64d"
#	define ZBX_FS_O64	"%I64o"
#	define ZBX_FS_X64	"%I64x"

#	define snprintf		_snprintf

#	define alloca		_alloca

#	ifndef uint32_t
typedef unsigned __int32	zbx_uint32_t;
#	else
typedef uint32_t		zbx_uint32_t;
#	endif

#	define strcasecmp	lstrcmpiA

#	if defined(__INT_MAX__) && __INT_MAX__ == 2147483647
typedef int	ssize_t;
#	else
typedef long	ssize_t;
#	endif

#else	/* _WINDOWS */
#	ifndef __UINT64_C
#		ifdef UINT64_C
#			define __UINT64_C(c)	(UINT64_C(c))
#		else
#			define __UINT64_C(c)	(c ## ULL)
#		endif
#	endif

#	ifndef __INT64_C
#		ifdef INT64_C
#			define __INT64_C(c)	(INT64_C(c))
#		else
#			define __INT64_C(c)	(c ## LL)
#		endif
#	endif

#	define zbx_uint64_t	uint64_t
#	if __WORDSIZE == 64 || defined(__64BIT__)
#		if defined(__APPLE__) && defined(__MACH__)	/* OS X */
#			define ZBX_FS_UI64	"%llu"
#			define ZBX_FS_UO64	"%llo"
#			define ZBX_FS_UX64	"%llx"
#		else
#			define ZBX_FS_UI64	"%lu"
#			define ZBX_FS_UO64	"%lo"
#			define ZBX_FS_UX64	"%lx"
#		endif
#	else
#		ifdef HAVE_LONG_LONG_QU
#			define ZBX_FS_UI64	"%qu"
#			define ZBX_FS_UO64	"%qo"
#			define ZBX_FS_UX64	"%qx"
#		else
#			define ZBX_FS_UI64	"%llu"
#			define ZBX_FS_UO64	"%llo"
#			define ZBX_FS_UX64	"%llx"
#		endif
#	endif

#	define zbx_int64_t	int64_t
#	if __WORDSIZE == 64 || defined(__64BIT__)
#		if defined(__APPLE__) && defined(__MACH__)	/* OS X */
#			define ZBX_FS_I64	"%lld"
#			define ZBX_FS_O64	"%llo"
#			define ZBX_FS_X64	"%llx"
#		else
#			define ZBX_FS_I64	"%ld"
#			define ZBX_FS_O64	"%lo"
#			define ZBX_FS_X64	"%lx"
#		endif
#	else
#		ifdef HAVE_LONG_LONG_QU
#			define ZBX_FS_I64	"%qd"
#			define ZBX_FS_O64	"%qo"
#			define ZBX_FS_X64	"%qx"
#		else
#			define ZBX_FS_I64	"%lld"
#			define ZBX_FS_O64	"%llo"
#			define ZBX_FS_X64	"%llx"
#		endif
#	endif

typedef uint32_t	zbx_uint32_t;

#endif	/* _WINDOWS */

#if defined(_WINDOWS)
#	define zbx_stat(path, buf)		__zbx_stat(path, buf)
#	define zbx_fstat(fd, buf)		_fstat64(fd, buf)

typedef __int64	zbx_offset_t;
#	define zbx_lseek(fd, offset, whence)	_lseeki64(fd, (zbx_offset_t)(offset), whence)

#elif defined(__MINGW32__)
#	define zbx_stat(path, buf)		__zbx_stat(path, buf)
#	define zbx_fstat(fd, buf)		_fstat64(fd, buf)

typedef off64_t	zbx_offset_t;
#	define zbx_lseek(fd, offset, whence)	lseek64(fd, (zbx_offset_t)(offset), whence)

#else
#	define zbx_stat(path, buf)		stat(path, buf)
#	define zbx_fstat(fd, buf)		fstat(fd, buf)

typedef off_t	zbx_offset_t;
#	define zbx_lseek(fd, offset, whence)	lseek(fd, (zbx_offset_t)(offset), whence)

#endif

#define ZBX_FS_DBL		"%lf"
#define ZBX_FS_DBL_EXT(p)	"%." #p "lf"
#define ZBX_FS_DBL64		"%.17G"

#ifdef HAVE_ORACLE
#	define ZBX_FS_DBL64_SQL	ZBX_FS_DBL64 "d"
#else
#	define ZBX_FS_DBL64_SQL	ZBX_FS_DBL64
#endif

#define ZBX_PTR_SIZE		sizeof(void *)
#define ZBX_FS_SIZE_T		ZBX_FS_UI64
#define ZBX_FS_SSIZE_T		ZBX_FS_I64
#define ZBX_FS_TIME_T		ZBX_FS_I64
#define zbx_fs_size_t		zbx_uint64_t	/* use this type only in calls to printf() for formatting size_t */
#define zbx_fs_ssize_t		zbx_int64_t	/* use this type only in calls to printf() for formatting ssize_t */
#define zbx_fs_time_t		zbx_int64_t	/* use this type only in calls to printf() for formatting time_t */

#ifndef S_ISREG
#	define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
#	define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#define ZBX_STR2UINT64(uint, string) is_uint64(string, &uint)
#define ZBX_OCT2UINT64(uint, string) sscanf(string, ZBX_FS_UO64, &uint)
#define ZBX_HEX2UINT64(uint, string) sscanf(string, ZBX_FS_UX64, &uint)

#define ZBX_STR2UCHAR(var, string) var = (unsigned char)atoi(string)

#define ZBX_CONST_STRING(str) "" str
#define ZBX_CONST_STRLEN(str) (sizeof(ZBX_CONST_STRING(str)) - 1)

typedef struct
{
	zbx_uint64_t	lo;
	zbx_uint64_t	hi;
}
zbx_uint128_t;

#define ZBX_SIZE_T_ALIGN8(size)	(((size) + 7) & ~(size_t)7)

/* macro to test if a signed value has been assigned to unsigned type (char, short, int, long long) */
#define ZBX_IS_TOP_BIT_SET(x)	(0 != ((__UINT64_C(1) << ((sizeof(x) << 3) - 1)) & (x)))

#if defined(_WINDOWS) || defined(__MINGW32__)
	#define localtime_r(x, y)	localtime_s(y, x)
#endif

typedef struct zbx_variant zbx_variant_t;

#endif
