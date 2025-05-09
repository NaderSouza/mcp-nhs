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

#include "simple.h"

#include "common.h"
#include "sysinfo.h"
#include "comms.h"
#include "log.h"
#include "cfg.h"
#include "telnet.h"
#include "../common/net.h"
#include "ntp.h"

#ifdef HAVE_LDAP
#	include <ldap.h>
#endif

#ifdef HAVE_LBER_H
#	include <lber.h>
#endif

ZBX_METRIC	parameters_simple[] =
/*	KEY			FLAG		FUNCTION		TEST PARAMETERS */
{
	{"net.tcp.service",	CF_HAVEPARAMS,	CHECK_SERVICE,		"ssh,127.0.0.1,22"},
	{"net.tcp.service.perf",CF_HAVEPARAMS,	CHECK_SERVICE_PERF,	"ssh,127.0.0.1,22"},
	{"net.udp.service",	CF_HAVEPARAMS,	CHECK_SERVICE,		"ntp,127.0.0.1,123"},
	{"net.udp.service.perf",CF_HAVEPARAMS,	CHECK_SERVICE_PERF,	"ntp,127.0.0.1,123"},
	{NULL}
};

#ifdef HAVE_LDAP
static int	check_ldap(const char *host, unsigned short port, int timeout, int *value_int)
{
	LDAP		*ldap	= NULL;
	LDAPMessage	*res	= NULL;
	LDAPMessage	*msg	= NULL;
	BerElement	*ber	= NULL;

	char	*attrs[2] = {"namingContexts", NULL };
	char	*attr	 = NULL;
	char	**valRes = NULL;
	int	ldapErr = 0;

	zbx_alarm_on(timeout);

	*value_int = 0;

	if (NULL == (ldap = ldap_init(host, port)))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "LDAP - initialization failed [%s:%hu]", host, port);
		goto lbl_ret;
	}

	#if defined(LDAP_OPT_SOCKET_BIND_ADDRESSES) && defined(HAVE_LDAP_SOURCEIP)
	if (NULL != CONFIG_SOURCE_IP)
	{
		if (LDAP_SUCCESS != (ldapErr = ldap_set_option(ldap, LDAP_OPT_SOCKET_BIND_ADDRESSES, CONFIG_SOURCE_IP)))
		{
			zabbix_log(LOG_LEVEL_DEBUG, "LDAP - failed to set source ip address [%s]",
					ldap_err2string(ldapErr));
			goto lbl_ret;
		}
	}
	#endif

	if (LDAP_SUCCESS != (ldapErr = ldap_search_s(ldap, "", LDAP_SCOPE_BASE, "(objectClass=*)", attrs, 0, &res)))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "LDAP - searching failed [%s] [%s]", host, ldap_err2string(ldapErr));
		goto lbl_ret;
	}

	if (NULL == (msg = ldap_first_entry(ldap, res)))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "LDAP - empty sort result. [%s] [%s]", host, ldap_err2string(ldapErr));
		goto lbl_ret;
	}

	if (NULL == (attr = ldap_first_attribute(ldap, msg, &ber)))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "LDAP - empty first entry result. [%s] [%s]", host, ldap_err2string(ldapErr));
		goto lbl_ret;
	}

	valRes = ldap_get_values(ldap, msg, attr);

	*value_int = 1;
lbl_ret:
	zbx_alarm_off();

	if (NULL != valRes)
		ldap_value_free(valRes);
	if (NULL != attr)
		ldap_memfree(attr);
	if (NULL != ber)
		ber_free(ber, 0);
	if (NULL != res)
		ldap_msgfree(res);
	if (NULL != ldap)
		ldap_unbind(ldap);

	return SYSINFO_RET_OK;
}
#endif	/* HAVE_LDAP */

