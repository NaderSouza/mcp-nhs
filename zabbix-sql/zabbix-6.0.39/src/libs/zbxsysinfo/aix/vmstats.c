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
#include "sysinfo.h"
#include "stats.h"

#define ZBX_MAX_WAIT_VMSTAT	2	/* maximum seconds to wait for vmstat data on the first call */

int	SYSTEM_STAT(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char	*section, *type;
	int	wait = ZBX_MAX_WAIT_VMSTAT;

	if (!VMSTAT_COLLECTOR_STARTED(collector))
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Collector is not started."));
		return SYSINFO_RET_FAIL;
	}

	/* if vmstat data is not available yet wait for collector to gather it */
	if (0 == collector->vmstat.data_available)
	{
		collector->vmstat.enabled = 1;

		while (wait--)
		{
			zbx_sleep(1);
			if (1 == collector->vmstat.data_available)
				break;
		}

		if (0 == collector->vmstat.data_available)
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "No data available in collector."));
			return SYSINFO_RET_FAIL;
		}
	}

	if (2 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	section = get_rparam(request, 0);
	type = get_rparam(request, 1);

	if (NULL == section)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
		return SYSINFO_RET_FAIL;
	}

	if (0 == strcmp(section, "ent"))
	{
		if (1 != request->nparam)
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid number of parameters."));
			return SYSINFO_RET_FAIL;
		}

		SET_DBL_RESULT(result, collector->vmstat.ent);
	}
	else if (NULL == type)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
		return SYSINFO_RET_FAIL;
	}
	else if (0 == strcmp(section, "kthr"))
	{
		if (0 == strcmp(type, "r"))
			SET_DBL_RESULT(result, collector->vmstat.kthr_r);
		else if (0 == strcmp(type, "b"))
			SET_DBL_RESULT(result, collector->vmstat.kthr_b);
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else if (0 == strcmp(section, "page"))
	{
		if (0 == strcmp(type, "fi"))
			SET_DBL_RESULT(result, collector->vmstat.fi);
		else if (0 == strcmp(type, "fo"))
			SET_DBL_RESULT(result, collector->vmstat.fo);
		else if (0 == strcmp(type, "pi"))
			SET_DBL_RESULT(result, collector->vmstat.pi);
		else if (0 == strcmp(type, "po"))
			SET_DBL_RESULT(result, collector->vmstat.po);
		else if (0 == strcmp(type, "fr"))
			SET_DBL_RESULT(result, collector->vmstat.fr);
		else if (0 == strcmp(type, "sr"))
			SET_DBL_RESULT(result, collector->vmstat.sr);
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else if (0 == strcmp(section, "faults"))
	{
		if (0 == strcmp(type, "in"))
			SET_DBL_RESULT(result, collector->vmstat.in);
		else if (0 == strcmp(type, "sy"))
			SET_DBL_RESULT(result, collector->vmstat.sy);
		else if (0 == strcmp(type, "cs"))
			SET_DBL_RESULT(result, collector->vmstat.cs);
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else if (0 == strcmp(section, "cpu"))
	{
		if (0 == strcmp(type, "us"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_us);
		else if (0 == strcmp(type, "sy"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_sy);
		else if (0 == strcmp(type, "id"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_id);
		else if (0 == strcmp(type, "wa"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_wa);
		else if (0 == strcmp(type, "pc"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_pc);
		else if (0 == strcmp(type, "ec"))
			SET_DBL_RESULT(result, collector->vmstat.cpu_ec);
		else if (0 == strcmp(type, "lbusy"))
		{
			if (0 == collector->vmstat.shared_enabled)
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "logical partition type is not \"shared\"."));
				return SYSINFO_RET_FAIL;
			}

			SET_DBL_RESULT(result, collector->vmstat.cpu_lbusy);
		}
		else if (0 == strcmp(type, "app"))
		{
			if (0 == collector->vmstat.shared_enabled)
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "logical partition type is not \"shared\"."));
				return SYSINFO_RET_FAIL;
			}

			if (0 == collector->vmstat.pool_util_authority)
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "pool utilization authority not set."));
				return SYSINFO_RET_FAIL;
			}

			SET_DBL_RESULT(result, collector->vmstat.cpu_app);
		}
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else if (0 == strcmp(section, "disk"))
	{
		if (0 == strcmp(type, "bps"))
			SET_UI64_RESULT(result, collector->vmstat.disk_bps);
		else if (0 == strcmp(type, "tps"))
			SET_DBL_RESULT(result, collector->vmstat.disk_tps);
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else if (0 == strcmp(section, "memory"))
	{
		if (0 == strcmp(type, "avm"))
		{
			if (0 != collector->vmstat.aix52stats)
			{
				SET_UI64_RESULT(result, collector->vmstat.mem_avm);
			}
			else
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "Support for system.stat[memory,avm] was not"
						" compiled in."));
				return SYSINFO_RET_FAIL;
			}
		}
		else if (0 == strcmp(type, "fre"))
			SET_UI64_RESULT(result, collector->vmstat.mem_fre);
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
			return SYSINFO_RET_FAIL;
		}
	}
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
		return SYSINFO_RET_FAIL;
	}

	return SYSINFO_RET_OK;
}
