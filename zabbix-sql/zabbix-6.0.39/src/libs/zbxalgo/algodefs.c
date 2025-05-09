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

#include "common.h"

#include "zbxalgo.h"

typedef unsigned char uchar;

/*
 * Bob Jenkins hash function (see http://burtleburtle.net/bob/hash/evahash.html)
 */
zbx_hash_t	zbx_hash_lookup2(const void *data, size_t len, zbx_hash_t seed)
{
	const uchar	*p = (const uchar *)data;

	zbx_hash_t	a, b, c;

#define	mix(a, b, c)						\
{								\
	a = a - b;	a = a - c;	a = a ^ (c >> 13);	\
	b = b - c;	b = b - a;	b = b ^ (a << 8);	\
	c = c - a;	c = c - b;	c = c ^ (b >> 13);	\
	a = a - b;	a = a - c;	a = a ^ (c >> 12);	\
	b = b - c;	b = b - a;	b = b ^ (a << 16);	\
	c = c - a;	c = c - b;	c = c ^ (b >> 5);	\
	a = a - b;	a = a - c;	a = a ^ (c >> 3);	\
	b = b - c;	b = b - a;	b = b ^ (a << 10);	\
	c = c - a;	c = c - b;	c = c ^ (b >> 15);	\
}

	a = b = 0x9e3779b9u;
	c = seed;

	while (len >= 12)
	{
		a = a + (p[0] + ((zbx_hash_t)p[1] << 8) + ((zbx_hash_t)p[2]  << 16) + ((zbx_hash_t)p[3]  << 24));
		b = b + (p[4] + ((zbx_hash_t)p[5] << 8) + ((zbx_hash_t)p[6]  << 16) + ((zbx_hash_t)p[7]  << 24));
		c = c + (p[8] + ((zbx_hash_t)p[9] << 8) + ((zbx_hash_t)p[10] << 16) + ((zbx_hash_t)p[11] << 24));

		mix(a, b, c);

		p += 12;
		len -= 12;
	}

	c = c + (zbx_hash_t)len;

	switch (len)
	{
		case 11:	c = c + ((zbx_hash_t)p[10] << 24);
			ZBX_FALLTHROUGH;
		case 10:	c = c + ((zbx_hash_t)p[9] << 16);
			ZBX_FALLTHROUGH;
		case 9:		c = c + ((zbx_hash_t)p[8] << 8);
			ZBX_FALLTHROUGH;
		case 8:		b = b + ((zbx_hash_t)p[7] << 24);
			ZBX_FALLTHROUGH;
		case 7:		b = b + ((zbx_hash_t)p[6] << 16);
			ZBX_FALLTHROUGH;
		case 6:		b = b + ((zbx_hash_t)p[5] << 8);
			ZBX_FALLTHROUGH;
		case 5:		b = b + p[4];
			ZBX_FALLTHROUGH;
		case 4:		a = a + ((zbx_hash_t)p[3] << 24);
			ZBX_FALLTHROUGH;
		case 3:		a = a + ((zbx_hash_t)p[2] << 16);
			ZBX_FALLTHROUGH;
		case 2:		a = a + ((zbx_hash_t)p[1] << 8);
			ZBX_FALLTHROUGH;
		case 1:		a = a + p[0];
	}

	mix(a, b, c);

	return c;
}

/*
 * modified FNV hash function (see http://www.isthe.com/chongo/tech/comp/fnv/)
 */
zbx_hash_t	zbx_hash_modfnv(const void *data, size_t len, zbx_hash_t seed)
{
	const uchar	*p = (const uchar *)data;

	zbx_hash_t	hash;

	hash = 2166136261u ^ seed;

	while (len-- >= 1)
	{
		hash = (hash ^ *(p++)) * 16777619u;
	}

	hash += hash << 13;
	hash ^= hash >> 7;
	hash += hash << 3;
	hash ^= hash >> 17;
	hash += hash << 5;

	return hash;
}

/*
 * Murmur (see http://sites.google.com/site/murmurhash/)
 */