static int	check_ssh(const char *host, unsigned short port, int timeout, int *value_int)
{
	int		ret, major, minor;
	zbx_socket_t	s;
	char		send_buf[MAX_STRING_LEN];
	const char	*buf;

	*value_int = 0;

	if (SUCCEED == (ret = zbx_tcp_connect(&s, CONFIG_SOURCE_IP, host, port, timeout, ZBX_TCP_SEC_UNENCRYPTED, NULL,
			NULL)))
	{
		while (NULL != (buf = zbx_tcp_recv_line(&s)))
		{
			/* parse buf for SSH identification string as per RFC 4253, section 4.2 */
			if (2 == sscanf(buf, "SSH-%d.%d-%*s", &major, &minor))
			{
				zbx_snprintf(send_buf, sizeof(send_buf), "SSH-%d.%d-zabbix_agent\r\n", major, minor);
				*value_int = 1;
				break;
			}
		}

		if (0 == *value_int)
			strscpy(send_buf, "0\n");

		ret = zbx_tcp_send_raw(&s, send_buf);
		zbx_tcp_close(&s);
	}

	if (FAIL == ret)
		zabbix_log(LOG_LEVEL_DEBUG, "SSH check error: %s", zbx_socket_strerror());

	return SYSINFO_RET_OK;
}

#ifdef HAVE_LIBCURL
static int	check_https(const char *host, unsigned short port, int timeout, int *value_int)
{
	CURL		*easyhandle;
	CURLoption	opt;
	CURLcode	err;
	char		https_host[MAX_STRING_LEN];

	*value_int = 0;

	if (NULL == (easyhandle = curl_easy_init()))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "%s: could not init cURL library", __func__);
		goto clean;
	}

	if (SUCCEED == is_ip6(host))
		zbx_snprintf(https_host, sizeof(https_host), "%s[%s]", (0 == strncmp(host, "https://", 8) ? "" : "https://"), host);
	else
		zbx_snprintf(https_host, sizeof(https_host), "%s%s", (0 == strncmp(host, "https://", 8) ? "" : "https://"), host);

	if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_USERAGENT, "Zabbix " ZABBIX_VERSION)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_URL, https_host)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_PORT, (long)port)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_NOBODY, 1L)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_SSL_VERIFYPEER, 0L)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_SSL_VERIFYHOST, 0L)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_TIMEOUT, (long)timeout)) ||
		CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = ZBX_CURLOPT_ACCEPT_ENCODING, "")))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "%s: could not set cURL option [%d]: %s",
				__func__, (int)opt, curl_easy_strerror(err));
		goto clean;
	}

#if LIBCURL_VERSION_NUM >= 0x071304
	/* CURLOPT_PROTOCOLS is supported starting with version 7.19.4 (0x071304) */
	/* CURLOPT_PROTOCOLS was deprecated in favor of CURLOPT_PROTOCOLS_STR starting with version 7.85.0 (0x075500) */
#	if LIBCURL_VERSION_NUM >= 0x075500
	if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_PROTOCOLS_STR, "HTTP,HTTPS")))
#	else
	if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_PROTOCOLS,
			CURLPROTO_HTTP | CURLPROTO_HTTPS)))
#	endif
	{
		zabbix_log(LOG_LEVEL_DEBUG, "%s: could not set cURL option [%d]: %s",
				__func__, (int)opt, curl_easy_strerror(err));
		goto clean;
	}
#endif

	if (NULL != CONFIG_SOURCE_IP)
	{
		if (CURLE_OK != (err = curl_easy_setopt(easyhandle, opt = CURLOPT_INTERFACE, CONFIG_SOURCE_IP)))
		{
			zabbix_log(LOG_LEVEL_DEBUG, "%s: could not set source interface option [%d]: %s",
					__func__, (int)opt, curl_easy_strerror(err));
			goto clean;
		}
	}

	if (CURLE_OK == (err = curl_easy_perform(easyhandle)))
		*value_int = 1;
	else
		zabbix_log(LOG_LEVEL_DEBUG, "%s: curl_easy_perform failed for [%s:%hu]: %s",
				__func__, host, port, curl_easy_strerror(err));
