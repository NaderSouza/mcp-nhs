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

#ifndef ZABBIX_SOFTWARE_H
#define ZABBIX_SOFTWARE_H

#include "zbxtypes.h"

#define SW_OS_FULL			"/proc/version"
#define SW_OS_SHORT 			"/proc/version_signature"
#define SW_OS_NAME			"/etc/issue.net"
#define SW_OS_NAME_RELEASE		"/etc/os-release"

#define SW_OS_OPTION_PRETTY_NAME	"PRETTY_NAME"

typedef struct
{
	const char	*name;
	const char	*test_cmd;	/* if this shell command has stdout output, package manager is present */
	const char	*list_cmd;	/* this command lists the installed packages */
	int		(*parser)(const char *line, char *package, size_t max_package_len);	/* for non-standard list (package per line), add a parser function */
}
ZBX_PACKAGE_MANAGER;

#endif	/* ZABBIX_SOFTWARE_H */
