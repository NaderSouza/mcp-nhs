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

#include "cfg.h"

#include "common.h"
#include "log.h"

#if defined(_WINDOWS) || defined(__MINGW32__)
#include <shlwapi.h>
#else
#include <libgen.h>
#endif

extern unsigned char	program_type;

char	*CONFIG_FILE		= NULL;

char	*CONFIG_LOG_TYPE_STR	= NULL;
int	CONFIG_LOG_TYPE		= LOG_TYPE_UNDEFINED;
char	*CONFIG_LOG_FILE	= NULL;
int	CONFIG_LOG_FILE_SIZE	= 1;
int	CONFIG_ALLOW_ROOT	= 0;
int	CONFIG_TIMEOUT		= 3;

static int	__parse_cfg_file(const char *cfg_file, struct cfg_line *cfg, int level, int optional, int strict,
		int noexit);

/******************************************************************************
 *                                                                            *
 * Purpose: see whether a file (e.g., "parameter.conf")                       *
 *          matches a pattern (e.g., "p*.conf")                               *
 *                                                                            *
 * Return value: SUCCEED - file matches a pattern                             *
 *               FAIL - otherwise                                             *
 *                                                                            *
 ******************************************************************************/
static int	match_glob(const char *file, const char *pattern)
{
	const char	*f, *g, *p, *q;

	f = file;
	p = pattern;

	while (1)
	{
		/* corner case */

		if ('\0' == *p)
			return '\0' == *f ? SUCCEED : FAIL;

		/* find a set of literal characters */

		while ('*' == *p)
			p++;

		for (q = p; '\0' != *q && '*' != *q; q++)
			;

		/* if literal characters are at the beginning... */

		if (pattern == p)
		{
#ifdef _WINDOWS
			if (0 != zbx_strncasecmp(f, p, q - p))
#else
			if (0 != strncmp(f, p, q - p))
#endif
				return FAIL;

			f += q - p;
			p = q;

			continue;
		}

		/* if literal characters are at the end... */

		if ('\0' == *q)
		{
			for (g = f; '\0' != *g; g++)
				;

			if (g - f < q - p)
				return FAIL;
#ifdef _WINDOWS
			return 0 == strcasecmp(g - (q - p), p) ? SUCCEED : FAIL;
#else
			return 0 == strcmp(g - (q - p), p) ? SUCCEED : FAIL;
#endif
		}

		/* if literal characters are in the middle... */

		while (1)
		{
			if ('\0' == *f)
				return FAIL;
#ifdef _WINDOWS
			if (0 == zbx_strncasecmp(f, p, q - p))
#else
			if (0 == strncmp(f, p, q - p))
#endif
			{
				f += q - p;
				p = q;

				break;
			}

			f++;
		}
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse a glob like "/usr/local/etc/zabbix_agentd.conf.d/p*.conf"   *
 *          into "/usr/local/etc/zabbix_agentd.conf.d" and "p*.conf" parts    *
 *                                                                            *
 * Parameters: glob    - [IN] glob as specified in Include directive          *
 *             path    - [OUT] parsed path, either directory or file          *
 *             pattern - [OUT] parsed pattern, if path is directory           *
 *                                                                            *
 * Return value: SUCCEED - glob is valid and was parsed successfully          *
 *               FAIL - otherwise                                             *
 *                                                                            *
 ******************************************************************************/
static int	parse_glob(const char *glob, char **path, char **pattern)
{
	const char	*p;

	if (NULL == (p = strchr(glob, '*')))
	{
		*path = zbx_strdup(NULL, glob);
		*pattern = NULL;

		goto trim;
	}

	if (NULL != strchr(p + 1, PATH_SEPARATOR))
	{
		zbx_error("%s: glob pattern should be the last component of the path", glob);
		return FAIL;
	}

	do
	{
		if (glob == p)
		{
			zbx_error("%s: path should be absolute", glob);
			return FAIL;
		}

		p--;
	}
	while (PATH_SEPARATOR != *p);

	*path = zbx_strdup(NULL, glob);
	(*path)[p - glob] = '\0';

	*pattern = zbx_strdup(NULL, p + 1);
trim:
#ifdef _WINDOWS
	if (0 != zbx_rtrim(*path, "\\") && NULL == *pattern)
		*pattern = zbx_strdup(NULL, "*");			/* make sure path is a directory */

	if (':' == (*path)[1] && '\0' == (*path)[2] && '\\' == glob[2])	/* retain backslash for "C:\" */
	{
		(*path)[2] = '\\';
		(*path)[3] = '\0';
	}
#else
	if (0 != zbx_rtrim(*path, "/") && NULL == *pattern)
		*pattern = zbx_strdup(NULL, "*");			/* make sure path is a directory */

	if ('\0' == (*path)[0] && '/' == glob[0])			/* retain forward slash for "/" */
	{
		(*path)[0] = '/';
		(*path)[1] = '\0';
	}
#endif
	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse directory with configuration files                          *
 *                                                                            *
 * Parameters: path    - full path to directory                               *
 *             pattern - pattern that files in the directory should match     *
 *             cfg     - pointer to configuration parameter structure         *
 *             level   - a level of included file                             *
 *             strict  - treat unknown parameters as error                    *
 *             noexit  - on error return FAIL instead of EXIT_FAILURE         *
 *                                                                            *
 * Return value: SUCCEED - parsed successfully                                *
 *               FAIL - error processing directory                            *
 *                                                                            *
 ******************************************************************************/
#ifdef _WINDOWS
static int	parse_cfg_dir(const char *path, const char *pattern, struct cfg_line *cfg, int level, int strict,
		int noexit)
{
	WIN32_FIND_DATAW	find_file_data;
	HANDLE			h_find;
	char 			*find_path = NULL, *file = NULL, *file_name;
	wchar_t			*wfind_path = NULL;
	int			ret = FAIL;

	find_path = zbx_dsprintf(find_path, "%s\\*", path);
	wfind_path = zbx_utf8_to_unicode(find_path);

	if (INVALID_HANDLE_VALUE == (h_find = FindFirstFileW(wfind_path, &find_file_data)))
		goto clean;

	while (0 != FindNextFileW(h_find, &find_file_data))
	{
		if (0 != (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			continue;

		file_name = zbx_unicode_to_utf8(find_file_data.cFileName);

		if (NULL != pattern && SUCCEED != match_glob(file_name, pattern))
		{
			zbx_free(file_name);
			continue;
		}

		file = zbx_dsprintf(file, "%s\\%s", path, file_name);

		zbx_free(file_name);

		if (SUCCEED != __parse_cfg_file(file, cfg, level, ZBX_CFG_FILE_REQUIRED, strict, noexit))
			goto close;
	}

	ret = SUCCEED;
close:
	zbx_free(file);
	FindClose(h_find);
clean:
	zbx_free(wfind_path);
	zbx_free(find_path);

	return ret;
}
#else
static int	parse_cfg_dir(const char *path, const char *pattern, struct cfg_line *cfg, int level, int strict,
		int noexit)
{
	DIR		*dir;
	struct dirent	*d;
	zbx_stat_t	sb;
	char		*file = NULL;
	int		ret = FAIL;

	if (NULL == (dir = opendir(path)))
	{
		zbx_error("%s: %s", path, zbx_strerror(errno));
		goto out;
	}

	while (NULL != (d = readdir(dir)))
	{
		file = zbx_dsprintf(file, "%s/%s", path, d->d_name);

		if (0 != zbx_stat(file, &sb) || 0 == S_ISREG(sb.st_mode))
			continue;

		if (NULL != pattern && SUCCEED != match_glob(d->d_name, pattern))
			continue;

		if (SUCCEED != __parse_cfg_file(file, cfg, level, ZBX_CFG_FILE_REQUIRED, strict, noexit))
			goto close;
	}

	ret = SUCCEED;
close:
	if (0 != closedir(dir))
	{
		zbx_error("%s: %s", path, zbx_strerror(errno));
		ret = FAIL;
	}

	zbx_free(file);
out:
	return ret;
}
#endif

static char	*expand_include_path(char *raw_path)
{
#if defined(_WINDOWS) || defined(__MINGW32__)
	wchar_t	*wraw_path;

	wraw_path = zbx_utf8_to_unicode(raw_path);

	if (TRUE == PathIsRelativeW(wraw_path))
	{
		wchar_t	*wconfig_path, dir_buf[_MAX_DIR];
		char	*dir_utf8, *result = NULL;

		zbx_free(wraw_path);

		wconfig_path = zbx_utf8_to_unicode(CONFIG_FILE);
		_wsplitpath(wconfig_path, NULL, dir_buf, NULL, NULL);

		zbx_free(wconfig_path);

		dir_utf8 = zbx_unicode_to_utf8(dir_buf);
		result = zbx_dsprintf(result, "%s%s", dir_utf8, raw_path);

		zbx_free(raw_path);
		zbx_free(dir_utf8);

		return result;
	}

	zbx_free(wraw_path);
#else
	if ('/' != *raw_path)
	{
		char	*cfg_file, *path;

		cfg_file = zbx_strdup(NULL, CONFIG_FILE);
		path = zbx_dsprintf(NULL, "%s/%s", dirname(cfg_file), raw_path);
		zbx_free(cfg_file);

		zbx_free(raw_path);

		return path;
	}
#endif
	return raw_path;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse "Include=..." line in configuration file                    *
 *                                                                            *
 * Parameters: cfg_file - full name of config file                            *
 *             cfg      - pointer to configuration parameter structure        *
 *             level    - a level of included file                            *
 *             strict   - treat unknown parameters as error                   *
 *             noexit   - on error return FAIL instead of EXIT_FAILURE        *
 *                                                                            *
 * Return value: SUCCEED - parsed successfully                                *
 *               FAIL - error processing object                               *
 *                                                                            *
 ******************************************************************************/
static int	parse_cfg_object(const char *cfg_file, struct cfg_line *cfg, int level, int strict, int noexit)
{
	int		ret = FAIL;
	char		*path = NULL, *pattern = NULL;
	zbx_stat_t	sb;

	if (SUCCEED != parse_glob(cfg_file, &path, &pattern))
		goto clean;

	path = expand_include_path(path);

	if (0 != zbx_stat(path, &sb))
	{
		zbx_error("%s: %s", path, zbx_strerror(errno));
		goto clean;
	}

	if (0 == S_ISDIR(sb.st_mode))
	{
		if (NULL == pattern)
		{
			ret = __parse_cfg_file(path, cfg, level, ZBX_CFG_FILE_REQUIRED, strict, noexit);
			goto clean;
		}

		zbx_error("%s: base path is not a directory", cfg_file);
		goto clean;
	}

	ret = parse_cfg_dir(path, pattern, cfg, level, strict, noexit);
clean:
	zbx_free(pattern);
	zbx_free(path);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse configuration file                                          *
 *                                                                            *
 * Parameters: cfg_file - full name of config file                            *
 *             cfg      - pointer to configuration parameter structure        *
 *             level    - a level of included file                            *
 *             optional - do not treat missing configuration file as error    *
 *             strict   - treat unknown parameters as error                   *
 *             noexit   - on error return FAIL instead of EXIT_FAILURE        *
 *                                                                            *
 * Return value: SUCCEED - parsed successfully                                *
 *               FAIL - error processing config file                          *
 *                                                                            *
 ******************************************************************************/
static int	__parse_cfg_file(const char *cfg_file, struct cfg_line *cfg, int level, int optional, int strict,
		int noexit)
{
#define ZBX_MAX_INCLUDE_LEVEL	10

#define ZBX_CFG_LTRIM_CHARS	"\t "
#define ZBX_CFG_RTRIM_CHARS	ZBX_CFG_LTRIM_CHARS "\r\n"

	FILE		*file;
	int		i, lineno, param_valid;
	char		line[MAX_STRING_LEN + 3], *parameter, *value;
	zbx_uint64_t	var;
	size_t		len;
#ifdef _WINDOWS
	wchar_t		*wcfg_file;
#endif
	if (++level > ZBX_MAX_INCLUDE_LEVEL)
	{
		zbx_error("Recursion detected! Skipped processing of '%s'.", cfg_file);
		return FAIL;
	}

	if (NULL != cfg_file)
	{
#ifdef _WINDOWS
		wcfg_file = zbx_utf8_to_unicode(cfg_file);
		file = _wfopen(wcfg_file, L"r");
		zbx_free(wcfg_file);

		if (NULL == file)
			goto cannot_open;
#else
		if (NULL == (file = fopen(cfg_file, "r")))
			goto cannot_open;
#endif
		for (lineno = 1; NULL != fgets(line, sizeof(line), file); lineno++)
		{
			/* check if line length exceeds limit (max. 2048 bytes) */
			len = strlen(line);
			if (MAX_STRING_LEN < len && NULL == strchr("\r\n", line[MAX_STRING_LEN]))
				goto line_too_long;

			zbx_ltrim(line, ZBX_CFG_LTRIM_CHARS);
			zbx_rtrim(line, ZBX_CFG_RTRIM_CHARS);

			if ('#' == *line || '\0' == *line)
				continue;

			/* we only support UTF-8 characters in the config file */
			if (SUCCEED != zbx_is_utf8(line))
				goto non_utf8;

			parameter = line;
			if (NULL == (value = strchr(line, '=')))
				goto non_key_value;

			*value++ = '\0';

			zbx_rtrim(parameter, ZBX_CFG_RTRIM_CHARS);
			zbx_ltrim(value, ZBX_CFG_LTRIM_CHARS);

			zabbix_log(LOG_LEVEL_DEBUG, "cfg: para: [%s] val [%s]", parameter, value);

			if (0 == strcmp(parameter, "Include"))
			{
				if (FAIL == parse_cfg_object(value, cfg, level, strict, noexit))
				{
					fclose(file);
					goto error;
				}

				continue;
			}

			param_valid = 0;

			for (i = 0; NULL != cfg[i].parameter; i++)
			{
				if (0 != strcmp(cfg[i].parameter, parameter))
					continue;

				param_valid = 1;

				zabbix_log(LOG_LEVEL_DEBUG, "accepted configuration parameter: '%s' = '%s'",
						parameter, value);

				switch (cfg[i].type)
				{
					case TYPE_INT:
						if (FAIL == str2uint64(value, "KMGT", &var))
							goto incorrect_config;

						if (cfg[i].min > var || (0 != cfg[i].max && var > cfg[i].max))
							goto incorrect_config;

						*((int *)cfg[i].variable) = (int)var;
						break;
					case TYPE_STRING_LIST:
						zbx_trim_str_list(value, ',');
						ZBX_FALLTHROUGH;
					case TYPE_STRING:
						*((char **)cfg[i].variable) =
								zbx_strdup(*((char **)cfg[i].variable), value);
						break;
					case TYPE_MULTISTRING:
						zbx_strarr_add((char ***)cfg[i].variable, value);
						break;
					case TYPE_UINT64:
						if (FAIL == str2uint64(value, "KMGT", &var))
							goto incorrect_config;

						if (cfg[i].min > var || (0 != cfg[i].max && var > cfg[i].max))
							goto incorrect_config;

						*((zbx_uint64_t *)cfg[i].variable) = var;
						break;
					case TYPE_CUSTOM:
						if (NULL != cfg[i].variable)
						{
							cfg_custom_parameter_parser_t	*p =
									(cfg_custom_parameter_parser_t*)cfg[i].variable;

							if (SUCCEED != p->cfg_custom_parameter_parser_func(value,
									&cfg[i]))
							{
								goto incorrect_config;
							}

							continue;
						}
						break;
					default:
						assert(0);
				}
			}

			if (0 == param_valid && ZBX_CFG_STRICT == strict)
				goto unknown_parameter;
		}
		fclose(file);
	}

	if (1 != level)	/* skip mandatory parameters check for included files */
		return SUCCEED;

	for (i = 0; NULL != cfg[i].parameter; i++) /* check for mandatory parameters */
	{
		if (PARM_MAND != cfg[i].mandatory)
			continue;

		switch (cfg[i].type)
		{
			case TYPE_INT:
				if (0 == *((int *)cfg[i].variable))
					goto missing_mandatory;
				break;
			case TYPE_STRING:
			case TYPE_STRING_LIST:
				if (NULL == (*(char **)cfg[i].variable))
					goto missing_mandatory;
				break;
			default:
				assert(0);
		}
	}

	return SUCCEED;
cannot_open:
	if (ZBX_CFG_FILE_REQUIRED != optional)
		return SUCCEED;
	zbx_error("cannot open config file \"%s\": %s", cfg_file, zbx_strerror(errno));
	goto error;
line_too_long:
	fclose(file);
	zbx_error("line %d exceeds %d byte length limit in config file \"%s\"", lineno, MAX_STRING_LEN, cfg_file);
	goto error;
non_utf8:
	fclose(file);
	zbx_error("non-UTF-8 character at line %d \"%s\" in config file \"%s\"", lineno, line, cfg_file);
	goto error;
non_key_value:
	fclose(file);
	zbx_error("invalid entry \"%s\" (not following \"parameter=value\" notation) in config file \"%s\", line %d",
			line, cfg_file, lineno);
	goto error;
incorrect_config:
	fclose(file);
	zbx_error("wrong value of \"%s\" in config file \"%s\", line %d", cfg[i].parameter, cfg_file, lineno);
	goto error;
unknown_parameter:
	fclose(file);
	zbx_error("unknown parameter \"%s\" in config file \"%s\", line %d", parameter, cfg_file, lineno);
	goto error;
missing_mandatory:
	zbx_error("missing mandatory parameter \"%s\" in config file \"%s\"", cfg[i].parameter, cfg_file);
error:
	if (0 == noexit)
		exit(EXIT_FAILURE);

	return FAIL;
}

int	parse_cfg_file(const char *cfg_file, struct cfg_line *cfg, int optional, int strict, int noexit)
{
	return __parse_cfg_file(cfg_file, cfg, 0, optional, strict, noexit);
}

int	check_cfg_feature_int(const char *parameter, int value, const char *feature)
{
	if (0 != value)
	{
		zbx_error("\"%s\" configuration parameter cannot be used: Zabbix %s was compiled without %s",
				parameter, get_program_type_string(program_type), feature);
		return FAIL;
	}

	return SUCCEED;
}

int	check_cfg_feature_str(const char *parameter, const char *value, const char *feature)
{
	if (NULL != value)
	{
		zbx_error("\"%s\" configuration parameter cannot be used: Zabbix %s was compiled without %s",
				parameter, get_program_type_string(program_type), feature);
		return FAIL;
	}

	return SUCCEED;
}

void	zbx_addr_free(zbx_addr_t *addr)
{
	zbx_free(addr->ip);
	zbx_free(addr);
}

void	zbx_addr_copy(zbx_vector_ptr_t *addr_to, const zbx_vector_ptr_t *addr_from)
{
	int	j;

	for (j = 0; j < addr_from->values_num; j++)
	{
		const zbx_addr_t	*addr;
		zbx_addr_t		*addr_ptr;

		addr = (const zbx_addr_t *)addr_from->values[j];

		addr_ptr = zbx_malloc(NULL, sizeof(zbx_addr_t));
		addr_ptr->ip = zbx_strdup(NULL, addr->ip);
		addr_ptr->port = addr->port;
		zbx_vector_ptr_append(addr_to, addr_ptr);
	}
}

static int	addr_compare_func(const void *d1, const void *d2)
{
	const zbx_addr_t	*a1 = *(const zbx_addr_t * const *)d1;
	const zbx_addr_t	*a2 = *(const zbx_addr_t * const *)d2;

	ZBX_RETURN_IF_NOT_EQUAL(a1->port, a2->port);

	return strcmp(a1->ip, a2->ip);
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse "ServerActive' parameter value and set destination servers  *
 *          using a callback function                                         *
 *                                                                            *
 ******************************************************************************/
int	zbx_set_data_destination_hosts(char *str, unsigned short port, const char *name, add_serveractive_host_f cb,
		zbx_vector_str_t *hostnames, void *data, char **error)
{
	char			*r, *r_node;
	zbx_vector_ptr_t	addrs, cluster_addrs;
	int			ret = SUCCEED;

	zbx_vector_ptr_create(&addrs);
	zbx_vector_ptr_create(&cluster_addrs);

	do
	{
		if (NULL != (r = strchr(str, ',')))
			*r = '\0';

		do
		{
			zbx_addr_t	*addr;

			if (NULL != (r_node = strchr(str, ';')))
				*r_node = '\0';

			addr = zbx_malloc(NULL, sizeof(zbx_addr_t));
			addr->ip = NULL;

			if (SUCCEED != parse_serveractive_element(str, &addr->ip, &addr->port, port))
			{
				*error = zbx_dsprintf(NULL, "error parsing the \"%s\" parameter: address \"%s\" is "
						"invalid", name, str);
				ret = FAIL;
			}
			else if (FAIL == is_supported_ip(addr->ip) && FAIL == zbx_validate_hostname(addr->ip))
			{
				*error = zbx_dsprintf(NULL, "error parsing the \"%s\" parameter: address \"%s\""
						" is invalid", name, str);
				ret = FAIL;
			}
			else if (SUCCEED == zbx_vector_ptr_search(&addrs, addr, addr_compare_func))
			{
				*error = zbx_dsprintf(NULL, "error parsing the \"%s\" parameter: address \"%s\""
						" specified more than once", name, str);
				ret = FAIL;
			}

			if (NULL != r_node)
			{
				*r_node = ';';
				str = r_node + 1;
			}

			zbx_vector_ptr_append(&cluster_addrs, addr);
			zbx_vector_ptr_append(&addrs, addr);

			if (FAIL == ret)
				goto fail;
		}
		while (NULL != r_node);

		cb(&cluster_addrs, hostnames, data);

		cluster_addrs.values_num = 0;

		if (NULL != r)
		{
			*r = ',';
			str = r + 1;
		}
	}
	while (NULL != r);
fail:
	zbx_vector_ptr_destroy(&cluster_addrs);
	zbx_vector_ptr_clear_ext(&addrs, (zbx_clean_func_t)zbx_addr_free);
	zbx_vector_ptr_destroy(&addrs);

	return ret;
}
