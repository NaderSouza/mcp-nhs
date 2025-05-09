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

#include "checks_java.h"

#include "log.h"
#include "zbxjson.h"

static int	parse_response(AGENT_RESULT *results, int *errcodes, int num, char *response,
		char *error, int max_error_len)
{
	const char		*p;
	struct zbx_json_parse	jp, jp_data, jp_row;
	char			*value = NULL;
	size_t			value_alloc = 0;
	int			i, ret = GATEWAY_ERROR;

	if (SUCCEED == zbx_json_open(response, &jp))
	{
		if (SUCCEED != zbx_json_value_by_name_dyn(&jp, ZBX_PROTO_TAG_RESPONSE, &value, &value_alloc, NULL))
		{
			zbx_snprintf(error, max_error_len, "No '%s' tag in received JSON", ZBX_PROTO_TAG_RESPONSE);
			goto exit;
		}

		if (0 == strcmp(value, ZBX_PROTO_VALUE_SUCCESS))
		{
			if (SUCCEED != zbx_json_brackets_by_name(&jp, ZBX_PROTO_TAG_DATA, &jp_data))
			{
				zbx_strlcpy(error, "Cannot open data array in received JSON", max_error_len);
				goto exit;
			}

			p = NULL;

			for (i = 0; i < num; i++)
			{
				if (SUCCEED != errcodes[i])
					continue;

				if (NULL == (p = zbx_json_next(&jp_data, p)))
				{
					zbx_strlcpy(error, "Not all values included in received JSON", max_error_len);
					goto exit;
				}

				if (SUCCEED != zbx_json_brackets_open(p, &jp_row))
				{
					zbx_strlcpy(error, "Cannot open value object in received JSON", max_error_len);
					goto exit;
				}

				if (SUCCEED == zbx_json_value_by_name_dyn(&jp_row, ZBX_PROTO_TAG_VALUE, &value,
						&value_alloc, NULL))
				{
					set_result_type(&results[i], ITEM_VALUE_TYPE_TEXT, value);
					errcodes[i] = SUCCEED;
				}
				else if (SUCCEED == zbx_json_value_by_name_dyn(&jp_row, ZBX_PROTO_TAG_ERROR, &value,
						&value_alloc, NULL))
				{
					SET_MSG_RESULT(&results[i], zbx_strdup(NULL, value));
					errcodes[i] = NOTSUPPORTED;
				}
				else
				{
					SET_MSG_RESULT(&results[i], zbx_strdup(NULL, "Cannot get item value or error message"));
					errcodes[i] = AGENT_ERROR;
				}
			}

			ret = SUCCEED;
		}
		else if (0 == strcmp(value, ZBX_PROTO_VALUE_FAILED))
		{
			if (SUCCEED == zbx_json_value_by_name(&jp, ZBX_PROTO_TAG_ERROR, error, max_error_len, NULL))
				ret = NETWORK_ERROR;
			else
				zbx_strlcpy(error, "Cannot get error message describing reasons for failure", max_error_len);

			goto exit;
		}
		else
		{
			zbx_snprintf(error, max_error_len, "Bad '%s' tag value '%s' in received JSON",
					ZBX_PROTO_TAG_RESPONSE, value);
			goto exit;
		}
	}
	else
	{
		zbx_strlcpy(error, "Cannot open received JSON", max_error_len);
		goto exit;
	}
exit:
	zbx_free(value);

	return ret;
}

int	get_value_java(unsigned char request, const DC_ITEM *item, AGENT_RESULT *result)
{
	int	errcode = SUCCEED;

	get_values_java(request, item, result, &errcode, 1);

	return errcode;
}

