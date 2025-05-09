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

#include "proxy.h"

#include "cfg.h"
#include "db.h"
#include "zbxdbupgrade.h"
#include "log.h"
#include "zbxgetopt.h"
#include "mutexs.h"

#include "sysinfo.h"
#include "zbxmodules.h"

#include "zbxnix.h"
#include "daemon.h"
#include "zbxself.h"

#include "../zabbix_server/dbsyncer/dbsyncer.h"
#include "../zabbix_server/discoverer/discoverer.h"
#include "../zabbix_server/httppoller/httppoller.h"
#include "housekeeper/housekeeper.h"
#include "../zabbix_server/pinger/pinger.h"
#include "../zabbix_server/poller/poller.h"
#include "../zabbix_server/trapper/trapper.h"
#include "../zabbix_server/trapper/proxydata.h"
#include "../zabbix_server/snmptrapper/snmptrapper.h"
#include "proxyconfig/proxyconfig.h"
#include "datasender/datasender.h"
#include "heart/heart.h"
#include "taskmanager/taskmanager.h"
#include "../zabbix_server/selfmon/selfmon.h"
#include "../zabbix_server/vmware/vmware.h"
#include "setproctitle.h"
#include "zbxcrypto.h"
#include "../zabbix_server/preprocessor/preproc_manager.h"
#include "../zabbix_server/preprocessor/preproc_worker.h"
#include "../zabbix_server/availability/avail_manager.h"
#include "zbxvault.h"
#include "sighandler.h"
#include "zbxrtc.h"

#ifdef HAVE_OPENIPMI
#include "../zabbix_server/ipmi/ipmi_manager.h"
#include "../zabbix_server/ipmi/ipmi_poller.h"
#endif

const char	*progname = NULL;
const char	title_message[] = "zabbix_proxy";
const char	syslog_app_name[] = "zabbix_proxy";
const char	*usage_message[] = {
	"[-c config-file]", NULL,
	"[-c config-file]", "-R runtime-option", NULL,
	"-h", NULL,
	"-V", NULL,
	NULL	/* end of text */
};

const char	*help_message[] = {
	"A Zabbix daemon that collects monitoring data from devices and sends it to",
	"Zabbix server.",
	"",
	"Options:",
	"  -c --config config-file        Path to the configuration file",
	"                                 (default: \"" DEFAULT_CONFIG_FILE "\")",
	"  -f --foreground                Run Zabbix proxy in foreground",
	"  -R --runtime-control runtime-option   Perform administrative functions",
	"",
	"    Runtime control options:",
	"      " ZBX_CONFIG_CACHE_RELOAD "        Reload configuration cache",
	"      " ZBX_HOUSEKEEPER_EXECUTE "        Execute the housekeeper",
	"      " ZBX_LOG_LEVEL_INCREASE "=target  Increase log level, affects all processes if",
	"                                   target is not specified",
	"      " ZBX_LOG_LEVEL_DECREASE "=target  Decrease log level, affects all processes if",
	"                                   target is not specified",
	"      " ZBX_SNMP_CACHE_RELOAD "          Reload SNMP cache",
	"      " ZBX_DIAGINFO "=section           Log internal diagnostic information of the",
	"                                 section (historycache, preprocessing, locks) or",
	"                                 everything if section is not specified",
	"      " ZBX_PROF_ENABLE "=target         Enable profiling, affects all processes if",
	"                                   target is not specified",
	"      " ZBX_PROF_DISABLE "=target        Disable profiling, affects all processes if",
	"                                   target is not specified",
	"",
	"      Log level control targets:",
	"        process-type             All processes of specified type",
	"                                 (configuration syncer, data sender, discoverer,",
	"                                 heartbeat sender, history syncer, housekeeper,",
	"                                 http poller, icmp pinger, ipmi manager,",
	"                                 ipmi poller, java poller, poller,",
	"                                 preprocessing manager, preprocessing worker,",
	"                                 self-monitoring, snmp trapper, task manager,",
	"                                 trapper, unreachable poller, vmware collector,",
	"                                 history poller, availability manager, odbc poller)",
	"        process-type,N           Process type and number (e.g., poller,3)",
	"        pid                      Process identifier",
	"",
	"      Profiling control targets:",
	"        process-type             All processes of specified type",
	"                                 (configuration syncer, data sender, discoverer,",
	"                                 heartbeat sender, history syncer, housekeeper,",
	"                                 http poller, icmp pinger, ipmi manager,",
	"                                 ipmi poller, java poller, poller,",
	"                                 preprocessing manager, preprocessing worker,",
	"                                 self-monitoring, snmp trapper, task manager,",
	"                                 trapper, unreachable poller, vmware collector,",
	"                                 history poller, availability manager, odbc poller)",
	"        process-type,N           Process type and number (e.g., history syncer,1)",
	"        pid                      Process identifier",
	"        scope                    Profiling scope",
	"                                 (rwlock, mutex, processing) can be used with process-type",
	"                                 (e.g., history syncer,1,processing)",
	"",
	"  -h --help                      Display this help message",
	"  -V --version                   Display version number",
	"",
	"Some configuration parameter default locations:",
	"  ExternalScripts                \"" DEFAULT_EXTERNAL_SCRIPTS_PATH "\"",
#ifdef HAVE_LIBCURL
	"  SSLCertLocation                \"" DEFAULT_SSL_CERT_LOCATION "\"",
	"  SSLKeyLocation                 \"" DEFAULT_SSL_KEY_LOCATION "\"",
#endif
	"  LoadModulePath                 \"" DEFAULT_LOAD_MODULE_PATH "\"",
	NULL	/* end of text */
};

/* COMMAND LINE OPTIONS */

/* long options */
static struct zbx_option	longopts[] =
{
	{"config",		1,	NULL,	'c'},
	{"foreground",		0,	NULL,	'f'},
	{"runtime-control",	1,	NULL,	'R'},
	{"help",		0,	NULL,	'h'},
	{"version",		0,	NULL,	'V'},
	{NULL}
};

/* short options */
static char	shortopts[] = "c:hVR:f";

/* end of COMMAND LINE OPTIONS */

int		threads_num = 0;
pid_t		*threads = NULL;
static int	*threads_flags;

unsigned char			program_type	= ZBX_PROGRAM_TYPE_PROXY_ACTIVE;

ZBX_THREAD_LOCAL unsigned char	process_type	= ZBX_PROCESS_TYPE_UNKNOWN;
ZBX_THREAD_LOCAL int		process_num	= 0;
ZBX_THREAD_LOCAL int		server_num	= 0;
static sigset_t			orig_mask;

int	CONFIG_PROXYMODE		= ZBX_PROXYMODE_ACTIVE;
int	CONFIG_DATASENDER_FORKS		= 1;
int	CONFIG_DISCOVERER_FORKS		= 1;
int	CONFIG_HOUSEKEEPER_FORKS	= 1;
int	CONFIG_PINGER_FORKS		= 1;
int	CONFIG_POLLER_FORKS		= 5;
int	CONFIG_UNREACHABLE_POLLER_FORKS	= 1;
int	CONFIG_HTTPPOLLER_FORKS		= 1;
int	CONFIG_IPMIPOLLER_FORKS		= 0;
int	CONFIG_TRAPPER_FORKS		= 5;
int	CONFIG_SNMPTRAPPER_FORKS	= 0;
int	CONFIG_JAVAPOLLER_FORKS		= 0;
int	CONFIG_SELFMON_FORKS		= 1;
int	CONFIG_PROXYPOLLER_FORKS	= 0;
int	CONFIG_ESCALATOR_FORKS		= 0;
int	CONFIG_ALERTER_FORKS		= 0;
int	CONFIG_TIMER_FORKS		= 0;
int	CONFIG_HEARTBEAT_FORKS		= 1;
int	CONFIG_COLLECTOR_FORKS		= 0;
int	CONFIG_PASSIVE_FORKS		= 0;
int	CONFIG_ACTIVE_FORKS		= 0;
int	CONFIG_TASKMANAGER_FORKS	= 1;
int	CONFIG_IPMIMANAGER_FORKS	= 0;
int	CONFIG_ALERTMANAGER_FORKS	= 0;
int	CONFIG_PREPROCMAN_FORKS		= 1;
int	CONFIG_PREPROCESSOR_FORKS	= 3;
int	CONFIG_LLDMANAGER_FORKS		= 0;
int	CONFIG_LLDWORKER_FORKS		= 0;
int	CONFIG_ALERTDB_FORKS		= 0;
int	CONFIG_HISTORYPOLLER_FORKS	= 1;	/* for zabbix[proxy_history] internal check */
int	CONFIG_AVAILMAN_FORKS		= 1;
int	CONFIG_SERVICEMAN_FORKS		= 0;
int	CONFIG_TRIGGERHOUSEKEEPER_FORKS	= 0;
int	CONFIG_ODBCPOLLER_FORKS		= 1;
int	CONFIG_HAMANAGER_FORKS		= 0;

