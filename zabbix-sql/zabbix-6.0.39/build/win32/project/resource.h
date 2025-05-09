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

//{{NO_DEPENDENCIES}}
// Microsoft Developer Studio generated include file.
// Used by resource.rc
//
#ifndef _RESOURCE_H_
#define _RESOURCE_H_

#include "..\..\..\include\version.h"

#if defined(ZABBIX_AGENT)
#	include "zabbix_agent_desc.h"
#elif defined(ZABBIX_GET)
#	include "zabbix_get_desc.h"
#elif defined(ZABBIX_SENDER)
#	include "zabbix_sender_desc.h"
#elif defined(ZABBIX_AGENT2)
#	include "zabbix_agent2_desc.h"
#endif

#define VER_FILEVERSION		ZABBIX_VERSION_MAJOR,ZABBIX_VERSION_MINOR,ZABBIX_VERSION_PATCH,ZABBIX_VERSION_RC_NUM
#define VER_FILEVERSION_STR	ZBX_STR(ZABBIX_VERSION_MAJOR) "." ZBX_STR(ZABBIX_VERSION_MINOR) "." \
					ZBX_STR(ZABBIX_VERSION_PATCH) "." ZBX_STR(ZABBIX_VERSION_REVISION) "\0"
#define VER_PRODUCTVERSION	ZABBIX_VERSION_MAJOR,ZABBIX_VERSION_MINOR,ZABBIX_VERSION_PATCH
#define VER_PRODUCTVERSION_STR	ZBX_STR(ZABBIX_VERSION_MAJOR) "." ZBX_STR(ZABBIX_VERSION_MINOR) "." \
					ZBX_STR(ZABBIX_VERSION_PATCH) ZABBIX_VERSION_RC "\0"
#define VER_COMPANYNAME_STR	"Zabbix SIA\0"
#define VER_LEGALCOPYRIGHT_STR	"Copyright (C) 2001-2025 " VER_COMPANYNAME_STR
#define VER_PRODUCTNAME_STR	"Zabbix\0"

// Next default values for new objects
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE	105
#define _APS_NEXT_COMMAND_VALUE		40001
#define _APS_NEXT_CONTROL_VALUE		1000
#define _APS_NEXT_SYMED_VALUE		101
#endif
#endif

#endif	/* _RESOURCE_H_ */