clean:
	curl_easy_cleanup(easyhandle);

	return SYSINFO_RET_OK;
}
#endif	/* HAVE_LIBCURL */

static int	check_telnet(const char *host, unsigned short port, int timeout, int *value_int)
{
	zbx_socket_t	s;
#ifdef _WINDOWS
	u_long		argp = 1;
#else
	int		flags;
#endif
	*value_int = 0;

	if (SUCCEED == zbx_tcp_connect(&s, CONFIG_SOURCE_IP, host, port, timeout, ZBX_TCP_SEC_UNENCRYPTED, NULL, NULL))
	{
#ifdef _WINDOWS
		ioctlsocket(s.socket, FIONBIO, &argp);	/* non-zero value sets the socket to non-blocking */
#else
		flags = fcntl(s.socket, F_GETFL);
		if (-1 == flags)
			zabbix_log(LOG_LEVEL_DEBUG, " error in getting the status flag: %s", zbx_strerror(errno));

		if (0 == (flags & O_NONBLOCK) && (-1 == fcntl(s.socket, F_SETFL, flags | O_NONBLOCK)))
		{
			zabbix_log(LOG_LEVEL_DEBUG, " error in setting the status flag: %s",
				zbx_strerror(errno));
		}
#endif
		if (SUCCEED == telnet_test_login(s.socket))
			*value_int = 1;
		else
			zabbix_log(LOG_LEVEL_DEBUG, "Telnet check error: no login prompt");

		zbx_tcp_close(&s);
	}
	else
	{
		zabbix_log(LOG_LEVEL_DEBUG, "%s error: %s", __func__, zbx_socket_strerror());
	}

	return SYSINFO_RET_OK;
}

/* validation functions for service checks */
static int	validate_smtp(const char *line)
{
	if (0 == strncmp(line, "220", 3))
	{
		if ('-' == line[3])
			return ZBX_TCP_EXPECT_IGNORE;

		if ('\0' == line[3] || ' ' == line[3])
			return ZBX_TCP_EXPECT_OK;
	}

	return ZBX_TCP_EXPECT_FAIL;
}

static int	validate_ftp(const char *line)
{
	if (0 == strncmp(line, "220 ", 4))
		return ZBX_TCP_EXPECT_OK;

	return ZBX_TCP_EXPECT_IGNORE;
}

static int	validate_pop(const char *line)
{
	return 0 == strncmp(line, "+OK", 3) ? ZBX_TCP_EXPECT_OK : ZBX_TCP_EXPECT_FAIL;
}

static int	validate_nntp(const char *line)
{
	if (0 == strncmp(line, "200", 3) || 0 == strncmp(line, "201", 3))
		return ZBX_TCP_EXPECT_OK;

	return ZBX_TCP_EXPECT_FAIL;
}

static int	validate_imap(const char *line)
{
	return 0 == strncmp(line, "* OK", 4) ? ZBX_TCP_EXPECT_OK : ZBX_TCP_EXPECT_FAIL;
}