int	CONFIG_LISTEN_PORT		= ZBX_DEFAULT_SERVER_PORT;
char	*CONFIG_LISTEN_IP		= NULL;
char	*CONFIG_SOURCE_IP		= NULL;
int	CONFIG_TRAPPER_TIMEOUT		= 300;

int	CONFIG_HOUSEKEEPING_FREQUENCY	= 1;
int	CONFIG_PROXY_LOCAL_BUFFER	= 0;
int	CONFIG_PROXY_OFFLINE_BUFFER	= 1;

int	CONFIG_HEARTBEAT_FREQUENCY	= 60;

int	CONFIG_PROXYCONFIG_FREQUENCY	= SEC_PER_HOUR;
int	CONFIG_PROXYDATA_FREQUENCY	= 1;

int	CONFIG_HISTSYNCER_FORKS		= 4;
int	CONFIG_HISTSYNCER_FREQUENCY	= 1;
int	CONFIG_CONFSYNCER_FORKS		= 1;

int	CONFIG_VMWARE_FORKS		= 0;
int	CONFIG_VMWARE_FREQUENCY		= 60;
int	CONFIG_VMWARE_PERF_FREQUENCY	= 60;
int	CONFIG_VMWARE_TIMEOUT		= 10;

zbx_uint64_t	CONFIG_CONF_CACHE_SIZE		= 8 * ZBX_MEBIBYTE;
zbx_uint64_t	CONFIG_HISTORY_CACHE_SIZE	= 16 * ZBX_MEBIBYTE;
zbx_uint64_t	CONFIG_HISTORY_INDEX_CACHE_SIZE	= 4 * ZBX_MEBIBYTE;
zbx_uint64_t	CONFIG_TRENDS_CACHE_SIZE	= 0;
zbx_uint64_t	CONFIG_TREND_FUNC_CACHE_SIZE	= 0;
zbx_uint64_t	CONFIG_VALUE_CACHE_SIZE		= 0;
zbx_uint64_t	CONFIG_VMWARE_CACHE_SIZE	= 8 * ZBX_MEBIBYTE;
zbx_uint64_t	CONFIG_EXPORT_FILE_SIZE;

int	CONFIG_UNREACHABLE_PERIOD	= 45;
int	CONFIG_UNREACHABLE_DELAY	= 15;
int	CONFIG_UNAVAILABLE_DELAY	= 60;
int	CONFIG_LOG_LEVEL		= LOG_LEVEL_WARNING;
char	*CONFIG_ALERT_SCRIPTS_PATH	= NULL;
char	*CONFIG_EXTERNALSCRIPTS		= NULL;
char	*CONFIG_TMPDIR			= NULL;
char	*CONFIG_FPING_LOCATION		= NULL;
char	*CONFIG_FPING6_LOCATION		= NULL;
char	*CONFIG_DBHOST			= NULL;
char	*CONFIG_DBNAME			= NULL;
char	*CONFIG_DBSCHEMA		= NULL;
char	*CONFIG_DBUSER			= NULL;
char	*CONFIG_DBPASSWORD		= NULL;
int	CONFIG_DBREAD_ONLY_RECOVERABLE	= 0;
char	*CONFIG_VAULTTOKEN		= NULL;
char	*CONFIG_VAULTURL		= NULL;
char	*CONFIG_VAULTDBPATH		= NULL;
char	*CONFIG_DBSOCKET		= NULL;
char	*CONFIG_DB_TLS_CONNECT		= NULL;
char	*CONFIG_DB_TLS_CERT_FILE	= NULL;
char	*CONFIG_DB_TLS_KEY_FILE		= NULL;
char	*CONFIG_DB_TLS_CA_FILE		= NULL;
char	*CONFIG_DB_TLS_CIPHER		= NULL;
char	*CONFIG_DB_TLS_CIPHER_13	= NULL;
char	*CONFIG_EXPORT_DIR		= NULL;
char	*CONFIG_EXPORT_TYPE		= NULL;
int	CONFIG_DBPORT			= 0;
int	CONFIG_ALLOW_UNSUPPORTED_DB_VERSIONS = 0;
int	CONFIG_ENABLE_REMOTE_COMMANDS	= 0;
int	CONFIG_LOG_REMOTE_COMMANDS	= 0;
int	CONFIG_UNSAFE_USER_PARAMETERS	= 0;

char	*CONFIG_SERVER			= NULL;
int	CONFIG_SERVER_PORT;
char	*CONFIG_HOSTNAME		= NULL;
char	*CONFIG_HOSTNAME_ITEM		= NULL;

char	*CONFIG_SNMPTRAP_FILE		= NULL;

char	*CONFIG_JAVA_GATEWAY		= NULL;
int	CONFIG_JAVA_GATEWAY_PORT	= ZBX_DEFAULT_GATEWAY_PORT;

char	*CONFIG_SSH_KEY_LOCATION	= NULL;

int	CONFIG_LOG_SLOW_QUERIES		= 0;	/* ms; 0 - disable */

/* zabbix server startup time */
int	CONFIG_SERVER_STARTUP_TIME	= 0;

char	*CONFIG_LOAD_MODULE_PATH	= NULL;
char	**CONFIG_LOAD_MODULE		= NULL;

char	*CONFIG_USER			= NULL;

/* web monitoring */
char	*CONFIG_SSL_CA_LOCATION		= NULL;
char	*CONFIG_SSL_CERT_LOCATION	= NULL;
char	*CONFIG_SSL_KEY_LOCATION	= NULL;

/* TLS parameters */
unsigned int	configured_tls_connect_mode = ZBX_TCP_SEC_UNENCRYPTED;
unsigned int	configured_tls_accept_modes = ZBX_TCP_SEC_UNENCRYPTED;

char	*CONFIG_TLS_CONNECT		= NULL;
char	*CONFIG_TLS_ACCEPT		= NULL;
char	*CONFIG_TLS_CA_FILE		= NULL;
char	*CONFIG_TLS_CRL_FILE		= NULL;
char	*CONFIG_TLS_SERVER_CERT_ISSUER	= NULL;
char	*CONFIG_TLS_SERVER_CERT_SUBJECT	= NULL;
char	*CONFIG_TLS_CERT_FILE		= NULL;
char	*CONFIG_TLS_KEY_FILE		= NULL;
char	*CONFIG_TLS_PSK_IDENTITY	= NULL;
char	*CONFIG_TLS_PSK_FILE		= NULL;
char	*CONFIG_TLS_CIPHER_CERT13	= NULL;
char	*CONFIG_TLS_CIPHER_CERT		= NULL;
char	*CONFIG_TLS_CIPHER_PSK13	= NULL;
char	*CONFIG_TLS_CIPHER_PSK		= NULL;
char	*CONFIG_TLS_CIPHER_ALL13	= NULL;
char	*CONFIG_TLS_CIPHER_ALL		= NULL;
char	*CONFIG_TLS_CIPHER_CMD13	= NULL;	/* not used in proxy, defined for linking with tls.c */
char	*CONFIG_TLS_CIPHER_CMD		= NULL;	/* not used in proxy, defined for linking with tls.c */

static char	*CONFIG_SOCKET_PATH	= NULL;

