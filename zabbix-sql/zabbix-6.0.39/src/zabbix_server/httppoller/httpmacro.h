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

#ifndef ZABBIX_HTTPMACRO_H
#define ZABBIX_HTTPMACRO_H

#include "db.h"

typedef struct
{
	DB_HTTPTEST		httptest;
	char			*headers;
	zbx_vector_ptr_pair_t	variables;
	/* httptest macro cache consisting of (key, value) pair array */
	zbx_vector_ptr_pair_t	macros;
}
zbx_httptest_t;

typedef struct
{
	DB_HTTPSTEP		*httpstep;
	zbx_httptest_t		*httptest;

	char			*url;
	char			*headers;
	char			*posts;

	zbx_vector_ptr_pair_t	variables;
}
zbx_httpstep_t;

void	http_variable_urlencode(const char *source, char **result);
int	http_substitute_variables(const zbx_httptest_t *httptest, char **data);
int	http_process_variables(zbx_httptest_t *httptest, zbx_vector_ptr_pair_t *variables, const char *data, char **err_str);

#endif