zbx_hash_t	zbx_hash_murmur2(const void *data, size_t len, zbx_hash_t seed)
{
	const uchar	*p = (const uchar *)data;

	zbx_hash_t	hash;

	const zbx_hash_t	m = 0x5bd1e995u;
	const zbx_hash_t	r = 24;

	hash = seed ^ (zbx_hash_t)len;

	while (len >= 4)
	{
		zbx_hash_t	k;

		k = p[0];
		k |= p[1] << 8;
		k |= p[2] << 16;
		k |= p[3] << 24;

		k *= m;
		k ^= k >> r;
		k *= m;

		hash *= m;
		hash ^= k;

		p += 4;
		len -= 4;
	}

	switch (len)
	{
		case 3:	hash ^= p[2] << 16;
			ZBX_FALLTHROUGH;
		case 2: hash ^= p[1] << 8;
			ZBX_FALLTHROUGH;
		case 1: hash ^= p[0];
			hash *= m;
	}

	hash ^= hash >> 13;
	hash *= m;
	hash ^= hash >> 15;

	return hash;
}

/*
 * sdbm (see http://www.cse.yorku.ca/~oz/hash.html)
 */
zbx_hash_t	zbx_hash_sdbm(const void *data, size_t len, zbx_hash_t seed)
{
	const uchar	*p = (const uchar *)data;

	zbx_hash_t	hash = seed;

#if	1

	while (len-- >= 1)
	{
		/* hash = *(p++) + hash * 65599; */

		hash = *(p++) + (hash << 6) + (hash << 16) - hash;
	}

#else	/* Duff's device */

#define	HASH_STEP	len--;						\
			hash = *(p++) + (hash << 6) + (hash << 16) - hash

	switch (len & 7)
	{
			do
			{
				HASH_STEP;
		case 7:		HASH_STEP;
		case 6:		HASH_STEP;
		case 5:		HASH_STEP;
		case 4:		HASH_STEP;
		case 3:		HASH_STEP;
		case 2:		HASH_STEP;
		case 1:		HASH_STEP;
		case 0:		;
			}
			while (len >= 8);
	}

#endif

	return hash;
}

/*
 * djb2 (see http://www.cse.yorku.ca/~oz/hash.html)
 */
zbx_hash_t	zbx_hash_djb2(const void *data, size_t len, zbx_hash_t seed)
{
	const uchar	*p = (const uchar *)data;

	zbx_hash_t	hash;

	hash = 5381u ^ seed;

	while (len-- >= 1)
	{
		/* hash = hash * 33 + *(p++); */

		hash = ((hash << 5) + hash) + *(p++);
	}

	return hash;
}

/*
 * see http://xoshiro.di.unimi.it/splitmix64.c
 */
zbx_hash_t	zbx_hash_splittable64(const void *data)
{
	zbx_uint64_t	value = *(const zbx_uint64_t *)data;

	value ^= value >> 30;
	value *= __UINT64_C(0xbf58476d1ce4e5b9);
	value ^= value >> 27;
	value *= __UINT64_C(0x94d049bb133111eb);
	value ^= value >> 31;

	return (zbx_hash_t)value ^ (value >> 32);
}

/* default hash functions */

zbx_hash_t	zbx_default_ptr_hash_func(const void *data)
{
	return ZBX_DEFAULT_PTR_HASH_ALGO(data, ZBX_PTR_SIZE, ZBX_DEFAULT_HASH_SEED);
}

zbx_hash_t	zbx_default_string_hash_func(const void *data)
{
	return ZBX_DEFAULT_STRING_HASH_ALGO(data, strlen((const char *)data), ZBX_DEFAULT_HASH_SEED);
}

zbx_hash_t	zbx_default_uint64_pair_hash_func(const void *data)
{
	const zbx_uint64_pair_t	*pair = (const zbx_uint64_pair_t *)data;

	zbx_hash_t		hash;

	hash = ZBX_DEFAULT_UINT64_HASH_FUNC(&pair->first);
	hash = ZBX_DEFAULT_UINT64_HASH_ALGO(&pair->second, sizeof(pair->second), hash);

	return hash;
}

/* default comparison functions */

int	zbx_default_int_compare_func(const void *d1, const void *d2)
{
	const int	*i1 = (const int *)d1;
	const int	*i2 = (const int *)d2;

	ZBX_RETURN_IF_NOT_EQUAL(*i1, *i2);

	return 0;
}

int	zbx_default_uint64_compare_func(const void *d1, const void *d2)
{
	const zbx_uint64_t	*i1 = (const zbx_uint64_t *)d1;
	const zbx_uint64_t	*i2 = (const zbx_uint64_t *)d2;

	ZBX_RETURN_IF_NOT_EQUAL(*i1, *i2);

	return 0;
}

int	zbx_default_uint64_ptr_compare_func(const void *d1, const void *d2)
{
	const zbx_uint64_t	*p1 = *(const zbx_uint64_t * const *)d1;
	const zbx_uint64_t	*p2 = *(const zbx_uint64_t * const *)d2;

	return zbx_default_uint64_compare_func(p1, p2);
}