void	get_values_java(unsigned char request, const DC_ITEM *items, AGENT_RESULT *results, int *errcodes, int num)
{
	zbx_socket_t	s;
	struct zbx_json	json;
	char		error[MAX_STRING_LEN];
	int		i, j, err = SUCCEED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() jmx_endpoint:'%s' num:%d", __func__, items[0].jmx_endpoint, num);

	for (j = 0; j < num; j++)	/* locate first supported item to use as a reference */
	{
		if (SUCCEED == errcodes[j])
			break;
	}

	if (j == num)	/* all items already NOTSUPPORTED (with invalid key or port) */
		goto out;

	zbx_json_init(&json, ZBX_JSON_STAT_BUF_LEN);

	if (NULL == CONFIG_JAVA_GATEWAY || '\0' == *CONFIG_JAVA_GATEWAY)
	{
		err = GATEWAY_ERROR;
		strscpy(error, "JavaGateway configuration parameter not set or empty");
		goto exit;
	}

	if (ZBX_JAVA_GATEWAY_REQUEST_INTERNAL == request)
	{
		zbx_json_addstring(&json, ZBX_PROTO_TAG_REQUEST, ZBX_PROTO_VALUE_JAVA_GATEWAY_INTERNAL,
				ZBX_JSON_TYPE_STRING);
	}
	else if (ZBX_JAVA_GATEWAY_REQUEST_JMX == request)
	{
		for (i = j + 1; i < num; i++)
		{
			if (SUCCEED != errcodes[i])
				continue;

			if (0 != strcmp(items[j].username, items[i].username) ||
					0 != strcmp(items[j].password, items[i].password) ||
					0 != strcmp(items[j].jmx_endpoint, items[i].jmx_endpoint))
			{
				err = GATEWAY_ERROR;
				strscpy(error, "Java poller received items with different connection parameters");
				goto exit;
			}
		}

		zbx_json_addstring(&json, ZBX_PROTO_TAG_REQUEST, ZBX_PROTO_VALUE_JAVA_GATEWAY_JMX, ZBX_JSON_TYPE_STRING);

		if ('\0' != *items[j].username)
		{
			zbx_json_addstring(&json, ZBX_PROTO_TAG_USERNAME, items[j].username, ZBX_JSON_TYPE_STRING);
		}
		if ('\0' != *items[j].password)
		{
			zbx_json_addstring(&json, ZBX_PROTO_TAG_PASSWORD, items[j].password, ZBX_JSON_TYPE_STRING);
		}
		if ('\0' != *items[j].jmx_endpoint)
		{
			zbx_json_addstring(&json, ZBX_PROTO_TAG_JMX_ENDPOINT, items[j].jmx_endpoint,
					ZBX_JSON_TYPE_STRING);
		}
	}
	else
		assert(0);

	zbx_json_addarray(&json, ZBX_PROTO_TAG_KEYS);
	for (i = j; i < num; i++)
	{
		if (SUCCEED != errcodes[i])
			continue;

		zbx_json_addstring(&json, NULL, items[i].key, ZBX_JSON_TYPE_STRING);
	}
	zbx_json_close(&json);

	if (SUCCEED == (err = zbx_tcp_connect(&s, CONFIG_SOURCE_IP, CONFIG_JAVA_GATEWAY, CONFIG_JAVA_GATEWAY_PORT,
			CONFIG_TIMEOUT, ZBX_TCP_SEC_UNENCRYPTED, NULL, NULL)))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "JSON before sending [%s]", json.buffer);

		if (SUCCEED == (err = zbx_tcp_send(&s, json.buffer)))
		{
			if (SUCCEED == (err = zbx_tcp_recv(&s)))
			{
				zabbix_log(LOG_LEVEL_DEBUG, "JSON back [%s]", s.buffer);

				err = parse_response(results, errcodes, num, s.buffer, error, sizeof(error));
			}
		}

		zbx_tcp_close(&s);
	}

	zbx_json_free(&json);

	if (FAIL == err)
	{
		strscpy(error, zbx_socket_strerror());
		err = GATEWAY_ERROR;
	}
exit:
	if (NETWORK_ERROR == err || GATEWAY_ERROR == err)
	{
		zabbix_log(LOG_LEVEL_DEBUG, "getting Java values failed: %s", error);

		for (i = j; i < num; i++)
		{
			if (SUCCEED != errcodes[i])
				continue;

			SET_MSG_RESULT(&results[i], zbx_strdup(NULL, error));
			errcodes[i] = err;
		}
	}
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}
