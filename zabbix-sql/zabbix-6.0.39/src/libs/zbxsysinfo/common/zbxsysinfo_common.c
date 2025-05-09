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

#include "zbxsysinfo_common.h"

#include "sysinfo.h"
#include "log.h"
#include "file.h"
#include "dir.h"
#include "net.h"
#include "dns.h"
#include "system.h"
#include "zabbix_stats.h"
#include "zbxexec.h"

#if !defined(_WINDOWS)
#	define VFS_TEST_FILE "/etc/passwd"
#	define VFS_TEST_REGEXP "root"
#	define VFS_TEST_DIR  "/var/log"
#else
#	define VFS_TEST_FILE "c:\\windows\\win.ini"
#	define VFS_TEST_REGEXP "fonts"
#	define VFS_TEST_DIR  "c:\\windows"
#endif

extern int	CONFIG_TIMEOUT;

static int	ONLY_ACTIVE(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	SYSTEM_RUN(AGENT_REQUEST *request, AGENT_RESULT *result);
static int	SYSTEM_RUN_LOCAL(AGENT_REQUEST *request, AGENT_RESULT *result);

ZBX_METRIC	parameters_common_local[] =
/*	KEY			FLAG		FUNCTION		TEST PARAMETERS */
{
	{"system.run",		CF_HAVEPARAMS,	SYSTEM_RUN_LOCAL, 	"echo test"},
	{NULL}
};

ZBX_METRIC	parameters_common[] =
/*	KEY			FLAG		FUNCTION		TEST PARAMETERS */
{
	{"system.localtime",	CF_HAVEPARAMS,	SYSTEM_LOCALTIME,	"utc"},
	{"system.run",		CF_HAVEPARAMS,	SYSTEM_RUN,		"echo test"},

	{"vfs.file.size",	CF_HAVEPARAMS,	VFS_FILE_SIZE,		VFS_TEST_FILE},
	{"vfs.file.time",	CF_HAVEPARAMS,	VFS_FILE_TIME,		VFS_TEST_FILE ",modify"},
	{"vfs.file.exists",	CF_HAVEPARAMS,	VFS_FILE_EXISTS,	VFS_TEST_FILE},
	{"vfs.file.contents",	CF_HAVEPARAMS,	VFS_FILE_CONTENTS,	VFS_TEST_FILE},
	{"vfs.file.regexp",	CF_HAVEPARAMS,	VFS_FILE_REGEXP,	VFS_TEST_FILE "," VFS_TEST_REGEXP},
	{"vfs.file.regmatch",	CF_HAVEPARAMS,	VFS_FILE_REGMATCH,	VFS_TEST_FILE "," VFS_TEST_REGEXP},
	{"vfs.file.md5sum",	CF_HAVEPARAMS,	VFS_FILE_MD5SUM,	VFS_TEST_FILE},
	{"vfs.file.cksum",	CF_HAVEPARAMS,	VFS_FILE_CKSUM,		VFS_TEST_FILE},
	{"vfs.file.owner",	CF_HAVEPARAMS,	VFS_FILE_OWNER,		VFS_TEST_FILE ",user,name"},
	{"vfs.file.permissions",CF_HAVEPARAMS,	VFS_FILE_PERMISSIONS,	VFS_TEST_FILE},
	{"vfs.file.get",	CF_HAVEPARAMS,	VFS_FILE_GET,		VFS_TEST_FILE},

	{"vfs.dir.size",	CF_HAVEPARAMS,	VFS_DIR_SIZE,		VFS_TEST_DIR},
	{"vfs.dir.count",	CF_HAVEPARAMS,	VFS_DIR_COUNT,		VFS_TEST_DIR},
	{"vfs.dir.get",		CF_HAVEPARAMS,	VFS_DIR_GET,		VFS_TEST_DIR},

	{"net.dns",		CF_HAVEPARAMS,	NET_DNS,		",zabbix.com"},
	{"net.dns.record",	CF_HAVEPARAMS,	NET_DNS_RECORD,		",zabbix.com"},
	{"net.tcp.dns",		CF_HAVEPARAMS,	NET_DNS,		",zabbix.com"}, /* deprecated */
	{"net.tcp.dns.query",	CF_HAVEPARAMS,	NET_DNS_RECORD,		",zabbix.com"}, /* deprecated */
	{"net.tcp.port",	CF_HAVEPARAMS,	NET_TCP_PORT,		",80"},

	{"system.users.num",	0,		SYSTEM_USERS_NUM,	NULL},

	{"log",			CF_HAVEPARAMS,	ONLY_ACTIVE,		"logfile"},
	{"log.count",		CF_HAVEPARAMS,	ONLY_ACTIVE,		"logfile"},
	{"logrt",		CF_HAVEPARAMS,	ONLY_ACTIVE,		"logfile"},
	{"logrt.count",		CF_HAVEPARAMS,	ONLY_ACTIVE,		"logfile"},
	{"eventlog",		CF_HAVEPARAMS,	ONLY_ACTIVE,		"system"},

	{"zabbix.stats",	CF_HAVEPARAMS,	ZABBIX_STATS,		"127.0.0.1,10051"},

	{NULL}
};

static const char	*user_parameter_dir = NULL;

void	set_user_parameter_dir(const char *path)
{
	user_parameter_dir = path;
}

static int	ONLY_ACTIVE(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	ZBX_UNUSED(request);

	SET_MSG_RESULT(result, zbx_strdup(NULL, "Accessible only as active check."));

	return SYSINFO_RET_FAIL;
}

static int	execute_str(const char *command, AGENT_RESULT *result, const char* dir)
{
	int		ret = SYSINFO_RET_FAIL;
	char		*cmd_result = NULL, error[MAX_STRING_LEN];

	if (SUCCEED != zbx_execute(command, &cmd_result, error, sizeof(error), CONFIG_TIMEOUT,
			ZBX_EXIT_CODE_CHECKS_DISABLED, dir))
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, error));
		goto out;
	}

	zbx_rtrim(cmd_result, ZBX_WHITESPACE);

	zabbix_log(LOG_LEVEL_DEBUG, "%s() command:'%s' len:" ZBX_FS_SIZE_T " cmd_result:'%.20s'",
			__func__, command, (zbx_fs_size_t)strlen(cmd_result), cmd_result);

	SET_TEXT_RESULT(result, zbx_strdup(NULL, cmd_result));

	ret = SYSINFO_RET_OK;