int	check_service(AGENT_REQUEST *request, const char *default_addr, AGENT_RESULT *result, int perf)
{
	unsigned short	port = 0;
	char		*service, *ip_str, ip[MAX_ZBX_DNSNAME_LEN + 1], *port_str;
	int		value_int, ret = SYSINFO_RET_FAIL;
	double		check_time;

	check_time = zbx_time();

	if (3 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		return SYSINFO_RET_FAIL;
	}

	service = get_rparam(request, 0);
	ip_str = get_rparam(request, 1);
	port_str = get_rparam(request, 2);

	if (NULL == service || '\0' == *service)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
		return SYSINFO_RET_FAIL;
	}

	if (NULL == ip_str || '\0' == *ip_str)
		strscpy(ip, default_addr);
	else
		strscpy(ip, ip_str);

	if (NULL != port_str && '\0' != *port_str && SUCCEED != is_ushort(port_str, &port))
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		return SYSINFO_RET_FAIL;
	}

	if (0 == strncmp("net.tcp.service", get_rkey(request), 15))
	{
		if (0 == strcmp(service, "ssh"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_SSH_PORT;
			ret = check_ssh(ip, port, CONFIG_TIMEOUT, &value_int);
		}
		else if (0 == strcmp(service, "ldap"))
		{
#ifdef HAVE_LDAP
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_LDAP_PORT;
			ret = check_ldap(ip, port, CONFIG_TIMEOUT, &value_int);
#else
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Support for LDAP check was not compiled in."));
#endif
		}
		else if (0 == strcmp(service, "smtp"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_SMTP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, validate_smtp, "QUIT\r\n", &value_int);
		}
		else if (0 == strcmp(service, "ftp"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_FTP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, validate_ftp, "QUIT\r\n", &value_int);
		}
		else if (0 == strcmp(service, "http"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_HTTP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, NULL, NULL, &value_int);
		}
		else if (0 == strcmp(service, "pop"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_POP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, validate_pop, "QUIT\r\n", &value_int);
		}
		else if (0 == strcmp(service, "nntp"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_NNTP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, validate_nntp, "QUIT\r\n", &value_int);
		}
		else if (0 == strcmp(service, "imap"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_IMAP_PORT;
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, validate_imap, "a1 LOGOUT\r\n", &value_int);
		}
		else if (0 == strcmp(service, "tcp"))
		{
			if (NULL == port_str || '\0' == *port_str)
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
				return SYSINFO_RET_FAIL;
			}
			ret = tcp_expect(ip, port, CONFIG_TIMEOUT, NULL, NULL, NULL, &value_int);
		}
		else if (0 == strcmp(service, "https"))
		{
#ifdef HAVE_LIBCURL
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_HTTPS_PORT;
			ret = check_https(ip, port, CONFIG_TIMEOUT, &value_int);
#else
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Support for HTTPS check was not compiled in."));
#endif
		}
		else if (0 == strcmp(service, "telnet"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_TELNET_PORT;
			ret = check_telnet(ip, port, CONFIG_TIMEOUT, &value_int);
		}
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
			return ret;
		}
	}
	else	/* net.udp.service */
	{
		if (0 == strcmp(service, "ntp"))
		{
			if (NULL == port_str || '\0' == *port_str)
				port = ZBX_DEFAULT_NTP_PORT;
			ret = check_ntp(ip, port, CONFIG_TIMEOUT, &value_int);
		}
		else
		{
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid first parameter."));
			return ret;
		}
	}

	if (SYSINFO_RET_OK == ret)
	{
		if (0 != perf)
		{
			if (0 != value_int)
			{
				check_time = zbx_time() - check_time;

				if (ZBX_FLOAT_PRECISION > check_time)
					check_time = ZBX_FLOAT_PRECISION;

				SET_DBL_RESULT(result, check_time);
			}
			else
				SET_DBL_RESULT(result, 0.0);
		}
		else
			SET_UI64_RESULT(result, value_int);
	}

	return ret;
}

/* Examples:
 *
 *   net.tcp.service[ssh]
 *   net.tcp.service[smtp,127.0.0.1]
 *   net.tcp.service[ssh,127.0.0.1,22]
 *
 *   net.udp.service[ntp]
 *   net.udp.service[ntp,127.0.0.1]
 *   net.udp.service[ntp,127.0.0.1,123]
 *
 *   net.tcp.service.perf[ssh]
 *   net.tcp.service.perf[smtp,127.0.0.1]
 *   net.tcp.service.perf[ssh,127.0.0.1,22]
 *
 *   net.udp.service.perf[ntp]
 *   net.udp.service.perf[ntp,127.0.0.1]
 *   net.udp.service.perf[ntp,127.0.0.1,123]
 *
 * The old name for these checks is check_service[*].
 */

int	CHECK_SERVICE(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	return check_service(request, "127.0.0.1", result, 0);
}

int	CHECK_SERVICE_PERF(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	return check_service(request, "127.0.0.1", result, 1);
}