char	*CONFIG_HISTORY_STORAGE_URL		= NULL;
char	*CONFIG_HISTORY_STORAGE_OPTS		= NULL;
int	CONFIG_HISTORY_STORAGE_PIPELINES	= 0;

char	*CONFIG_STATS_ALLOWED_IP	= NULL;
int	CONFIG_TCP_MAX_BACKLOG_SIZE	= SOMAXCONN;

int	CONFIG_DOUBLE_PRECISION		= ZBX_DB_DBL_PRECISION_ENABLED;

zbx_vector_ptr_t	zbx_addrs;

typedef struct
{
	zbx_rtc_t	*rtc;
	zbx_socket_t	*listen_sock;
}
zbx_on_exit_args_t;

int	get_process_info_by_thread(int local_server_num, unsigned char *local_process_type, int *local_process_num);

int	get_process_info_by_thread(int local_server_num, unsigned char *local_process_type, int *local_process_num)
{
	int	server_count = 0;

	if (0 == local_server_num)
	{
		/* fail if the main process is queried */
		return FAIL;
	}
	else if (local_server_num <= (server_count += CONFIG_CONFSYNCER_FORKS))
	{
		/* make initial configuration sync before worker processes are forked on active Zabbix proxy */
		*local_process_type = ZBX_PROCESS_TYPE_CONFSYNCER;
		*local_process_num = local_server_num - server_count + CONFIG_CONFSYNCER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_TRAPPER_FORKS))
	{
		/* make initial configuration sync before worker processes are forked on passive Zabbix proxy */
		*local_process_type = ZBX_PROCESS_TYPE_TRAPPER;
		*local_process_num = local_server_num - server_count + CONFIG_TRAPPER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_PREPROCMAN_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_PREPROCMAN;
		*local_process_num = local_server_num - server_count + CONFIG_PREPROCMAN_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_PREPROCESSOR_FORKS))
	{
		/* data collection processes might utilize CPU fully, start manager and worker processes beforehand */
		*local_process_type = ZBX_PROCESS_TYPE_PREPROCESSOR;
		*local_process_num = local_server_num - server_count + CONFIG_PREPROCESSOR_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_HEARTBEAT_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_HEARTBEAT;
		*local_process_num = local_server_num - server_count + CONFIG_HEARTBEAT_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_DATASENDER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_DATASENDER;
		*local_process_num = local_server_num - server_count + CONFIG_DATASENDER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_IPMIMANAGER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_IPMIMANAGER;
		*local_process_num = local_server_num - server_count + CONFIG_IPMIMANAGER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_HOUSEKEEPER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_HOUSEKEEPER;
		*local_process_num = local_server_num - server_count + CONFIG_HOUSEKEEPER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_HTTPPOLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_HTTPPOLLER;
		*local_process_num = local_server_num - server_count + CONFIG_HTTPPOLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_DISCOVERER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_DISCOVERER;
		*local_process_num = local_server_num - server_count + CONFIG_DISCOVERER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_HISTSYNCER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_HISTSYNCER;
		*local_process_num = local_server_num - server_count + CONFIG_HISTSYNCER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_IPMIPOLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_IPMIPOLLER;
		*local_process_num = local_server_num - server_count + CONFIG_IPMIPOLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_JAVAPOLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_JAVAPOLLER;
		*local_process_num = local_server_num - server_count + CONFIG_JAVAPOLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_SNMPTRAPPER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_SNMPTRAPPER;
		*local_process_num = local_server_num - server_count + CONFIG_SNMPTRAPPER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_SELFMON_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_SELFMON;
		*local_process_num = local_server_num - server_count + CONFIG_SELFMON_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_VMWARE_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_VMWARE;
		*local_process_num = local_server_num - server_count + CONFIG_VMWARE_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_TASKMANAGER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_TASKMANAGER;
		*local_process_num = local_server_num - server_count + CONFIG_TASKMANAGER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_POLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_POLLER;
		*local_process_num = local_server_num - server_count + CONFIG_POLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_UNREACHABLE_POLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_UNREACHABLE;
		*local_process_num = local_server_num - server_count + CONFIG_UNREACHABLE_POLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_PINGER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_PINGER;
		*local_process_num = local_server_num - server_count + CONFIG_PINGER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_HISTORYPOLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_HISTORYPOLLER;
		*local_process_num = local_server_num - server_count + CONFIG_HISTORYPOLLER_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_AVAILMAN_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_AVAILMAN;
		*local_process_num = local_server_num - server_count + CONFIG_AVAILMAN_FORKS;
	}
	else if (local_server_num <= (server_count += CONFIG_ODBCPOLLER_FORKS))
	{
		*local_process_type = ZBX_PROCESS_TYPE_ODBCPOLLER;
		*local_process_num = local_server_num - server_count + CONFIG_ODBCPOLLER_FORKS;
	}
	else
		return FAIL;

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: set configuration defaults                                        *
 *                                                                            *
 ******************************************************************************/
