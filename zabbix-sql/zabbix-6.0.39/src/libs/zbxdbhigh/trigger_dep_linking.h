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

#ifndef ZABBIX_TRIGGER_DEP_LINKING_H
#define ZABBIX_TRIGGER_DEP_LINKING_H

#include "common.h"
#include "zbxalgo.h"

#define TRIGGER_DEP_SYNC_INSERT_OP 0
#define TRIGGER_DEP_SYNC_UPDATE_OP 1
int	DBsync_template_dependencies_for_triggers(zbx_uint64_t hostid, const zbx_vector_uint64_t *trids, int is_update);

#endif