int	zbx_default_str_compare_func(const void *d1, const void *d2)
{
	return strcmp(*(const char * const *)d1, *(const char * const *)d2);
}

int	zbx_natural_str_compare_func(const void *d1, const void *d2)
{
	return zbx_strcmp_natural(*(const char * const *)d1, *(const char * const *)d2);
}

int	zbx_default_ptr_compare_func(const void *d1, const void *d2)
{
	const void	*p1 = *(const void * const *)d1;
	const void	*p2 = *(const void * const *)d2;

	ZBX_RETURN_IF_NOT_EQUAL(p1, p2);

	return 0;
}

int	zbx_default_uint64_pair_compare_func(const void *d1, const void *d2)
{
	const zbx_uint64_pair_t	*p1 = (const zbx_uint64_pair_t *)d1;
	const zbx_uint64_pair_t	*p2 = (const zbx_uint64_pair_t *)d2;

	ZBX_RETURN_IF_NOT_EQUAL(p1->first, p2->first);
	ZBX_RETURN_IF_NOT_EQUAL(p1->second, p2->second);

	return 0;
}

int	zbx_default_dbl_compare_func(const void *d1, const void *d2)
{
	const double	*p1 = (const double *)d1;
	const double	*p2 = (const double *)d2;

	ZBX_RETURN_IF_DBL_NOT_EQUAL(*p1, *p2);

	return 0;
}

/* default memory management functions */

void	*zbx_default_mem_malloc_func(void *old, size_t size)
{
	return zbx_malloc(old, size);
}

void	*zbx_default_mem_realloc_func(void *old, size_t size)
{
	return zbx_realloc(old, size);
}

void	zbx_default_mem_free_func(void *ptr)
{
	zbx_free(ptr);
}

/* numeric functions */

int	is_prime(int n)
{
	int i;

	if (n <= 1)
		return 0;
	if (n == 2)
		return 1;
	if (n % 2 == 0)
		return 0;

	for (i = 3; i * i <= n; i+=2)
		if (n % i == 0)
			return 0;

	return 1;
}

int	next_prime(int n)
{
	while (!is_prime(n))
		n++;

	return n;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculate integer part of square root of a 32 bit integer value   *
 *                                                                            *
 * Parameters: value     - [IN] the value to calculate square root for        *
 *                                                                            *
 * Return value: the integer part of square root                              *
 *                                                                            *
 * Comments: Uses basic digit by digit square root calculation algorithm with *
 *           binary base.                                                     *
 *                                                                            *
 ******************************************************************************/
unsigned int	zbx_isqrt32(unsigned int value)
{
	unsigned int	i, remainder = 0, result = 0, p;

	for (i = 0; i < 16; i++)
	{
		result <<= 1;
		remainder = (remainder << 2) + (value >> 30);
		value <<= 2;

		p = (result << 1) | 1;
		if (p <= remainder)
		{
			remainder -= p;
			result |= 1;
		}
	}

	return result;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculate UUID version 4 as string of 32 symbols                  *
 *                                                                            *
 * Parameters: seed    - [IN] string for seed calculation                     *
 *                                                                            *
 * Return value: uuid string                                                  *
 *                                                                            *
 ******************************************************************************/
char	*zbx_gen_uuid4(const char *seed)
{
	size_t		i;
	const char	*hex = "0123456789abcdef";
	char		*ptr, *uuid;
	md5_state_t	state;
	md5_byte_t	hash[MD5_DIGEST_SIZE];

#define ZBX_UUID_VERSION	4
#define ZBX_UUID_VARIANT	2

	ptr = uuid = (char *)zbx_malloc(NULL, 2 * MD5_DIGEST_SIZE + 1);

	zbx_md5_init(&state);
	zbx_md5_append(&state, (const md5_byte_t *)seed, (int)strlen(seed));
	zbx_md5_finish(&state, hash);

	hash[6] = (md5_byte_t)((hash[6] & 0xf) | (ZBX_UUID_VERSION << 4));
	hash[8] = (md5_byte_t)((hash[8] & 0x3f) | (ZBX_UUID_VARIANT << 6));

	for (i = 0; i < MD5_DIGEST_SIZE; i++)
	{
		*ptr++ = hex[(hash[i] >> 4) & 0xf];
		*ptr++ = hex[hash[i] & 0xf];
	}

	*ptr = '\0';

	return uuid;
}