static void	zbx_set_defaults(void)
{
	AGENT_RESULT	result;
	char		**value = NULL;

	CONFIG_SERVER_STARTUP_TIME = time(NULL);

	if (NULL == CONFIG_HOSTNAME)
	{
		if (NULL == CONFIG_HOSTNAME_ITEM)
			CONFIG_HOSTNAME_ITEM = zbx_strdup(CONFIG_HOSTNAME_ITEM, "system.hostname");

		init_result(&result);

		if (SUCCEED == process(CONFIG_HOSTNAME_ITEM, PROCESS_LOCAL_COMMAND, &result) &&
				NULL != (value = GET_STR_RESULT(&result)))
		{
			assert(*value);

			if (MAX_ZBX_HOSTNAME_LEN < strlen(*value))
			{
				(*value)[MAX_ZBX_HOSTNAME_LEN] = '\0';
				zabbix_log(LOG_LEVEL_WARNING, "proxy name truncated to [%s])", *value);
			}

			CONFIG_HOSTNAME = zbx_strdup(CONFIG_HOSTNAME, *value);
		}
		else
			zabbix_log(LOG_LEVEL_WARNING, "failed to get proxy name from [%s])", CONFIG_HOSTNAME_ITEM);

		free_result(&result);
	}
	else if (NULL != CONFIG_HOSTNAME_ITEM)
	{
		zabbix_log(LOG_LEVEL_WARNING, "both Hostname and HostnameItem defined, using [%s]", CONFIG_HOSTNAME);
	}

	if (NULL == CONFIG_DBHOST)
		CONFIG_DBHOST = zbx_strdup(CONFIG_DBHOST, "localhost");

	if (NULL == CONFIG_SNMPTRAP_FILE)
		CONFIG_SNMPTRAP_FILE = zbx_strdup(CONFIG_SNMPTRAP_FILE, "/tmp/zabbix_traps.tmp");

	if (NULL == CONFIG_PID_FILE)
		CONFIG_PID_FILE = zbx_strdup(CONFIG_PID_FILE, "/tmp/zabbix_proxy.pid");

	if (NULL == CONFIG_TMPDIR)
		CONFIG_TMPDIR = zbx_strdup(CONFIG_TMPDIR, "/tmp");

	if (NULL == CONFIG_FPING_LOCATION)
		CONFIG_FPING_LOCATION = zbx_strdup(CONFIG_FPING_LOCATION, "/usr/sbin/fping");
#ifdef HAVE_IPV6
	if (NULL == CONFIG_FPING6_LOCATION)
		CONFIG_FPING6_LOCATION = zbx_strdup(CONFIG_FPING6_LOCATION, "/usr/sbin/fping6");
#endif
	if (NULL == CONFIG_EXTERNALSCRIPTS)
		CONFIG_EXTERNALSCRIPTS = zbx_strdup(CONFIG_EXTERNALSCRIPTS, DEFAULT_EXTERNAL_SCRIPTS_PATH);

	if (NULL == CONFIG_LOAD_MODULE_PATH)
		CONFIG_LOAD_MODULE_PATH = zbx_strdup(CONFIG_LOAD_MODULE_PATH, DEFAULT_LOAD_MODULE_PATH);
#ifdef HAVE_LIBCURL
	if (NULL == CONFIG_SSL_CERT_LOCATION)
		CONFIG_SSL_CERT_LOCATION = zbx_strdup(CONFIG_SSL_CERT_LOCATION, DEFAULT_SSL_CERT_LOCATION);

	if (NULL == CONFIG_SSL_KEY_LOCATION)
		CONFIG_SSL_KEY_LOCATION = zbx_strdup(CONFIG_SSL_KEY_LOCATION, DEFAULT_SSL_KEY_LOCATION);
#endif
	if (ZBX_PROXYMODE_ACTIVE != CONFIG_PROXYMODE || 0 == CONFIG_HEARTBEAT_FREQUENCY)
		CONFIG_HEARTBEAT_FORKS = 0;

	if (ZBX_PROXYMODE_PASSIVE == CONFIG_PROXYMODE)
	{
		CONFIG_DATASENDER_FORKS = 0;
		program_type = ZBX_PROGRAM_TYPE_PROXY_PASSIVE;
	}

	if (NULL == CONFIG_LOG_TYPE_STR)
		CONFIG_LOG_TYPE_STR = zbx_strdup(CONFIG_LOG_TYPE_STR, ZBX_OPTION_LOGTYPE_FILE);

	if (NULL == CONFIG_SOCKET_PATH)
		CONFIG_SOCKET_PATH = zbx_strdup(CONFIG_SOCKET_PATH, "/tmp");

	if (0 != CONFIG_IPMIPOLLER_FORKS)
		CONFIG_IPMIMANAGER_FORKS = 1;

	if (NULL == CONFIG_VAULTURL)
		CONFIG_VAULTURL = zbx_strdup(CONFIG_VAULTURL, "https://127.0.0.1:8200");

	if (0 == CONFIG_SERVER_PORT)
	{
		CONFIG_SERVER_PORT = ZBX_DEFAULT_SERVER_PORT;
	}
	else
	{
		zabbix_log(LOG_LEVEL_WARNING, "ServerPort parameter is deprecated,"
					" please specify port in Server parameter separated by ':' instead");
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: validate configuration parameters                                 *
 *                                                                            *
 ******************************************************************************/
static void	zbx_validate_config(ZBX_TASK_EX *task)
{
	char	*ch_error;
	int	err = 0;

	if (NULL == CONFIG_HOSTNAME)
	{
		zabbix_log(LOG_LEVEL_CRIT, "\"Hostname\" configuration parameter is not defined");
		err = 1;
	}
	else if (FAIL == zbx_check_hostname(CONFIG_HOSTNAME, &ch_error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "invalid \"Hostname\" configuration parameter '%s': %s", CONFIG_HOSTNAME,
				ch_error);
		zbx_free(ch_error);
		err = 1;
	}

	if (0 == CONFIG_UNREACHABLE_POLLER_FORKS && 0 != CONFIG_POLLER_FORKS + CONFIG_JAVAPOLLER_FORKS)
	{
		zabbix_log(LOG_LEVEL_CRIT, "\"StartPollersUnreachable\" configuration parameter must not be 0"
				" if regular or Java pollers are started");
		err = 1;
	}

	if ((NULL == CONFIG_JAVA_GATEWAY || '\0' == *CONFIG_JAVA_GATEWAY) && 0 < CONFIG_JAVAPOLLER_FORKS)
	{
		zabbix_log(LOG_LEVEL_CRIT, "\"JavaGateway\" configuration parameter is not specified or empty");
		err = 1;
	}

	if (ZBX_PROXYMODE_ACTIVE == CONFIG_PROXYMODE)
	{
		if (NULL != strchr(CONFIG_SERVER, ','))
		{
			zabbix_log(LOG_LEVEL_CRIT, "\"Server\" configuration parameter must not contain comma");
			err = 1;
		}
	}
	else if (ZBX_PROXYMODE_PASSIVE == CONFIG_PROXYMODE && FAIL == zbx_validate_peer_list(CONFIG_SERVER, &ch_error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "invalid entry in \"Server\" configuration parameter: %s", ch_error);
		zbx_free(ch_error);
		err = 1;
	}

	if (NULL != CONFIG_SOURCE_IP && SUCCEED != is_supported_ip(CONFIG_SOURCE_IP))
	{
		zabbix_log(LOG_LEVEL_CRIT, "invalid \"SourceIP\" configuration parameter: '%s'", CONFIG_SOURCE_IP);
		err = 1;
	}

	if (NULL != CONFIG_STATS_ALLOWED_IP && FAIL == zbx_validate_peer_list(CONFIG_STATS_ALLOWED_IP, &ch_error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "invalid entry in \"StatsAllowedIP\" configuration parameter: %s", ch_error);
		zbx_free(ch_error);
		err = 1;
	}
#if !defined(HAVE_IPV6)
	err |= (FAIL == check_cfg_feature_str("Fping6Location", CONFIG_FPING6_LOCATION, "IPv6 support"));
#endif
#if !defined(HAVE_LIBCURL)
	err |= (FAIL == check_cfg_feature_str("SSLCALocation", CONFIG_SSL_CA_LOCATION, "cURL library"));
	err |= (FAIL == check_cfg_feature_str("SSLCertLocation", CONFIG_SSL_CERT_LOCATION, "cURL library"));
	err |= (FAIL == check_cfg_feature_str("SSLKeyLocation", CONFIG_SSL_KEY_LOCATION, "cURL library"));
	err |= (FAIL == check_cfg_feature_str("VaultToken", CONFIG_VAULTTOKEN, "cURL library"));
	err |= (FAIL == check_cfg_feature_str("VaultDBPath", CONFIG_VAULTDBPATH, "cURL library"));
#endif
#if !defined(HAVE_LIBXML2) || !defined(HAVE_LIBCURL)
	err |= (FAIL == check_cfg_feature_int("StartVMwareCollectors", CONFIG_VMWARE_FORKS, "VMware support"));

	/* parameters VMwareFrequency, VMwarePerfFrequency, VMwareCacheSize, VMwareTimeout are not checked here */
	/* because they have non-zero default values */
#endif

	if (SUCCEED != zbx_validate_log_parameters(task))
		err = 1;

#if !(defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL))
	err |= (FAIL == check_cfg_feature_str("TLSConnect", CONFIG_TLS_CONNECT, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSAccept", CONFIG_TLS_ACCEPT, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSCAFile", CONFIG_TLS_CA_FILE, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSCRLFile", CONFIG_TLS_CRL_FILE, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSServerCertIssuer", CONFIG_TLS_SERVER_CERT_ISSUER, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSServerCertSubject", CONFIG_TLS_SERVER_CERT_SUBJECT, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSCertFile", CONFIG_TLS_CERT_FILE, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSKeyFile", CONFIG_TLS_KEY_FILE, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSPSKIdentity", CONFIG_TLS_PSK_IDENTITY, "TLS support"));
	err |= (FAIL == check_cfg_feature_str("TLSPSKFile", CONFIG_TLS_PSK_FILE, "TLS support"));
#endif
#if !(defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL))
	err |= (FAIL == check_cfg_feature_str("TLSCipherCert", CONFIG_TLS_CIPHER_CERT, "GnuTLS or OpenSSL"));
	err |= (FAIL == check_cfg_feature_str("TLSCipherPSK", CONFIG_TLS_CIPHER_PSK, "GnuTLS or OpenSSL"));
	err |= (FAIL == check_cfg_feature_str("TLSCipherAll", CONFIG_TLS_CIPHER_ALL, "GnuTLS or OpenSSL"));
#endif
#if !defined(HAVE_OPENSSL)
	err |= (FAIL == check_cfg_feature_str("TLSCipherCert13", CONFIG_TLS_CIPHER_CERT13, "OpenSSL 1.1.1 or newer"));
	err |= (FAIL == check_cfg_feature_str("TLSCipherPSK13", CONFIG_TLS_CIPHER_PSK13, "OpenSSL 1.1.1 or newer"));
	err |= (FAIL == check_cfg_feature_str("TLSCipherAll13", CONFIG_TLS_CIPHER_ALL13, "OpenSSL 1.1.1 or newer"));
#endif

#if !defined(HAVE_OPENIPMI)
	err |= (FAIL == check_cfg_feature_int("StartIPMIPollers", CONFIG_IPMIPOLLER_FORKS, "IPMI support"));
#endif

	err |= (FAIL == zbx_db_validate_config_features());

	if (0 != err)
		exit(EXIT_FAILURE);
}

static int	proxy_add_serveractive_host_cb(const zbx_vector_ptr_t *addrs, zbx_vector_str_t *hostnames, void *data)
{
	ZBX_UNUSED(hostnames);
	ZBX_UNUSED(data);

	zbx_addr_copy(&zbx_addrs, addrs);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: parse config file and update configuration parameters             *
 *                                                                            *
 * Comments: will terminate process if parsing fails                          *
 *                                                                            *
 ******************************************************************************/
static void	zbx_load_config(ZBX_TASK_EX *task)
{
	static struct cfg_line	cfg[] =
	{
		/* PARAMETER,			VAR,					TYPE,
			MANDATORY,	MIN,			MAX */
		{"ProxyMode",			&CONFIG_PROXYMODE,			TYPE_INT,
			PARM_OPT,	ZBX_PROXYMODE_ACTIVE,	ZBX_PROXYMODE_PASSIVE},
		{"Server",			&CONFIG_SERVER,				TYPE_STRING,
			PARM_MAND,	0,			0},
		{"ServerPort",			&CONFIG_SERVER_PORT,			TYPE_INT,
			PARM_OPT,	1024,			32767},
		{"Hostname",			&CONFIG_HOSTNAME,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"HostnameItem",		&CONFIG_HOSTNAME_ITEM,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"StartDBSyncers",		&CONFIG_HISTSYNCER_FORKS,		TYPE_INT,
			PARM_OPT,	1,			100},
		{"StartDiscoverers",		&CONFIG_DISCOVERER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			250},
		{"StartHTTPPollers",		&CONFIG_HTTPPOLLER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartPingers",		&CONFIG_PINGER_FORKS,			TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartPollers",		&CONFIG_POLLER_FORKS,			TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartPollersUnreachable",	&CONFIG_UNREACHABLE_POLLER_FORKS,	TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartIPMIPollers",		&CONFIG_IPMIPOLLER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartTrappers",		&CONFIG_TRAPPER_FORKS,			TYPE_INT,
			PARM_OPT,	0,			1000},
		{"StartJavaPollers",		&CONFIG_JAVAPOLLER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1000},
		{"JavaGateway",			&CONFIG_JAVA_GATEWAY,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"JavaGatewayPort",		&CONFIG_JAVA_GATEWAY_PORT,		TYPE_INT,
			PARM_OPT,	1024,			32767},
		{"SNMPTrapperFile",		&CONFIG_SNMPTRAP_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"StartSNMPTrapper",		&CONFIG_SNMPTRAPPER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1},
		{"CacheSize",			&CONFIG_CONF_CACHE_SIZE,		TYPE_UINT64,
			PARM_OPT,	128 * ZBX_KIBIBYTE,	__UINT64_C(64) * ZBX_GIBIBYTE},
		{"HistoryCacheSize",		&CONFIG_HISTORY_CACHE_SIZE,		TYPE_UINT64,
			PARM_OPT,	128 * ZBX_KIBIBYTE,	__UINT64_C(2) * ZBX_GIBIBYTE},
		{"HistoryIndexCacheSize",	&CONFIG_HISTORY_INDEX_CACHE_SIZE,	TYPE_UINT64,
			PARM_OPT,	128 * ZBX_KIBIBYTE,	__UINT64_C(2) * ZBX_GIBIBYTE},
		{"HousekeepingFrequency",	&CONFIG_HOUSEKEEPING_FREQUENCY,		TYPE_INT,
			PARM_OPT,	0,			24},
		{"ProxyLocalBuffer",		&CONFIG_PROXY_LOCAL_BUFFER,		TYPE_INT,
			PARM_OPT,	0,			720},
		{"ProxyOfflineBuffer",		&CONFIG_PROXY_OFFLINE_BUFFER,		TYPE_INT,
			PARM_OPT,	1,			720},
		{"HeartbeatFrequency",		&CONFIG_HEARTBEAT_FREQUENCY,		TYPE_INT,
			PARM_OPT,	0,			ZBX_PROXY_HEARTBEAT_FREQUENCY_MAX},
		{"ConfigFrequency",		&CONFIG_PROXYCONFIG_FREQUENCY,		TYPE_INT,
			PARM_OPT,	1,			SEC_PER_WEEK},
		{"DataSenderFrequency",		&CONFIG_PROXYDATA_FREQUENCY,		TYPE_INT,
			PARM_OPT,	1,			SEC_PER_HOUR},
		{"TmpDir",			&CONFIG_TMPDIR,				TYPE_STRING,
			PARM_OPT,	0,			0},
		{"FpingLocation",		&CONFIG_FPING_LOCATION,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"Fping6Location",		&CONFIG_FPING6_LOCATION,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"Timeout",			&CONFIG_TIMEOUT,			TYPE_INT,
			PARM_OPT,	1,			30},
		{"TrapperTimeout",		&CONFIG_TRAPPER_TIMEOUT,		TYPE_INT,
			PARM_OPT,	1,			300},
		{"UnreachablePeriod",		&CONFIG_UNREACHABLE_PERIOD,		TYPE_INT,
			PARM_OPT,	1,			SEC_PER_HOUR},
		{"UnreachableDelay",		&CONFIG_UNREACHABLE_DELAY,		TYPE_INT,
			PARM_OPT,	1,			SEC_PER_HOUR},
		{"UnavailableDelay",		&CONFIG_UNAVAILABLE_DELAY,		TYPE_INT,
			PARM_OPT,	1,			SEC_PER_HOUR},
		{"ListenIP",			&CONFIG_LISTEN_IP,			TYPE_STRING_LIST,
			PARM_OPT,	0,			0},
		{"ListenPort",			&CONFIG_LISTEN_PORT,			TYPE_INT,
			PARM_OPT,	1024,			32767},
		{"SourceIP",			&CONFIG_SOURCE_IP,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DebugLevel",			&CONFIG_LOG_LEVEL,			TYPE_INT,
			PARM_OPT,	0,			5},
		{"PidFile",			&CONFIG_PID_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"LogType",			&CONFIG_LOG_TYPE_STR,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"LogFile",			&CONFIG_LOG_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"LogFileSize",			&CONFIG_LOG_FILE_SIZE,			TYPE_INT,
			PARM_OPT,	0,			1024},
		{"ExternalScripts",		&CONFIG_EXTERNALSCRIPTS,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBHost",			&CONFIG_DBHOST,				TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBName",			&CONFIG_DBNAME,				TYPE_STRING,
			PARM_MAND,	0,			0},
		{"DBSchema",			&CONFIG_DBSCHEMA,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBUser",			&CONFIG_DBUSER,				TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBPassword",			&CONFIG_DBPASSWORD,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"VaultToken",			&CONFIG_VAULTTOKEN,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"VaultURL",			&CONFIG_VAULTURL,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"VaultDBPath",			&CONFIG_VAULTDBPATH,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBSocket",			&CONFIG_DBSOCKET,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBPort",			&CONFIG_DBPORT,				TYPE_INT,
			PARM_OPT,	1024,			65535},
		{"AllowUnsupportedDBVersions",	&CONFIG_ALLOW_UNSUPPORTED_DB_VERSIONS,	TYPE_INT,
			PARM_OPT,	0,			1},
		{"DBTLSConnect",		&CONFIG_DB_TLS_CONNECT,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBTLSCertFile",		&CONFIG_DB_TLS_CERT_FILE,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBTLSKeyFile",		&CONFIG_DB_TLS_KEY_FILE,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBTLSCAFile",			&CONFIG_DB_TLS_CA_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBTLSCipher",			&CONFIG_DB_TLS_CIPHER,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"DBTLSCipher13",		&CONFIG_DB_TLS_CIPHER_13,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"SSHKeyLocation",		&CONFIG_SSH_KEY_LOCATION,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"LogSlowQueries",		&CONFIG_LOG_SLOW_QUERIES,		TYPE_INT,
			PARM_OPT,	0,			3600000},
		{"LoadModulePath",		&CONFIG_LOAD_MODULE_PATH,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"LoadModule",			&CONFIG_LOAD_MODULE,			TYPE_MULTISTRING,
			PARM_OPT,	0,			0},
		{"StartVMwareCollectors",	&CONFIG_VMWARE_FORKS,			TYPE_INT,
			PARM_OPT,	0,			250},
		{"VMwareFrequency",		&CONFIG_VMWARE_FREQUENCY,		TYPE_INT,
			PARM_OPT,	10,			SEC_PER_DAY},
		{"VMwarePerfFrequency",		&CONFIG_VMWARE_PERF_FREQUENCY,		TYPE_INT,
			PARM_OPT,	10,			SEC_PER_DAY},
		{"VMwareCacheSize",		&CONFIG_VMWARE_CACHE_SIZE,		TYPE_UINT64,
			PARM_OPT,	256 * ZBX_KIBIBYTE,	__UINT64_C(2) * ZBX_GIBIBYTE},
		{"VMwareTimeout",		&CONFIG_VMWARE_TIMEOUT,			TYPE_INT,
			PARM_OPT,	1,			300},
		{"AllowRoot",			&CONFIG_ALLOW_ROOT,			TYPE_INT,
			PARM_OPT,	0,			1},
		{"User",			&CONFIG_USER,				TYPE_STRING,
			PARM_OPT,	0,			0},
		{"SSLCALocation",		&CONFIG_SSL_CA_LOCATION,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"SSLCertLocation",		&CONFIG_SSL_CERT_LOCATION,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"SSLKeyLocation",		&CONFIG_SSL_KEY_LOCATION,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSConnect",			&CONFIG_TLS_CONNECT,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSAccept",			&CONFIG_TLS_ACCEPT,			TYPE_STRING_LIST,
			PARM_OPT,	0,			0},
		{"TLSCAFile",			&CONFIG_TLS_CA_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCRLFile",			&CONFIG_TLS_CRL_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSServerCertIssuer",		&CONFIG_TLS_SERVER_CERT_ISSUER,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSServerCertSubject",	&CONFIG_TLS_SERVER_CERT_SUBJECT,	TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCertFile",			&CONFIG_TLS_CERT_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSKeyFile",			&CONFIG_TLS_KEY_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSPSKIdentity",		&CONFIG_TLS_PSK_IDENTITY,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSPSKFile",			&CONFIG_TLS_PSK_FILE,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherCert13",		&CONFIG_TLS_CIPHER_CERT13,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherCert",		&CONFIG_TLS_CIPHER_CERT,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherPSK13",		&CONFIG_TLS_CIPHER_PSK13,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherPSK",		&CONFIG_TLS_CIPHER_PSK,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherAll13",		&CONFIG_TLS_CIPHER_ALL13,		TYPE_STRING,
			PARM_OPT,	0,			0},
		{"TLSCipherAll",		&CONFIG_TLS_CIPHER_ALL,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"SocketDir",			&CONFIG_SOCKET_PATH,			TYPE_STRING,
			PARM_OPT,	0,			0},
		{"EnableRemoteCommands",	&CONFIG_ENABLE_REMOTE_COMMANDS,		TYPE_INT,
			PARM_OPT,	0,			1},
		{"LogRemoteCommands",		&CONFIG_LOG_REMOTE_COMMANDS,		TYPE_INT,
			PARM_OPT,	0,			1},
		{"StatsAllowedIP",		&CONFIG_STATS_ALLOWED_IP,		TYPE_STRING_LIST,
			PARM_OPT,	0,			0},
		{"StartPreprocessors",		&CONFIG_PREPROCESSOR_FORKS,		TYPE_INT,
			PARM_OPT,	1,			1000},
		{"StartHistoryPollers",		&CONFIG_HISTORYPOLLER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1000},
		{"ListenBacklog",		&CONFIG_TCP_MAX_BACKLOG_SIZE,		TYPE_INT,
			PARM_OPT,	0,			INT_MAX},
		{"StartODBCPollers",		&CONFIG_ODBCPOLLER_FORKS,		TYPE_INT,
			PARM_OPT,	0,			1000},
		{NULL}
	};

	/* initialize multistrings */
	zbx_strarr_init(&CONFIG_LOAD_MODULE);

	parse_cfg_file(CONFIG_FILE, cfg, ZBX_CFG_FILE_REQUIRED, ZBX_CFG_STRICT, ZBX_CFG_EXIT_FAILURE);

	zbx_set_defaults();

	CONFIG_LOG_TYPE = zbx_get_log_type(CONFIG_LOG_TYPE_STR);

	zbx_validate_config(task);

	zbx_vector_ptr_create(&zbx_addrs);

	if (ZBX_PROXYMODE_PASSIVE != CONFIG_PROXYMODE)
	{
		char	*error;

		if (FAIL == zbx_set_data_destination_hosts(CONFIG_SERVER, (unsigned short)CONFIG_SERVER_PORT, "Server",
				proxy_add_serveractive_host_cb, NULL, NULL, &error))
		{
			zbx_error("%s", error);
			exit(EXIT_FAILURE);
		}
	}

#if defined(HAVE_MYSQL) || defined(HAVE_POSTGRESQL)
	zbx_db_validate_config();
#endif
#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
	zbx_tls_validate_config();
#endif
}

/******************************************************************************
 *                                                                            *
 * Purpose: free configuration memory                                         *
 *                                                                            *
 ******************************************************************************/
static void	zbx_free_config(void)
{
	zbx_strarr_free(&CONFIG_LOAD_MODULE);
}

/******************************************************************************
 *                                                                            *
 * Purpose: executes proxy processes                                          *
 *                                                                            *
 ******************************************************************************/
int	main(int argc, char **argv)
{
	ZBX_TASK_EX	t = {ZBX_TASK_START};
	char		ch;
	int		opt_c = 0, opt_r = 0;

#if defined(PS_OVERWRITE_ARGV) || defined(PS_PSTAT_ARGV)
	argv = setproctitle_save_env(argc, argv);
#endif
	progname = get_program_name(argv[0]);

	/* parse the command-line */
	while ((char)EOF != (ch = (char)zbx_getopt_long(argc, argv, shortopts, longopts, NULL)))
	{
		switch (ch)
		{
			case 'c':
				opt_c++;
				if (NULL == CONFIG_FILE)
					CONFIG_FILE = zbx_strdup(CONFIG_FILE, zbx_optarg);
				break;
			case 'R':
				opt_r++;
				t.opts = zbx_strdup(t.opts, zbx_optarg);
				t.task = ZBX_TASK_RUNTIME_CONTROL;
				break;
			case 'h':
				help();
				exit(EXIT_SUCCESS);
				break;
			case 'V':
				version();
				exit(EXIT_SUCCESS);
				break;
			case 'f':
				t.flags |= ZBX_TASK_FLAG_FOREGROUND;
				break;
			default:
				usage();
				exit(EXIT_FAILURE);
				break;
		}
	}

	/* every option may be specified only once */
	if (1 < opt_c || 1 < opt_r)
	{
		if (1 < opt_c)
			zbx_error("option \"-c\" or \"--config\" specified multiple times");
		if (1 < opt_r)
			zbx_error("option \"-R\" or \"--runtime-control\" specified multiple times");

		exit(EXIT_FAILURE);
	}

	/* Parameters which are not option values are invalid. The check relies on zbx_getopt_internal() which */
	/* always permutes command line arguments regardless of POSIXLY_CORRECT environment variable. */
	if (argc > zbx_optind)
	{
		int	i;

		for (i = zbx_optind; i < argc; i++)
			zbx_error("invalid parameter \"%s\"", argv[i]);

		exit(EXIT_FAILURE);
	}

	if (NULL == CONFIG_FILE)
		CONFIG_FILE = zbx_strdup(NULL, DEFAULT_CONFIG_FILE);

	/* required for simple checks */
	init_metrics();

	zbx_load_config(&t);

	if (ZBX_TASK_RUNTIME_CONTROL == t.task)
	{
		int	ret;
		char	*error = NULL;

		if (FAIL == zbx_ipc_service_init_env(CONFIG_SOCKET_PATH, &error))
		{
			zbx_error("cannot initialize IPC services: %s", error);
			zbx_free(error);
			exit(EXIT_FAILURE);
		}

		if (SUCCEED != (ret = zbx_rtc_process(t.opts, &error)))
		{
			zbx_error("Cannot perform runtime control command: %s", error);
			zbx_free(error);
		}

		exit(SUCCEED == ret ? EXIT_SUCCESS : EXIT_FAILURE);
	}

	return daemon_start(CONFIG_ALLOW_ROOT, CONFIG_USER, t.flags);
}

static void	zbx_check_db(void)
{
	struct zbx_db_version_info_t	db_version_info;

	if (FAIL == zbx_db_check_version_info(&db_version_info, CONFIG_ALLOW_UNSUPPORTED_DB_VERSIONS))
	{
		zbx_free(db_version_info.friendly_current_version);
		exit(EXIT_FAILURE);
	}

	zbx_free(db_version_info.friendly_current_version);
}

#ifdef HAVE_ORACLE
static void	zbx_check_db_tables(void)
{
	zbx_db_table_prepare("items", NULL);
	zbx_db_table_prepare("item_preproc", NULL);
}
#endif

int	MAIN_ZABBIX_ENTRY(int flags)
{
	zbx_socket_t		listen_sock = {0};
	char			*error = NULL;
	int			i, db_type, ret;
	zbx_rtc_t		rtc;
	zbx_timespec_t		rtc_timeout = {1, 0};
	zbx_on_exit_args_t	exit_args = {.rtc = NULL, .listen_sock = NULL};

	if (0 != (flags & ZBX_TASK_FLAG_FOREGROUND))
	{
		printf("Starting Zabbix Proxy (%s) [%s]. Zabbix %s (revision %s).\nPress Ctrl+C to exit.\n\n",
				ZBX_PROXYMODE_PASSIVE == CONFIG_PROXYMODE ? "passive" : "active",
				CONFIG_HOSTNAME, ZABBIX_VERSION, ZABBIX_REVISION);
	}

	zbx_block_signals(&orig_mask);

	if (FAIL == zbx_ipc_service_init_env(CONFIG_SOCKET_PATH, &error))
	{
		zbx_error("cannot initialize IPC services: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != zbx_locks_create(&error))
	{
		zbx_error("cannot create locks: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != zabbix_open_log(CONFIG_LOG_TYPE, CONFIG_LOG_LEVEL, CONFIG_LOG_FILE, &error))
	{
		zbx_error("cannot open log:%s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

#ifdef HAVE_NETSNMP
#	define SNMP_FEATURE_STATUS 	"YES"
#else
#	define SNMP_FEATURE_STATUS 	" NO"
#endif
#ifdef HAVE_OPENIPMI
#	define IPMI_FEATURE_STATUS 	"YES"
#else
#	define IPMI_FEATURE_STATUS 	" NO"
#endif
#ifdef HAVE_LIBCURL
#	define LIBCURL_FEATURE_STATUS	"YES"
#else
#	define LIBCURL_FEATURE_STATUS	" NO"
#endif
#if defined(HAVE_LIBCURL) && defined(HAVE_LIBXML2)
#	define VMWARE_FEATURE_STATUS	"YES"
#else
#	define VMWARE_FEATURE_STATUS	" NO"
#endif
#ifdef HAVE_UNIXODBC
#	define ODBC_FEATURE_STATUS 	"YES"
#else
#	define ODBC_FEATURE_STATUS 	" NO"
#endif
#if defined(HAVE_SSH2) || defined(HAVE_SSH)
#	define SSH_FEATURE_STATUS 	"YES"
#else
#	define SSH_FEATURE_STATUS 	" NO"
#endif
#ifdef HAVE_IPV6
#	define IPV6_FEATURE_STATUS 	"YES"
#else
#	define IPV6_FEATURE_STATUS 	" NO"
#endif
#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
#	define TLS_FEATURE_STATUS	"YES"
#else
#	define TLS_FEATURE_STATUS	" NO"
#endif

	zabbix_log(LOG_LEVEL_INFORMATION, "Starting Zabbix Proxy (%s) [%s]. Zabbix %s (revision %s).",
			ZBX_PROXYMODE_PASSIVE == CONFIG_PROXYMODE ? "passive" : "active",
			CONFIG_HOSTNAME, ZABBIX_VERSION, ZABBIX_REVISION);

	zabbix_log(LOG_LEVEL_INFORMATION, "**** Enabled features ****");
	zabbix_log(LOG_LEVEL_INFORMATION, "SNMP monitoring:       " SNMP_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "IPMI monitoring:       " IPMI_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "Web monitoring:        " LIBCURL_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "VMware monitoring:     " VMWARE_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "ODBC:                  " ODBC_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "SSH support:           " SSH_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "IPv6 support:          " IPV6_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "TLS support:           " TLS_FEATURE_STATUS);
	zabbix_log(LOG_LEVEL_INFORMATION, "**************************");

	zabbix_log(LOG_LEVEL_INFORMATION, "using configuration file: %s", CONFIG_FILE);

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
	if (SUCCEED != zbx_coredump_disable())
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot disable core dump, exiting...");
		exit(EXIT_FAILURE);
	}
#endif
	if (FAIL == zbx_load_modules(CONFIG_LOAD_MODULE_PATH, CONFIG_LOAD_MODULE, CONFIG_TIMEOUT, 1))
	{
		zabbix_log(LOG_LEVEL_CRIT, "loading modules failed, exiting...");
		exit(EXIT_FAILURE);
	}

	zbx_free_config();

	if (SUCCEED != zbx_rtc_init(&rtc, &error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize runtime control service: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	exit_args.rtc = &rtc;
	zbx_set_on_exit_args(&exit_args);

	zbx_unblock_signals(&orig_mask);

	if (SUCCEED != init_database_cache(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize database cache: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != init_proxy_history_lock(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize lock for passive proxy history: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != init_configuration_cache(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize configuration cache: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != init_selfmon_collector(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize self-monitoring: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (0 != CONFIG_VMWARE_FORKS && SUCCEED != zbx_vmware_init(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize VMware cache: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != zbx_vault_init_token_from_env(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize vault token: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != zbx_vault_init_db_credentials(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize database credentials from vault: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	if (SUCCEED != DBinit(&error))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot initialize database: %s", error);
		zbx_free(error);
		exit(EXIT_FAILURE);
	}

	DBconnect(ZBX_DB_CONNECT_NORMAL);
	DBinit_autoincrement_options();

	if (ZBX_DB_UNKNOWN == (db_type = zbx_db_get_database_type()))
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot use database \"%s\": database is not a Zabbix database",
				CONFIG_DBNAME);
		exit(EXIT_FAILURE);
	}
	else if (ZBX_DB_PROXY != db_type)
	{
		zabbix_log(LOG_LEVEL_CRIT, "cannot use database \"%s\": Zabbix proxy cannot work with a"
				" Zabbix server database", CONFIG_DBNAME);
		exit(EXIT_FAILURE);
	}

	DBcheck_character_set();
	zbx_check_db();

	if (SUCCEED != DBcheck_version())
		exit(EXIT_FAILURE);
#ifdef HAVE_ORACLE
	zbx_check_db_tables();
#endif
	DBclose();
	threads_num = CONFIG_CONFSYNCER_FORKS + CONFIG_HEARTBEAT_FORKS + CONFIG_DATASENDER_FORKS
			+ CONFIG_POLLER_FORKS + CONFIG_UNREACHABLE_POLLER_FORKS + CONFIG_TRAPPER_FORKS
			+ CONFIG_PINGER_FORKS + CONFIG_HOUSEKEEPER_FORKS + CONFIG_HTTPPOLLER_FORKS
			+ CONFIG_DISCOVERER_FORKS + CONFIG_HISTSYNCER_FORKS + CONFIG_IPMIPOLLER_FORKS
			+ CONFIG_JAVAPOLLER_FORKS + CONFIG_SNMPTRAPPER_FORKS + CONFIG_SELFMON_FORKS
			+ CONFIG_VMWARE_FORKS + CONFIG_IPMIMANAGER_FORKS + CONFIG_TASKMANAGER_FORKS
			+ CONFIG_PREPROCMAN_FORKS + CONFIG_PREPROCESSOR_FORKS + CONFIG_HISTORYPOLLER_FORKS
			+ CONFIG_AVAILMAN_FORKS + CONFIG_ODBCPOLLER_FORKS;

	threads = (pid_t *)zbx_calloc(threads, (size_t)threads_num, sizeof(pid_t));
	threads_flags = (int *)zbx_calloc(threads_flags, (size_t)threads_num, sizeof(int));

	if (0 != CONFIG_TRAPPER_FORKS)
	{
		exit_args.listen_sock = &listen_sock;

		zbx_block_signals(&orig_mask);

		if (FAIL == zbx_tcp_listen(&listen_sock, CONFIG_LISTEN_IP, (unsigned short)CONFIG_LISTEN_PORT))
		{
			zabbix_log(LOG_LEVEL_CRIT, "listener failed: %s", zbx_socket_strerror());
			exit(EXIT_FAILURE);
		}

		zbx_unblock_signals(&orig_mask);
	}

#if defined(HAVE_GNUTLS) || defined(HAVE_OPENSSL)
	zbx_tls_init_parent();
#endif
	zabbix_log(LOG_LEVEL_INFORMATION, "proxy #0 started [main process]");

	for (i = 0; i < threads_num; i++)
	{
		zbx_thread_args_t	thread_args;
		unsigned char		poller_type;

		if (FAIL == get_process_info_by_thread(i + 1, &thread_args.process_type, &thread_args.process_num))
		{
			THIS_SHOULD_NEVER_HAPPEN;
			exit(EXIT_FAILURE);
		}

		thread_args.server_num = i + 1;
		thread_args.args = NULL;

		switch (thread_args.process_type)
		{
			case ZBX_PROCESS_TYPE_CONFSYNCER:
				zbx_thread_start(proxyconfig_thread, &thread_args, &threads[i]);
				if (FAIL == zbx_rtc_wait_for_sync_finish(&rtc))
					goto out;
				break;
			case ZBX_PROCESS_TYPE_TRAPPER:
				thread_args.args = &listen_sock;
				zbx_thread_start(trapper_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_HEARTBEAT:
				zbx_thread_start(heart_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_DATASENDER:
				zbx_thread_start(datasender_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_POLLER:
				poller_type = ZBX_POLLER_TYPE_NORMAL;
				thread_args.args = &poller_type;
				zbx_thread_start(poller_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_UNREACHABLE:
				poller_type = ZBX_POLLER_TYPE_UNREACHABLE;
				thread_args.args = &poller_type;
				zbx_thread_start(poller_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_PINGER:
				zbx_thread_start(pinger_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_HOUSEKEEPER:
				zbx_thread_start(housekeeper_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_HTTPPOLLER:
				zbx_thread_start(httppoller_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_DISCOVERER:
				zbx_thread_start(discoverer_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_HISTSYNCER:
				threads_flags[i] = ZBX_THREAD_PRIORITY_FIRST;
				zbx_thread_start(dbsyncer_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_JAVAPOLLER:
				poller_type = ZBX_POLLER_TYPE_JAVA;
				thread_args.args = &poller_type;
				zbx_thread_start(poller_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_SNMPTRAPPER:
				zbx_thread_start(snmptrapper_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_SELFMON:
				zbx_thread_start(selfmon_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_VMWARE:
				zbx_thread_start(vmware_thread, &thread_args, &threads[i]);
				break;
#ifdef HAVE_OPENIPMI
			case ZBX_PROCESS_TYPE_IPMIMANAGER:
				zbx_thread_start(ipmi_manager_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_IPMIPOLLER:
				zbx_thread_start(ipmi_poller_thread, &thread_args, &threads[i]);
				break;
#endif
			case ZBX_PROCESS_TYPE_TASKMANAGER:
				zbx_thread_start(taskmanager_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_PREPROCMAN:
				zbx_thread_start(preprocessing_manager_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_PREPROCESSOR:
				zbx_thread_start(preprocessing_worker_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_HISTORYPOLLER:
				poller_type = ZBX_POLLER_TYPE_HISTORY;
				thread_args.args = &poller_type;
				zbx_thread_start(poller_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_AVAILMAN:
				threads_flags[i] = ZBX_THREAD_PRIORITY_FIRST;
				zbx_thread_start(availability_manager_thread, &thread_args, &threads[i]);
				break;
			case ZBX_PROCESS_TYPE_ODBCPOLLER:
				poller_type = ZBX_POLLER_TYPE_ODBC;
				thread_args.args = &poller_type;
				zbx_thread_start(poller_thread, &thread_args, &threads[i]);
				break;
		}
	}

	zbx_unset_exit_on_terminate();

	while (ZBX_IS_RUNNING())
	{
		zbx_ipc_client_t	*client;
		zbx_ipc_message_t	*message;

		(void)zbx_ipc_service_recv(&rtc.service, &rtc_timeout, &client, &message);

		if (NULL != message)
		{
			zbx_rtc_dispatch(&rtc, client, message);
			zbx_ipc_message_free(message);
		}

		if (NULL != client)
			zbx_ipc_client_release(client);

		if (0 < (ret = waitpid((pid_t)-1, &i, WNOHANG)))
		{
			zabbix_log(LOG_LEVEL_CRIT, "PROCESS EXIT: %d", ret);
			sig_exiting = ZBX_EXIT_FAILURE;
			break;
		}

		if (-1 == ret && EINTR != errno)
		{
			zabbix_log(LOG_LEVEL_ERR, "failed to wait on child processes: %s", zbx_strerror(errno));
			sig_exiting = ZBX_EXIT_FAILURE;
			break;
		}
	}
out:
	if (SUCCEED == ZBX_EXIT_STATUS())
		zbx_rtc_shutdown_subs(&rtc);

	zbx_on_exit(ZBX_EXIT_STATUS(), &exit_args);

	return SUCCEED;
}

void	zbx_on_exit(int ret, void *on_exit_args)
{
	zabbix_log(LOG_LEVEL_DEBUG, "zbx_on_exit() called with ret:%d", ret);

	if (NULL != threads)
	{
		zbx_threads_wait(threads, threads_flags, threads_num, ret);	/* wait for all child processes to exit */
		zbx_free(threads);
		zbx_free(threads_flags);
	}
#ifdef HAVE_PTHREAD_PROCESS_SHARED
	zbx_locks_disable();
#endif
	free_metrics();
	zbx_ipc_service_free_env();

	DBconnect(ZBX_DB_CONNECT_EXIT);
	free_database_cache(ZBX_SYNC_ALL);
	free_configuration_cache();
	DBclose();

	DBdeinit();

	/* free vmware support */
	zbx_vmware_destroy();

	free_selfmon_collector();
	free_proxy_history_lock();

	zbx_unload_modules();

	zabbix_log(LOG_LEVEL_INFORMATION, "Zabbix Proxy stopped. Zabbix %s (revision %s).",
			ZABBIX_VERSION, ZABBIX_REVISION);

	if (NULL != on_exit_args)
	{
		zbx_on_exit_args_t	*args = (zbx_on_exit_args_t *)on_exit_args;

		if (NULL != args->listen_sock)
			zbx_tcp_unlisten(args->listen_sock);

		if (NULL != args->rtc)
			zbx_ipc_service_close(&args->rtc->service);
	}

	zabbix_close_log();

	zbx_locks_destroy();

#if defined(PS_OVERWRITE_ARGV)
	setproctitle_free_env();
#endif
	exit(EXIT_SUCCESS);
}
