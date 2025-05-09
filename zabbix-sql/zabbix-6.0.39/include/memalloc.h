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

#ifndef ZABBIX_MEMALLOC_H
#define ZABBIX_MEMALLOC_H

#include "zbxtypes.h"

#define MEM_MIN_ALLOC	24	/* should be a multiple of 8 and at least (2 * ZBX_PTR_SIZE) */

#define MEM_MIN_BUCKET_SIZE	MEM_MIN_ALLOC
#define MEM_MAX_BUCKET_SIZE	256 /* starting from this size all free chunks are put into the same bucket */
#define MEM_BUCKET_COUNT	((MEM_MAX_BUCKET_SIZE - MEM_MIN_BUCKET_SIZE) / 8 + 1)

typedef struct
{
	void		*base;
	void		**buckets;
	void		*lo_bound;
	void		*hi_bound;
	zbx_uint64_t	free_size;
	zbx_uint64_t	used_size;
	zbx_uint64_t	orig_size;
	zbx_uint64_t	total_size;
	int		shm_id;

	/* Continue execution in out of memory situation.                         */
	/* Normally allocator forces exit when it runs out of allocatable memory. */
	/* Set this flag to 1 to allow execution in out of memory situations.     */
	char		allow_oom;

	const char	*mem_descr;
	const char	*mem_param;
}
zbx_mem_info_t;

typedef struct
{
	zbx_uint64_t	free_size;
	zbx_uint64_t	used_size;
	zbx_uint64_t	min_chunk_size;
	zbx_uint64_t	max_chunk_size;
	zbx_uint64_t	overhead;
	unsigned int	chunks_num[MEM_BUCKET_COUNT];
	unsigned int	free_chunks;
	unsigned int	used_chunks;
}
zbx_mem_stats_t;

int	zbx_mem_create(zbx_mem_info_t **info, zbx_uint64_t size, const char *descr, const char *param, int allow_oom,
		char **error);
void	zbx_mem_destroy(zbx_mem_info_t *info);

#define	zbx_mem_malloc(info, old, size) __zbx_mem_malloc(__FILE__, __LINE__, info, old, size)
#define	zbx_mem_realloc(info, old, size) __zbx_mem_realloc(__FILE__, __LINE__, info, old, size)
#define	zbx_mem_free(info, ptr)				\
							\
do							\
{							\
	__zbx_mem_free(__FILE__, __LINE__, info, ptr);	\
	ptr = NULL;					\
}							\
while (0)

void	*__zbx_mem_malloc(const char *file, int line, zbx_mem_info_t *info, const void *old, size_t size);
void	*__zbx_mem_realloc(const char *file, int line, zbx_mem_info_t *info, void *old, size_t size);
void	__zbx_mem_free(const char *file, int line, zbx_mem_info_t *info, void *ptr);

void	zbx_mem_clear(zbx_mem_info_t *info);

void	zbx_mem_get_stats(const zbx_mem_info_t *info, zbx_mem_stats_t *stats);
void	zbx_mem_dump_stats(int level, zbx_mem_info_t *info);

size_t	zbx_mem_required_size(int chunks_num, const char *descr, const char *param);
zbx_uint64_t	zbx_mem_required_chunk_size(zbx_uint64_t size);

#define ZBX_MEM_FUNC1_DECL_MALLOC(__prefix)				\
static void	*__prefix ## _mem_malloc_func(void *old, size_t size)
#define ZBX_MEM_FUNC1_DECL_REALLOC(__prefix)				\
static void	*__prefix ## _mem_realloc_func(void *old, size_t size)
#define ZBX_MEM_FUNC1_DECL_FREE(__prefix)				\
static void	__prefix ## _mem_free_func(void *ptr)

#define ZBX_MEM_FUNC1_IMPL_MALLOC(__prefix, __info)			\
									\
static void	*__prefix ## _mem_malloc_func(void *old, size_t size)	\
{									\
	return zbx_mem_malloc(__info, old, size);			\
}

#define ZBX_MEM_FUNC1_IMPL_REALLOC(__prefix, __info)			\
									\
static void	*__prefix ## _mem_realloc_func(void *old, size_t size)	\
{									\
	return zbx_mem_realloc(__info, old, size);			\
}

#define ZBX_MEM_FUNC1_IMPL_FREE(__prefix, __info)			\
									\
static void	__prefix ## _mem_free_func(void *ptr)			\
{									\
	zbx_mem_free(__info, ptr);					\
}

#define ZBX_MEM_FUNC_DECL(__prefix)					\
									\
ZBX_MEM_FUNC1_DECL_MALLOC(__prefix);					\
ZBX_MEM_FUNC1_DECL_REALLOC(__prefix);					\
ZBX_MEM_FUNC1_DECL_FREE(__prefix);

#define ZBX_MEM_FUNC_IMPL(__prefix, __info)				\
									\
ZBX_MEM_FUNC1_IMPL_MALLOC(__prefix, __info)				\
ZBX_MEM_FUNC1_IMPL_REALLOC(__prefix, __info)				\
ZBX_MEM_FUNC1_IMPL_FREE(__prefix, __info)

#endif
