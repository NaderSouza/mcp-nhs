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
#include "threads.h"
#include "module.h"

#include "zbxcrypto.h"

#ifdef HAVE_ICONV
#	include <iconv.h>
#endif

static const char	copyright_message[] =
	"Copyright (C) 2025 Zabbix SIA\n"
	"License GPLv2+: GNU GPL version 2 or later <https://www.gnu.org/licenses/>.\n"
	"This is free software: you are free to change and redistribute it according to\n"
	"the license. There is NO WARRANTY, to the extent permitted by law.";

static const char	help_message_footer[] =
	"Report bugs to: <https://support.zabbix.com>\n"
	"Zabbix home page: <http://www.zabbix.com>\n"
	"Documentation: <https://www.zabbix.com/documentation>";

/******************************************************************************
 *                                                                            *
 * Purpose: print version and compilation time of application on stdout       *
 *          by application request with parameter '-V'                        *
 *                                                                            *
 * Comments:  title_message - is global variable which must be initialized    *
 *                            in each zabbix application                      *
 *                                                                            *
 ******************************************************************************/
void	version(void)
{
	printf("%s (Zabbix) %s\n", title_message, ZABBIX_VERSION);
	printf("Revision %s %s, compilation time: %s %s\n\n", ZABBIX_REVISION, ZABBIX_REVDATE, __DATE__, __TIME__);
	puts(copyright_message);
#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
	printf("\n");
	zbx_tls_version();
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: print application parameters on stdout with layout suitable for   *
 *          80-column terminal                                                *
 *                                                                            *
 * Comments:  usage_message - is global variable which must be initialized    *
 *                            in each zabbix application                      *
 *                                                                            *
 ******************************************************************************/
void	usage(void)
{
#define ZBX_MAXCOL	79
#define ZBX_SPACE1	"  "			/* left margin for the first line */
#define ZBX_SPACE2	"               "	/* left margin for subsequent lines */
	const char	**p = usage_message;

	if (NULL != *p)
		printf("usage:\n");

	while (NULL != *p)
	{
		size_t	pos;

		printf("%s%s", ZBX_SPACE1, progname);
		pos = ZBX_CONST_STRLEN(ZBX_SPACE1) + strlen(progname);

		while (NULL != *p)
		{
			size_t	len;

			len = strlen(*p);

			if (ZBX_MAXCOL > pos + len)
			{
				pos += len + 1;
				printf(" %s", *p);
			}
			else
			{
				pos = ZBX_CONST_STRLEN(ZBX_SPACE2) + len + 1;
				printf("\n%s %s", ZBX_SPACE2, *p);
			}

			p++;
		}

		printf("\n");
		p++;
	}
#undef ZBX_MAXCOL
#undef ZBX_SPACE1
#undef ZBX_SPACE2
}

/******************************************************************************
 *                                                                            *
 * Purpose: print help of application parameters on stdout by application     *
 *          request with parameter '-h'                                       *
 *                                                                            *
 * Comments:  help_message - is global variable which must be initialized     *
 *                            in each zabbix application                      *
 *                                                                            *
 ******************************************************************************/
void	help(void)
{
	const char	**p = help_message;

	usage();
	printf("\n");

	while (NULL != *p)
		printf("%s\n", *p++);

	printf("\n");
	puts(help_message_footer);
}

/******************************************************************************
 *                                                                            *
 * Purpose: Print error text to the stderr                                    *
 *                                                                            *
 * Parameters: fmt - format of message                                        *
 *                                                                            *
 ******************************************************************************/
void	zbx_error(const char *fmt, ...)
{
	va_list	args;

	va_start(args, fmt);

	fprintf(stderr, "%s [%li]: ", progname, zbx_get_thread_id());
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	fflush(stderr);

	va_end(args);
}

/******************************************************************************
 *                                                                            *
 * Purpose: Secure version of snprintf function.                              *
 *          Add zero character at the end of string.                          *
 *                                                                            *
 * Parameters: str - destination buffer pointer                               *
 *             count - size of destination buffer                             *
 *             fmt - format                                                   *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_snprintf(char *str, size_t count, const char *fmt, ...)
{
	size_t	written_len;
	va_list	args;

	va_start(args, fmt);
	written_len = zbx_vsnprintf(str, count, fmt, args);
	va_end(args);

	return written_len;
}

#if defined(__hpux)
#include "log.h"
	/* On HP-UX 11.23 vsnprintf(NULL, 0, fmt, args) cannot be used to     */
	/* determine the required buffer size - the result is program crash   */
	/* (ZBX-23404). Also, it returns -1 if buffer is too small.           */
	/* On HP-UX 11.31 vsnprintf() works as expected.                      */
	/* Agent can be compiled on HP-UX 11.23 but can be running on         */
	/* HP-UX 11.31 - it needs to adapt to vsnprintf() at runtime.         */

static int	vsnprintf_small_buf_test(const char *fmt, ...)
{
	char	buf[4];	/* large enough to store "ABC"+'\0' without corrupting stack */
	int	res;
	va_list	args;

	va_start(args, fmt);
	/* vsnprintf() with too small buffer, only "A"+'\0' can fit */
	res = vsnprintf(buf, sizeof(buf) - 2, fmt, args);
	va_end(args);

	return res;
}

#define VSNPRINTF_UNKNOWN	-1
#define VSNPRINTF_NOT_C99	0
#define VSNPRINTF_IS_C99	1

static int	test_vsnprintf(void)
{
	int	res = vsnprintf_small_buf_test("%s", "ABC");

	zabbix_log(LOG_LEVEL_DEBUG, "vsnprintf() returned %d", res);

	if (0 < res)
		return VSNPRINTF_IS_C99;
	else if (-1 == res)
		return VSNPRINTF_NOT_C99;

	zabbix_log(LOG_LEVEL_CRIT, "vsnprintf() returned %d", res);

	THIS_SHOULD_NEVER_HAPPEN;
	exit(EXIT_FAILURE);
}

int	zbx_hpux_vsnprintf_is_c99(void)
{
	static int	is_c99_vsnprintf = VSNPRINTF_UNKNOWN;

	if (VSNPRINTF_UNKNOWN == is_c99_vsnprintf)
		is_c99_vsnprintf = test_vsnprintf();

	return (VSNPRINTF_IS_C99 == is_c99_vsnprintf) ? SUCCEED : FAIL;
}
#undef VSNPRINTF_UNKNOWN
#undef VSNPRINTF_NOT_C99
#undef VSNPRINTF_IS_C99
#endif

/******************************************************************************
 *                                                                            *
 * Purpose: Secure version of snprintf function.                              *
 *          Add zero character at the end of string.                          *
 *          Reallocs memory if not enough.                                    *
 *                                                                            *
 * Parameters: str       - [IN/OUT] destination buffer pointer                *
 *             alloc_len - [IN/OUT] already allocated memory                  *
 *             offset    - [IN/OUT] offset for writing                        *
 *             fmt       - [IN] format                                        *
 *                                                                            *
 ******************************************************************************/
void	zbx_snprintf_alloc(char **str, size_t *alloc_len, size_t *offset, const char *fmt, ...)
{
	va_list	args;
	size_t	avail_len, written_len;

#if defined(__hpux)
	if (SUCCEED != zbx_hpux_vsnprintf_is_c99())
	{
#define INITIAL_ALLOC_LEN	128
		int	bytes_written = 0;

		if (NULL == *str)
		{
			*alloc_len = INITIAL_ALLOC_LEN;
			*str = (char *)zbx_malloc(NULL, *alloc_len);
			*offset = 0;
		}

		while (1)
		{
			avail_len = *alloc_len - *offset;
			va_start(args, fmt);
			bytes_written = vsnprintf(*str + *offset, avail_len, fmt, args);
			va_end(args);

			if (0 <= bytes_written)
				break;

			if (-1 == bytes_written)
			{
				*alloc_len *= 2;
				*str = (char *)zbx_realloc(*str, *alloc_len);
				continue;
			}

			zabbix_log(LOG_LEVEL_CRIT, "vsnprintf() returned %d", bytes_written);

			THIS_SHOULD_NEVER_HAPPEN;
			exit(EXIT_FAILURE);
		}

		*offset += bytes_written;

		return;
#undef INITIAL_ALLOC_LEN
	}
	/* HP-UX vsnprintf() looks C99-compliant, proceed with common implementation */
#endif
retry:
	if (NULL == *str)
	{
		/* zbx_vsnprintf() returns bytes actually written instead of bytes to write, */
		/* so we have to use the standard function                                   */
		va_start(args, fmt);
		*alloc_len = vsnprintf(NULL, 0, fmt, args) + 2;	/* '\0' + one byte to prevent the operation retry */
		va_end(args);
		*offset = 0;
		*str = (char *)zbx_malloc(*str, *alloc_len);
	}

	avail_len = *alloc_len - *offset;
	va_start(args, fmt);
	written_len = zbx_vsnprintf(*str + *offset, avail_len, fmt, args);
	va_end(args);

	if (written_len == avail_len - 1)
	{
		*alloc_len *= 2;
		*str = (char *)zbx_realloc(*str, *alloc_len);

		goto retry;
	}

	*offset += written_len;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Secure version of vsnprintf function.                             *
 *          Add zero character at the end of string.                          *
 *                                                                            *
 * Parameters: str   - [IN/OUT] destination buffer pointer                    *
 *             count - [IN] size of destination buffer                        *
 *             fmt   - [IN] format                                            *
 *                                                                            *
 * Return value: the number of characters in the output buffer                *
 *               (not including the trailing '\0')                            *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_vsnprintf(char *str, size_t count, const char *fmt, va_list args)
{
	int	written_len = 0;

	if (0 < count)
	{
		if (0 > (written_len = vsnprintf(str, count, fmt, args)))
			written_len = (int)count - 1;		/* count an output error as a full buffer */
		else
			written_len = MIN(written_len, (int)count - 1);		/* result could be truncated */
	}
	str[written_len] = '\0';	/* always write '\0', even if buffer size is 0 or vsnprintf() error */

	return (size_t)written_len;
}

/******************************************************************************
 *                                                                            *
 * Purpose: If there is no '\0' byte among the first n bytes of src,          *
 *          then all n bytes will be placed into the dest buffer.             *
 *          In other case only strlen() bytes will be placed there.           *
 *          Add zero character at the end of string.                          *
 *          Reallocs memory if not enough.                                    *
 *                                                                            *
 * Parameters: str       - [IN/OUT] destination buffer pointer                *
 *             alloc_len - [IN/OUT] already allocated memory                  *
 *             offset    - [IN/OUT] offset for writing                        *
 *             src       - [IN] copied string                                 *
 *             n         - [IN] maximum number of bytes to copy               *
 *                                                                            *
 ******************************************************************************/
void	zbx_strncpy_alloc(char **str, size_t *alloc_len, size_t *offset, const char *src, size_t n)
{
	if (NULL == *str)
	{
		*alloc_len = n + 1;
		*offset = 0;
		*str = (char *)zbx_malloc(*str, *alloc_len);
	}
	else if (*offset + n >= *alloc_len)
	{
		if (0 == *alloc_len)
		{
			THIS_SHOULD_NEVER_HAPPEN;
			exit(EXIT_FAILURE);
		}

		while (*offset + n >= *alloc_len)
			*alloc_len *= 2;
		*str = (char *)zbx_realloc(*str, *alloc_len);
	}

	while (0 != n && '\0' != *src)
	{
		(*str)[(*offset)++] = *src++;
		n--;
	}

	(*str)[*offset] = '\0';
}

void	zbx_str_memcpy_alloc(char **str, size_t *alloc_len, size_t *offset, const char *src, size_t n)
{
	if (NULL == *str)
	{
		*alloc_len = n + 1;
		*offset = 0;
		*str = (char *)zbx_malloc(*str, *alloc_len);
	}
	else if (*offset + n >= *alloc_len)
	{
		while (*offset + n >= *alloc_len)
			*alloc_len *= 2;
		*str = (char *)zbx_realloc(*str, *alloc_len);
	}

	memcpy(*str + *offset, src, n);
	*offset += n;
	(*str)[*offset] = '\0';
}

void	zbx_strcpy_alloc(char **str, size_t *alloc_len, size_t *offset, const char *src)
{
	zbx_strncpy_alloc(str, alloc_len, offset, src, strlen(src));
}

void	zbx_chrcpy_alloc(char **str, size_t *alloc_len, size_t *offset, char c)
{
	zbx_strncpy_alloc(str, alloc_len, offset, &c, 1);
}

void	zbx_strquote_alloc_opt(char **str, size_t *str_alloc, size_t *str_offset, const char *value_str, int option)
{
	size_t		size;
	const char	*src;
	char		*dst;

	for (size = 2, src = value_str; '\0' != *src; src++)
	{
		switch (*src)
		{
			case '\\':
				if (ZBX_STRQUOTE_SKIP_BACKSLASH == option)
					break;
				ZBX_FALLTHROUGH;
			case '"':
				size++;
		}
		size++;
	}

	if (*str_alloc <= *str_offset + size)
	{
		if (0 == *str_alloc)
			*str_alloc = size;

		do
		{
			*str_alloc *= 2;
		}
		while (*str_alloc - *str_offset <= size);

		*str = zbx_realloc(*str, *str_alloc);
	}

	dst = *str + *str_offset;
	*dst++ = '"';

	for (src = value_str; '\0' != *src; src++, dst++)
	{
		switch (*src)
		{
			case '\\':
				if (ZBX_STRQUOTE_SKIP_BACKSLASH == option)
					break;
				ZBX_FALLTHROUGH;
			case '"':
				*dst++ = '\\';
				break;
		}

		*dst = *src;
	}

	*dst++ = '"';
	*dst = '\0';
	*str_offset += size;
}

/* Has to be rewritten to avoid malloc */
char	*string_replace(const char *str, const char *sub_str1, const char *sub_str2)
{
	char *new_str = NULL;
	const char *p;
	const char *q;
	const char *r;
	char *t;
	long len, diff, count = 0;

	assert(str);
	assert(sub_str1);
	assert(sub_str2);

	len = (long)strlen(sub_str1);

	/* count the number of occurrences of sub_str1 */
	for ( p=str; (p = strstr(p, sub_str1)); p+=len, count++ );

	if (0 == count)
		return zbx_strdup(NULL, str);

	diff = (long)strlen(sub_str2) - len;

        /* allocate new memory */
	new_str = (char *)zbx_malloc(new_str, (size_t)(strlen(str) + count*diff + 1)*sizeof(char));

        for (q=str,t=new_str,p=str; (p = strstr(p, sub_str1)); )
        {
                /* copy until next occurrence of sub_str1 */
                for ( ; q < p; *t++ = *q++);
                q += len;
                p = q;
                for ( r = sub_str2; (*t++ = *r++); );
                --t;
        }
        /* copy the tail of str */
        for( ; *q ; *t++ = *q++ );

	*t = '\0';

	return new_str;
}

/******************************************************************************
 *                                                                            *
 * Purpose: delete all right '0' and '.' for the string                       *
 *                                                                            *
 * Parameters: s - string to trim '0'                                         *
 *                                                                            *
 * Return value: string without right '0'                                     *
 *                                                                            *
 * Comments: 10.0100 => 10.01, 10. => 10                                      *
 *                                                                            *
 ******************************************************************************/
void	del_zeros(char *s)
{
	int	trim = 0;
	size_t	len = 0;

	while ('\0' != s[len])
	{
		if ('e' == s[len] || 'E' == s[len])
		{
			/* don't touch numbers that are written in scientific notation */
			return;
		}

		if ('.' == s[len])
		{
			/* number has decimal part */

			if (1 == trim)
			{
				/* don't touch invalid numbers with more than one decimal separator */
				return;
			}

			trim = 1;
		}

		len++;
	}

	if (1 == trim)
	{
		size_t	i;

		for (i = len - 1; ; i--)
		{
			if ('0' == s[i])
			{
				s[i] = '\0';
			}
			else if ('.' == s[i])
			{
				s[i] = '\0';
				break;
			}
			else
			{
				break;
			}
		}
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: Strip characters from the end of a string                         *
 *                                                                            *
 * Parameters: str - string for processing                                    *
 *             charlist - null terminated list of characters                  *
 *                                                                            *
 * Return value: number of trimmed characters                                 *
 *                                                                            *
 ******************************************************************************/
int	zbx_rtrim(char *str, const char *charlist)
{
	char	*p;
	int	count = 0;

	if (NULL == str || '\0' == *str)
		return count;

	for (p = str + strlen(str) - 1; p >= str && NULL != strchr(charlist, *p); p--)
	{
		*p = '\0';
		count++;
	}

	return count;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Strip characters from the beginning of a string                   *
 *                                                                            *
 * Parameters: str - string for processing                                    *
 *             charlist - null terminated list of characters                  *
 *                                                                            *
 ******************************************************************************/
void	zbx_ltrim(char *str, const char *charlist)
{
	char	*p;

	if (NULL == str || '\0' == *str)
		return;

	for (p = str; '\0' != *p && NULL != strchr(charlist, *p); p++)
		;

	if (p == str)
		return;

	while ('\0' != *p)
		*str++ = *p++;

	*str = '\0';
}

/******************************************************************************
 *                                                                            *
 * Purpose: Removes leading and trailing characters from the specified        *
 *          character string                                                  *
 *                                                                            *
 * Parameters: str      - [IN/OUT] string for processing                      *
 *             charlist - [IN] null terminated list of characters             *
 *                                                                            *
 ******************************************************************************/
void	zbx_lrtrim(char *str, const char *charlist)
{
	zbx_rtrim(str, charlist);
	zbx_ltrim(str, charlist);
}

/******************************************************************************
 *                                                                            *
 * Purpose: Remove characters 'charlist' from the whole string                *
 *                                                                            *
 * Parameters: str - string for processing                                    *
 *             charlist - null terminated list of characters                  *
 *                                                                            *
 ******************************************************************************/
void	zbx_remove_chars(char *str, const char *charlist)
{
	char	*p;

	if (NULL == str || NULL == charlist || '\0' == *str || '\0' == *charlist)
		return;

	for (p = str; '\0' != *p; p++)
	{
		if (NULL == strchr(charlist, *p))
			*str++ = *p;
	}

	*str = '\0';
}

/******************************************************************************
 *                                                                            *
 * Purpose: converts text to printable string by converting special           *
 *          characters to escape sequences                                    *
 *                                                                            *
 * Parameters: text - [IN] the text to convert                                *
 *                                                                            *
 * Return value: The text converted in printable format                       *
 *                                                                            *
 ******************************************************************************/
char	*zbx_str_printable_dyn(const char *text)
{
	size_t		out_alloc = 0;
	const char	*pin;
	char		*out, *pout;

	for (pin = text; '\0' != *pin; pin++)
	{
		switch (*pin)
		{
			case '\n':
			case '\t':
			case '\r':
				out_alloc += 2;
				break;
			default:
				out_alloc++;
				break;
		}
	}

	out = zbx_malloc(NULL, ++out_alloc);

	for (pin = text, pout = out; '\0' != *pin; pin++)
	{
		switch (*pin)
		{
			case '\n':
				*pout++ = '\\';
				*pout++ = 'n';
				break;
			case '\t':
				*pout++ = '\\';
				*pout++ = 't';
				break;
			case '\r':
				*pout++ = '\\';
				*pout++ = 'r';
				break;
			default:
				*pout++ = *pin;
				break;
		}
	}
	*pout = '\0';

	return out;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Copy src to string dst of size siz. At most siz - 1 characters    *
 *          will be copied. Always null terminates (unless siz == 0).         *
 *                                                                            *
 * Return value: the number of characters copied (excluding the null byte)    *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_strlcpy(char *dst, const char *src, size_t siz)
{
	const char	*s = src;

	if (0 != siz)
	{
		while (0 != --siz && '\0' != *s)
			*dst++ = *s++;

		*dst = '\0';
	}

	return s - src;	/* count does not include null */
}

/******************************************************************************
 *                                                                            *
 * Purpose: Appends src to string dst of size siz (unlike strncat, size is    *
 *          the full size of dst, not space left). At most siz - 1 characters *
 *          will be copied. Always null terminates (unless                    *
 *          siz <= strlen(dst)).                                              *
 *                                                                            *
 ******************************************************************************/
void	zbx_strlcat(char *dst, const char *src, size_t siz)
{
	while ('\0' != *dst)
	{
		dst++;
		siz--;
	}

	zbx_strlcpy(dst, src, siz);
}

/******************************************************************************
 *                                                                            *
 * Purpose: copies utf-8 string + terminating zero character into specified   *
 *          buffer                                                            *
 *                                                                            *
 * Return value: the number of copied bytes excluding terminating zero        *
 *               character.                                                   *
 *                                                                            *
 * Comments: If the source string is larger than destination buffer then the  *
 *           string is truncated after last valid utf-8 character rather than *
 *           byte.                                                            *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_strlcpy_utf8(char *dst, const char *src, size_t size)
{
	size = zbx_strlen_utf8_nbytes(src, size - 1);
	memcpy(dst, src, size);
	dst[size] = '\0';

	return size;
}

/******************************************************************************
 *                                                                            *
 * Purpose: dynamical formatted output conversion                             *
 *                                                                            *
 * Return value: formatted string                                             *
 *                                                                            *
 * Comments: returns a pointer to allocated memory                            *
 *                                                                            *
 ******************************************************************************/
char	*zbx_dvsprintf(char *dest, const char *f, va_list args)
{
	char	*string = NULL;
	int	n, size = MAX_STRING_LEN >> 1;

	va_list curr;

	while (1)
	{
		string = (char *)zbx_malloc(string, size);

		va_copy(curr, args);
		n = vsnprintf(string, size, f, curr);
		va_end(curr);

		if (0 <= n && n < size)
			break;

		/* result was truncated */
		if (-1 == n)
			size = size * 3 / 2 + 1;	/* the length is unknown */
		else
			size = n + 1;	/* n bytes + trailing '\0' */

		zbx_free(string);
	}

	zbx_free(dest);

	return string;
}

/******************************************************************************
 *                                                                            *
 * Purpose: dynamical formatted output conversion                             *
 *                                                                            *
 * Return value: formatted string                                             *
 *                                                                            *
 * Comments: returns a pointer to allocated memory                            *
 *                                                                            *
 ******************************************************************************/
char	*zbx_dsprintf(char *dest, const char *f, ...)
{
	char	*string;
	va_list args;

	va_start(args, f);

	string = zbx_dvsprintf(dest, f, args);

	va_end(args);

	return string;
}

/******************************************************************************
 *                                                                            *
 * Purpose: dynamical cating of strings                                       *
 *                                                                            *
 * Return value: new pointer of string                                        *
 *                                                                            *
 * Comments: returns a pointer to allocated memory                            *
 *           zbx_strdcat(NULL, "") will return "", not NULL!                  *
 *                                                                            *
 ******************************************************************************/
char	*zbx_strdcat(char *dest, const char *src)
{
	size_t	len_dest, len_src;

	if (NULL == src)
		return dest;

	if (NULL == dest)
		return zbx_strdup(NULL, src);

	len_dest = strlen(dest);
	len_src = strlen(src);

	dest = (char *)zbx_realloc(dest, len_dest + len_src + 1);

	zbx_strlcpy(dest + len_dest, src, len_src + 1);

	return dest;
}

/******************************************************************************
 *                                                                            *
 * Purpose: dynamical cating of formatted strings                             *
 *                                                                            *
 * Return value: new pointer of string                                        *
 *                                                                            *
 * Comments: returns a pointer to allocated memory                            *
 *                                                                            *
 ******************************************************************************/
char	*zbx_strdcatf(char *dest, const char *f, ...)
{
	char	*string, *result;
	va_list	args;

	va_start(args, f);
	string = zbx_dvsprintf(NULL, f, args);
	va_end(args);

	result = zbx_strdcat(dest, string);

	zbx_free(string);

	return result;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check a byte stream for a valid hostname                          *
 *                                                                            *
 * Parameters: hostname - pointer to the first char of hostname               *
 *             error - pointer to the error message (can be NULL)             *
 *                                                                            *
 * Return value: return SUCCEED if hostname is valid                          *
 *               or FAIL if hostname contains invalid chars, is empty         *
 *               or is longer than MAX_ZBX_HOSTNAME_LEN                       *
 *                                                                            *
 ******************************************************************************/
int	zbx_check_hostname(const char *hostname, char **error)
{
	int	len = 0;

	while ('\0' != hostname[len])
	{
		if (FAIL == is_hostname_char(hostname[len]))
		{
			if (NULL != error)
				*error = zbx_dsprintf(NULL, "name contains invalid character '%c'", hostname[len]);
			return FAIL;
		}

		len++;
	}

	if (0 == len)
	{
		if (NULL != error)
			*error = zbx_strdup(NULL, "name is empty");
		return FAIL;
	}

	if (MAX_ZBX_HOSTNAME_LEN < len)
	{
		if (NULL != error)
			*error = zbx_dsprintf(NULL, "name is too long (max %d characters)", MAX_ZBX_HOSTNAME_LEN);
		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: advances pointer to first invalid character in string             *
 *          ensuring that everything before it is a valid key                 *
 *                                                                            *
 *  e.g., system.run[cat /etc/passwd | awk -F: '{ print $1 }']                *
 *                                                                            *
 * Parameters: exp - [IN/OUT] pointer to the first char of key                *
 *                                                                            *
 *  e.g., {host:system.run[cat /etc/passwd | awk -F: '{ print $1 }'].last(0)} *
 *              ^                                                             *
 * Return value: returns FAIL only if no key is present (length 0),           *
 *               or the whole string is invalid. SUCCEED otherwise.           *
 *                                                                            *
 * Comments: the pointer is advanced to the first invalid character even if   *
 *           FAIL is returned (meaning there is a syntax error in item key).  *
 *           If necessary, the caller must keep a copy of pointer original    *
 *           value.                                                           *
 *                                                                            *
 ******************************************************************************/
int	parse_key(const char **exp)
{
	const char	*s;

	for (s = *exp; SUCCEED == is_key_char(*s); s++)
		;

	if (*exp == s)	/* the key is empty */
		return FAIL;

	if ('[' == *s)	/* for instance, net.tcp.port[,80] */
	{
		int	state = 0;	/* 0 - init, 1 - inside quoted param, 2 - inside unquoted param */
		int	array = 0;	/* array nest level */

		for (s++; '\0' != *s; s++)
		{
			switch (state)
			{
				/* init state */
				case 0:
					if (',' == *s)
						;
					else if ('"' == *s)
						state = 1;
					else if ('[' == *s)
					{
						if (0 == array)
							array = 1;
						else
							goto fail;	/* incorrect syntax: multi-level array */
					}
					else if (']' == *s && 0 != array)
					{
						array = 0;
						s++;

						while (' ' == *s)	/* skip trailing spaces after closing ']' */
							s++;

						if (']' == *s)
							goto succeed;

						if (',' != *s)
							goto fail;	/* incorrect syntax */
					}
					else if (']' == *s && 0 == array)
						goto succeed;
					else if (' ' != *s)
						state = 2;
					break;
				/* quoted */
				case 1:
					if ('"' == *s)
					{
						while (' ' == s[1])	/* skip trailing spaces after closing quotes */
							s++;

						if (0 == array && ']' == s[1])
						{
							s++;
							goto succeed;
						}

						if (',' != s[1] && !(0 != array && ']' == s[1]))
						{
							s++;
							goto fail;	/* incorrect syntax */
						}

						state = 0;
					}
					else if ('\\' == *s && '"' == s[1])
						s++;
					break;
				/* unquoted */
				case 2:
					if (',' == *s || (']' == *s && 0 != array))
					{
						s--;
						state = 0;
					}
					else if (']' == *s && 0 == array)
						goto succeed;
					break;
			}
		}
fail:
		*exp = s;
		return FAIL;
succeed:
		s++;
	}

	*exp = s;
	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return hostname and key                                           *
 *          <hostname:>key                                                    *
 *                                                                            *
 * Parameters:                                                                *
 *         exp - pointer to the first char of hostname                        *
 *                host:key[key params]                                        *
 *                ^                                                           *
 *                                                                            *
 * Return value: return SUCCEED or FAIL                                       *
 *                                                                            *
 ******************************************************************************/
int	parse_host_key(char *exp, char **host, char **key)
{
	char	*p, *s;

	if (NULL == exp || '\0' == *exp)
		return FAIL;

	for (p = exp, s = exp; '\0' != *p; p++)	/* check for optional hostname */
	{
		if (':' == *p)	/* hostname:vfs.fs.size[/,total]
				 * --------^
				 */
		{
			*p = '\0';
			*host = zbx_strdup(NULL, s);
			*p++ = ':';

			s = p;
			break;
		}

		if (SUCCEED != is_hostname_char(*p))
			break;
	}

	*key = zbx_strdup(NULL, s);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculate the required size for the escaped string                *
 *                                                                            *
 * Parameters: src - [IN] null terminated source string                       *
 *             charlist - [IN] null terminated to-be-escaped character list   *
 *                                                                            *
 * Return value: size of the escaped string                                   *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_get_escape_string_len(const char *src, const char *charlist)
{
	size_t	sz = 0;

	for (; '\0' != *src; src++, sz++)
	{
		if (NULL != strchr(charlist, *src))
			sz++;
	}

	return sz;
}

/******************************************************************************
 *                                                                            *
 * Purpose: escape characters in the source string                            *
 *                                                                            *
 * Parameters: src - [IN] null terminated source string                       *
 *             charlist - [IN] null terminated to-be-escaped character list   *
 *                                                                            *
 * Return value: the escaped string                                           *
 *                                                                            *
 ******************************************************************************/
char	*zbx_dyn_escape_string(const char *src, const char *charlist)
{
	size_t	sz;
	char	*d, *dst = NULL;

	sz = zbx_get_escape_string_len(src, charlist) + 1;

	dst = (char *)zbx_malloc(dst, sz);

	for (d = dst; '\0' != *src; src++)
	{
		if (NULL != strchr(charlist, *src))
			*d++ = '\\';

		*d++ = *src;
	}

	*d = '\0';

	return dst;
}

/******************************************************************************
 *                                                                            *
 * Purpose: escape characters in the source string to fixed output buffer     *
 *                                                                            *
 * Parameters: dst      - [OUT] the output buffer                             *
 *             len      - [IN] the output buffer size                         *
 *             src      - [IN] null terminated source string                  *
 *             charlist - [IN] null terminated to-be-escaped character list   *
 *                                                                            *
 * Return value: SUCCEED - the string was escaped successfully.               *
 *               FAIL    - output buffer is too small.                        *
 *                                                                            *
 ******************************************************************************/
int	zbx_escape_string(char *dst, size_t len, const char *src, const char *charlist)
{
	for (; '\0' != *src; src++)
	{
		if (NULL != strchr(charlist, *src))
		{
			if (0 == --len)
				return FAIL;
			*dst++ = '\\';
		}
		else
		{
			if (0 == --len)
				return FAIL;
		}

		*dst++ = *src;
	}

	*dst = '\0';

	return SUCCEED;
}

char	*zbx_age2str(int age)
{
	size_t		offset = 0;
	int		days, hours, minutes, seconds;
	static char	buffer[32];

	days = (int)((double)age / SEC_PER_DAY);
	hours = (int)((double)(age - days * SEC_PER_DAY) / SEC_PER_HOUR);
	minutes = (int)((double)(age - days * SEC_PER_DAY - hours * SEC_PER_HOUR) / SEC_PER_MIN);
	seconds = (int)((double)(age - days * SEC_PER_DAY - hours * SEC_PER_HOUR - minutes * SEC_PER_MIN));

	if (0 != days)
		offset += zbx_snprintf(buffer + offset, sizeof(buffer) - offset, "%dd ", days);
	if (0 != days || 0 != hours)
		offset += zbx_snprintf(buffer + offset, sizeof(buffer) - offset, "%dh ", hours);
	if (0 != days || 0 != hours || 0 != minutes)
		offset += zbx_snprintf(buffer + offset, sizeof(buffer) - offset, "%dm ", minutes);

	zbx_snprintf(buffer + offset, sizeof(buffer) - offset, "%ds", seconds);

	return buffer;
}

char	*zbx_date2str(time_t date, const char *tz)
{
	static char	buffer[11];
	struct tm	*tm;

	tm = zbx_localtime(&date, tz);
	zbx_snprintf(buffer, sizeof(buffer), "%.4d.%.2d.%.2d",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday);

	return buffer;
}

char	*zbx_time2str(time_t time, const char *tz)
{
	static char	buffer[9];
	struct tm	*tm;

	tm = zbx_localtime(&time, tz);
	zbx_snprintf(buffer, sizeof(buffer), "%.2d:%.2d:%.2d",
			tm->tm_hour,
			tm->tm_min,
			tm->tm_sec);
	return buffer;
}

int	zbx_strncasecmp(const char *s1, const char *s2, size_t n)
{
	if (NULL == s1 && NULL == s2)
		return 0;

	if (NULL == s1)
		return 1;

	if (NULL == s2)
		return -1;

	while (0 != n && '\0' != *s1 && '\0' != *s2 &&
			tolower((unsigned char)*s1) == tolower((unsigned char)*s2))
	{
		s1++;
		s2++;
		n--;
	}

	return 0 == n ? 0 : tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

char	*zbx_strcasestr(const char *haystack, const char *needle)
{
	size_t		sz_h, sz_n;
	const char	*p;

	if (NULL == needle || '\0' == *needle)
		return (char *)haystack;

	if (NULL == haystack || '\0' == *haystack)
		return NULL;

	sz_h = strlen(haystack);
	sz_n = strlen(needle);
	if (sz_h < sz_n)
		return NULL;

	for (p = haystack; p <= &haystack[sz_h - sz_n]; p++)
	{
		if (0 == zbx_strncasecmp(p, needle, sz_n))
			return (char *)p;
	}

	return NULL;
}

int	cmp_key_id(const char *key_1, const char *key_2)
{
	const char	*p, *q;

	for (p = key_1, q = key_2; *p == *q && '\0' != *q && '[' != *q; p++, q++)
		;

	return ('\0' == *p || '[' == *p) && ('\0' == *q || '[' == *q) ? SUCCEED : FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Returns process name                                              *
 *                                                                            *
 * Parameters: proc_type - [IN] process type; ZBX_PROCESS_TYPE_*              *
 *                                                                            *
 * Comments: used in internals checks zabbix["process",...], process titles   *
 *           and log files                                                    *
 *                                                                            *
 ******************************************************************************/
const char	*get_process_type_string(unsigned char proc_type)
{
	switch (proc_type)
	{
		case ZBX_PROCESS_TYPE_POLLER:
			return "poller";
		case ZBX_PROCESS_TYPE_UNREACHABLE:
			return "unreachable poller";
		case ZBX_PROCESS_TYPE_IPMIPOLLER:
			return "ipmi poller";
		case ZBX_PROCESS_TYPE_PINGER:
			return "icmp pinger";
		case ZBX_PROCESS_TYPE_JAVAPOLLER:
			return "java poller";
		case ZBX_PROCESS_TYPE_HTTPPOLLER:
			return "http poller";
		case ZBX_PROCESS_TYPE_TRAPPER:
			return "trapper";
		case ZBX_PROCESS_TYPE_SNMPTRAPPER:
			return "snmp trapper";
		case ZBX_PROCESS_TYPE_PROXYPOLLER:
			return "proxy poller";
		case ZBX_PROCESS_TYPE_ESCALATOR:
			return "escalator";
		case ZBX_PROCESS_TYPE_HISTSYNCER:
			return "history syncer";
		case ZBX_PROCESS_TYPE_DISCOVERER:
			return "discoverer";
		case ZBX_PROCESS_TYPE_ALERTER:
			return "alerter";
		case ZBX_PROCESS_TYPE_TIMER:
			return "timer";
		case ZBX_PROCESS_TYPE_HOUSEKEEPER:
			return "housekeeper";
		case ZBX_PROCESS_TYPE_DATASENDER:
			return "data sender";
		case ZBX_PROCESS_TYPE_CONFSYNCER:
			return "configuration syncer";
		case ZBX_PROCESS_TYPE_HEARTBEAT:
			return "heartbeat sender";
		case ZBX_PROCESS_TYPE_SELFMON:
			return "self-monitoring";
		case ZBX_PROCESS_TYPE_VMWARE:
			return "vmware collector";
		case ZBX_PROCESS_TYPE_COLLECTOR:
			return "collector";
		case ZBX_PROCESS_TYPE_LISTENER:
			return "listener";
		case ZBX_PROCESS_TYPE_ACTIVE_CHECKS:
			return "active checks";
		case ZBX_PROCESS_TYPE_TASKMANAGER:
			return "task manager";
		case ZBX_PROCESS_TYPE_IPMIMANAGER:
			return "ipmi manager";
		case ZBX_PROCESS_TYPE_ALERTMANAGER:
			return "alert manager";
		case ZBX_PROCESS_TYPE_PREPROCMAN:
			return "preprocessing manager";
		case ZBX_PROCESS_TYPE_PREPROCESSOR:
			return "preprocessing worker";
		case ZBX_PROCESS_TYPE_LLDMANAGER:
			return "lld manager";
		case ZBX_PROCESS_TYPE_LLDWORKER:
			return "lld worker";
		case ZBX_PROCESS_TYPE_ALERTSYNCER:
			return "alert syncer";
		case ZBX_PROCESS_TYPE_HISTORYPOLLER:
			return "history poller";
		case ZBX_PROCESS_TYPE_AVAILMAN:
			return "availability manager";
		case ZBX_PROCESS_TYPE_REPORTMANAGER:
			return "report manager";
		case ZBX_PROCESS_TYPE_REPORTWRITER:
			return "report writer";
		case ZBX_PROCESS_TYPE_SERVICEMAN:
			return "service manager";
		case ZBX_PROCESS_TYPE_TRIGGERHOUSEKEEPER:
			return "trigger housekeeper";
		case ZBX_PROCESS_TYPE_HA_MANAGER:
			return "ha manager";
		case ZBX_PROCESS_TYPE_ODBCPOLLER:
			return "odbc poller";
		case ZBX_PROCESS_TYPE_MAIN:
			return "main";
	}

	THIS_SHOULD_NEVER_HAPPEN;
	exit(EXIT_FAILURE);
}

int	get_process_type_by_name(const char *proc_type_str)
{
	int	i;

	for (i = 0; i < ZBX_PROCESS_TYPE_COUNT; i++)
	{
		if (0 == strcmp(proc_type_str, get_process_type_string((unsigned char)i)))
			return i;
	}

	if (0 == strcmp(proc_type_str, get_process_type_string(ZBX_PROCESS_TYPE_MAIN)))
		return ZBX_PROCESS_TYPE_MAIN;

	return ZBX_PROCESS_TYPE_UNKNOWN;
}

const char	*get_program_type_string(unsigned char program_type)
{
	switch (program_type)
	{
		case ZBX_PROGRAM_TYPE_SERVER:
			return "server";
		case ZBX_PROGRAM_TYPE_PROXY_ACTIVE:
		case ZBX_PROGRAM_TYPE_PROXY_PASSIVE:
			return "proxy";
		case ZBX_PROGRAM_TYPE_AGENTD:
			return "agent";
		case ZBX_PROGRAM_TYPE_SENDER:
			return "sender";
		case ZBX_PROGRAM_TYPE_GET:
			return "get";
		default:
			return "unknown";
	}
}

const char	*zbx_permission_string(int perm)
{
	switch (perm)
	{
		case PERM_DENY:
			return "dn";
		case PERM_READ:
			return "r";
		case PERM_READ_WRITE:
			return "rw";
		default:
			return "unknown";
	}
}

const char	*zbx_agent_type_string(zbx_item_type_t item_type)
{
	switch (item_type)
	{
		case ITEM_TYPE_ZABBIX:
			return "Zabbix agent";
		case ITEM_TYPE_SNMP:
			return "SNMP agent";
		case ITEM_TYPE_IPMI:
			return "IPMI agent";
		case ITEM_TYPE_JMX:
			return "JMX agent";
		default:
			return "generic";
	}
}

const char	*zbx_item_value_type_string(zbx_item_value_type_t value_type)
{
	switch (value_type)
	{
		case ITEM_VALUE_TYPE_FLOAT:
			return "Numeric (float)";
		case ITEM_VALUE_TYPE_STR:
			return "Character";
		case ITEM_VALUE_TYPE_LOG:
			return "Log";
		case ITEM_VALUE_TYPE_UINT64:
			return "Numeric (unsigned)";
		case ITEM_VALUE_TYPE_TEXT:
			return "Text";
		default:
			return "unknown";
	}
}

const char	*zbx_interface_type_string(zbx_interface_type_t type)
{
	switch (type)
	{
		case INTERFACE_TYPE_AGENT:
			return "Zabbix agent";
		case INTERFACE_TYPE_SNMP:
			return "SNMP";
		case INTERFACE_TYPE_IPMI:
			return "IPMI";
		case INTERFACE_TYPE_JMX:
			return "JMX";
		case INTERFACE_TYPE_OPT:
			return "optional";
		case INTERFACE_TYPE_ANY:
			return "any";
		case INTERFACE_TYPE_UNKNOWN:
		default:
			return "unknown";
	}
}

const char	*zbx_sysinfo_ret_string(int ret)
{
	switch (ret)
	{
		case SYSINFO_RET_OK:
			return "SYSINFO_SUCCEED";
		case SYSINFO_RET_FAIL:
			return "SYSINFO_FAIL";
		default:
			return "SYSINFO_UNKNOWN";
	}
}

const char	*zbx_result_string(int result)
{
	switch (result)
	{
		case SUCCEED:
			return "SUCCEED";
		case FAIL:
			return "FAIL";
		case CONFIG_ERROR:
			return "CONFIG_ERROR";
		case NOTSUPPORTED:
			return "NOTSUPPORTED";
		case NETWORK_ERROR:
			return "NETWORK_ERROR";
		case TIMEOUT_ERROR:
			return "TIMEOUT_ERROR";
		case AGENT_ERROR:
			return "AGENT_ERROR";
		case GATEWAY_ERROR:
			return "GATEWAY_ERROR";
		case SIG_ERROR:
			return "SIG_ERROR";
		case SYSINFO_RET_FAIL:
			return "SYSINFO_RET_FAIL";
		default:
			return "unknown";
	}
}

const char	*zbx_item_logtype_string(unsigned char logtype)
{
	switch (logtype)
	{
		case ITEM_LOGTYPE_INFORMATION:
			return "Information";
		case ITEM_LOGTYPE_WARNING:
			return "Warning";
		case ITEM_LOGTYPE_ERROR:
			return "Error";
		case ITEM_LOGTYPE_FAILURE_AUDIT:
			return "Failure Audit";
		case ITEM_LOGTYPE_SUCCESS_AUDIT:
			return "Success Audit";
		case ITEM_LOGTYPE_CRITICAL:
			return "Critical";
		case ITEM_LOGTYPE_VERBOSE:
			return "Verbose";
		default:
			return "unknown";
	}
}

const char	*zbx_dservice_type_string(zbx_dservice_type_t service)
{
	switch (service)
	{
		case SVC_SSH:
			return "SSH";
		case SVC_LDAP:
			return "LDAP";
		case SVC_SMTP:
			return "SMTP";
		case SVC_FTP:
			return "FTP";
		case SVC_HTTP:
			return "HTTP";
		case SVC_POP:
			return "POP";
		case SVC_NNTP:
			return "NNTP";
		case SVC_IMAP:
			return "IMAP";
		case SVC_TCP:
			return "TCP";
		case SVC_AGENT:
			return "Zabbix agent";
		case SVC_SNMPv1:
			return "SNMPv1 agent";
		case SVC_SNMPv2c:
			return "SNMPv2c agent";
		case SVC_SNMPv3:
			return "SNMPv3 agent";
		case SVC_ICMPPING:
			return "ICMP ping";
		case SVC_HTTPS:
			return "HTTPS";
		case SVC_TELNET:
			return "Telnet";
		default:
			return "unknown";
	}
}

const char	*zbx_alert_type_string(unsigned char type)
{
	switch (type)
	{
		case ALERT_TYPE_MESSAGE:
			return "message";
		default:
			return "script";
	}
}

const char	*zbx_alert_status_string(unsigned char type, unsigned char status)
{
	switch (status)
	{
		case ALERT_STATUS_SENT:
			return (ALERT_TYPE_MESSAGE == type ? "sent" : "executed");
		case ALERT_STATUS_NOT_SENT:
			return "in progress";
		default:
			return "failed";
	}
}

const char	*zbx_escalation_status_string(unsigned char status)
{
	switch (status)
	{
		case ESCALATION_STATUS_ACTIVE:
			return "active";
		case ESCALATION_STATUS_SLEEP:
			return "sleep";
		case ESCALATION_STATUS_COMPLETED:
			return "completed";
		default:
			return "unknown";
	}
}

const char	*zbx_trigger_value_string(unsigned char value)
{
	switch (value)
	{
		case TRIGGER_VALUE_PROBLEM:
			return "PROBLEM";
		case TRIGGER_VALUE_OK:
			return "OK";
		default:
			return "unknown";
	}
}

const char	*zbx_trigger_state_string(unsigned char state)
{
	switch (state)
	{
		case TRIGGER_STATE_NORMAL:
			return "Normal";
		case TRIGGER_STATE_UNKNOWN:
			return "Unknown";
		default:
			return "unknown";
	}
}

const char	*zbx_item_state_string(unsigned char state)
{
	switch (state)
	{
		case ITEM_STATE_NORMAL:
			return "Normal";
		case ITEM_STATE_NOTSUPPORTED:
			return "Not supported";
		default:
			return "unknown";
	}
}

const char	*zbx_event_value_string(unsigned char source, unsigned char object, unsigned char value)
{
	if (EVENT_SOURCE_TRIGGERS == source || EVENT_SOURCE_SERVICE == source)
	{
		switch (value)
		{
			case EVENT_STATUS_PROBLEM:
				return "PROBLEM";
			case EVENT_STATUS_RESOLVED:
				return "RESOLVED";
			default:
				return "unknown";
		}
	}

	if (EVENT_SOURCE_INTERNAL == source)
	{
		switch (object)
		{
			case EVENT_OBJECT_TRIGGER:
				return zbx_trigger_state_string(value);
			case EVENT_OBJECT_ITEM:
			case EVENT_OBJECT_LLDRULE:
				return zbx_item_state_string(value);
		}
	}

	return "unknown";
}

#if defined(_WINDOWS) || defined(__MINGW32__)
/******************************************************************************
 *                                                                            *
 * Parameters: encoding - [IN] non-empty string, code page identifier         *
 *                        (as in libiconv or Windows SDK docs)                *
 *             codepage - [OUT] code page number                              *
 *                                                                            *
 * Return value: SUCCEED on success                                           *
 *               FAIL on failure                                              *
 *                                                                            *
 ******************************************************************************/
static int	get_codepage(const char *encoding, unsigned int *codepage)
{
	typedef struct
	{
		unsigned int	codepage;
		const char	*name;
	}
	codepage_t;

	int		i;
	char		buf[16];
	codepage_t	cp[] = {{0, "ANSI"}, {37, "IBM037"}, {437, "IBM437"}, {500, "IBM500"}, {708, "ASMO-708"},
			{709, NULL}, {710, NULL}, {720, "DOS-720"}, {737, "IBM737"}, {775, "IBM775"}, {850, "IBM850"},
			{852, "IBM852"}, {855, "IBM855"}, {857, "IBM857"}, {858, "IBM00858"}, {860, "IBM860"},
			{861, "IBM861"}, {862, "DOS-862"}, {863, "IBM863"}, {864, "IBM864"}, {865, "IBM865"},
			{866, "CP866"}, {869, "IBM869"}, {870, "IBM870"}, {874, "WINDOWS-874"}, {875, "CP875"},
			{932, "SHIFT_JIS"}, {936, "GB2312"}, {949, "KS_C_5601-1987"}, {950, "BIG5"}, {1026, "IBM1026"},
			{1047, "IBM01047"}, {1140, "IBM01140"}, {1141, "IBM01141"}, {1142, "IBM01142"},
			{1143, "IBM01143"}, {1144, "IBM01144"}, {1145, "IBM01145"}, {1146, "IBM01146"},
			{1147, "IBM01147"}, {1148, "IBM01148"}, {1149, "IBM01149"}, {1200, "UTF-16"},
			{1200, "UTF-16LE"}, {1201, "UNICODEFFFE"},{1201, "UTF-16BE"}, {1250, "WINDOWS-1250"},
			{1251, "WINDOWS-1251"}, {1252, "WINDOWS-1252"}, {1253, "WINDOWS-1253"}, {1254, "WINDOWS-1254"},
			{1255, "WINDOWS-1255"}, {1256, "WINDOWS-1256"}, {1257, "WINDOWS-1257"}, {1258, "WINDOWS-1258"},
			{1361, "JOHAB"}, {10000, "MACINTOSH"}, {10001, "X-MAC-JAPANESE"}, {10002, "X-MAC-CHINESETRAD"},
			{10003, "X-MAC-KOREAN"}, {10004, "X-MAC-ARABIC"}, {10005, "X-MAC-HEBREW"},
			{10006, "X-MAC-GREEK"}, {10007, "X-MAC-CYRILLIC"}, {10008, "X-MAC-CHINESESIMP"},
			{10010, "X-MAC-ROMANIAN"}, {10017, "X-MAC-UKRAINIAN"}, {10021, "X-MAC-THAI"},
			{10029, "X-MAC-CE"}, {10079, "X-MAC-ICELANDIC"}, {10081, "X-MAC-TURKISH"},
			{10082, "X-MAC-CROATIAN"}, {12000, "UTF-32"}, {12001, "UTF-32BE"}, {20000, "X-CHINESE_CNS"},
			{20001, "X-CP20001"}, {20002, "X_CHINESE-ETEN"}, {20003, "X-CP20003"}, {20004, "X-CP20004"},
			{20005, "X-CP20005"}, {20105, "X-IA5"}, {20106, "X-IA5-GERMAN"}, {20107, "X-IA5-SWEDISH"},
			{20108, "X-IA5-NORWEGIAN"}, {20127, "US-ASCII"}, {20261, "X-CP20261"}, {20269, "X-CP20269"},
			{20273, "IBM273"}, {20277, "IBM277"}, {20278, "IBM278"}, {20280, "IBM280"}, {20284, "IBM284"},
			{20285, "IBM285"}, {20290, "IBM290"}, {20297, "IBM297"}, {20420, "IBM420"}, {20423, "IBM423"},
			{20424, "IBM424"}, {20833, "X-EBCDIC-KOREANEXTENDED"}, {20838, "IBM-THAI"}, {20866, "KOI8-R"},
			{20871, "IBM871"}, {20880, "IBM880"}, {20905, "IBM905"}, {20924, "IBM00924"}, {20932, "EUC-JP"},
			{20936, "X-CP20936"}, {20949, "X-CP20949"}, {21025, "CP1025"}, {21027, NULL}, {21866, "KOI8-U"},
			{28591, "ISO-8859-1"}, {28592, "ISO-8859-2"}, {28593, "ISO-8859-3"}, {28594, "ISO-8859-4"},
			{28595, "ISO-8859-5"}, {28596, "ISO-8859-6"}, {28597, "ISO-8859-7"}, {28598, "ISO-8859-8"},
			{28599, "ISO-8859-9"}, {28603, "ISO-8859-13"}, {28605, "ISO-8859-15"}, {29001, "X-EUROPA"},
			{38598, "ISO-8859-8-I"}, {50220, "ISO-2022-JP"}, {50221, "CSISO2022JP"}, {50222, "ISO-2022-JP"},
			{50225, "ISO-2022-KR"}, {50227, "X-CP50227"}, {50229, NULL}, {50930, NULL}, {50931, NULL},
			{50933, NULL}, {50935, NULL}, {50936, NULL}, {50937, NULL}, {50939, NULL}, {51932, "EUC-JP"},
			{51936, "EUC-CN"}, {51949, "EUC-KR"}, {51950, NULL}, {52936, "HZ-GB-2312"}, {54936, "GB18030"},
			{57002, "X-ISCII-DE"}, {57003, "X-ISCII-BE"}, {57004, "X-ISCII-TA"}, {57005, "X-ISCII-TE"},
			{57006, "X-ISCII-AS"}, {57007, "X-ISCII-OR"}, {57008, "X-ISCII-KA"}, {57009, "X-ISCII-MA"},
			{57010, "X-ISCII-GU"}, {57011, "X-ISCII-PA"}, {65000, "UTF-7"}, {65001, "UTF-8"}, {0, NULL}};

	/* by name */
	for (i = 0; 0 != cp[i].codepage || NULL != cp[i].name; i++)
	{
		if (NULL == cp[i].name)
			continue;

		if (0 == strcmp(encoding, cp[i].name))
		{
			*codepage = cp[i].codepage;
			return SUCCEED;
		}
	}

	/* by number */
	for (i = 0; 0 != cp[i].codepage || NULL != cp[i].name; i++)
	{
		_itoa_s(cp[i].codepage, buf, sizeof(buf), 10);
		if (0 == strcmp(encoding, buf))
		{
			*codepage = cp[i].codepage;
			return SUCCEED;
		}
	}

	/* by 'cp' + number */
	for (i = 0; 0 != cp[i].codepage || NULL != cp[i].name; i++)
	{
		zbx_snprintf(buf, sizeof(buf), "cp%li", cp[i].codepage);
		if (0 == strcmp(encoding, buf))
		{
			*codepage = cp[i].codepage;
			return SUCCEED;
		}
	}

	return FAIL;
}

/* convert from selected code page to unicode */
static wchar_t	*zbx_to_unicode(unsigned int codepage, const char *cp_string)
{
	wchar_t	*wide_string = NULL;
	int	wide_size;

	wide_size = MultiByteToWideChar(codepage, 0, cp_string, -1, NULL, 0);
	wide_string = (wchar_t *)zbx_malloc(wide_string, (size_t)wide_size * sizeof(wchar_t));

	/* convert from cp_string to wide_string */
	MultiByteToWideChar(codepage, 0, cp_string, -1, wide_string, wide_size);

	return wide_string;
}

/* convert from Windows ANSI code page to unicode */
wchar_t	*zbx_acp_to_unicode(const char *acp_string)
{
	return zbx_to_unicode(CP_ACP, acp_string);
}

/* convert from Windows OEM code page to unicode */
wchar_t	*zbx_oemcp_to_unicode(const char *oemcp_string)
{
	return zbx_to_unicode(CP_OEMCP, oemcp_string);
}

int	zbx_acp_to_unicode_static(const char *acp_string, wchar_t *wide_string, int wide_size)
{
	/* convert from acp_string to wide_string */
	if (0 == MultiByteToWideChar(CP_ACP, 0, acp_string, -1, wide_string, wide_size))
		return FAIL;

	return SUCCEED;
}

/* convert from UTF-8 to unicode */
wchar_t	*zbx_utf8_to_unicode(const char *utf8_string)
{
	return zbx_to_unicode(CP_UTF8, utf8_string);
}

/* convert from unicode to utf8 */
char	*zbx_unicode_to_utf8(const wchar_t *wide_string)
{
	char	*utf8_string = NULL;
	int	utf8_size;

	utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, NULL, 0, NULL, NULL);
	utf8_string = (char *)zbx_malloc(utf8_string, (size_t)utf8_size);

	/* convert from wide_string to utf8_string */
	WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, utf8_string, utf8_size, NULL, NULL);

	return utf8_string;
}

/* convert from unicode to utf8 */
char	*zbx_unicode_to_utf8_static(const wchar_t *wide_string, char *utf8_string, int utf8_size)
{
	/* convert from wide_string to utf8_string */
	if (0 == WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, utf8_string, utf8_size, NULL, NULL))
		*utf8_string = '\0';

	return utf8_string;
}
#endif

void	zbx_strlower(char *str)
{
	for (; '\0' != *str; str++)
		*str = tolower(*str);
}

void	zbx_strupper(char *str)
{
	for (; '\0' != *str; str++)
		*str = toupper(*str);
}

const char	*get_bom_econding(char *in, size_t in_size)
{
	if (3 <= in_size && 0 == strncmp("\xef\xbb\xbf", in, 3))
	{
		return "UTF-8";
	}
	else if (2 <= in_size && 0 == strncmp("\xff\xfe", in, 2))
	{
		return "UTF-16LE";
	}
	else if (2 <= in_size && 0 == strncmp("\xfe\xff", in, 2))
	{
		return "UTF-16BE";
	}

	return "";
}

#if defined(_WINDOWS) || defined(__MINGW32__)
#include "log.h"
char	*convert_to_utf8(char *in, size_t in_size, const char *encoding)
{
#define STATIC_SIZE	1024
	wchar_t		wide_string_static[STATIC_SIZE], *wide_string = NULL;
	int		wide_size;
	char		*utf8_string = NULL;
	int		utf8_size;
	unsigned int	codepage;
	int		bom_detected = 0;

	/* try to guess encoding using BOM if it exists */
	if (3 <= in_size && 0 == strncmp("\xef\xbb\xbf", in, 3))
	{
		bom_detected = 1;

		if ('\0' == *encoding)
			encoding = "UTF-8";
	}
	else if (2 <= in_size && 0 == strncmp("\xff\xfe", in, 2))
	{
		bom_detected = 1;

		if ('\0' == *encoding)
			encoding = "UTF-16";
	}
	else if (2 <= in_size && 0 == strncmp("\xfe\xff", in, 2))
	{
		bom_detected = 1;

		if ('\0' == *encoding)
			encoding = "UNICODEFFFE";
	}

	if ('\0' == *encoding || FAIL == get_codepage(encoding, &codepage))
	{
		utf8_size = (int)in_size + 1;
		utf8_string = zbx_malloc(utf8_string, utf8_size);
		memcpy(utf8_string, in, in_size);
		utf8_string[in_size] = '\0';
		return utf8_string;
	}

	zabbix_log(LOG_LEVEL_DEBUG, "convert_to_utf8() in_size:%d encoding:'%s' codepage:%u", in_size, encoding,
			codepage);

	if (65001 == codepage)
	{
		/* remove BOM */
		if (bom_detected)
			in += 3;
	}

	if (1200 == codepage)		/* Unicode UTF-16, little-endian byte order */
	{
		wide_size = (int)in_size / 2;

		/* remove BOM */
		if (bom_detected)
		{
			in += 2;
			wide_size--;
		}

		wide_string = (wchar_t *)in;

	}
	else if (1201 == codepage)	/* unicodeFFFE UTF-16, big-endian byte order */
	{
		wchar_t *wide_string_be;
		int	i;

		wide_size = (int)in_size / 2;

		/* remove BOM */
		if (bom_detected)
		{
			in += 2;
			wide_size--;
		}

		wide_string_be = (wchar_t *)in;

		if (wide_size > STATIC_SIZE)
			wide_string = (wchar_t *)zbx_malloc(wide_string, (size_t)wide_size * sizeof(wchar_t));
		else
			wide_string = wide_string_static;

		/* convert from big-endian 'in' to little-endian 'wide_string' */
		for (i = 0; i < wide_size; i++)
			wide_string[i] = ((wide_string_be[i] << 8) & 0xff00) | ((wide_string_be[i] >> 8) & 0xff);
	}
	else
	{
		wide_size = MultiByteToWideChar(codepage, 0, in, (int)in_size, NULL, 0);

		if (wide_size > STATIC_SIZE)
			wide_string = (wchar_t *)zbx_malloc(wide_string, (size_t)wide_size * sizeof(wchar_t));
		else
			wide_string = wide_string_static;

		/* convert from 'in' to 'wide_string' */
		MultiByteToWideChar(codepage, 0, in, (int)in_size, wide_string, wide_size);
	}

	utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, NULL, 0, NULL, NULL);
	utf8_string = (char *)zbx_malloc(utf8_string, (size_t)utf8_size + 1/* '\0' */);

	/* convert from 'wide_string' to 'utf8_string' */
	WideCharToMultiByte(CP_UTF8, 0, wide_string, wide_size, utf8_string, utf8_size, NULL, NULL);
	utf8_string[utf8_size] = '\0';

	if (wide_string != wide_string_static && wide_string != (wchar_t *)in)
		zbx_free(wide_string);

	return utf8_string;
}
#elif defined(HAVE_ICONV)
char	*convert_to_utf8(char *in, size_t in_size, const char *encoding)
{
	iconv_t		cd;
	size_t		in_size_left, out_size_left, sz, out_alloc = 0;
	const char	to_code[] = "UTF-8";
	char		*out = NULL, *p;

	out_alloc = in_size + 1;
	p = out = (char *)zbx_malloc(out, out_alloc);

	/* try to guess encoding using BOM if it exists */
	if ('\0' == *encoding)
		encoding = get_bom_econding(in, in_size);

	if ('\0' == *encoding || (iconv_t)-1 == (cd = iconv_open(to_code, encoding)))
	{
		memcpy(out, in, in_size);
		out[in_size] = '\0';
		return out;
	}

	in_size_left = in_size;
	out_size_left = out_alloc - 1;

	while ((size_t)(-1) == iconv(cd, &in, &in_size_left, &p, &out_size_left))
	{
		if (E2BIG != errno)
			break;

		sz = (size_t)(p - out);
		out_alloc += in_size;
		out_size_left += in_size;
		p = out = (char *)zbx_realloc(out, out_alloc);
		p += sz;
	}

	*p = '\0';

	iconv_close(cd);

	/* remove BOM */
	if (3 <= p - out && 0 == strncmp("\xef\xbb\xbf", out, 3))
		memmove(out, out + 3, (size_t)(p - out - 2));

	return out;
}
#endif	/* HAVE_ICONV */

size_t	zbx_strlen_utf8(const char *text)
{
	size_t	n = 0;

	while ('\0' != *text)
	{
		if (0x80 != (0xc0 & *text++))
			n++;
	}

	return n;
}

char	*zbx_strshift_utf8(char *text, size_t num)
{
	while ('\0' != *text && 0 < num)
	{
		if (0x80 != (0xc0 & *(++text)))
			num--;
	}

	return text;
}

/******************************************************************************
 *                                                                            *
 * Purpose: Returns the size (in bytes) of a UTF-8 encoded character or 0     *
 *          if the character is not a valid UTF-8.                            *
 *                                                                            *
 * Parameters: text - [IN] pointer to the 1st byte of UTF-8 character         *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_utf8_char_len(const char *text)
{
	if (0 == (*text & 0x80))		/* ASCII */
		return 1;
	else if (0xc0 == (*text & 0xe0))	/* 11000010-11011111 starts a 2-byte sequence */
		return 2;
	else if (0xe0 == (*text & 0xf0))	/* 11100000-11101111 starts a 3-byte sequence */
		return 3;
	else if (0xf0 == (*text & 0xf8))	/* 11110000-11110100 starts a 4-byte sequence */
		return 4;
#if ZBX_MAX_BYTES_IN_UTF8_CHAR != 4
#	error "zbx_utf8_char_len() is not synchronized with ZBX_MAX_BYTES_IN_UTF8_CHAR"
#endif
	return 0;				/* not a valid UTF-8 character */
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculates number of bytes in utf8 text limited by utf8_maxlen    *
 *          characters                                                        *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_strlen_utf8_nchars(const char *text, size_t utf8_maxlen)
{
	size_t		sz = 0, csz = 0;
	const char	*next;

	while ('\0' != *text && 0 < utf8_maxlen && 0 != (csz = zbx_utf8_char_len(text)))
	{
		next = text + csz;
		while (next > text)
		{
			if ('\0' == *text++)
				return sz;
		}
		sz += csz;
		utf8_maxlen--;
	}

	return sz;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculates number of bytes in utf8 text limited by maxlen bytes   *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_strlen_utf8_nbytes(const char *text, size_t maxlen)
{
	size_t	sz;

	sz = strlen(text);

	if (sz > maxlen)
	{
		sz = maxlen;

		/* ensure that the string is not cut in the middle of UTF-8 sequence */
		while (0x80 == (0xc0 & text[sz]) && 0 < sz)
			sz--;
	}

	return sz;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculates number of chars in utf8 text limited by maxlen bytes   *
 *                                                                            *
 ******************************************************************************/
size_t	zbx_charcount_utf8_nbytes(const char *text, size_t maxlen)
{
	size_t	n = 0;

	maxlen = zbx_strlen_utf8_nbytes(text, maxlen);

	while ('\0' != *text && maxlen > 0)
	{
		if (0x80 != (0xc0 & *text++))
			n++;

		maxlen--;
	}

	return n;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check UTF-8 sequences                                             *
 *                                                                            *
 * Parameters: text - [IN] pointer to the string                              *
 *                                                                            *
 * Return value: SUCCEED if string is valid or FAIL otherwise                 *
 *                                                                            *
 ******************************************************************************/
int	zbx_is_utf8(const char *text)
{
	unsigned int	utf32;
	unsigned char	*utf8;
	size_t		i, mb_len, expecting_bytes = 0;

	while ('\0' != *text)
	{
		/* single ASCII character */
		if (0 == (*text & 0x80))
		{
			text++;
			continue;
		}

		/* unexpected continuation byte or invalid UTF-8 bytes '\xfe' & '\xff' */
		if (0x80 == (*text & 0xc0) || 0xfe == (*text & 0xfe))
			return FAIL;

		/* multibyte sequence */

		utf8 = (unsigned char *)text;

		if (0xc0 == (*text & 0xe0))		/* 2-bytes multibyte sequence */
			expecting_bytes = 1;
		else if (0xe0 == (*text & 0xf0))	/* 3-bytes multibyte sequence */
			expecting_bytes = 2;
		else if (0xf0 == (*text & 0xf8))	/* 4-bytes multibyte sequence */
			expecting_bytes = 3;
		else if (0xf8 == (*text & 0xfc))	/* 5-bytes multibyte sequence */
			expecting_bytes = 4;
		else if (0xfc == (*text & 0xfe))	/* 6-bytes multibyte sequence */
			expecting_bytes = 5;

		mb_len = expecting_bytes + 1;
		text++;

		for (; 0 != expecting_bytes; expecting_bytes--)
		{
			/* not a continuation byte */
			if (0x80 != (*text++ & 0xc0))
				return FAIL;
		}

		/* overlong sequence */
		if (0xc0 == (utf8[0] & 0xfe) ||
				(0xe0 == utf8[0] && 0x00 == (utf8[1] & 0x20)) ||
				(0xf0 == utf8[0] && 0x00 == (utf8[1] & 0x30)) ||
				(0xf8 == utf8[0] && 0x00 == (utf8[1] & 0x38)) ||
				(0xfc == utf8[0] && 0x00 == (utf8[1] & 0x3c)))
		{
			return FAIL;
		}

		utf32 = 0;

		if (0xc0 == (utf8[0] & 0xe0))
			utf32 = utf8[0] & 0x1f;
		else if (0xe0 == (utf8[0] & 0xf0))
			utf32 = utf8[0] & 0x0f;
		else if (0xf0 == (utf8[0] & 0xf8))
			utf32 = utf8[0] & 0x07;
		else if (0xf8 == (utf8[0] & 0xfc))
			utf32 = utf8[0] & 0x03;
		else if (0xfc == (utf8[0] & 0xfe))
			utf32 = utf8[0] & 0x01;

		for (i = 1; i < mb_len; i++)
		{
			utf32 <<= 6;
			utf32 += utf8[i] & 0x3f;
		}

		/* according to the Unicode standard the high and low
		 * surrogate halves used by UTF-16 (U+D800 through U+DFFF)
		 * and values above U+10FFFF are not legal
		 */
		if (utf32 > 0x10ffff || 0xd800 == (utf32 & 0xf800))
			return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: replace invalid UTF-8 sequences of bytes with '?' character       *
 *                                                                            *
 * Parameters: text - [IN/OUT] pointer to the first char                      *
 *                                                                            *
 ******************************************************************************/
void	zbx_replace_invalid_utf8(char *text)
{
	char	*out = text;

	while ('\0' != *text)
	{
		if (0 == (*text & 0x80))			/* single ASCII character */
			*out++ = *text++;
		else if (0x80 == (*text & 0xc0) ||		/* unexpected continuation byte */
				0xfe == (*text & 0xfe))		/* invalid UTF-8 bytes '\xfe' & '\xff' */
		{
			*out++ = ZBX_UTF8_REPLACE_CHAR;
			text++;
		}
		else						/* multibyte sequence */
		{
			unsigned int	utf32;
			unsigned char	*utf8 = (unsigned char *)out;
			size_t		i, mb_len, expecting_bytes = 0;
			int		ret = SUCCEED;

			if (0xc0 == (*text & 0xe0))		/* 2-bytes multibyte sequence */
				expecting_bytes = 1;
			else if (0xe0 == (*text & 0xf0))	/* 3-bytes multibyte sequence */
				expecting_bytes = 2;
			else if (0xf0 == (*text & 0xf8))	/* 4-bytes multibyte sequence */
				expecting_bytes = 3;
			else if (0xf8 == (*text & 0xfc))	/* 5-bytes multibyte sequence */
				expecting_bytes = 4;
			else if (0xfc == (*text & 0xfe))	/* 6-bytes multibyte sequence */
				expecting_bytes = 5;

			*out++ = *text++;

			for (; 0 != expecting_bytes; expecting_bytes--)
			{
				if (0x80 != (*text & 0xc0))	/* not a continuation byte */
				{
					ret = FAIL;
					break;
				}

				*out++ = *text++;
			}

			mb_len = out - (char *)utf8;

			if (SUCCEED == ret)
			{
				if (0xc0 == (utf8[0] & 0xfe) ||	/* overlong sequence */
						(0xe0 == utf8[0] && 0x00 == (utf8[1] & 0x20)) ||
						(0xf0 == utf8[0] && 0x00 == (utf8[1] & 0x30)) ||
						(0xf8 == utf8[0] && 0x00 == (utf8[1] & 0x38)) ||
						(0xfc == utf8[0] && 0x00 == (utf8[1] & 0x3c)))
				{
					ret = FAIL;
				}
			}

			if (SUCCEED == ret)
			{
				utf32 = 0;

				if (0xc0 == (utf8[0] & 0xe0))
					utf32 = utf8[0] & 0x1f;
				else if (0xe0 == (utf8[0] & 0xf0))
					utf32 = utf8[0] & 0x0f;
				else if (0xf0 == (utf8[0] & 0xf8))
					utf32 = utf8[0] & 0x07;
				else if (0xf8 == (utf8[0] & 0xfc))
					utf32 = utf8[0] & 0x03;
				else if (0xfc == (utf8[0] & 0xfe))
					utf32 = utf8[0] & 0x01;

				for (i = 1; i < mb_len; i++)
				{
					utf32 <<= 6;
					utf32 += utf8[i] & 0x3f;
				}

				/* according to the Unicode standard the high and low
				 * surrogate halves used by UTF-16 (U+D800 through U+DFFF)
				 * and values above U+10FFFF are not legal
				 */
				if (utf32 > 0x10ffff || 0xd800 == (utf32 & 0xf800))
					ret = FAIL;
			}

			if (SUCCEED != ret)
			{
				out -= mb_len;
				*out++ = ZBX_UTF8_REPLACE_CHAR;
			}
		}
	}

	*out = '\0';
}

void	dos2unix(char *str)
{
	char	*o = str;

	while ('\0' != *str)
	{
		if ('\r' == str[0] && '\n' == str[1])	/* CR+LF (Windows) */
			str++;
		*o++ = *str++;
	}
	*o = '\0';
}

int	is_ascii_string(const char *str)
{
	while ('\0' != *str)
	{
		if (0 != ((1 << 7) & *str))	/* check for range 0..127 */
			return FAIL;

		str++;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: wrap long string at specified position with linefeeds             *
 *                                                                            *
 * Parameters: src     - input string                                         *
 *             maxline - maximum length of a line                             *
 *             delim   - delimiter to use as linefeed (default "\n" if NULL)  *
 *                                                                            *
 * Return value: newly allocated copy of input string with linefeeds          *
 *                                                                            *
 * Comments: allocates memory                                                 *
 *                                                                            *
 ******************************************************************************/
char	*str_linefeed(const char *src, size_t maxline, const char *delim)
{
	size_t		src_size, dst_size, delim_size, left;
	int		feeds;		/* number of feeds */
	char		*dst = NULL;	/* output with linefeeds */
	const char	*p_src;
	char		*p_dst;

	assert(NULL != src);
	assert(0 < maxline);

	/* default delimiter */
	if (NULL == delim)
		delim = "\n";

	src_size = strlen(src);
	delim_size = strlen(delim);

	/* make sure we don't feed the last line */
	feeds = (int)(src_size / maxline - (0 != src_size % maxline || 0 == src_size ? 0 : 1));

	left = src_size - feeds * maxline;
	dst_size = src_size + feeds * delim_size + 1;

	/* allocate memory for output */
	dst = (char *)zbx_malloc(dst, dst_size);

	p_src = src;
	p_dst = dst;

	/* copy chunks appending linefeeds */
	while (0 < feeds--)
	{
		memcpy(p_dst, p_src, maxline);
		p_src += maxline;
		p_dst += maxline;

		memcpy(p_dst, delim, delim_size);
		p_dst += delim_size;
	}

	if (0 < left)
	{
		/* copy what's left */
		memcpy(p_dst, p_src, left);
		p_dst += left;
	}

	*p_dst = '\0';

	return dst;
}

/******************************************************************************
 *                                                                            *
 * Purpose: initialize dynamic string array                                   *
 *                                                                            *
 * Parameters: arr - a pointer to array of strings                            *
 *                                                                            *
 * Comments: allocates memory, calls assert() if that fails                   *
 *                                                                            *
 ******************************************************************************/
void	zbx_strarr_init(char ***arr)
{
	*arr = (char **)zbx_malloc(*arr, sizeof(char *));
	**arr = NULL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: add a string to dynamic string array                              *
 *                                                                            *
 * Parameters: arr - a pointer to array of strings                            *
 *             entry - string to add                                          *
 *                                                                            *
 * Comments: allocates memory, calls assert() if that fails                   *
 *                                                                            *
 ******************************************************************************/
void	zbx_strarr_add(char ***arr, const char *entry)
{
	int	i;

	assert(entry);

	for (i = 0; NULL != (*arr)[i]; i++)
		;

	*arr = (char **)zbx_realloc(*arr, sizeof(char *) * (i + 2));

	(*arr)[i] = zbx_strdup((*arr)[i], entry);
	(*arr)[++i] = NULL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: free dynamic string array memory                                  *
 *                                                                            *
 * Parameters: arr - array of strings                                         *
 *                                                                            *
 ******************************************************************************/
void	zbx_strarr_free(char ***arr)
{
	char	**p;

	for (p = *arr; NULL != *p; p++)
		zbx_free(*p);
	zbx_free(*arr);
}

/******************************************************************************
 *                                                                            *
 * Purpose: replace data block with 'value'                                   *
 *                                                                            *
 * Parameters: data  - [IN/OUT] pointer to the string                         *
 *             l     - [IN] left position of the block                        *
 *             r     - [IN/OUT] right position of the block                   *
 *             value - [IN] the string to replace the block with              *
 *                                                                            *
 ******************************************************************************/
void	zbx_replace_string(char **data, size_t l, size_t *r, const char *value)
{
	size_t	sz_data, sz_block, sz_value;
	char	*src, *dst;

	sz_value = strlen(value);
	sz_block = *r - l + 1;

	if (sz_value != sz_block)
	{
		sz_data = *r + strlen(*data + *r);
		sz_data += sz_value - sz_block;

		if (sz_value > sz_block)
			*data = (char *)zbx_realloc(*data, sz_data + 1);

		src = *data + l + sz_block;
		dst = *data + l + sz_value;

		memmove(dst, src, sz_data - l - sz_value + 1);

		*r = l + sz_value - 1;
	}

	memcpy(&(*data)[l], value, sz_value);
}

/******************************************************************************
 *                                                                            *
 * Purpose: remove whitespace surrounding a string list item delimiters       *
 *                                                                            *
 * Parameters: list      - the list (a string containing items separated by   *
 *                         delimiter)                                         *
 *             delimiter - the list delimiter                                 *
 *                                                                            *
 ******************************************************************************/
void	zbx_trim_str_list(char *list, char delimiter)
{
	/* NB! strchr(3): "terminating null byte is considered part of the string" */
	const char	*whitespace = " \t";
	char		*out, *in;

	out = in = list;

	while ('\0' != *in)
	{
		/* trim leading spaces from list item */
		while ('\0' != *in && NULL != strchr(whitespace, *in))
			in++;

		/* copy list item */
		while (delimiter != *in && '\0' != *in)
			*out++ = *in++;

		/* trim trailing spaces from list item */
		if (out > list)
		{
			while (NULL != strchr(whitespace, *(--out)))
				;
			out++;
		}
		if (delimiter == *in)
			*out++ = *in++;
	}
	*out = '\0';
}

/******************************************************************************
 *                                                                            *
 * Purpose:                                                                   *
 *     compares two strings where any of them can be a NULL pointer           *
 *                                                                            *
 * Parameters: same as strcmp() except NULL values are allowed                *
 *                                                                            *
 * Return value: same as strcmp()                                             *
 *                                                                            *
 * Comments: NULL is less than any string                                     *
 *                                                                            *
 ******************************************************************************/
int	zbx_strcmp_null(const char *s1, const char *s2)
{
	if (NULL == s1)
		return NULL == s2 ? 0 : -1;

	if (NULL == s2)
		return 1;

	return strcmp(s1, s2);
}

/******************************************************************************
 *                                                                            *
 * Purpose:                                                                   *
 *     parses user macro and finds its end position and context location      *
 *                                                                            *
 * Parameters:                                                                *
 *     macro     - [IN] the macro to parse                                    *
 *     macro_r   - [OUT] the position of ending '}' character                 *
 *     context_l - [OUT] the position of context start character (first non   *
 *                       space character after context separator ':')         *
 *                       0 if macro does not have context specified.          *
 *     context_r - [OUT] the position of context end character (either the    *
 *                       ending '"' for quoted context values or the last     *
 *                       character before the ending '}' character)           *
 *                       0 if macro does not have context specified.          *
 *     context_op - [OUT] the context matching operator (optional):           *
 *                          CONDITION_OPERATOR_EQUAL                          *
 *                          CONDITION_OPERATOR_REGEXP                         *
 *                                                                            *
 * Return value:                                                              *
 *     SUCCEED - the macro was parsed successfully.                           *
 *     FAIL    - the macro parsing failed, the content of output variables    *
 *               is not defined.                                              *
 *                                                                            *
 ******************************************************************************/
int	zbx_user_macro_parse(const char *macro, int *macro_r, int *context_l, int *context_r, unsigned char *context_op)
{
	int	i;

	/* find the end of macro name by skipping {$ characters and iterating through */
	/* valid macro name characters                                                */
	for (i = 2; SUCCEED == is_macro_char(macro[i]); i++)
		;

	/* check for empty macro name */
	if (2 == i)
		return FAIL;

	if ('}' == macro[i])
	{
		/* no macro context specified, parsing done */
		*macro_r = i;
		*context_l = 0;
		*context_r = 0;

		if (NULL != context_op)
			*context_op = CONDITION_OPERATOR_EQUAL;

		return SUCCEED;
	}

	/* fail if the next character is not a macro context separator */
	if  (':' != macro[i])
		return FAIL;

	i++;
	if (NULL != context_op)
	{
		if (0 == strncmp(macro + i, ZBX_MACRO_REGEX_PREFIX, ZBX_CONST_STRLEN(ZBX_MACRO_REGEX_PREFIX)))
		{
			*context_op = CONDITION_OPERATOR_REGEXP;
			i += ZBX_CONST_STRLEN(ZBX_MACRO_REGEX_PREFIX);
		}
		else
			*context_op = CONDITION_OPERATOR_EQUAL;
	}

	/* skip the whitespace after macro context separator */
	while (' ' == macro[i])
		i++;

	*context_l = i;

	if ('"' == macro[i])
	{
		i++;

		/* process quoted context */
		for (; '"' != macro[i]; i++)
		{
			if ('\0' == macro[i])
				return FAIL;

			if ('\\' == macro[i] && '"' == macro[i + 1])
				i++;
		}

		*context_r = i;

		while (' ' == macro[++i])
			;
	}
	else
	{
		/* process unquoted context */
		for (; '}' != macro[i]; i++)
		{
			if ('\0' == macro[i])
				return FAIL;
		}

		*context_r = i - 1;
	}

	if ('}' != macro[i])
		return FAIL;

	*macro_r = i;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose:                                                                   *
 *     parses user macro {$MACRO:<context>} into {$MACRO} and <context>       *
 *     strings                                                                *
 *                                                                            *
 * Parameters:                                                                *
 *     macro   - [IN] the macro to parse                                      *
 *     name    - [OUT] the macro name without context                         *
 *     context - [OUT] the unquoted macro context, NULL for macros without    *
 *                     context                                                *
 *     length  - [OUT] the length of parsed macro (optional)                  *
 *     context_op - [OUT] the context matching operator (optional):           *
 *                          CONDITION_OPERATOR_EQUAL                          *
 *                          CONDITION_OPERATOR_REGEXP                         *
 *                                                                            *
 * Return value:                                                              *
 *     SUCCEED - the macro was parsed successfully                            *
 *     FAIL    - the macro parsing failed, invalid parameter syntax           *
 *                                                                            *
 ******************************************************************************/
int	zbx_user_macro_parse_dyn(const char *macro, char **name, char **context, int *length, unsigned char *context_op)
{
	const char	*ptr;
	int		macro_r, context_l, context_r;
	size_t		len;

	if (SUCCEED != zbx_user_macro_parse(macro, &macro_r, &context_l, &context_r, context_op))
		return FAIL;

	zbx_free(*context);

	if (0 != context_l)
	{
		ptr = macro + context_l;

		/* find the context separator ':' by stripping spaces before context */
		while (' ' == *(--ptr))
			;

		/* remove regex: prefix from macro name for regex contexts */
		if (NULL != context_op && CONDITION_OPERATOR_REGEXP == *context_op)
			ptr -= ZBX_CONST_STRLEN(ZBX_MACRO_REGEX_PREFIX);

		/* extract the macro name and close with '}' character */
		len = ptr - macro + 1;
		*name = (char *)zbx_realloc(*name, len + 1);
		memcpy(*name, macro, len - 1);
		(*name)[len - 1] = '}';
		(*name)[len] = '\0';

		*context = zbx_user_macro_unquote_context_dyn(macro + context_l, context_r - context_l + 1);
	}
	else
	{
		*name = (char *)zbx_realloc(*name, macro_r + 2);
		zbx_strlcpy(*name, macro, macro_r + 2);
	}

	if (NULL != length)
		*length = macro_r + 1;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose:                                                                   *
 *     extracts the macro context unquoting if necessary                      *
 *                                                                            *
 * Parameters:                                                                *
 *     context - [IN] the macro context inside a user macro                   *
 *     len     - [IN] the macro context length (including quotes for quoted   *
 *                    contexts)                                               *
 *                                                                            *
 * Return value:                                                              *
 *     A string containing extracted macro context. This string must be freed *
 *     by the caller.                                                         *
 *                                                                            *
 ******************************************************************************/
char	*zbx_user_macro_unquote_context_dyn(const char *context, int len)
{
	int	quoted = 0;
	char	*buffer, *ptr;

	ptr = buffer = (char *)zbx_malloc(NULL, len + 1);

	if ('"' == *context)
	{
		quoted = 1;
		context++;
		len--;
	}

	while (0 < len)
	{
		if (1 == quoted && '\\' == *context && '"' == context[1])
		{
			context++;
			len--;
		}

		*ptr++ = *context++;
		len--;
	}

	if (1 == quoted)
		ptr--;

	*ptr = '\0';

	return buffer;
}

/******************************************************************************
 *                                                                            *
 * Purpose:                                                                   *
 *     quotes user macro context if necessary                                 *
 *                                                                            *
 * Parameters:                                                                *
 *     context     - [IN] the macro context                                   *
 *     force_quote - [IN] if non zero then context quoting is enforced        *
 *     error       - [OUT] the error message                                  *
 *                                                                            *
 * Return value:                                                              *
 *     A string containing quoted macro context on success, NULL on error.    *
 *                                                                            *
 ******************************************************************************/
char	*zbx_user_macro_quote_context_dyn(const char *context, int force_quote, char **error)
{
	int		len, quotes = 0;
	char		*buffer, *ptr_buffer;
	const char	*ptr_context = context, *start = context;

	if ('"' == *ptr_context || ' ' == *ptr_context)
		force_quote = 1;

	for (; '\0' != *ptr_context; ptr_context++)
	{
		if ('}' == *ptr_context)
			force_quote = 1;

		if ('"' == *ptr_context)
			quotes++;
	}

	if (0 == force_quote)
		return zbx_strdup(NULL, context);

	len = (int)strlen(context) + 2 + quotes;
	ptr_buffer = buffer = (char *)zbx_malloc(NULL, len + 1);

	*ptr_buffer++ = '"';

	while ('\0' != *context)
	{
		if ('"' == *context)
			*ptr_buffer++ = '\\';

		*ptr_buffer++ = *context++;
	}

	if ('\\' == *(ptr_buffer - 1))
	{
		*error = zbx_dsprintf(*error, "quoted context \"%s\" cannot end with '\\' character", start);
		zbx_free(buffer);
		return NULL;
	}

	*ptr_buffer++ = '"';
	*ptr_buffer++ = '\0';

	return buffer;
}

/******************************************************************************
 *                                                                            *
 * Purpose: escape single quote in shell command arguments                    *
 *                                                                            *
 * Parameters: arg - [IN] the argument to escape                              *
 *                                                                            *
 * Return value: The escaped argument.                                        *
 *                                                                            *
 ******************************************************************************/
char	*zbx_dyn_escape_shell_single_quote(const char *arg)
{
	int		len = 1; /* include terminating zero character */
	const char	*pin;
	char		*arg_esc, *pout;

	for (pin = arg; '\0' != *pin; pin++)
	{
		if ('\'' == *pin)
			len += 3;
		len++;
	}

	pout = arg_esc = (char *)zbx_malloc(NULL, len);

	for (pin = arg; '\0' != *pin; pin++)
	{
		if ('\'' == *pin)
		{
			*pout++ = '\'';
			*pout++ = '\\';
			*pout++ = '\'';
			*pout++ = '\'';
		}
		else
			*pout++ = *pin;
	}

	*pout = '\0';

	return arg_esc;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses function name                                              *
 *                                                                            *
 * Parameters: expr     - [IN] the function expression: func(p1, p2,...)      *
 *             length   - [OUT] the function name length or the amount of     *
 *                              characters that can be safely skipped         *
 *                                                                            *
 * Return value: SUCCEED - the function name was successfully parsed          *
 *               FAIL    - failed to parse function name                      *
 *                                                                            *
 ******************************************************************************/
static int	function_parse_name(const char *expr, size_t *length)
{
	const char	*ptr;

	for (ptr = expr; SUCCEED == is_function_char(*ptr); ptr++)
		;

	*length = ptr - expr;

	return ptr != expr && '(' == *ptr ? SUCCEED : FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses function parameter                                         *
 *                                                                            *
 * Parameters: expr      - [IN] pre-validated function parameter list         *
 *             param_pos - [OUT] the parameter position, excluding leading    *
 *                               whitespace                                   *
 *             length    - [OUT] the parameter length including trailing      *
 *                               whitespace for unquoted parameter            *
 *             sep_pos   - [OUT] the parameter separator character            *
 *                               (',' or '\0' or ')') position                *
 *                                                                            *
 ******************************************************************************/
void	zbx_function_param_parse(const char *expr, size_t *param_pos, size_t *length, size_t *sep_pos)
{
	zbx_function_param_parse_ext(expr, 0, 0, param_pos, length, sep_pos);
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse function parameter                                          *
 *                                                                            *
 * Parameters: expr           - [IN] pre-validated function parameter list    *
 *             allowed_macros - [IN] bitmask of macros allowed in function    *
 *                                   parameters (seeZBX_TOKEN_* defines)      *
 *             esc_bs         - [IN] 0 - don't escape backslashes in strings  *
 *             param_pos      - [OUT] the parameter position, excluding       *
 *                                    leading whitespace                      *
 *             length         - [OUT] the parameter length including trailing *
 *                                    whitespace for unquoted parameter       *
 *             sep_pos        - [OUT] the parameter separator character       *
 *                                    (',' or '\0' or ')') position           *
 *                                                                            *
 ******************************************************************************/
void	zbx_function_param_parse_ext(const char *expr, zbx_uint32_t allowed_macros, int esc_bs, size_t *param_pos,
		size_t *length, size_t *sep_pos)
{
	const char	*ptr = expr;

	/* skip the leading whitespace */
	while (' ' == *ptr)
		ptr++;

	*param_pos = ptr - expr;

	if ('"' == *ptr)	/* quoted parameter */
	{
		for (ptr++; '"' != *ptr; ptr++)
		{
			if ('\\' == *ptr)
			{
				if ('"' == ptr[1])
				{
					ptr++;
					continue;
				}

				if (ZBX_BACKSLASH_ESC_OFF == esc_bs)
					continue;

				ptr++;
			}

			if ('\0' == *ptr)
			{
				*length = ptr - expr - *param_pos;
				goto out;
			}
		}

		*length = ++ptr - expr - *param_pos;

		/* skip trailing whitespace to find the next parameter */
		while (' ' == *ptr)
			ptr++;
	}
	else	/* unquoted parameter */
	{
		zbx_token_t	token;

		for (ptr = expr; ; ptr++)
		{
			switch (*ptr)
			{
				case '\0':
				case ')':
				case ',':
					*length = ptr - expr - *param_pos;
					goto out;
				case '{':
					if (SUCCEED == zbx_token_find(ptr, 0, &token, ZBX_TOKEN_SEARCH_BASIC) &&
							0 == token.loc.l && 0 != (allowed_macros & token.type))
					{
						ptr += token.loc.r;
					}
					break;
			}
		}
	}
out:
	*sep_pos = ptr - expr;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse trigger function parameter                                  *
 *                                                                            *
 * Parameters: expr      - [IN] pre-validated function parameter list         *
 *             param_pos - [OUT] the parameter position, excluding leading    *
 *                               whitespace                                   *
 *             length    - [OUT] the parameter length including trailing      *
 *                               whitespace for unquoted parameter            *
 *             sep_pos   - [OUT] the parameter separator character            *
 *                               (',' or '\0' or ')') position                *
 *                                                                            *
 ******************************************************************************/
void	zbx_trigger_function_param_parse(const char *expr, size_t *param_pos, size_t *length, size_t *sep_pos)
{
	zbx_function_param_parse_ext(expr, ZBX_TOKEN_USER_MACRO, ZBX_BACKSLASH_ESC_OFF, param_pos, length, sep_pos);
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse trigger prototype function parameter                        *
 *                                                                            *
 * Parameters: expr      - [IN] pre-validated function parameter list         *
 *             param_pos - [OUT] the parameter position, excluding leading    *
 *                               whitespace                                   *
 *             length    - [OUT] the parameter length including trailing      *
 *                               whitespace for unquoted parameter            *
 *             sep_pos   - [OUT] the parameter separator character            *
 *                               (',' or '\0' or ')') position                *
 *                                                                            *
 ******************************************************************************/
void	zbx_lld_trigger_function_param_parse(const char *expr, size_t *param_pos, size_t *length, size_t *sep_pos)
{
	zbx_function_param_parse_ext(expr, ZBX_TOKEN_USER_MACRO | ZBX_TOKEN_LLD_MACRO | ZBX_TOKEN_LLD_FUNC_MACRO,
			ZBX_BACKSLASH_ESC_OFF, param_pos, length, sep_pos);
}


/******************************************************************************
 *                                                                            *
 * Purpose: unquotes function parameter                                       *
 *                                                                            *
 * Parameters: param -  [IN] the parameter to unquote                         *
 *             len   -  [IN] the parameter length                             *
 *             quoted - [OUT] the flag that specifies whether parameter was   *
 *                            quoted before extraction                        *
 *                                                                            *
 * Return value: The unquoted parameter. This value must be freed by the      *
 *               caller.                                                      *
 *                                                                            *
 ******************************************************************************/
char	*zbx_function_param_unquote_dyn(const char *param, size_t len, int *quoted)
{
	char	*out;

	out = (char *)zbx_malloc(NULL, len + 1);

	if (0 == (*quoted = (0 != len && '"' == *param)))
	{
		/* unquoted parameter - simply copy it */
		memcpy(out, param, len);
		out[len] = '\0';
	}
	else
	{
		/* quoted parameter - remove enclosing " and replace \" with " */
		const char	*pin;
		char		*pout = out;

		for (pin = param + 1; (size_t)(pin - param) < len - 1; pin++)
		{
			if ('\\' == pin[0] && '"' == pin[1])
				pin++;

			*pout++ = *pin;
		}

		*pout = '\0';
	}

	return out;
}

/******************************************************************************
 *                                                                            *
 * Purpose: quotes function parameter                                         *
 *                                                                            *
 * Parameters: param   - [IN/OUT] function parameter                          *
 *             forced  - [IN] 1 - enclose parameter in " even if it does not  *
 *                                contain any special characters              *
 *                            0 - do nothing if the parameter does not        *
 *                                contain any special characters              *
 *                                                                            *
 * Return value: SUCCEED - if parameter was successfully quoted or quoting    *
 *                         was not necessary                                  *
 *               FAIL    - if parameter needs to but cannot be quoted due to  *
 *                         backslash in the end                               *
 *                                                                            *
 ******************************************************************************/
int	zbx_function_param_quote(char **param, int forced)
{
	size_t	sz_src, sz_dst;

	if (0 == forced && '"' != **param && ' ' != **param && NULL == strchr(*param, ',') &&
			NULL == strchr(*param, ')'))
	{
		return SUCCEED;
	}

	if (0 != (sz_src = strlen(*param)) && '\\' == (*param)[sz_src - 1])
		return FAIL;

	sz_dst = zbx_get_escape_string_len(*param, "\"") + 3;

	*param = (char *)zbx_realloc(*param, sz_dst);

	(*param)[--sz_dst] = '\0';
	(*param)[--sz_dst] = '"';

	while (0 < sz_src)
	{
		(*param)[--sz_dst] = (*param)[--sz_src];
		if ('"' == (*param)[sz_src])
			(*param)[--sz_dst] = '\\';
	}
	(*param)[--sz_dst] = '"';

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return parameter by index (Nparam) from parameter list (params)   *
 *                                                                            *
 * Parameters:                                                                *
 *      params - [IN] parameter list                                          *
 *      Nparam - [IN] requested parameter index (from 1)                      *
 *                                                                            *
 * Return value:                                                              *
 *      NULL - requested parameter missing                                    *
 *      otherwise - requested parameter                                       *
 *                                                                            *
 ******************************************************************************/
char	*zbx_function_get_param_dyn(const char *params, int Nparam)
{
	const char	*ptr;
	size_t		sep_pos, params_len;
	char		*out = NULL;
	int		idx = 0;

	params_len = strlen(params) + 1;

	for (ptr = params; ++idx <= Nparam && ptr < params + params_len; ptr += sep_pos + 1)
	{
		size_t	param_pos, param_len;
		int	quoted;

		zbx_function_param_parse(ptr, &param_pos, &param_len, &sep_pos);

		if (idx == Nparam)
			out = zbx_function_param_unquote_dyn(ptr + param_pos, param_len, &quoted);
	}

	return out;
}

/******************************************************************************
 *                                                                            *
 * Purpose: validate parameters and give position of terminator if found and  *
 *          not quoted                                                        *
 *                                                                            *
 * Parameters: expr       - [IN] string to parse that contains parameters     *
 *                                                                            *
 *             terminator - [IN] use ')' if parameters end with               *
 *                               parenthesis or '\0' if ends with NULL        *
 *                               terminator                                   *
 *             par_r      - [OUT] position of the terminator if found         *
 *             lpp_offset - [OUT] offset of the last parsed parameter         *
 *             lpp_len    - [OUT] length of the last parsed parameter         *
 *                                                                            *
 * Return value: SUCCEED -  closing parenthesis was found or other custom     *
 *                          terminator and not quoted and return info about a *
 *                          last processed parameter.                         *
 *               FAIL    -  does not look like a valid function parameter     *
 *                          list and return info about a last processed       *
 *                          parameter.                                        *
 *                                                                            *
 ******************************************************************************/
static int	function_validate_parameters(const char *expr, char terminator, size_t *par_r, size_t *lpp_offset,
		size_t *lpp_len)
{
#define ZBX_FUNC_PARAM_NEXT		0
#define ZBX_FUNC_PARAM_QUOTED		1
#define ZBX_FUNC_PARAM_UNQUOTED		2
#define ZBX_FUNC_PARAM_POSTQUOTED	3

	const char	*ptr;
	int		state = ZBX_FUNC_PARAM_NEXT;

	*lpp_offset = 0;

	for (ptr = expr; '\0' != *ptr; ptr++)
	{
		if (terminator == *ptr && ZBX_FUNC_PARAM_QUOTED != state)
		{
			*par_r = ptr - expr;
			return SUCCEED;
		}

		switch (state)
		{
			case ZBX_FUNC_PARAM_NEXT:
				*lpp_offset = ptr - expr;
				if ('"' == *ptr)
					state = ZBX_FUNC_PARAM_QUOTED;
				else if (' ' != *ptr && ',' != *ptr)
					state = ZBX_FUNC_PARAM_UNQUOTED;
				break;
			case ZBX_FUNC_PARAM_QUOTED:
				if ('"' == *ptr && '\\' != *(ptr - 1))
					state = ZBX_FUNC_PARAM_POSTQUOTED;
				break;
			case ZBX_FUNC_PARAM_UNQUOTED:
				if (',' == *ptr)
					state = ZBX_FUNC_PARAM_NEXT;
				break;
			case ZBX_FUNC_PARAM_POSTQUOTED:
				if (',' == *ptr)
				{
					state = ZBX_FUNC_PARAM_NEXT;
				}
				else if (' ' != *ptr)
				{
					*lpp_len = ptr - (expr + *lpp_offset);
					return FAIL;
				}
				break;
			default:
				THIS_SHOULD_NEVER_HAPPEN;
		}
	}

	*lpp_len = ptr - (expr + *lpp_offset);

	if (terminator == *ptr && ZBX_FUNC_PARAM_QUOTED != state)
	{
		*par_r = ptr - expr;
		return SUCCEED;
	}

	return FAIL;

#undef ZBX_FUNC_PARAM_NEXT
#undef ZBX_FUNC_PARAM_QUOTED
#undef ZBX_FUNC_PARAM_UNQUOTED
#undef ZBX_FUNC_PARAM_POSTQUOTED
}

/******************************************************************************
 *                                                                            *
 * Purpose: given the position of opening function parenthesis find the       *
 *          position of a closing one                                         *
 *                                                                            *
 * Parameters: expr       - [IN] string to parse                              *
 *             par_l      - [IN] position of the opening parenthesis          *
 *             par_r      - [OUT] position of the closing parenthesis         *
 *             lpp_offset - [OUT] offset of the last parsed parameter         *
 *             lpp_len    - [OUT] length of the last parsed parameter         *
 *                                                                            *
 * Return value: SUCCEED - closing parenthesis was found                      *
 *               FAIL    - string after par_l does not look like a valid      *
 *                         function parameter list                            *
 *                                                                            *
 ******************************************************************************/
static int	function_match_parenthesis(const char *expr, size_t par_l, size_t *par_r, size_t *lpp_offset,
		size_t *lpp_len)
{
	if (SUCCEED == function_validate_parameters(expr + par_l + 1, ')', par_r, lpp_offset, lpp_len))
	{
		*par_r += par_l + 1;
		return SUCCEED;
	}

	*lpp_offset += par_l + 1;
	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: validate parameters that end with '\0'                            *
 *                                                                            *
 * Parameters: expr       - [IN] string to parse that contains parameters     *
 *             length     - [OUT] length of parameters                        *
 *                                                                            *
 * Return value: SUCCEED -  null termination encountered when quotes are      *
 *                          closed and no other error                         *
 *               FAIL    -  does not look like a valid                        *
 *                          function parameter list                           *
 *                                                                            *
 ******************************************************************************/
int	zbx_function_validate_parameters(const char *expr, size_t *length)
{
	size_t offset, len;

	return function_validate_parameters(expr, '\0', length, &offset, &len);
}

/******************************************************************************
 *                                                                            *
 * Purpose: check whether expression starts with a valid function             *
 *                                                                            *
 * Parameters: expr          - [IN] string to parse                           *
 *             par_l         - [OUT] position of the opening parenthesis      *
 *                                   or the amount of characters to skip      *
 *             par_r         - [OUT] position of the closing parenthesis      *
 *             error         - [OUT] error message                            *
 *             max_error_len - [IN] error size                                *
 *                                                                            *
 * Return value: SUCCEED - string starts with a valid function                *
 *               FAIL    - string does not start with a function and par_l    *
 *                         characters can be safely skipped                   *
 *                                                                            *
 ******************************************************************************/
static int	zbx_function_validate(const char *expr, size_t *par_l, size_t *par_r, char *error, int max_error_len)
{
	size_t	lpp_offset, lpp_len;

	/* try to validate function name */
	if (SUCCEED == function_parse_name(expr, par_l))
	{
		/* now we know the position of '(', try to find ')' */
		if (SUCCEED == function_match_parenthesis(expr, *par_l, par_r, &lpp_offset, &lpp_len))
			return SUCCEED;

		if (NULL != error && *par_l > *par_r)
		{
			zbx_snprintf(error, max_error_len, "Incorrect function '%.*s' expression. "
				"Check expression part starting from: %.*s",
				(int)*par_l, expr, (int)lpp_len, expr + lpp_offset);

			return FAIL;
		}
	}

	if (NULL != error)
		zbx_snprintf(error, max_error_len, "Incorrect function expression: %s", expr);

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: performs natural comparison of two strings                        *
 *                                                                            *
 * Parameters: s1 - [IN] the first string                                     *
 *             s2 - [IN] the second string                                    *
 *                                                                            *
 * Return value:  0: the strings are equal                                    *
 *               <0: s1 < s2                                                  *
 *               >0: s1 > s2                                                  *
 *                                                                            *
 ******************************************************************************/
int	zbx_strcmp_natural(const char *s1, const char *s2)
{
	int	ret, value1, value2;

	for (;'\0' != *s1 && '\0' != *s2; s1++, s2++)
	{
		if (0 == isdigit(*s1) || 0 == isdigit(*s2))
		{
			if (0 != (ret = *s1 - *s2))
				return ret;

			continue;
		}

		value1 = 0;
		while (0 != isdigit(*s1))
			value1 = value1 * 10 + *s1++ - '0';

		value2 = 0;
		while (0 != isdigit(*s2))
			value2 = value2 * 10 + *s2++ - '0';

		if (0 != (ret = value1 - value2))
			return ret;

		if ('\0' == *s1 || '\0' == *s2)
			break;
	}

	return *s1 - *s2;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses user macro token                                           *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the user macro was parsed successfully             *
 *               FAIL    - macro does not point at valid user macro           *
 *                                                                            *
 * Comments: If the macro points at valid user macro in the expression then   *
 *           the generic token fields are set and the token->data.user_macro  *
 *           structure is filled with user macro specific data.               *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_user_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	size_t			offset;
	int			macro_r, context_l, context_r;
	zbx_token_user_macro_t	*data;

	if (SUCCEED != zbx_user_macro_parse(macro, &macro_r, &context_l, &context_r, NULL))
		return FAIL;

	offset = macro - expression;

	/* initialize token */
	token->type = ZBX_TOKEN_USER_MACRO;
	token->loc.l = offset;
	token->loc.r = offset + macro_r;

	/* initialize token data */
	data = &token->data.user_macro;
	data->name.l = offset + 2;

	if (0 != context_l)
	{
		const char *ptr = macro + context_l;

		/* find the context separator ':' by stripping spaces before context */
		while (' ' == *(--ptr))
			;

		data->name.r = offset + (ptr - macro) - 1;

		data->context.l = offset + context_l;
		data->context.r = offset + context_r;
	}
	else
	{
		data->name.r = token->loc.r - 1;
		data->context.l = 0;
		data->context.r = 0;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses lld macro token                                            *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the lld macro was parsed successfully              *
 *               FAIL    - macro does not point at valid lld macro            *
 *                                                                            *
 * Comments: If the macro points at valid lld macro in the expression then    *
 *           the generic token fields are set and the token->data.lld_macro   *
 *           structure is filled with lld macro specific data.                *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_lld_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	const char		*ptr;
	size_t			offset;
	zbx_token_macro_t	*data;

	/* find the end of lld macro by validating its name until the closing bracket } */
	for (ptr = macro + 2; '}' != *ptr; ptr++)
	{
		if ('\0' == *ptr)
			return FAIL;

		if (SUCCEED != is_macro_char(*ptr))
			return FAIL;
	}

	/* empty macro name */
	if (2 == ptr - macro)
		return FAIL;

	offset = macro - expression;

	/* initialize token */
	token->type = ZBX_TOKEN_LLD_MACRO;
	token->loc.l = offset;
	token->loc.r = offset + (ptr - macro);

	/* initialize token data */
	data = &token->data.lld_macro;
	data->name.l = offset + 2;
	data->name.r = token->loc.r - 1;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses expression macro token                                     *
 *                                                                            *
 * Parameters: expression    - [IN] the expression                            *
 *             macro         - [IN] the beginning of the token                *
 *             token_search - [IN] specify if references will be searched     *
 *             token         - [OUT] the token data                           *
 *                                                                            *
 * Return value: SUCCEED - the expression macro was parsed successfully       *
 *               FAIL    - macro does not point at valid expression macro     *
 *                                                                            *
 * Comments: If the macro points at valid expression macro in the expression  *
 *           then the generic token fields are set and the                    *
 *           token->data.expression_macro structure is filled with expression *
 *           macro specific data.                                             *
 *           Contents of macro are not validated because expression macro may *
 *           contain user macro contexts and item keys with string arguments. *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_expression_macro(const char *expression, const char *macro, zbx_token_search_t token_search,
		zbx_token_t *token)
{
	const char			*ptr;
	size_t				offset;
	zbx_token_expression_macro_t	*data;
	int				quoted = 0;

	for (ptr = macro + 2; '\0' != *ptr ; ptr++)
	{
		if (1 == quoted)
		{
			if ('\\' == *ptr)
			{
				if ('\0' == *(++ptr))
					break;
				continue;
			}

			if ('"' == *ptr)
				quoted = 0;

			continue;
		}

		if ('{' == *ptr)
		{
			zbx_token_t	tmp;

			/* nested expression macros are not supported */
			if ('?' == ptr[1])
				continue;

			token_search &= ~ZBX_TOKEN_SEARCH_EXPRESSION_MACRO;
			if (SUCCEED == zbx_token_find(ptr, 0, &tmp, token_search))
			{
				switch (tmp.type)
				{
					case ZBX_TOKEN_MACRO:
					case ZBX_TOKEN_LLD_MACRO:
					case ZBX_TOKEN_LLD_FUNC_MACRO:
					case ZBX_TOKEN_USER_MACRO:
					case ZBX_TOKEN_SIMPLE_MACRO:
						ptr += tmp.loc.r;
						break;
				}
			}
		}
		else if ('}' == *ptr)
		{
			/* empty macro */
			if (ptr == macro + 2)
				return FAIL;

			offset = macro - expression;

			/* initialize token */
			token->type = ZBX_TOKEN_EXPRESSION_MACRO;
			token->loc.l = offset;
			token->loc.r = offset + (ptr - macro);

			/* initialize token data */
			data = &token->data.expression_macro;
			data->expression.l = offset + 2;
			data->expression.r = token->loc.r - 1;

			return SUCCEED;
		}
		else if ('"' == *ptr)
			quoted = 1;
	}

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses object id token                                            *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the object id was parsed successfully              *
 *               FAIL    - macro does not point at valid object id            *
 *                                                                            *
 * Comments: If the macro points at valid object id in the expression then    *
 *           the generic token fields are set and the token->data.objectid    *
 *           structure is filled with object id specific data.                *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_objectid(const char *expression, const char *macro, zbx_token_t *token)
{
	const char		*ptr;
	size_t			offset;
	zbx_token_macro_t	*data;

	/* find the end of object id by checking if it contains digits until the closing bracket } */
	for (ptr = macro + 1; '}' != *ptr; ptr++)
	{
		if ('\0' == *ptr)
			return FAIL;

		if (0 == isdigit(*ptr))
			return FAIL;
	}

	/* empty object id */
	if (1 == ptr - macro)
		return FAIL;

	offset = macro - expression;

	/* initialize token */
	token->type = ZBX_TOKEN_OBJECTID;
	token->loc.l = offset;
	token->loc.r = offset + (ptr - macro);

	/* initialize token data */
	data = &token->data.objectid;
	data->name.l = offset + 1;
	data->name.r = token->loc.r - 1;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses macro name segment                                         *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             segment    - [IN] the segment start                            *
 *             strict     - [OUT] 1 - macro contains only standard characters *
 *                                    (upper case alphanumeric characters,    *
 *                                     dots and underscores)                  *
 *                                0 - the last segment contains lowercase or  *
 *                                    quoted characters                       *
 *             next       - [OUT] offset of the next character after the      *
 *                                segment                                     *
 *                                                                            *
 * Return value: SUCCEED - the segment was parsed successfully                *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_macro_segment(const char *expression, const char *segment, int *strict, int *next)
{
	const char	*ptr = segment;

	if ('"' != *ptr)
	{
		for (*strict = 1; '\0' != *ptr; ptr++)
		{
			if (0 != isalpha((unsigned char)*ptr))
			{
				if (0 == isupper((unsigned char)*ptr))
					*strict = 0;
				continue;
			}

			if (0 != isdigit((unsigned char)*ptr))
				continue;

			if ('_' == *ptr)
				continue;

			break;
		}

		/* check for empty segment */
		if (ptr == segment)
			return FAIL;

		*next = ptr - expression;
	}
	else
	{
		for (*strict = 0, ptr++; '"' != *ptr; ptr++)
		{
			if ('\0' == *ptr)
				return FAIL;

			if ('\\' == *ptr)
			{
				ptr++;
				if ('\\' != *ptr && '"' != *ptr)
					return FAIL;
			}
		}

		/* check for empty segment */
		if (1 == ptr - segment)
			return FAIL;

		*next = ptr - expression + 1;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses macro name                                                 *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             ptr        - [IN] the beginning of macro name                  *
 *             loc        - [OUT] the macro name location                     *
 *                                                                            *
 * Return value: SUCCEED - the simple macro was parsed successfully           *
 *               FAIL    - macro does not point at valid macro                *
 *                                                                            *
 * Comments: Note that the character following macro name must be inspected   *
 *           to draw any conclusions. For example for normal macros it must   *
 *           be '}' or it's not a valid macro.                                *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_macro_name(const char *expression, const char *ptr, zbx_strloc_t *loc)
{
	int	strict, offset, ret;

	loc->l = ptr - expression;

	while (SUCCEED == (ret = token_parse_macro_segment(expression, ptr, &strict, &offset)))
	{
		if (0 == strict && expression + loc->l == ptr)
			return FAIL;

		ptr = expression + offset;

		if ('.' != *ptr || 0 == strict)
		{
			loc->r = ptr - expression - 1;
			break;
		}
		ptr++;
	}
	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses normal macro token                                         *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the simple macro was parsed successfully           *
 *               FAIL    - macro does not point at valid macro                *
 *                                                                            *
 * Comments: If the macro points at valid macro in the expression then        *
 *           the generic token fields are set and the token->data.macro       *
 *           structure is filled with simple macro specific data.             *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	zbx_strloc_t		loc;
	zbx_token_macro_t	*data;

	if (SUCCEED != token_parse_macro_name(expression, macro + 1, &loc))
		return FAIL;

	if ('}' != expression[loc.r + 1])
		return FAIL;

	/* initialize token */
	token->type = ZBX_TOKEN_MACRO;
	token->loc.l = loc.l - 1;
	token->loc.r = loc.r + 1;

	/* initialize token data */
	data = &token->data.macro;
	data->name = loc;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses function inside token                                      *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             func       - [IN] the beginning of the function                *
 *             func_loc   - [OUT] the function location relative to the       *
 *                                expression (including parameters)           *
 *                                                                            *
 * Return value: SUCCEED - the function was parsed successfully               *
 *               FAIL    - func does not point at valid function              *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_function(const char *expression, const char *func,
		zbx_strloc_t *func_loc, zbx_strloc_t *func_param)
{
	size_t	par_l, par_r;

	if (SUCCEED != zbx_function_validate(func, &par_l, &par_r, NULL, 0))
		return FAIL;

	func_loc->l = func - expression;
	func_loc->r = func_loc->l + par_r;

	func_param->l = func_loc->l + par_l;
	func_param->r = func_loc->l + par_r;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses function macro token                                       *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             func       - [IN] the beginning of the macro function in the   *
 *                               token                                        *
 *             token      - [OUT] the token data                              *
 *             token_type - [IN] type flag ZBX_TOKEN_FUNC_MACRO or            *
 *                               ZBX_TOKEN_LLD_FUNC_MACRO                     *
 *                                                                            *
 * Return value: SUCCEED - the function macro was parsed successfully         *
 *               FAIL    - macro does not point at valid function macro       *
 *                                                                            *
 * Comments: If the macro points at valid function macro in the expression    *
 *           then the generic token fields are set and the                    *
 *           token->data.func_macro or token->data.lld_func_macro structures  *
 *           depending on token type flag are filled with function macro      *
 *           specific data.                                                   *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_func_macro(const char *expression, const char *macro, const char *func,
		zbx_token_t *token, int token_type)
{
	zbx_strloc_t		func_loc, func_param;
	zbx_token_func_macro_t	*data;
	const char		*ptr;
	size_t			offset;

	if ('\0' == *func)
		return FAIL;

	if (SUCCEED != token_parse_function(expression, func, &func_loc, &func_param))
		return FAIL;

	ptr = expression + func_loc.r + 1;

	/* skip trailing whitespace and verify that token ends with } */

	while (' ' == *ptr)
		ptr++;

	if ('}' != *ptr)
		return FAIL;

	offset = macro - expression;

	/* initialize token */
	token->type = token_type;
	token->loc.l = offset;
	token->loc.r = ptr - expression;

	/* initialize token data */
	data = ZBX_TOKEN_FUNC_MACRO == token_type ? &token->data.func_macro : &token->data.lld_func_macro;
	data->macro.l = offset + 1;
	data->macro.r = func_loc.l - 2;

	data->func = func_loc;
	data->func_param = func_param;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses simple macro token with given key                          *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             key        - [IN] the beginning of host key inside the token   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the function macro was parsed successfully         *
 *               FAIL    - macro does not point at valid simple macro         *
 *                                                                            *
 * Comments: Simple macros have format {<host>:<key>.<func>(<params>)}        *
 *           {HOST.HOSTn} macro can be used for host name and {ITEM.KEYn}     *
 *           macro can be used for item key.                                  *
 *                                                                            *
 *           If the macro points at valid simple macro in the expression      *
 *           then the generic token fields are set and the                    *
 *           token->data.simple_macro structure is filled with simple macro   *
 *           specific data.                                                   *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_simple_macro_key(const char *expression, const char *macro, const char *key,
		zbx_token_t *token)
{
	size_t				offset;
	zbx_token_simple_macro_t	*data;
	const char			*ptr = key;
	zbx_strloc_t			key_loc, func_loc, func_param;

	if (SUCCEED != parse_key(&ptr))
	{
		zbx_token_t	key_token;

		if (SUCCEED != token_parse_macro(expression, key, &key_token))
			return FAIL;

		ptr = expression + key_token.loc.r + 1;
	}

	/* If the key is without parameters, then parse_key() will move cursor past function name - */
	/* at the start of its parameters. In this case move cursor back before function.           */
	if ('(' == *ptr)
	{
		while ('.' != *(--ptr))
			;
	}

	/* check for empty key */
	if (0 == ptr - key)
		return FAIL;

	if (SUCCEED != token_parse_function(expression, ptr + 1, &func_loc, &func_param))
		return FAIL;

	key_loc.l = key - expression;
	key_loc.r = ptr - expression - 1;

	ptr = expression + func_loc.r + 1;

	/* skip trailing whitespace and verify that token ends with } */

	while (' ' == *ptr)
		ptr++;

	if ('}' != *ptr)
		return FAIL;

	offset = macro - expression;

	/* initialize token */
	token->type = ZBX_TOKEN_SIMPLE_MACRO;
	token->loc.l = offset;
	token->loc.r = ptr - expression;

	/* initialize token data */
	data = &token->data.simple_macro;
	data->host.l = offset + 1;
	data->host.r = offset + (key - macro) - 2;

	data->key = key_loc;
	data->func = func_loc;
	data->func_param = func_param;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses simple macro token                                         *
 *                                                                            *
 * Parameters: expression - [IN] the expression                               *
 *             macro      - [IN] the beginning of the token                   *
 *             token      - [OUT] the token data                              *
 *                                                                            *
 * Return value: SUCCEED - the simple macro was parsed successfully           *
 *               FAIL    - macro does not point at valid simple macro         *
 *                                                                            *
 * Comments: Simple macros have format {<host>:<key>.<func>(<params>)}        *
 *           {HOST.HOSTn} macro can be used for host name and {ITEM.KEYn}     *
 *           macro can be used for item key.                                  *
 *                                                                            *
 *           If the macro points at valid simple macro in the expression      *
 *           then the generic token fields are set and the                    *
 *           token->data.simple_macro structure is filled with simple macro   *
 *           specific data.                                                   *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_simple_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	const char	*ptr;

	/* Find the end of host name by validating its name until the closing bracket }.          */
	/* {HOST.HOSTn} macro usage in the place of host name is handled by nested macro parsing. */
	for (ptr = macro + 1; ':' != *ptr; ptr++)
	{
		if ('\0' == *ptr)
			return FAIL;

		if (SUCCEED != is_hostname_char(*ptr))
			return FAIL;
	}

	/* check for empty host name */
	if (1 == ptr - macro)
		return FAIL;

	return token_parse_simple_macro_key(expression, macro, ptr + 1, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: parses token with nested macros                                   *
 *                                                                            *
 * Parameters: expression   - [IN] the expression                             *
 *             macro        - [IN] the beginning of the token                 *
 *             token_search - [IN] specify if references will be searched     *
 *             token        - [OUT] the token data                            *
 *                                                                            *
 * Return value: SUCCEED - the token was parsed successfully                  *
 *               FAIL    - macro does not point at valid function or simple   *
 *                         macro                                              *
 *                                                                            *
 * Comments: This function parses token with a macro inside it. There are     *
 *           three types of nested macros - low-level discovery function      *
 *           macros, function macros and a specific case of simple macros     *
 *           where {HOST.HOSTn} macro is used as host name.                   *
 *                                                                            *
 *           If the macro points at valid macro in the expression then        *
 *           the generic token fields are set and either the                  *
 *           token->data.lld_func_macro, token->data.func_macro or            *
 *           token->data.simple_macro (depending on token type) structure is  *
 *           filled with macro specific data.                                 *
 *                                                                            *
 ******************************************************************************/
static int	token_parse_nested_macro(const char *expression, const char *macro, zbx_token_search_t token_search,
		zbx_token_t *token)
{
	const char	*ptr;

	if ('#' == macro[2])
	{
		/* find the end of the nested macro by validating its name until the closing bracket '}' */
		for (ptr = macro + 3; '}' != *ptr; ptr++)
		{
			if ('\0' == *ptr)
				return FAIL;

			if (SUCCEED != is_macro_char(*ptr))
				return FAIL;
		}

		/* empty macro name */
		if (3 == ptr - macro)
			return FAIL;
	}
	else if ('?' == macro[2])
	{
		zbx_token_t	expr_token;

		if (0 == (token_search & ZBX_TOKEN_SEARCH_EXPRESSION_MACRO))
			return FAIL;

		if (SUCCEED != token_parse_expression_macro(expression, macro + 1, token_search, &expr_token))
			return FAIL;

		ptr = expression + expr_token.loc.r;
	}
	else
	{
		zbx_strloc_t	loc;

		if (SUCCEED != token_parse_macro_name(expression, macro + 2, &loc))
			return FAIL;

		if ('}' != expression[loc.r + 1])
			return FAIL;

		ptr = expression + loc.r + 1;
	}

	/* Determine the token type.                                                   */
	/* Nested macros formats:                                                      */
	/*               low-level discovery function macros  {{#MACRO}.function()}    */
	/*               function macros                      {{MACRO}.function()}     */
	/*               simple macros                        {{MACRO}:key.function()} */
	if ('.' == ptr[1])
	{
		return token_parse_func_macro(expression, macro, ptr + 2, token, '#' == macro[2] ?
				ZBX_TOKEN_LLD_FUNC_MACRO : ZBX_TOKEN_FUNC_MACRO);
	}
	else if (0 != (token_search & ZBX_TOKEN_SEARCH_SIMPLE_MACRO) && '#' != macro[2] && ':' == ptr[1])
		return token_parse_simple_macro_key(expression, macro, ptr + 2, token);

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: finds token {} inside expression starting at specified position   *
 *          also searches for reference if requested                          *
 *                                                                            *
 * Parameters: expression   - [IN] the expression                             *
 *             pos          - [IN] the starting position                      *
 *             token        - [OUT] the token data                            *
 *             token_search - [IN] specify if references will be searched     *
 *                                                                            *
 * Return value: SUCCEED - the token was parsed successfully                  *
 *               FAIL    - expression does not contain valid token.           *
 *                                                                            *
 * Comments: The token field locations are specified as offsets from the      *
 *           beginning of the expression.                                     *
 *                                                                            *
 *           Simply iterating through tokens can be done with:                *
 *                                                                            *
 *           zbx_token_t token = {0};                                         *
 *                                                                            *
 *           while (SUCCEED == zbx_token_find(expression, token.loc.r + 1,    *
 *                       &token))                                             *
 *           {                                                                *
 *                   process_token(expression, &token);                       *
 *           }                                                                *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_find(const char *expression, int pos, zbx_token_t *token, zbx_token_search_t token_search)
{
	int		ret = FAIL;
	const char	*ptr = expression + pos, *dollar = ptr;

	while (SUCCEED != ret)
	{
		int	 quoted = 0;

		/* skip macros in string constants when looking for functionid */
		for (; '{' != *ptr || 0 != quoted; ptr++)
		{
			if ('\0' == *ptr)
				break;

			if (0 != (token_search & ZBX_TOKEN_SEARCH_FUNCTIONID))
			{
				switch (*ptr)
				{
					case '\\':
						if (0 != quoted)
						{
							if ('\0' == *(++ptr))
								return FAIL;
						}
						break;
					case '"':
						quoted = !quoted;
						break;
				}
			}
		}

		if (0 != (token_search & ZBX_TOKEN_SEARCH_REFERENCES))
		{
			while (NULL != (dollar = strchr(dollar, '$')) && ptr > dollar)
			{
				if (0 == isdigit(dollar[1]))
				{
					dollar++;
					continue;
				}

				token->data.reference.index = dollar[1] - '0';
				token->type = ZBX_TOKEN_REFERENCE;
				token->loc.l = dollar - expression;
				token->loc.r = token->loc.l + 1;
				return SUCCEED;
			}

			if (NULL == dollar)
				token_search &= ~ZBX_TOKEN_SEARCH_REFERENCES;
		}

		if ('\0' == *ptr)
			return FAIL;

		if ('\0' == ptr[1])
			return FAIL;

		switch (ptr[1])
		{
			case '$':
				ret = token_parse_user_macro(expression, ptr, token);
				break;
			case '#':
				ret = token_parse_lld_macro(expression, ptr, token);
				break;
			case '?':
				if (0 != (token_search & ZBX_TOKEN_SEARCH_EXPRESSION_MACRO))
					ret = token_parse_expression_macro(expression, ptr, token_search, token);
				break;
			case '{':
				ret = token_parse_nested_macro(expression, ptr, token_search, token);
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (SUCCEED == (ret = token_parse_objectid(expression, ptr, token)))
					break;
				ZBX_FALLTHROUGH;
			default:
				if (SUCCEED != (ret = token_parse_macro(expression, ptr, token)) &&
						0 != (token_search & ZBX_TOKEN_SEARCH_SIMPLE_MACRO))
				{
					ret = token_parse_simple_macro(expression, ptr, token);
				}
		}

		ptr++;
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: public wrapper for token_parse_user_macro() function              *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_parse_user_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	return token_parse_user_macro(expression, macro, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: public wrapper for token_parse_macro() function                   *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_parse_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	return token_parse_macro(expression, macro, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: public wrapper for token_parse_objectid() function                *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_parse_objectid(const char *expression, const char *macro, zbx_token_t *token)
{
	return token_parse_objectid(expression, macro, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: public wrapper for token_parse_lld_macro() function               *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_parse_lld_macro(const char *expression, const char *macro, zbx_token_t *token)
{
	return token_parse_lld_macro(expression, macro, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: public wrapper for token_parse_nested_macro() function            *
 *                                                                            *
 ******************************************************************************/
int	zbx_token_parse_nested_macro(const char *expression, const char *macro, int simple_macro_find,
		zbx_token_t *token)
{
	return token_parse_nested_macro(expression, macro, simple_macro_find, token);
}

/******************************************************************************
 *                                                                            *
 * Purpose: count calculated item (prototype) formula characters that can be  *
 *          skipped without the risk of missing a function                    *
 *                                                                            *
 ******************************************************************************/
static size_t	zbx_no_function(const char *expr)
{
	const char	*ptr = expr;
	int		inside_quote = 0, len, c_l, c_r;
	zbx_token_t	token;

	while ('\0' != *ptr)
	{
		switch  (*ptr)
		{
			case '\\':
				if (0 != inside_quote)
					ptr++;
				break;
			case '"':
				inside_quote = !inside_quote;
				ptr++;
				continue;
		}

		if (inside_quote)
		{
			if ('\0' == *ptr)
				break;
			ptr++;
			continue;
		}

		if ('{' == *ptr && '$' == *(ptr + 1) && SUCCEED == zbx_user_macro_parse(ptr, &len, &c_l, &c_r, NULL))
		{
			ptr += len + 1;	/* skip to the position after user macro */
		}
		else if ('{' == *ptr && '{' == *(ptr + 1) && '#' == *(ptr + 2) &&
				SUCCEED == token_parse_nested_macro(ptr, ptr, 0, &token))
		{
			ptr += token.loc.r - token.loc.l + 1;
		}
		else if (SUCCEED != is_function_char(*ptr))
		{
			ptr++;	/* skip one character which cannot belong to function name */
		}
		else if ((0 == strncmp("and", ptr, len = ZBX_CONST_STRLEN("and")) ||
				0 == strncmp("not", ptr, len = ZBX_CONST_STRLEN("not")) ||
				0 == strncmp("or", ptr, len = ZBX_CONST_STRLEN("or"))) &&
				NULL != strchr("()" ZBX_WHITESPACE, ptr[len]))
		{
			ptr += len;	/* skip to the position after and/or/not operator */
		}
		else if (ptr > expr && 0 != isdigit(*(ptr - 1)) && NULL != strchr(ZBX_UNIT_SYMBOLS, *ptr))
		{
			ptr++;	/* skip unit suffix symbol if it's preceded by a digit */
		}
		else
			break;
	}

	return ptr - expr;
}

/******************************************************************************
 *                                                                            *
 * Purpose: find the location of the next function and its parameters in      *
 *          calculated item (prototype) formula                               *
 *                                                                            *
 * Parameters: expr          - [IN] string to parse                           *
 *             func_pos      - [OUT] function position in the string          *
 *             par_l         - [OUT] position of the opening parenthesis      *
 *             par_r         - [OUT] position of the closing parenthesis      *
 *             error         - [OUT] error message                            *
 *             max_error_len - [IN] error size                                *
 *                                                                            *
 * Return value: SUCCEED - function was found at func_pos                     *
 *               FAIL    - there are no functions in the expression           *
 *                                                                            *
 ******************************************************************************/
int	zbx_function_find(const char *expr, size_t *func_pos, size_t *par_l, size_t *par_r, char *error,
		int max_error_len)
{
	const char	*ptr;

	for (ptr = expr; '\0' != *ptr; ptr += *par_l)
	{
		/* skip the part of expression that is definitely not a function */
		ptr += zbx_no_function(ptr);
		*par_r = 0;

		/* try to validate function candidate */
		if (SUCCEED != zbx_function_validate(ptr, par_l, par_r, error, max_error_len))
		{
			if (*par_l > *par_r)
				return FAIL;

			continue;
		}

		*func_pos = ptr - expr;
		*par_l += *func_pos;
		*par_r += *func_pos;
		return SUCCEED;
	}

	zbx_snprintf(error, max_error_len, "Incorrect function expression: %s", expr);

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check if pattern matches the specified value                      *
 *                                                                            *
 * Parameters: value    - [IN] the value to match                             *
 *             pattern  - [IN] the pattern to match                           *
 *             op       - [IN] the matching operator                          *
 *                                                                            *
 * Return value: SUCCEED - matches, FAIL - otherwise                          *
 *                                                                            *
 ******************************************************************************/
int	zbx_strmatch_condition(const char *value, const char *pattern, unsigned char op)
{
	int	ret = FAIL;

	switch (op)
	{
		case CONDITION_OPERATOR_EQUAL:
			if (0 == strcmp(value, pattern))
				ret = SUCCEED;
			break;
		case CONDITION_OPERATOR_NOT_EQUAL:
			if (0 != strcmp(value, pattern))
				ret = SUCCEED;
			break;
		case CONDITION_OPERATOR_LIKE:
			if (NULL != strstr(value, pattern))
				ret = SUCCEED;
			break;
		case CONDITION_OPERATOR_NOT_LIKE:
			if (NULL == strstr(value, pattern))
				ret = SUCCEED;
			break;
	}

	return ret;
}

int	zbx_uint64match_condition(zbx_uint64_t value, zbx_uint64_t pattern, unsigned char op)
{
	int	ret = FAIL;

	switch (op)
	{
		case CONDITION_OPERATOR_EQUAL:
			if (value == pattern)
				ret = SUCCEED;
			break;
		case CONDITION_OPERATOR_NOT_EQUAL:
			if (value != pattern)
				ret = SUCCEED;
			break;
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse a number like "12.345"                                      *
 *                                                                            *
 * Parameters: number - [IN] start of number                                  *
 *             len    - [OUT] length of parsed number                         *
 *                                                                            *
 * Return value: SUCCEED - the number was parsed successfully                 *
 *               FAIL    - invalid number                                     *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *           The token field locations are specified as offsets from the      *
 *           beginning of the expression.                                     *
 *                                                                            *
 ******************************************************************************/
int	zbx_number_parse(const char *number, int *len)
{
	int	digits = 0, dots = 0;

	*len = 0;

	while (1)
	{
		if (0 != isdigit(number[*len]))
		{
			(*len)++;
			digits++;
			continue;
		}

		if ('.' == number[*len])
		{
			(*len)++;
			dots++;
			continue;
		}

		if ('e' == number[*len] || 'E' == number[*len])
		{
			(*len)++;

			if ('-' == number[*len] || '+' == number[*len])
				(*len)++;

			if (0 == isdigit(number[*len]))
				return FAIL;

			while (0 != isdigit(number[++(*len)]));

			if ('.' == number[*len] ||'e' == number[*len] || 'E' == number[*len])
				return FAIL;
		}

		if (1 > digits || 1 < dots)
			return FAIL;

		return SUCCEED;
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse a suffixed number like "12.345K"                            *
 *                                                                            *
 * Parameters: number - [IN] start of number                                  *
 *             len    - [OUT] length of parsed number                         *
 *                                                                            *
 * Return value: SUCCEED - the number was parsed successfully                 *
 *               FAIL    - invalid number                                     *
 *                                                                            *
 * Comments: !!! Don't forget to sync the code with PHP !!!                   *
 *           The token field locations are specified as offsets from the      *
 *           beginning of the expression.                                     *
 *                                                                            *
 ******************************************************************************/
int	zbx_suffixed_number_parse(const char *number, int *len)
{
	if (FAIL == zbx_number_parse(number, len))
		return FAIL;

	if (0 != isalpha(number[*len]) && NULL != strchr(ZBX_UNIT_SYMBOLS, number[*len]))
		(*len)++;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: find number of parameters in parameter list                       *
 *                                                                            *
 * Parameters:                                                                *
 *      p - [IN] parameter list                                               *
 *                                                                            *
 * Return value: number of parameters (starting from 1) or                    *
 *               0 if syntax error                                            *
 *                                                                            *
 * Comments:  delimiter for parameters is ','. Empty parameter list or a list *
 *            containing only spaces is handled as having one empty parameter *
 *            and 1 is returned.                                              *
 *                                                                            *
 ******************************************************************************/
int	num_param(const char *p)
{
/* 0 - init, 1 - inside quoted param, 2 - inside unquoted param */
	int	ret = 1, state, array;

	if (p == NULL)
		return 0;

	for (state = 0, array = 0; '\0' != *p; p++)
	{
		switch (state) {
		/* Init state */
		case 0:
			if (',' == *p)
			{
				if (0 == array)
					ret++;
			}
			else if ('"' == *p)
				state = 1;
			else if ('[' == *p)
			{
				if (0 == array)
					array = 1;
				else
					return 0;	/* incorrect syntax: multi-level array */
			}
			else if (']' == *p && 0 != array)
			{
				array = 0;

				while (' ' == p[1])	/* skip trailing spaces after closing ']' */
					p++;

				if (',' != p[1] && '\0' != p[1])
					return 0;	/* incorrect syntax */
			}
			else if (']' == *p && 0 == array)
				return 0;		/* incorrect syntax */
			else if (' ' != *p)
				state = 2;
			break;
		/* Quoted */
		case 1:
			if ('"' == *p)
			{
				while (' ' == p[1])	/* skip trailing spaces after closing quotes */
					p++;

				if (',' != p[1] && '\0' != p[1] && (0 == array || ']' != p[1]))
					return 0;	/* incorrect syntax */

				state = 0;
			}
			else if ('\\' == *p && '"' == p[1])
				p++;
			break;
		/* Unquoted */
		case 2:
			if (',' == *p || (']' == *p && 0 != array))
			{
				p--;
				state = 0;
			}
			else if (']' == *p && 0 == array)
				return 0;		/* incorrect syntax */
			break;
		}
	}

	/* missing terminating '"' character */
	if (state == 1)
		return 0;

	/* missing terminating ']' character */
	if (array != 0)
		return 0;

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return parameter by index (num) from parameter list (param)       *
 *                                                                            *
 * Parameters:                                                                *
 *      p       - [IN]  parameter list                                        *
 *      num     - [IN]  requested parameter index                             *
 *      buf     - [OUT] pointer of output buffer                              *
 *      max_len - [IN]  size of output buffer                                 *
 *      type    - [OUT] parameter type (may be NULL)                          *
 *                                                                            *
 * Return value:                                                              *
 *      1 - requested parameter missing or buffer overflow                    *
 *      0 - requested parameter found (value - 'buf' can be empty string)     *
 *                                                                            *
 * Comments:  delimiter for parameters is ','                                 *
 *                                                                            *
 ******************************************************************************/
int	get_param(const char *p, int num, char *buf, size_t max_len, zbx_request_parameter_type_t *type)
{
#define ZBX_ASSIGN_PARAM				\
{							\
	if (buf_i == max_len)				\
		return 1;	/* buffer overflow */	\
	buf[buf_i++] = *p;				\
}

	int	state;	/* 0 - init, 1 - inside quoted param, 2 - inside unquoted param */
	int	array, idx = 1;
	size_t	buf_i = 0;

	if (NULL != type)
		*type = REQUEST_PARAMETER_TYPE_UNDEFINED;

	if (0 == max_len)
		return 1;	/* buffer overflow */

	max_len--;	/* '\0' */

	for (state = 0, array = 0; '\0' != *p && idx <= num; p++)
	{
		switch (state)
		{
			/* init state */
			case 0:
				if (',' == *p)
				{
					if (0 == array)
						idx++;
					else if (idx == num)
						ZBX_ASSIGN_PARAM;
				}
				else if ('"' == *p)
				{
					state = 1;

					if (idx == num)
					{
						if (NULL != type && REQUEST_PARAMETER_TYPE_UNDEFINED == *type)
							*type = REQUEST_PARAMETER_TYPE_STRING;

						if (0 != array)
							ZBX_ASSIGN_PARAM;
					}
				}
				else if ('[' == *p)
				{
					if (idx == num)
					{
						if (NULL != type && REQUEST_PARAMETER_TYPE_UNDEFINED == *type)
							*type = REQUEST_PARAMETER_TYPE_ARRAY;

						if (0 != array)
							ZBX_ASSIGN_PARAM;
					}
					array++;
				}
				else if (']' == *p && 0 != array)
				{
					array--;
					if (0 != array && idx == num)
						ZBX_ASSIGN_PARAM;

					/* skip spaces */
					while (' ' == p[1])
						p++;

					if (',' != p[1] && '\0' != p[1] && (0 == array || ']' != p[1]))
						return 1;	/* incorrect syntax */
				}
				else if (' ' != *p)
				{
					if (idx == num)
					{
						if (NULL != type && REQUEST_PARAMETER_TYPE_UNDEFINED == *type)
							*type = REQUEST_PARAMETER_TYPE_STRING;

						ZBX_ASSIGN_PARAM;
					}

					state = 2;
				}
				break;
			case 1:
				/* quoted */

				if ('"' == *p)
				{
					if (0 != array && idx == num)
						ZBX_ASSIGN_PARAM;

					/* skip spaces */
					while (' ' == p[1])
						p++;

					if (',' != p[1] && '\0' != p[1] && (0 == array || ']' != p[1]))
						return 1;	/* incorrect syntax */

					state = 0;
				}
				else if ('\\' == *p && '"' == p[1])
				{
					if (idx == num && 0 != array)
						ZBX_ASSIGN_PARAM;

					p++;

					if (idx == num)
						ZBX_ASSIGN_PARAM;
				}
				else if (idx == num)
					ZBX_ASSIGN_PARAM;
				break;
			case 2:
				/* unquoted */

				if (',' == *p || (']' == *p && 0 != array))
				{
					p--;
					state = 0;
				}
				else if (idx == num)
					ZBX_ASSIGN_PARAM;
				break;
		}

		if (idx > num)
			break;
	}
#undef ZBX_ASSIGN_PARAM

	/* missing terminating '"' character */
	if (1 == state)
		return 1;

	/* missing terminating ']' character */
	if (0 != array)
		return 1;

	buf[buf_i] = '\0';

	if (idx >= num)
		return 0;

	return 1;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return length of the parameter by index (num)                     *
 *          from parameter list (param)                                       *
 *                                                                            *
 * Parameters:                                                                *
 *      p   - [IN]  parameter list                                            *
 *      num - [IN]  requested parameter index                                 *
 *      sz  - [OUT] length of requested parameter                             *
 *                                                                            *
 * Return value:                                                              *
 *      1 - requested parameter missing                                       *
 *      0 - requested parameter found                                         *
 *          (for first parameter result is always 0)                          *
 *                                                                            *
 * Comments: delimiter for parameters is ','                                  *
 *                                                                            *
 ******************************************************************************/
static int	get_param_len(const char *p, int num, size_t *sz)
{
/* 0 - init, 1 - inside quoted param, 2 - inside unquoted param */
	int	state, array, idx = 1;

	*sz = 0;

	for (state = 0, array = 0; '\0' != *p && idx <= num; p++)
	{
		switch (state) {
		/* Init state */
		case 0:
			if (',' == *p)
			{
				if (0 == array)
					idx++;
				else if (idx == num)
					(*sz)++;
			}
			else if ('"' == *p)
			{
				state = 1;
				if (0 != array && idx == num)
					(*sz)++;
			}
			else if ('[' == *p)
			{
				if (0 != array && idx == num)
					(*sz)++;
				array++;
			}
			else if (']' == *p && 0 != array)
			{
				array--;
				if (0 != array && idx == num)
					(*sz)++;

				/* skip spaces */
				while (' ' == p[1])
					p++;

				if (',' != p[1] && '\0' != p[1] && (0 == array || ']' != p[1]))
					return 1;	/* incorrect syntax */
			}
			else if (' ' != *p)
			{
				if (idx == num)
					(*sz)++;
				state = 2;
			}
			break;
		/* Quoted */
		case 1:
			if ('"' == *p)
			{
				if (0 != array && idx == num)
					(*sz)++;

				/* skip spaces */
				while (' ' == p[1])
					p++;

				if (',' != p[1] && '\0' != p[1] && (0 == array || ']' != p[1]))
					return 1;	/* incorrect syntax */

				state = 0;
			}
			else if ('\\' == *p && '"' == p[1])
			{
				if (idx == num && 0 != array)
					(*sz)++;

				p++;

				if (idx == num)
					(*sz)++;
			}
			else if (idx == num)
				(*sz)++;
			break;
		/* Unquoted */
		case 2:
			if (',' == *p || (']' == *p && 0 != array))
			{
				p--;
				state = 0;
			}
			else if (idx == num)
				(*sz)++;
			break;
		}

		if (idx > num)
			break;
	}

	/* missing terminating '"' character */
	if (state == 1)
		return 1;

	/* missing terminating ']' character */
	if (array != 0)
		return 1;

	if (idx >= num)
		return 0;

	return 1;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return parameter by index (num) from parameter list (param)       *
 *                                                                            *
 * Parameters:                                                                *
 *      p    - [IN] parameter list                                            *
 *      num  - [IN] requested parameter index                                 *
 *      type - [OUT] parameter type (may be NULL)                             *
 *                                                                            *
 * Return value:                                                              *
 *      NULL - requested parameter missing                                    *
 *      otherwise - requested parameter                                       *
 *          (for first parameter result is not NULL)                          *
 *                                                                            *
 * Comments:  delimiter for parameters is ','                                 *
 *                                                                            *
 ******************************************************************************/
char	*get_param_dyn(const char *p, int num, zbx_request_parameter_type_t *type)
{
	char	*buf = NULL;
	size_t	sz;

	if (0 != get_param_len(p, num, &sz))
		return buf;

	buf = (char *)zbx_malloc(buf, sz + 1);

	if (0 != get_param(p, num, buf, sz + 1, type))
		zbx_free(buf);

	return buf;
}

/******************************************************************************
 *                                                                            *
 * Purpose: replaces an item key, SNMP OID or their parameters when callback  *
 *          function returns a new string                                     *
 *                                                                            *
 * Comments: auxiliary function for replace_key_params_dyn()                  *
 *                                                                            *
 ******************************************************************************/
static int	replace_key_param(char **data, int key_type, size_t l, size_t *r, int level, int num, int quoted,
		replace_key_param_f cb, void *cb_data)
{
	char	c = (*data)[*r], *param = NULL;
	int	ret;

	(*data)[*r] = '\0';
	ret = cb(*data + l, key_type, level, num, quoted, cb_data, &param);
	(*data)[*r] = c;

	if (NULL != param)
	{
		(*r)--;
		zbx_replace_string(data, l, r, param);
		(*r)++;

		zbx_free(param);
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: replaces an item key, SNMP OID or their parameters by using       *
 *          callback function                                                 *
 *                                                                            *
 * Parameters:                                                                *
 *      data      - [IN/OUT] item key or SNMP OID                             *
 *      key_type  - [IN] ZBX_KEY_TYPE_*                                       *
 *      cb        - [IN] callback function                                    *
 *      cb_data   - [IN] callback function custom data                        *
 *      error     - [OUT] error message                                       *
 *      maxerrlen - [IN] error size                                           *
 *                                                                            *
 * Return value: SUCCEED - function executed successfully                     *
 *               FAIL - otherwise, error will contain error message           *
 *                                                                            *
 ******************************************************************************/
int	replace_key_params_dyn(char **data, int key_type, replace_key_param_f cb, void *cb_data, char *error,
		size_t maxerrlen)
{
	typedef enum
	{
		ZBX_STATE_NEW,
		ZBX_STATE_END,
		ZBX_STATE_UNQUOTED,
		ZBX_STATE_QUOTED
	}
	zbx_parser_state_t;

	size_t			i = 0, l = 0;
	int			level = 0, num = 0, ret = SUCCEED;
	zbx_parser_state_t	state = ZBX_STATE_NEW;

	if (ZBX_KEY_TYPE_ITEM == key_type)
	{
		for (; SUCCEED == is_key_char((*data)[i]) && '\0' != (*data)[i]; i++)
			;

		if (0 == i)
			goto clean;

		if ('[' != (*data)[i] && '\0' != (*data)[i])
			goto clean;
	}
	else
	{
		zbx_token_t	token;
		int		len, c_l, c_r;

		while ('\0' != (*data)[i])
		{
			if ('{' == (*data)[i] && '$' == (*data)[i + 1] &&
					SUCCEED == zbx_user_macro_parse(&(*data)[i], &len, &c_l, &c_r, NULL))
			{
				i += len + 1;	/* skip to the position after user macro */
			}
			else if ('{' == (*data)[i] && '{' == (*data)[i + 1] && '#' == (*data)[i + 2] &&
					SUCCEED == token_parse_nested_macro(&(*data)[i], &(*data)[i], 0, &token))
			{
				i += token.loc.r - token.loc.l + 1;
			}
			else if ('[' != (*data)[i])
			{
				i++;
			}
			else
				break;
		}
	}

	ret = replace_key_param(data, key_type, 0, &i, level, num, 0, cb, cb_data);

	for (; '\0' != (*data)[i] && FAIL != ret; i++)
	{
		switch (state)
		{
			case ZBX_STATE_NEW:	/* a new parameter started */
				switch ((*data)[i])
				{
					case ' ':
						break;
					case ',':
						ret = replace_key_param(data, key_type, i, &i, level, num, 0, cb,
								cb_data);
						if (1 == level)
							num++;
						break;
					case '[':
						if (2 == level)
							goto clean;	/* incorrect syntax: multi-level array */
						level++;
						if (1 == level)
							num++;
						break;
					case ']':
						ret = replace_key_param(data, key_type, i, &i, level, num, 0, cb,
								cb_data);
						level--;
						state = ZBX_STATE_END;
						break;
					case '"':
						state = ZBX_STATE_QUOTED;
						l = i;
						break;
					default:
						state = ZBX_STATE_UNQUOTED;
						l = i;
				}
				break;
			case ZBX_STATE_END:	/* end of parameter */
				switch ((*data)[i])
				{
					case ' ':
						break;
					case ',':
						state = ZBX_STATE_NEW;
						if (1 == level)
							num++;
						break;
					case ']':
						if (0 == level)
							goto clean;	/* incorrect syntax: redundant ']' */
						level--;
						break;
					default:
						goto clean;
				}
				break;
			case ZBX_STATE_UNQUOTED:	/* an unquoted parameter */
				if (']' == (*data)[i] || ',' == (*data)[i])
				{
					ret = replace_key_param(data, key_type, l, &i, level, num, 0, cb, cb_data);

					i--;
					state = ZBX_STATE_END;
				}
				break;
			case ZBX_STATE_QUOTED:	/* a quoted parameter */
				if ('"' == (*data)[i] && '\\' != (*data)[i - 1])
				{
					i++;
					ret = replace_key_param(data, key_type, l, &i, level, num, 1, cb, cb_data);
					i--;

					state = ZBX_STATE_END;
				}
				break;
		}
	}
clean:
	if (0 == i || '\0' != (*data)[i] || 0 != level)
	{
		if (NULL != error)
		{
			zbx_snprintf(error, maxerrlen, "Invalid %s at position " ZBX_FS_SIZE_T,
					(ZBX_KEY_TYPE_ITEM == key_type ? "item key" : "SNMP OID"), (zbx_fs_size_t)i);
		}
		ret = FAIL;
	}

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: remove parameter by index (num) from parameter list (param)       *
 *                                                                            *
 * Parameters:                                                                *
 *      param  - parameter list                                               *
 *      num    - requested parameter index                                    *
 *                                                                            *
 * Comments: delimiter for parameters is ','                                  *
 *                                                                            *
 ******************************************************************************/
void	remove_param(char *param, int num)
{
	int	state = 0;	/* 0 - unquoted parameter, 1 - quoted parameter */
	int	idx = 1, skip_char = 0;
	char	*p;

	for (p = param; '\0' != *p; p++)
	{
		switch (state)
		{
			case 0:			/* in unquoted parameter */
				if (',' == *p)
				{
					if (1 == idx && 1 == num)
						skip_char = 1;
					idx++;
				}
				else if ('"' == *p)
					state = 1;
				break;
			case 1:			/* in quoted parameter */
				if ('"' == *p && '\\' != *(p - 1))
					state = 0;
				break;
		}
		if (idx != num && 0 == skip_char)
			*param++ = *p;

		skip_char = 0;
	}

	*param = '\0';
}

/******************************************************************************
 *                                                                            *
 * Purpose: check if string is contained in a list of delimited strings       *
 *                                                                            *
 * Parameters: list      - [IN] strings a,b,ccc,ddd                           *
 *             value     - [IN] value                                         *
 *             len       - [IN] value length                                  *
 *             delimiter - [IN] delimiter                                     *
 *                                                                            *
 * Return value: SUCCEED - string is in the list, FAIL - otherwise            *
 *                                                                            *
 ******************************************************************************/
int	str_n_in_list(const char *list, const char *value, size_t len, char delimiter)
{
	const char	*end;
	size_t		token_len, next = 1;

	while ('\0' != *list)
	{
		if (NULL != (end = strchr(list, delimiter)))
		{
			token_len = end - list;
			next = 1;
		}
		else
		{
			token_len = strlen(list);
			next = 0;
		}

		if (len == token_len && 0 == memcmp(list, value, len))
			return SUCCEED;

		list += token_len + next;
	}

	if (1 == next && 0 == len)
		return SUCCEED;

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check if string is contained in a list of delimited strings       *
 *                                                                            *
 * Parameters: list      - strings a,b,ccc,ddd                                *
 *             value     - value                                              *
 *             delimiter - delimiter                                          *
 *                                                                            *
 * Return value: SUCCEED - string is in the list, FAIL - otherwise            *
 *                                                                            *
 ******************************************************************************/
int	str_in_list(const char *list, const char *value, char delimiter)
{
	return str_n_in_list(list, value, strlen(value), delimiter);
}

/******************************************************************************
 *                                                                            *
 * Purpose: return parameter by index (num) from parameter list (param)       *
 *          to be used for keys: key[param1,param2]                           *
 *                                                                            *
 * Parameters:                                                                *
 *      param   - parameter list                                              *
 *      num     - requested parameter index                                   *
 *      buf     - pointer of output buffer                                    *
 *      max_len - size of output buffer                                       *
 *                                                                            *
 * Return value:                                                              *
 *      1 - requested parameter missing                                       *
 *      0 - requested parameter found (value - 'buf' can be empty string)     *
 *                                                                            *
 * Comments:  delimiter for parameters is ','                                 *
 *                                                                            *
 ******************************************************************************/
int	get_key_param(char *param, int num, char *buf, size_t max_len)
{
	int	ret;
	char	*pl, *pr;

	pl = strchr(param, '[');
	pr = strrchr(param, ']');

	if (NULL == pl || NULL == pr || pl > pr)
		return 1;

	*pr = '\0';
	ret = get_param(pl + 1, num, buf, max_len, NULL);
	*pr = ']';

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: calculate count of parameters from parameter list (param)         *
 *          to be used for keys: key[param1,param2]                           *
 *                                                                            *
 * Parameters:                                                                *
 *      param  - parameter list                                               *
 *                                                                            *
 * Return value: count of parameters                                          *
 *                                                                            *
 * Comments:  delimiter for parameters is ','                                 *
 *                                                                            *
 ******************************************************************************/
int	num_key_param(char *param)
{
	int	ret;
	char	*pl, *pr;

	if (NULL == param)
		return 0;

	pl = strchr(param, '[');
	pr = strrchr(param, ']');

	if (NULL == pl || NULL == pr || pl > pr)
		return 0;

	*pr = '\0';
	ret = num_param(pl + 1);
	*pr = ']';

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: to replace memory block and allocate more memory if needed        *
 *                                                                            *
 * Parameters: data       - [IN/OUT] allocated memory                         *
 *             data_alloc - [IN/OUT] allocated memory size                    *
 *             data_len   - [IN/OUT] used memory size                         *
 *             offset     - [IN] offset of memory block to be replaced        *
 *             sz_to      - [IN] size of block that need to be replaced       *
 *             from       - [IN] what to replace with                         *
 *             sz_from    - [IN] size of new block                            *
 *                                                                            *
 * Return value: once data is replaced offset can become less, bigger or      *
 *               remain unchanged                                             *
 ******************************************************************************/
int	zbx_replace_mem_dyn(char **data, size_t *data_alloc, size_t *data_len, size_t offset, size_t sz_to,
		const char *from, size_t sz_from)
{
	size_t	sz_changed = sz_from - sz_to;

	if (0 != sz_changed)
	{
		char	*to;

		*data_len += sz_changed;

		if (*data_len > *data_alloc)
		{
			while (*data_len > *data_alloc)
				*data_alloc *= 2;

			*data = (char *)zbx_realloc(*data, *data_alloc);
		}

		to = *data + offset;
		memmove(to + sz_from, to + sz_to, *data_len - (to - *data) - sz_from);
	}

	memcpy(*data + offset, from, sz_from);

	return (int)sz_changed;
}

/******************************************************************************
 *                                                                            *
 * Purpose: splits string                                                     *
 *                                                                            *
 * Parameters: src       - [IN] source string                                 *
 *             delimiter - [IN] delimiter                                     *
 *             left      - [IN/OUT] first part of the string                  *
 *             right     - [IN/OUT] second part of the string or NULL, if     *
 *                                  delimiter was not found                   *
 *                                                                            *
 ******************************************************************************/
void	zbx_strsplit(const char *src, char delimiter, char **left, char **right)
{
	char	*delimiter_ptr;

	if (NULL == (delimiter_ptr = strchr(src, delimiter)))
	{
		*left = zbx_strdup(NULL, src);
		*right = NULL;
	}
	else
	{
		size_t	left_size;
		size_t	right_size;

		left_size = (size_t)(delimiter_ptr - src) + 1;
		right_size = strlen(src) - (size_t)(delimiter_ptr - src);

		*left = zbx_malloc(NULL, left_size);
		*right = zbx_malloc(NULL, right_size);

		memcpy(*left, src, left_size - 1);
		(*left)[left_size - 1] = '\0';
		memcpy(*right, delimiter_ptr + 1, right_size);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: Removes spaces from both ends of the string, then unquotes it if  *
 *          double quotation mark is present on both ends of the string. If   *
 *          strip_plus_sign is non-zero, then removes single "+" sign from    *
 *          the beginning of the trimmed and unquoted string.                 *
 *                                                                            *
 *          This function does not guarantee that the resulting string        *
 *          contains numeric value. It is meant to be used for removing       *
 *          "valid" characters from the value that is expected to be numeric  *
 *          before checking if value is numeric.                              *
 *                                                                            *
 * Parameters: str             - [IN/OUT] string for processing               *
 *             strip_plus_sign - [IN] non-zero if "+" should be stripped      *
 *                                                                            *
 ******************************************************************************/
static void	zbx_trim_number(char *str, int strip_plus_sign)
{
	char	*left = str;			/* pointer to the first character */
	char	*right = strchr(str, '\0') - 1; /* pointer to the last character, not including terminating null-char */

	if (left > right)
	{
		/* string is empty before any trimming */
		return;
	}

	while (' ' == *left)
	{
		left++;
	}

	while (' ' == *right && left < right)
	{
		right--;
	}

	if ('"' == *left && '"' == *right && left < right)
	{
		left++;
		right--;
	}

	if (0 != strip_plus_sign && '+' == *left)
	{
		left++;
	}

	if (left > right)
	{
		/* string is empty after trimming */
		*str = '\0';
		return;
	}

	if (str < left)
	{
		while (left <= right)
		{
			*str++ = *left++;
		}
		*str = '\0';
	}
	else
	{
		*(right + 1) = '\0';
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: Removes spaces from both ends of the string, then unquotes it if  *
 *          double quotation mark is present on both ends of the string, then *
 *          removes single "+" sign from the beginning of the trimmed and     *
 *          unquoted string.                                                  *
 *                                                                            *
 *          This function does not guarantee that the resulting string        *
 *          contains integer value. It is meant to be used for removing       *
 *          "valid" characters from the value that is expected to be numeric  *
 *          before checking if value is numeric.                              *
 *                                                                            *
 * Parameters: str - [IN/OUT] string for processing                           *
 *                                                                            *
 ******************************************************************************/
void	zbx_trim_integer(char *str)
{
	zbx_trim_number(str, 1);
}

/******************************************************************************
 *                                                                            *
 * Purpose: Removes spaces from both ends of the string, then unquotes it if  *
 *          double quotation mark is present on both ends of the string.      *
 *                                                                            *
 *          This function does not guarantee that the resulting string        *
 *          contains floating-point number. It is meant to be used for        *
 *          removing "valid" characters from the value that is expected to be *
 *          numeric before checking if value is numeric.                      *
 *                                                                            *
 * Parameters: str - [IN/OUT] string for processing                           *
 *                                                                            *
 ******************************************************************************/
void	zbx_trim_float(char *str)
{
	zbx_trim_number(str, 0);
}

/******************************************************************************
 *                                                                            *
 * Purpose: extracts protocol version from value                              *
 *                                                                            *
 * Parameters:                                                                *
 *     value      - [IN] textual representation of version                    *
 *                                                                            *
 * Return value: The protocol version if it was successfully extracted,       *
 *               otherwise -1                                                 *
 *                                                                            *
 ******************************************************************************/
int	zbx_get_component_version(char *value)
{
	char	*pminor, *ptr;

	if (NULL == (pminor = strchr(value, '.')))
		return FAIL;

	*pminor++ = '\0';

	if (NULL != (ptr = strchr(pminor, '.')))
		*ptr = '\0';

	return ZBX_COMPONENT_VERSION(atoi(value), atoi(pminor));
}

/******************************************************************************
 *                                                                            *
 * Purpose: extracts value from a string, unquoting if necessary              *
 *                                                                            *
 * Parameters:                                                                *
 *    text  - [IN] the text containing value to extract                       *
 *    len   - [IN] length (in bytes) of the value to extract.                 *
 *            It can be 0. It must not exceed length of 'text' string.        *
 *    value - [OUT] the extracted value                                       *
 *                                                                            *
 * Return value: SUCCEED - the value was extracted successfully               *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 * Comments: When unquoting value only " and \ character escapes are accepted.*
 *                                                                            *
 ******************************************************************************/
int	zbx_str_extract(const char *text, size_t len, char **value)
{
	char		*tmp, *out;
	const char	*in;

	tmp = zbx_malloc(NULL, len + 1);

	if (0 == len)
	{
		*tmp = '\0';
		*value = tmp;
		return SUCCEED;
	}

	if ('"' != *text)
	{
		memcpy(tmp, text, len);
		tmp[len] = '\0';
		*value = tmp;
		return SUCCEED;
	}

	if (2 > len)
		goto fail;

	for (out = tmp, in = text + 1; '"' != *in; in++)
	{
		if ((size_t)(in - text) >= len - 1)
			goto fail;

		if ('\\' == *in)
		{
			if ((size_t)(++in - text) >= len - 1)
				goto fail;

			if ('"' != *in && '\\' != *in)
				goto fail;
		}
		*out++ = *in;
	}

	if ((size_t)(in - text) != len - 1)
		goto fail;

	*out = '\0';
	*value = tmp;
	return SUCCEED;
fail:
	zbx_free(tmp);
	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check the item key characters length and, if the length exceeds   *
 *          max allowable characters length, truncate the item key, while     *
 *          maintaining the right square bracket                              *
 *                                                                            *
 * Parameters: key      - [IN] item key for processing                        *
 *             char_max - [IN] item key max characters length                 *
 *             buf      - [IN/OUT] buffer for short version of item key       *
 *             buf_len  - [IN] buffer size for short version of item key      *
 *                                                                            *
 * Return value: The item key that does not exceed passed length              *
 *                                                                            *
 ******************************************************************************/
const char	*zbx_truncate_itemkey(const char *key, const size_t char_max, char *buf, const size_t buf_len)
{
#	define ZBX_SUFFIX	"..."
#	define ZBX_BSUFFIX	"[...]"

	size_t	key_byte_count, key_char_total;
	int	is_bracket = 0;
	char	*bracket_l;

	if (char_max >= (key_char_total = zbx_strlen_utf8(key)))
		return key;

	if (NULL != (bracket_l = strchr(key, '[')))
		is_bracket = 1;

	if (char_max < ZBX_CONST_STRLEN(ZBX_SUFFIX) + 2 * is_bracket)	/* [...] or ... */
		return key;

	if (0 != is_bracket)
	{
		size_t	key_char_count, param_char_count, param_byte_count;

		key_char_count = zbx_charcount_utf8_nbytes(key, bracket_l - key);
		param_char_count = key_char_total - key_char_count;

		if (param_char_count <= ZBX_CONST_STRLEN(ZBX_BSUFFIX))
		{
			if (char_max < param_char_count + ZBX_CONST_STRLEN(ZBX_SUFFIX))
				return key;

			key_byte_count = 1 + zbx_strlen_utf8_nchars(key, char_max - param_char_count -
					ZBX_CONST_STRLEN(ZBX_SUFFIX));
			param_byte_count = 1 + zbx_strlen_utf8_nchars(bracket_l, key_char_count);

			if (buf_len < key_byte_count + ZBX_CONST_STRLEN(ZBX_SUFFIX) + param_byte_count - 1)
				return key;

			key_byte_count = zbx_strlcpy_utf8(buf, key, key_byte_count);
			key_byte_count += zbx_strlcpy_utf8(&buf[key_byte_count], ZBX_SUFFIX, sizeof(ZBX_SUFFIX));
			zbx_strlcpy_utf8(&buf[key_byte_count], bracket_l, param_byte_count);

			return buf;
		}

		if (key_char_count + ZBX_CONST_STRLEN(ZBX_BSUFFIX) > char_max)
		{
			if (char_max <= ZBX_CONST_STRLEN(ZBX_SUFFIX) + ZBX_CONST_STRLEN(ZBX_BSUFFIX))
				return key;

			key_byte_count = 1 + zbx_strlen_utf8_nchars(key, char_max - ZBX_CONST_STRLEN(ZBX_SUFFIX) -
					ZBX_CONST_STRLEN(ZBX_BSUFFIX));

			if (buf_len < key_byte_count + ZBX_CONST_STRLEN(ZBX_SUFFIX) + ZBX_CONST_STRLEN(ZBX_BSUFFIX))
				return key;

			key_byte_count = zbx_strlcpy_utf8(buf, key, key_byte_count);
			key_byte_count += zbx_strlcpy_utf8(&buf[key_byte_count], ZBX_SUFFIX, sizeof(ZBX_SUFFIX));
			zbx_strlcpy_utf8(&buf[key_byte_count], ZBX_BSUFFIX, sizeof(ZBX_BSUFFIX));

			return buf;
		}
	}

	key_byte_count = 1 + zbx_strlen_utf8_nchars(key, char_max - (ZBX_CONST_STRLEN(ZBX_SUFFIX) + is_bracket));

	if (buf_len < key_byte_count + ZBX_CONST_STRLEN(ZBX_SUFFIX) + is_bracket)
		return key;

	key_byte_count = zbx_strlcpy_utf8(buf, key, key_byte_count);
	zbx_strlcpy_utf8(&buf[key_byte_count], ZBX_SUFFIX, sizeof(ZBX_SUFFIX));

	if (0 != is_bracket)
		zbx_strlcpy_utf8(&buf[key_byte_count + ZBX_CONST_STRLEN(ZBX_SUFFIX)], "]", sizeof("]"));

	return buf;

#	undef ZBX_SUFFIX
#	undef ZBX_BSUFFIX
}

/******************************************************************************
 *                                                                            *
 * Purpose: check the value characters length and, if the length exceeds      *
 *          max allowable characters length, truncate the value               *
 *                                                                            *
 * Parameters: val      - [IN] value for processing                           *
 *             char_max - [IN] value max characters length                    *
 *             buf      - [IN/OUT] buffer for short version of value          *
 *             buf_len  - [IN] buffer size for short version of value         *
 *                                                                            *
 * Return value: The value that does not exceed passed length                 *
 *                                                                            *
 ******************************************************************************/
const char	*zbx_truncate_value(const char *val, const size_t char_max, char *buf, const size_t buf_len)
{
#	define ZBX_SUFFIX	"..."

	size_t	key_byte_count;

	if (char_max >= zbx_strlen_utf8(val))
		return val;

	key_byte_count = 1 + zbx_strlen_utf8_nchars(val, char_max - ZBX_CONST_STRLEN(ZBX_SUFFIX));

	if (buf_len < key_byte_count + ZBX_CONST_STRLEN(ZBX_SUFFIX))
		return val;

	key_byte_count = zbx_strlcpy_utf8(buf, val, key_byte_count);
	zbx_strlcpy_utf8(&buf[key_byte_count], ZBX_SUFFIX, sizeof(ZBX_SUFFIX));

	return buf;

#	undef ZBX_SUFFIX
}

/******************************************************************************
 *                                                                            *
 * Purpose: converts double value to string and truncates insignificant       *
 *          precision                                                         *
 *                                                                            *
 * Parameters: buffer - [OUT] the output buffer                               *
 *             size   - [IN] the output buffer size                           *
 *             val    - [IN] double value to be converted                     *
 *                                                                            *
 * Return value: the output buffer with printed value                         *
 *                                                                            *
 ******************************************************************************/
const char	*zbx_print_double(char *buffer, size_t size, double val)
{
	double	ipart;

	if (0.0 == modf(val, &ipart))
	{
		zbx_snprintf(buffer, size, ZBX_FS_DBL64, val);
	}
	else
	{
		zbx_snprintf(buffer, size, "%.15G", val);

		if (atof(buffer) != val)
			zbx_snprintf(buffer, size, ZBX_FS_DBL64, val);
	}

	return buffer;
}

/******************************************************************************
 *                                                                            *
 * Purpose: unquotes valid substring at the specified location                *
 *                                                                            *
 * Parameters: src    - [IN] source string                                    *
 *             left   - [IN] left substring position start)                   *
 *             right  - [IN] right substring position (end)                   *
 *             option - [IN] whether to unquote baskslash or not              *
 *                                                                            *
 * Return value: The unquoted and copied substring.                           *
 *                                                                            *
 ******************************************************************************/
char	*zbx_substr_unquote_opt(const char *src, size_t left, size_t right, int option)
{
	char	*str, *ptr;

	if ('"' == src[left])
	{
		src += left + 1;
		str = zbx_malloc(NULL, right - left);
		ptr = str;

		while ('"' != *src)
		{
			if ('\\' == *src)
			{
				switch (*(++src))
				{
					case '\\':
						*ptr++ = '\\';
						if (ZBX_STRQUOTE_SKIP_BACKSLASH == option)
							continue;
						break;
					case '"':
						*ptr++ = '"';
						break;
					case '\0':
						THIS_SHOULD_NEVER_HAPPEN;
						*ptr = '\0';
						return str;
					default:
						if (ZBX_STRQUOTE_SKIP_BACKSLASH == option)
						{
							*ptr++ = '\\';
							*ptr++ = *src;
						}
				}
			}
			else
				*ptr++ = *src;
			src++;
		}
		*ptr = '\0';
	}
	else
	{
		str = zbx_malloc(NULL, right - left + 2);
		memcpy(str, src + left, right - left + 1);
		str[right - left + 1] = '\0';
	}

	return str;
}

/******************************************************************************
 *                                                                            *
 * Purpose: unquotes valid substring at the specified location                *
 *                                                                            *
 * Parameters: src   - [IN] source string                                     *
 *             left  - [IN] left substring position start)                    *
 *             right - [IN] right substring position (end)                    *
 *                                                                            *
 * Return value: The unquoted and copied substring.                           *
 *                                                                            *
 ******************************************************************************/
char	*zbx_substr_unquote(const char *src, size_t left, size_t right)
{
	return zbx_substr_unquote_opt(src, left, right, ZBX_STRQUOTE_DEFAULT);
}

/******************************************************************************
 *                                                                            *
 * Purpose: extracts substring at the specified location                      *
 *                                                                            *
 * Parameters: src   - [IN] the source string                                 *
 *             left  - [IN] the left substring position 9start)               *
 *             right - [IN] the right substirng position (end)                *
 *                                                                            *
 * Return value: The unquoted and copied substring.                           *
 *                                                                            *
 ******************************************************************************/
char	*zbx_substr(const char *src, size_t left, size_t right)
{
	char	*str;

	str = zbx_malloc(NULL, right - left + 2);
	memcpy(str, src + left, right - left + 1);
	str[right - left + 1] = '\0';

	return str;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return pointer to the next utf-8 character                        *
 *                                                                            *
 * Parameters: str  - [IN] the input string                                   *
 *                                                                            *
 * Return value: A pointer to the next utf-8 character.                       *
 *                                                                            *
 ******************************************************************************/
static const char	*utf8_chr_next(const char *str)
{
	++str;

	while (0x80 == (0xc0 & (unsigned char)*str))
		str++;

	return str;
}

/******************************************************************************
 *                                                                            *
 * Purpose: return pointer to the previous utf-8 character                    *
 *                                                                            *
 * Parameters: str   - [IN] the input string                                  *
 *             start - [IN] the start of the initial string                   *
 *                                                                            *
 * Return value: A pointer to the previous utf-8 character.                   *
 *                                                                            *
 ******************************************************************************/
static char	*utf8_chr_prev(char *str, const char *start)
{
	do
	{
		if (--str < start)
			return NULL;
	}
	while (0x80 == (0xc0 & (unsigned char)*str));

	return str;
}

/******************************************************************************
 *                                                                            *
 * Purpose: checks if string contains utf-8 character                         *
 *                                                                            *
 * Parameters: seq  - [IN] the input string                                   *
 *             c    - [IN] the utf-8 character to look for                    *
 *                                                                            *
 * Return value: SUCCEED - the string contains the specified character        *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
static int	strchr_utf8(const char *seq, const char *c)
{
	size_t	len, c_len;

	if (0 == (c_len = zbx_utf8_char_len(c)))
		return FAIL;

	if (1 == c_len)
		return (NULL == strchr(seq, *c) ? FAIL : SUCCEED);

	/* check for broken utf-8 sequence in character */
	if (c + c_len != utf8_chr_next(c))
		return FAIL;

	while ('\0' != *seq)
	{
		len = (size_t)(utf8_chr_next(seq) - seq);

		if (len == c_len && 0 == memcmp(seq, c, len))
			return SUCCEED;

		seq += len;
	}

	return FAIL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: trim the specified utf-8 characters from the left side of input   *
 *          string                                                            *
 *                                                                            *
 * Parameters: str      - [IN] the input string                               *
 *             charlist - [IN] the characters to trim                         *
 *                                                                            *
 ******************************************************************************/
void	zbx_ltrim_utf8(char *str, const char *charlist)
{
	const char	*next;

	for (next = str; '\0' != *next; next = utf8_chr_next(next))
	{
		if (SUCCEED != strchr_utf8(charlist, next))
			break;
	}

	if (next != str)
	{
		size_t	len;

		if (0 != (len = strlen(next)))
			memmove(str, next, len);

		str[len] = '\0';
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: trim the specified utf-8 characters from the right side of input  *
 *          string                                                            *
 *                                                                            *
 * Parameters: str      - [IN] the input string                               *
 *             charlist - [IN] the characters to trim                         *
 *                                                                            *
 ******************************************************************************/
void	zbx_rtrim_utf8(char *str, const char *charlist)
{
	char	*prev, *last;

	for (last = str + strlen(str), prev = last; NULL != prev; prev = utf8_chr_prev(prev, str))
	{
		if (SUCCEED != strchr_utf8(charlist, prev))
			break;

		if ((last = prev) <= str)
			break;
	}

	*last = '\0';
}