out:
	zbx_free(cmd_result);

	return ret;
}

int	EXECUTE_USER_PARAMETER(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	if (1 != request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	return execute_str(get_rparam(request, 0), result, user_parameter_dir);
}

int	EXECUTE_STR(const char *command, AGENT_RESULT *result)
{
	return execute_str(command, result, NULL);
}

int	EXECUTE_DBL(const char *command, AGENT_RESULT *result)
{
	if (SYSINFO_RET_OK != EXECUTE_STR(command, result))
		return SYSINFO_RET_FAIL;

	if (NULL == GET_DBL_RESULT(result))
	{
		zabbix_log(LOG_LEVEL_WARNING, "Remote command [%s] result is not double", command);
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid result. Double is expected."));
		return SYSINFO_RET_FAIL;
	}

	UNSET_RESULT_EXCLUDING(result, AR_DOUBLE);

	return SYSINFO_RET_OK;
}

int	EXECUTE_INT(const char *command, AGENT_RESULT *result)
{
	if (SYSINFO_RET_OK != EXECUTE_STR(command, result))
		return SYSINFO_RET_FAIL;

	if (NULL == GET_UI64_RESULT(result))
	{
		zabbix_log(LOG_LEVEL_WARNING, "Remote command [%s] result is not unsigned integer", command);
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid result. Unsigned integer is expected."));
		return SYSINFO_RET_FAIL;
	}

	UNSET_RESULT_EXCLUDING(result, AR_UINT64);

	return SYSINFO_RET_OK;
}

static int	system_run(AGENT_REQUEST *request, AGENT_RESULT *result, int level)
{
	char	*command, *flag;

	if (2 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	command = get_rparam(request, 0);
	flag = get_rparam(request, 1);

	if (NULL == command || '\0' == *command)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
		return SYSINFO_RET_FAIL;
	}

	zabbix_log(level, "Executing command '%s'", command);

	if (NULL == flag || '\0' == *flag || 0 == strcmp(flag, "wait"))	/* default parameter */
	{
		return EXECUTE_STR(command, result);
	}
	else if (0 == strcmp(flag, "nowait"))
	{
		if (SUCCEED != zbx_execute_nowait(command))
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Cannot execute command."));
			return SYSINFO_RET_FAIL;
		}
	}
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid second parameter."));
		return SYSINFO_RET_FAIL;
	}

	SET_UI64_RESULT(result, 1);

	return SYSINFO_RET_OK;
}

static int	SYSTEM_RUN(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	int	level;

	level = LOG_LEVEL_DEBUG;

	if (0 != CONFIG_LOG_REMOTE_COMMANDS)
		level = LOG_LEVEL_WARNING;

	return system_run(request, result, level);
}

static int	SYSTEM_RUN_LOCAL(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	return system_run(request, result, LOG_LEVEL_DEBUG);
}
