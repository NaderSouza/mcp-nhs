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

#ifndef ZABBIX_OPERATIONS_H
#define ZABBIX_OPERATIONS_H

#include "dbcache.h"

extern int	CONFIG_TIMEOUT;

void	op_template_add(const DB_EVENT *event, zbx_config_t *cfg, zbx_vector_uint64_t *lnk_templateids);
void	op_template_del(const DB_EVENT *event, zbx_vector_uint64_t *del_templateids);
void	op_groups_add(const DB_EVENT *event,  zbx_config_t *cfg, zbx_vector_uint64_t *groupids);
void	op_groups_del(const DB_EVENT *event, zbx_vector_uint64_t *groupids);
void	op_host_add(const DB_EVENT *event, zbx_config_t *cfg);
void	op_host_del(const DB_EVENT *event);
void	op_host_enable(const DB_EVENT *event, zbx_config_t *cfg);
void	op_host_disable(const DB_EVENT *event, zbx_config_t *cfg);
void	op_host_inventory_mode(const DB_EVENT *event, zbx_config_t *cfg, int inventory_mode);

#endif
