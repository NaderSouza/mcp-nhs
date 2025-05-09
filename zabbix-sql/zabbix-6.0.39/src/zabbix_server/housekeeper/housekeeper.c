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

#include "housekeeper.h"
#include "log.h"
#include "daemon.h"
#include "zbxself.h"
#include "zbxserver.h"
#include "zbxrtc.h"

#include "history_compress.h"
#include "../../libs/zbxdbcache/valuecache.h"


extern ZBX_THREAD_LOCAL unsigned char	process_type;
extern unsigned char			program_type;
extern ZBX_THREAD_LOCAL int		server_num, process_num;
extern struct zbx_db_version_info_t	db_version_info;

#if defined(HAVE_POSTGRESQL)
extern int	ZBX_TSDB_VERSION;
#endif

static int	hk_period;

#define HK_INITIAL_DELETE_QUEUE_SIZE	4096

/* the maximum number of housekeeping periods to be removed per single housekeeping cycle */
#define HK_MAX_DELETE_PERIODS		4

#define HK_MIN_CLOCK_UNDEFINED		0
#define HK_MIN_CLOCK_ALWAYS_RECHECK	-1

/* global configuration data containing housekeeping configuration */
static zbx_config_t	cfg;

/* Housekeeping rule definition.                                */
/* A housekeeping rule describes table from which records older */
/* than history setting must be removed according to optional   */
/* filter.                                                      */
typedef struct
{
	/* target table name */
	const char	*table;

	/* ID field name, required to select IDs of records that must be deleted */
	char	*field_name;

	/* Optional filter, must be empty string if not used. Only the records matching */
	/* filter are subject to housekeeping procedures.                               */
	const char	*filter;

	/* The oldest record in table (with filter in effect). The min_clock value is   */
	/* read from the database when accessed for the first time and then during      */
	/* housekeeping procedures updated to the last 'cutoff' value.                  */
	int		min_clock;

	/* a reference to the settings value specifying number of seconds the records must be kept */
	int		*phistory;
}
zbx_hk_rule_t;

/* housekeeper table => configuration data mapping.                       */
/* This structure is used to map table names used in housekeeper table to */
/* configuration data.                                                    */
typedef struct
{
	/* housekeeper table name */
	const char		*name;

	/* a reference to housekeeping configuration enable value for this table */
	unsigned char		*poption_mode;

	/* a reference to the housekeeping configuration overwrite option for this table */
	unsigned char		*poption_global;
}
zbx_hk_cleanup_table_t;

static unsigned char poption_mode_regular	= ZBX_HK_MODE_REGULAR;
static unsigned char poption_global_disabled	= ZBX_HK_OPTION_DISABLED;

/* Housekeeper table mapping to housekeeping configuration values.    */
/* This mapping is used to exclude disabled tables from housekeeping  */
/* cleanup procedure.                                                 */
static zbx_hk_cleanup_table_t	hk_cleanup_tables[] = {
	{"history",		&cfg.hk.history_mode,	&cfg.hk.history_global},
	{"history_log",		&cfg.hk.history_mode,	&cfg.hk.history_global},
	{"history_str",		&cfg.hk.history_mode,	&cfg.hk.history_global},
	{"history_text",	&cfg.hk.history_mode,	&cfg.hk.history_global},
	{"history_uint",	&cfg.hk.history_mode,	&cfg.hk.history_global},
	{"trends",		&cfg.hk.trends_mode,	&cfg.hk.trends_global},
	{"trends_uint",		&cfg.hk.trends_mode,	&cfg.hk.trends_global},
	/* force events housekeeping mode on to perform problem cleanup when events housekeeping is disabled */
	{"events",		&poption_mode_regular,	&poption_global_disabled},
	{NULL}
};

/* trends table offsets in the hk_cleanup_tables[] mapping  */
#define HK_UPDATE_CACHE_OFFSET_TREND_FLOAT	ITEM_VALUE_TYPE_MAX
#define HK_UPDATE_CACHE_OFFSET_TREND_UINT	(HK_UPDATE_CACHE_OFFSET_TREND_FLOAT + 1)
#define HK_UPDATE_CACHE_TREND_COUNT		2

/* the oldest record timestamp cache for items in history tables */
typedef struct
{
	zbx_uint64_t	itemid;
	int		min_clock;
}
zbx_hk_item_cache_t;

/* Delete queue item definition.                                     */
/* The delete queue item defines an item that should be processed by */
/* housekeeping procedure (records older than min_clock seconds      */
/* must be removed from database).                                   */
typedef struct
{
	zbx_uint64_t	itemid;
	int		min_clock;
}
zbx_hk_delete_queue_t;

/* this structure is used to remove old records from history (trends) tables */
typedef struct
{
	/* the target table name */
	const char		*table;

	/* history setting field name in items table (history|trends) */
	const char		*history;

	/* a reference to the housekeeping configuration mode (enable) option for this table */
	unsigned char		*poption_mode;

	/* a reference to the housekeeping configuration overwrite option for this table */
	unsigned char		*poption_global;

	/* a reference to the housekeeping configuration history value for this table */
	int			*poption;

	/* type for checking which values are sent to the history storage */
	unsigned char		type;

	/* the oldest item record timestamp cache for target table */
	zbx_hashset_t		item_cache;

	/* the item delete queue */
	zbx_vector_ptr_t	delete_queue;
}
zbx_hk_history_rule_t;

/* The history item rules, used for housekeeping history and trends tables */
/* The order of the rules must match the order of value types in zbx_item_value_type_t. */
static zbx_hk_history_rule_t	hk_history_rules[] = {
	{.table = "history",		.history = "history",	.poption_mode = &cfg.hk.history_mode,
			.poption_global = &cfg.hk.history_global,	.poption = &cfg.hk.history,
			.type = ITEM_VALUE_TYPE_FLOAT},
	{.table = "history_str",	.history = "history",	.poption_mode = &cfg.hk.history_mode,
			.poption_global = &cfg.hk.history_global,	.poption = &cfg.hk.history,
			.type = ITEM_VALUE_TYPE_STR},
	{.table = "history_log",	.history = "history",	.poption_mode = &cfg.hk.history_mode,
			.poption_global = &cfg.hk.history_global,	.poption = &cfg.hk.history,
			.type = ITEM_VALUE_TYPE_LOG},
	{.table = "history_uint",	.history = "history",	.poption_mode = &cfg.hk.history_mode,
			.poption_global = &cfg.hk.history_global,	.poption = &cfg.hk.history,
			.type = ITEM_VALUE_TYPE_UINT64},
	{.table = "history_text",	.history = "history",	.poption_mode = &cfg.hk.history_mode,
			.poption_global = &cfg.hk.history_global,	.poption = &cfg.hk.history,
			.type = ITEM_VALUE_TYPE_TEXT},
	{.table = "trends",		.history = "trends",	.poption_mode = &cfg.hk.trends_mode,
			.poption_global = &cfg.hk.trends_global,	.poption = &cfg.hk.trends,
			.type = ITEM_VALUE_TYPE_FLOAT},
	{.table = "trends_uint",	.history = "trends",	.poption_mode = &cfg.hk.trends_mode,
			.poption_global = &cfg.hk.trends_global,	.poption = &cfg.hk.trends,
			.type = ITEM_VALUE_TYPE_UINT64},
	{NULL}
};

/******************************************************************************
 *                                                                            *
 * Purpose: compare two delete queue items by their itemid                    *
 *                                                                            *
 * Parameters: d1 - [IN] the first delete queue item to compare               *
 *             d2 - [IN] the second delete queue item to compare              *
 *                                                                            *
 * Return value: <0 - the first item is less than the second                  *
 *               >0 - the first item is greater than the second               *
 *               =0 - the items are the same                                  *
 *                                                                            *
 * Comments: this function is used to sort delete queue by itemids            *
 *                                                                            *
 ******************************************************************************/
static int	hk_item_update_cache_compare(const void *d1, const void *d2)
{
	zbx_hk_delete_queue_t	*r1 = *(zbx_hk_delete_queue_t **)d1;
	zbx_hk_delete_queue_t	*r2 = *(zbx_hk_delete_queue_t **)d2;

	ZBX_RETURN_IF_NOT_EQUAL(r1->itemid, r2->itemid);

	return 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: add item to the delete queue if necessary                         *
 *                                                                            *
 * Parameters: rule        - [IN/OUT] the history housekeeping rule           *
 *             now         - [IN] the current timestamp                       *
 *             item_record - [IN/OUT] the record from item cache containing   *
 *                           item to process and its oldest record timestamp  *
 *             history     - [IN] a number of seconds the history data for    *
 *                           item_record must be kept.                        *
 *                                                                            *
 * Comments: If item is added to delete queue, its oldest record timestamp    *
 *           (min_clock) is updated to the calculated 'cutoff' value.         *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_delete_queue_append(zbx_hk_history_rule_t *rule, int now,
		zbx_hk_item_cache_t *item_record, int history)
{
	int	keep_from;

	if (history > now)
		return;	/* there shouldn't be any records with negative timestamps, nothing to do */

	keep_from = now - history;

	if (keep_from > item_record->min_clock)
	{
		zbx_hk_delete_queue_t	*update_record;

		/* update oldest timestamp in item cache */
		item_record->min_clock = MIN(keep_from, item_record->min_clock + HK_MAX_DELETE_PERIODS * hk_period);

		update_record = (zbx_hk_delete_queue_t *)zbx_malloc(NULL, sizeof(zbx_hk_delete_queue_t));
		update_record->itemid = item_record->itemid;
		update_record->min_clock = item_record->min_clock;
		zbx_vector_ptr_append(&rule->delete_queue, update_record);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: prepares history housekeeping rule                                *
 *                                                                            *
 * Parameters: rule        - [IN/OUT] the history housekeeping rule           *
 *                                                                            *
 * Comments: This function is called to initialize history rule data either   *
 *           at start or when housekeeping is enabled for this rule.          *
 *           It caches item history data and also prepares delete queue to be *
 *           processed during the first run.                                  *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_prepare(zbx_hk_history_rule_t *rule)
{
	DB_RESULT	result;
	DB_ROW		row;

	zbx_hashset_create(&rule->item_cache, 1024, ZBX_DEFAULT_UINT64_HASH_FUNC, ZBX_DEFAULT_UINT64_COMPARE_FUNC);

	zbx_vector_ptr_create(&rule->delete_queue);
	zbx_vector_ptr_reserve(&rule->delete_queue, HK_INITIAL_DELETE_QUEUE_SIZE);

	result = DBselect("select itemid,min(clock) from %s group by itemid", rule->table);

	while (NULL != (row = DBfetch(result)))
	{
		zbx_uint64_t		itemid;
		int			min_clock;
		zbx_hk_item_cache_t	item_record;

		ZBX_STR2UINT64(itemid, row[0]);
		min_clock = atoi(row[1]);

		item_record.itemid = itemid;
		item_record.min_clock = min_clock;

		zbx_hashset_insert(&rule->item_cache, &item_record, sizeof(zbx_hk_item_cache_t));
	}

	DBfree_result(result);
}

/******************************************************************************
 *                                                                            *
 * Purpose: releases history housekeeping rule                                *
 *                                                                            *
 * Parameters: rule  - [IN/OUT] the history housekeeping rule                 *
 *                                                                            *
 * Comments: This function is called to release resources allocated by        *
 *           history housekeeping rule after housekeeping was disabled        *
 *           for the table referred by this rule.                             *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_release(zbx_hk_history_rule_t *rule)
{
	if (0 == rule->item_cache.num_slots)
		return;

	zbx_hashset_destroy(&rule->item_cache);
	zbx_vector_ptr_destroy(&rule->delete_queue);
}

/******************************************************************************
 *                                                                            *
 * Purpose: updates history housekeeping rule with item history setting and   *
 *          adds item to the delete queue if necessary                        *
 *                                                                            *
 * Parameters: rules    - [IN] housekeeping rules to check                    *
 *             count    - [IN] number of housekeeping rules to check          *
 *             rule_add - [IN] active housekeeping rule for this item         *
 *             now      - [IN] the current timestamp                          *
 *             itemid   - [IN] the item to update                             *
 *             history  - [IN] the number of seconds the item data            *
 *                             should be kept in history/trends               *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_item_update(zbx_hk_history_rule_t *rules, int count, const zbx_hk_history_rule_t *rule_add,
		int now, zbx_uint64_t itemid, int history)
{
	zbx_hk_history_rule_t	*rule;

	/* item can be cached in multiple rules when value type has been changed */
	for (rule = rules; rule - rules < count; rule++)
	{
		zbx_hk_item_cache_t	*item_record;

		if (0 == rule->item_cache.num_slots)
			continue;

		if (NULL == (item_record = (zbx_hk_item_cache_t *)zbx_hashset_search(&rule->item_cache, &itemid)))
		{
			zbx_hk_item_cache_t	item_data = {itemid, now};

			if (rule_add != rule)
				continue;

			if (NULL == (item_record = (zbx_hk_item_cache_t *)zbx_hashset_insert(&rule->item_cache,
					&item_data, sizeof(zbx_hk_item_cache_t))))
			{
				continue;
			}
		}

		hk_history_delete_queue_append(rule, now, item_record, history);
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: updates history housekeeping rule with the latest item history    *
 *          settings and prepares delete queue                                *
 *                                                                            *
 * Parameters: rule  - [IN/OUT] the history housekeeping rule                 *
 *             now   - [IN] the current timestamp                             *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_update(zbx_hk_history_rule_t *rules, int now)
{
	DB_RESULT	result;
	DB_ROW		row;
	char		*tmp = NULL;

	result = DBselect(
			"select i.itemid,i.value_type,i.history,i.trends,h.hostid"
			" from items i,hosts h"
			" where i.flags in (%d,%d)"
				" and i.hostid=h.hostid"
				" and h.status in (%d,%d)",
			ZBX_FLAG_DISCOVERY_NORMAL, ZBX_FLAG_DISCOVERY_CREATED,
			HOST_STATUS_MONITORED, HOST_STATUS_NOT_MONITORED);

	while (NULL != (row = DBfetch(result)))
	{
		zbx_uint64_t		itemid, hostid;
		int			history, trends, value_type;
		zbx_hk_history_rule_t	*rule, *rule_add;

		ZBX_STR2UINT64(itemid, row[0]);
		value_type = atoi(row[1]);
		ZBX_STR2UINT64(hostid, row[4]);

		if (value_type < ITEM_VALUE_TYPE_MAX &&
				ZBX_HK_MODE_REGULAR == *(rule = rules + value_type)->poption_mode)
		{
			tmp = zbx_strdup(tmp, row[2]);
			substitute_simple_macros(NULL, NULL, NULL, NULL, &hostid, NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, &tmp, MACRO_TYPE_COMMON, NULL, 0);

			if (SUCCEED != is_time_suffix(tmp, &history, ZBX_LENGTH_UNLIMITED))
			{
				zabbix_log(LOG_LEVEL_WARNING, "invalid history storage period '%s' for itemid '%s'",
						tmp, row[0]);
				continue;
			}

			if (0 != history && (ZBX_HK_HISTORY_MIN > history || ZBX_HK_PERIOD_MAX < history))
			{
				zabbix_log(LOG_LEVEL_WARNING, "invalid history storage period for itemid '%s'", row[0]);
				continue;
			}

			if (0 != history && ZBX_HK_OPTION_DISABLED != *rule->poption_global)
				history = *rule->poption;

			hk_history_item_update(rules, ITEM_VALUE_TYPE_MAX, rule, now, itemid, history);
		}

		/* trend rules are shared between all trend types, so we can default to floating type */
		rule = &rules[HK_UPDATE_CACHE_OFFSET_TREND_FLOAT];

		if (ZBX_HK_MODE_REGULAR != *rule->poption_mode)
			continue;

		if (ITEM_VALUE_TYPE_FLOAT == value_type || ITEM_VALUE_TYPE_UINT64 == value_type)
		{
			if (value_type == ITEM_VALUE_TYPE_UINT64)
				rule_add = &rules[HK_UPDATE_CACHE_OFFSET_TREND_UINT];
			else
				rule_add = rule;

			tmp = zbx_strdup(tmp, row[3]);
			substitute_simple_macros(NULL, NULL, NULL, NULL, &hostid, NULL, NULL, NULL, NULL, NULL, NULL,
					NULL, &tmp, MACRO_TYPE_COMMON, NULL, 0);

			if (SUCCEED != is_time_suffix(tmp, &trends, ZBX_LENGTH_UNLIMITED))
			{
				zabbix_log(LOG_LEVEL_WARNING, "invalid trends storage period '%s' for itemid '%s'",
						tmp, row[0]);
				continue;
			}
			else if (0 != trends && (ZBX_HK_TRENDS_MIN > trends || ZBX_HK_PERIOD_MAX < trends))
			{
				zabbix_log(LOG_LEVEL_WARNING, "invalid trends storage period for itemid '%s'", row[0]);
				continue;
			}
		}
		else
		{
			/* if item type was changed from numeric to non-numeric use default trends */
			/* storage period for old trends data removal                              */
			trends = SEC_PER_DAY * 365;

			rule_add = NULL;
		}

		if (0 != trends && ZBX_HK_OPTION_DISABLED != *rule->poption_global)
			trends = *rule->poption;

		hk_history_item_update(rules + HK_UPDATE_CACHE_OFFSET_TREND_FLOAT, HK_UPDATE_CACHE_TREND_COUNT,
				rule_add, now, itemid, trends);
	}
	DBfree_result(result);

	zbx_free(tmp);
}

/******************************************************************************
 *                                                                            *
 * Purpose: prepares history housekeeping delete queues for all defined       *
 *          history rules.                                                    *
 *                                                                            *
 * Parameters: rules  - [IN/OUT] the history housekeeping rules               *
 *             now    - [IN] the current timestamp                            *
 *                                                                            *
 * Comments: This function also handles history rule initializing/releasing   *
 *           when the rule just became enabled/disabled.                      *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_delete_queue_prepare_all(zbx_hk_history_rule_t *rules, int now)
{
	zbx_hk_history_rule_t	*rule;
	unsigned char		items_update = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	/* prepare history item cache (hashset containing itemid:min_clock values) */
	for (rule = rules; NULL != rule->table; rule++)
	{
		if (ZBX_HK_MODE_REGULAR == *rule->poption_mode)
		{
			if (0 == rule->item_cache.num_slots)
				hk_history_prepare(rule);

			items_update = 1;
		}
		else if (0 != rule->item_cache.num_slots)
			hk_history_release(rule);
	}

	/* Since we maintain two separate global period settings - for history and for trends */
	/* we need to scan items table if either of these is off. Thus setting both global periods */
	/* to override is very beneficial for performance. */
	if (0 != items_update)
		hk_history_update(rules, now);	/* scan items and update min_clock using per item settings */

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

/******************************************************************************
 *                                                                            *
 * Purpose: clears the history housekeeping delete queue                      *
 *                                                                            *
 * Parameters: rule   - [IN/OUT] the history housekeeping rule                *
 *             now    - [IN] the current timestamp                            *
 *                                                                            *
 ******************************************************************************/
static void	hk_history_delete_queue_clear(zbx_hk_history_rule_t *rule)
{
	zbx_vector_ptr_clear_ext(&rule->delete_queue, zbx_ptr_free);
}

/******************************************************************************
 *                                                                            *
 * Purpose: drop appropriate partitions from the history and trends tables    *
 *                                                                            *
 * Parameters: rules - [IN/OUT] history housekeeping rules                    *
 *             now   - [IN] the current timestamp                             *
 *                                                                            *
 ******************************************************************************/
static void	hk_drop_partition_for_rule(zbx_hk_history_rule_t *rule, int now)
{
#if defined(HAVE_POSTGRESQL)
	int		history_seconds;
	DB_RESULT	result;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() now:%d", __func__, now);

	history_seconds = *rule->poption;

	if (0 == history_seconds)
	{
		zabbix_log(LOG_LEVEL_TRACE, "%s: table=%s delete all", __func__, rule->table);

		result = DBselect(1 == ZBX_DB_TSDB_V1 ?
				"select drop_chunks(table_name=>'%s',newer_than=>0)" :
				"select drop_chunks(relation=>'%s',newer_than=>0)",
				rule->table);
	}
	else
	{
		int	keep_from;

		if (ZBX_HK_HISTORY_MIN > history_seconds || ZBX_HK_PERIOD_MAX < history_seconds)
		{
			zabbix_log(LOG_LEVEL_WARNING, "invalid history storage period for table '%s'", rule->table);
			goto out;
		}

		keep_from = now - history_seconds;

		zabbix_log(LOG_LEVEL_TRACE, "%s: table=%s keep_from=%d", __func__, rule->table, keep_from);

		result = DBselect(1 == ZBX_DB_TSDB_V1 ?
				"select drop_chunks(table_name=>'%s',older_than=>%d)" :
				"select drop_chunks(relation=>'%s',older_than=>%d)",
				rule->table, keep_from);
	}

	if (NULL == result)
		zabbix_log(LOG_LEVEL_ERR, "cannot drop chunks for %s", rule->table);
	else
		DBfree_result(result);
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);

	return;
#else
	ZBX_UNUSED(rule);
	ZBX_UNUSED(now);
#endif
}

#if defined(HAVE_POSTGRESQL)
static void	hk_update_dbversion_status(void)
{
	struct zbx_json	db_version_json;

	zbx_json_initarray(&db_version_json, ZBX_JSON_STAT_BUF_LEN);

	zbx_tsdb_update_dbversion_info(&db_version_info);

	zbx_db_version_json_create(&db_version_json, &db_version_info);
	zbx_db_flush_version_requirements(db_version_json.buffer);

	zbx_json_free(&db_version_json);
}
#endif

/******************************************************************************
 *                                                                            *
 * Purpose: performs housekeeping for history and trends tables               *
 *                                                                            *
 * Parameters: now    - [IN] the current timestamp                            *
 *                                                                            *
 ******************************************************************************/

static int	housekeeping_history_and_trends(int now)
{
	int				deleted = 0, i, rc;
	zbx_hk_history_rule_t		*rule;
#if defined(HAVE_POSTGRESQL)
	int				ignore_history = 0, ignore_trends = 0;
#endif
	zabbix_log(LOG_LEVEL_DEBUG, "In %s() now:%d", __func__, now);

	/* prepare delete queues for all history housekeeping rules */
	hk_history_delete_queue_prepare_all(hk_history_rules, now);

#if defined(HAVE_POSTGRESQL)
	if (ZBX_TSDB_VERSION > 0)
	{
		hk_update_dbversion_status();
	}
#endif

	/* Loop through the history rules. Each rule is a history table (such as history_log, trends_uint, etc) */
	/* we need to clear records from */
	for (rule = hk_history_rules; NULL != rule->table; rule++)
	{
		if (ZBX_HK_MODE_DISABLED == *rule->poption_mode)
			goto skip;

		/* If partitioning enabled for history and/or trends then drop partitions with expired history.  */
		/* ZBX_HK_MODE_PARTITION is set during configuration sync based on the following: */
		/* 1. "Override item history (or trend) period" must be on 2. DB must be PostgreSQL */
		/* 3. config.db.extension must be set to "timescaledb" */
		if (ZBX_HK_MODE_PARTITION == *rule->poption_mode)
		{
			hk_drop_partition_for_rule(rule, now);
			goto skip;
		}

#if defined(HAVE_POSTGRESQL)
		if (ZBX_TSDB_VERSION > 0)
		{
			if (0 == strcmp(rule->history, "history"))
			{
				if (1 == ignore_history)
					goto skip;

				if (1 == db_version_info.history_compressed_chunks)
				{
					zabbix_log(LOG_LEVEL_WARNING, "Unable to perform housekeeping for history "
							"tables due to having compressed chunks and disabled item history period override.");

					ignore_history = 1;
					goto skip;
				}
			}
			else if (0 == strcmp(rule->history, "trends"))
			{
				if (1 == ignore_trends)
					goto skip;

				if (1 == db_version_info.trends_compressed_chunks)
				{
					zabbix_log(LOG_LEVEL_WARNING, "Unable to perform housekeeping for trends "
							"tables due to having compressed chunks and disabled item trends period override.");

					ignore_trends = 1;
					goto skip;
				}
			}
		}
#endif

		/* process delete queue for the housekeeping rule */

		zbx_vector_ptr_sort(&rule->delete_queue, hk_item_update_cache_compare);

		for (i = 0; i < rule->delete_queue.values_num; i++)
		{
			zbx_hk_delete_queue_t	*item_record = (zbx_hk_delete_queue_t *)rule->delete_queue.values[i];

			rc = DBexecute("delete from %s where itemid=" ZBX_FS_UI64 " and clock<%d",
					rule->table, item_record->itemid, item_record->min_clock);
			if (ZBX_DB_OK < rc)
				deleted += rc;
		}

skip:
		/* clear history rule delete queue so it's ready for the next housekeeping cycle */
		hk_history_delete_queue_clear(rule);
	}

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

/******************************************************************************
 *                                                                            *
 * Purpose: removes old records from a table according to the specified rule  *
 *                                                                            *
 * Parameters: now  - [IN] the current time in seconds                        *
 *             rule - [IN/OUT] the housekeeping rule specifying table to      *
 *                    clean and the required data (fields, filters, time)     *
 *                                                                            *
 * Return value: the number of deleted records                                *
 *                                                                            *
 ******************************************************************************/
static int	housekeeping_process_rule(int now, zbx_hk_rule_t *rule)
{
	DB_RESULT	result;
	DB_ROW		row;
	int		keep_from, id_field_str_type, deleted = 0, min_clock = rule->min_clock;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() table:'%s' field_name:'%s' filter:'%s' min_clock:%d now:%d",
			__func__, rule->table, rule->field_name, rule->filter, rule->min_clock, now);

	/* NOTE: Do not forget to add here tables whose id column is string-type.                    */
	/* Now only audit field has string id, if in the future this list of exceptions is increased */
	/* DBget_table() and DBget_field() functions with cache could be used to determine if string */
	/* or int version is required.                                                               */
	id_field_str_type = (0 == strcmp("auditid", rule->field_name)) ? 1 : 0;

	/* initialize min_clock with the oldest record timestamp from database */
	if (HK_MIN_CLOCK_ALWAYS_RECHECK == min_clock || HK_MIN_CLOCK_UNDEFINED == min_clock)
	{
		result = DBselect("select min(clock) from %s%s%s", rule->table,
				('\0' != *rule->filter ? " where " : ""), rule->filter);
		if (NULL != (row = DBfetch(result)) && SUCCEED != DBis_null(row[0]))
			min_clock = atoi(row[0]);
		else
			min_clock = now;

		DBfree_result(result);
	}

	/* Delete the old records from database. Don't remove more than 4 x housekeeping */
	/* periods worth of data to prevent database stalling.                           */
	keep_from = now - *rule->phistory;
	if (keep_from > min_clock)
	{
		char			buffer[MAX_STRING_LEN];
		char			*sql = NULL;
		size_t			sql_alloc = 0, sql_offset;
		zbx_vector_uint64_t	ids_uint64;
		zbx_vector_str_t	ids_str;
		int			ret;

		if (0 == id_field_str_type)
			zbx_vector_uint64_create(&ids_uint64);
		else
			zbx_vector_str_create(&ids_str);

		min_clock = MIN(keep_from, min_clock + HK_MAX_DELETE_PERIODS * hk_period);

		zbx_snprintf(buffer, sizeof(buffer),
			"select %s"
			" from %s"
			" where clock<%d%s%s"
			" order by %s",
			rule->field_name, rule->table, min_clock, '\0' != *rule->filter ? " and " : "", rule->filter,
			rule->field_name);

		while (1)
		{
			/* Select IDs of records that must be deleted, this allows to avoid locking for every   */
			/* record the search encounters when using delete statement, thus eliminates deadlocks. */
			if (0 == CONFIG_MAX_HOUSEKEEPER_DELETE)
				result = DBselect("%s", buffer);
			else
				result = DBselectN(buffer, CONFIG_MAX_HOUSEKEEPER_DELETE);

			while (NULL != (row = DBfetch(result)))
			{
				if (0 == id_field_str_type)
				{
					zbx_uint64_t	id;

					ZBX_STR2UINT64(id, row[0]);
					zbx_vector_uint64_append(&ids_uint64, id);
				}
				else
					zbx_vector_str_append(&ids_str, zbx_strdup(NULL, row[0]));
			}

			DBfree_result(result);

			if (0 == id_field_str_type)
			{
				if (0 == ids_uint64.values_num)
					break;
			}
			else
			{
				if (0 == ids_str.values_num)
					break;
			}

			sql_offset = 0;
			zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, "delete from %s where", rule->table);

			if (0 == id_field_str_type)
			{
				DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, rule->field_name,
						ids_uint64.values, ids_uint64.values_num);
			}
			else
			{
				DBadd_str_condition_alloc(&sql, &sql_alloc, &sql_offset, rule->field_name,
						(const char**)ids_str.values, ids_str.values_num);
			}

			ret = DBexecute("%s", sql);

			if (0 == id_field_str_type)
				zbx_vector_uint64_clear(&ids_uint64);
			else
				zbx_vector_str_clear_ext(&ids_str, zbx_str_free);

			if (ZBX_DB_OK > ret)
				break;

			deleted += ret;
		}

		zbx_free(sql);

		if (0 == id_field_str_type)
			zbx_vector_uint64_destroy(&ids_uint64);
		else
			zbx_vector_str_destroy(&ids_str);
	}

	if (HK_MIN_CLOCK_ALWAYS_RECHECK != rule->min_clock)
		rule->min_clock = min_clock;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

/******************************************************************************
 *                                                                            *
 * Purpose: delete limited count of rows from table                           *
 *                                                                            *
 * Return value: number of deleted rows or less than 0 if an error occurred   *
 *                                                                            *
 ******************************************************************************/
static int	DBdelete_from_table(const char *tablename, const char *filter, int limit)
{
	if (0 == limit)
	{
		return DBexecute(
				"delete from %s"
				" where %s",
				tablename,
				filter);
	}
	else
	{
#if defined(HAVE_ORACLE)
		return DBexecute(
				"delete from %s"
				" where %s"
					" and rownum<=%d",
				tablename,
				filter,
				limit);
#elif defined(HAVE_MYSQL)
		return DBexecute(
				"delete from %s"
				" where %s limit %d",
				tablename,
				filter,
				limit);
#elif defined(HAVE_POSTGRESQL)
		return DBexecute(
				"delete from %s"
				" where %s and ctid = any(array(select ctid from %s"
					" where %s limit %d))",
				tablename,
				filter,
				tablename,
				filter,
				limit);
#elif defined(HAVE_SQLITE3)
		return DBexecute(
				"delete from %s"
				" where %s",
				tablename,
				filter);
#endif
	}

	return 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: perform problem table cleanup                                     *
 *                                                                            *
 * Parameters: table    - [IN] the problem table name                         *
 * Parameters: source   - [IN] the event source                               *
 *             object   - [IN] the event object type                          *
 *             objectid - [IN] the event object identifier                    *
 *             more     - [OUT] 1 if there might be more data to remove,      *
 *                              otherwise the value is not changed            *
 *                                                                            *
 * Return value: number of rows deleted                                       *
 *                                                                            *
 ******************************************************************************/
static int	hk_problem_cleanup(const char *table, int source, int object, zbx_uint64_t objectid, int *more)
{
	char	filter[MAX_STRING_LEN];
	int	ret;

	zbx_snprintf(filter, sizeof(filter), "source=%d and object=%d and objectid=" ZBX_FS_UI64,
			source, object, objectid);

	ret = DBdelete_from_table(table, filter, CONFIG_MAX_HOUSEKEEPER_DELETE);

	if (ZBX_DB_OK > ret || (0 != CONFIG_MAX_HOUSEKEEPER_DELETE && ret >= CONFIG_MAX_HOUSEKEEPER_DELETE))
		*more = 1;

	return ZBX_DB_OK <= ret ? ret : 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: perform generic table cleanup                                     *
 *                                                                            *
 * Parameters: table    - [IN] the table name                                 *
 *             field    - [IN] the field name                                 *
 *             objectid - [IN] the field value                                *
 *             more     - [OUT] 1 if there might be more data to remove,      *
 *                              otherwise the value is not changed            *
 *                                                                            *
 * Return value: number of rows deleted                                       *
 *                                                                            *
 ******************************************************************************/
static int	hk_table_cleanup(const char *table, const char *field, zbx_uint64_t id, int *more)
{
	char	filter[MAX_STRING_LEN];
	int	ret;

	zbx_snprintf(filter, sizeof(filter), "%s=" ZBX_FS_UI64, field, id);

	ret = DBdelete_from_table(table, filter, CONFIG_MAX_HOUSEKEEPER_DELETE);

	if (ZBX_DB_OK > ret || (0 != CONFIG_MAX_HOUSEKEEPER_DELETE && ret >= CONFIG_MAX_HOUSEKEEPER_DELETE))
		*more = 1;

	return ZBX_DB_OK <= ret ? ret : 0;
}

/******************************************************************************
 *                                                                            *
 * Purpose: remove deleted items/triggers data                                *
 *                                                                            *
 * Return value: number of rows deleted                                       *
 *                                                                            *
 * Comments: sqlite3 does not use CONFIG_MAX_HOUSEKEEPER_DELETE, deletes all  *
 *                                                                            *
 ******************************************************************************/
static int	housekeeping_cleanup(void)
{
	DB_RESULT		result;
	DB_ROW			row;
	int			deleted = 0;
	zbx_vector_uint64_t	housekeeperids;
	char			*sql = NULL, *table_name_esc;
	size_t			sql_alloc = 0, sql_offset = 0;
	zbx_hk_cleanup_table_t *table;
	zbx_uint64_t		housekeeperid, objectid;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_vector_uint64_create(&housekeeperids);

	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset,
			"select housekeeperid,tablename,field,value"
			" from housekeeper"
			" where tablename in (");

	/* assemble list of tables included in the housekeeping procedure */
	for (table = hk_cleanup_tables; NULL != table->name; table++)
	{
		if (ZBX_HK_MODE_REGULAR != *table->poption_mode)
			continue;

		table_name_esc = DBdyn_escape_string(table->name);

		zbx_chrcpy_alloc(&sql, &sql_alloc, &sql_offset, '\'');
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, table_name_esc);
		zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, "',");

		zbx_free(table_name_esc);
	}
	sql_offset--;

	/* order by tablename to effectively use DB cache */
	zbx_strcpy_alloc(&sql, &sql_alloc, &sql_offset, ") order by tablename");

	result = DBselect("%s", sql);

	while (NULL != (row = DBfetch(result)))
	{
		int	more = 0;

		ZBX_STR2UINT64(housekeeperid, row[0]);
		ZBX_STR2UINT64(objectid, row[3]);

		if (0 == strcmp(row[1], "events")) /* events name is used for backwards compatibility with frontend */
		{
			const char	*table_name = "problem";

			if (0 == strcmp(row[2], "triggerid"))
			{
				deleted += hk_problem_cleanup(table_name, EVENT_SOURCE_INTERNAL, EVENT_OBJECT_TRIGGER,
						objectid, &more);
			}
			else if (0 == strcmp(row[2], "itemid"))
			{
				deleted += hk_problem_cleanup(table_name, EVENT_SOURCE_INTERNAL, EVENT_OBJECT_ITEM,
						objectid, &more);
			}
			else if (0 == strcmp(row[2], "lldruleid"))
			{
				deleted += hk_problem_cleanup(table_name, EVENT_SOURCE_INTERNAL, EVENT_OBJECT_LLDRULE,
						objectid, &more);
			}
			else if (0 == strcmp(row[2], "serviceid"))
			{
				deleted += hk_problem_cleanup(table_name, EVENT_SOURCE_SERVICE, EVENT_OBJECT_SERVICE,
						objectid, &more);
			}
		}
		else
			deleted += hk_table_cleanup(row[1], row[2], objectid, &more);

		if (0 == more)
			zbx_vector_uint64_append(&housekeeperids, housekeeperid);
	}
	DBfree_result(result);

	if (0 != housekeeperids.values_num)
	{
		zbx_vector_uint64_sort(&housekeeperids, ZBX_DEFAULT_UINT64_COMPARE_FUNC);
		DBexecute_multiple_query("delete from housekeeper where", "housekeeperid", &housekeeperids);
	}

	zbx_free(sql);

	zbx_vector_uint64_destroy(&housekeeperids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

static int	housekeeping_sessions(int now)
{
	int	deleted = 0, rc;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() now:%d", __func__, now);

	if (ZBX_HK_OPTION_ENABLED == cfg.hk.sessions_mode)
	{
		char	*sql = NULL;
		size_t	sql_alloc = 0, sql_offset = 0;

		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, "lastaccess<%d", now - cfg.hk.sessions);
		rc = DBdelete_from_table("sessions", sql, CONFIG_MAX_HOUSEKEEPER_DELETE);
		zbx_free(sql);

		if (ZBX_DB_OK <= rc)
			deleted = rc;
	}

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

static int	housekeeping_services(int now)
{
	static zbx_hk_rule_t	rule = {"service_alarms", "servicealarmid", "", HK_MIN_CLOCK_UNDEFINED,
			&cfg.hk.services};

	if (ZBX_HK_OPTION_ENABLED == cfg.hk.services_mode)
		return housekeeping_process_rule(now, &rule);

	return 0;
}

static int	housekeeping_audit(int now)
{
	static zbx_hk_rule_t	rule = {"auditlog", "auditid", "", HK_MIN_CLOCK_UNDEFINED, &cfg.hk.audit};

	if (ZBX_HK_OPTION_ENABLED == cfg.hk.audit_mode)
		return housekeeping_process_rule(now, &rule);

	return 0;
}

static int	housekeeping_autoreg_host(void)
{
	DB_RESULT		result;
	DB_ROW			row;
	int			deleted = 0;
	zbx_vector_uint64_t	autoreg_hostids;
	char			*sql = NULL;
	size_t			sql_alloc = 0, sql_offset = 0;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset,
			"select ah.autoreg_hostid"
			" from autoreg_host ah"
			" where not exists ("
					"select null"
					" from hosts h"
					" where ah.host=h.host"
			")"
				" and not exists ("
						"select null"
						" from events e"
						" where ah.autoreg_hostid=e.objectid"
							" and e.source=%d"
							" and e.object=%d"
				")"
			" order by ah.autoreg_hostid",
			EVENT_SOURCE_AUTOREGISTRATION,
			EVENT_OBJECT_ZABBIX_ACTIVE
	);

	result = DBselectN(sql, CONFIG_MAX_HOUSEKEEPER_DELETE);

	zbx_vector_uint64_create(&autoreg_hostids);

	while (NULL != (row = DBfetch(result)))
	{
		zbx_uint64_t	autoreg_hostid;

		ZBX_STR2UINT64(autoreg_hostid, row[0]);

		zbx_vector_uint64_append(&autoreg_hostids, autoreg_hostid);

	}
	DBfree_result(result);

	if (autoreg_hostids.values_num != 0)
	{
		int	ret;

		sql_offset = 0;

		zbx_snprintf_alloc(&sql, &sql_alloc, &sql_offset, "delete from autoreg_host where");

		DBadd_condition_alloc(&sql, &sql_alloc, &sql_offset, "autoreg_hostid", autoreg_hostids.values,
				autoreg_hostids.values_num);

		ret = DBexecute("%s", sql);

		if (ZBX_DB_OK <= ret)
			deleted = ret;
	}

	zbx_free(sql);

	zbx_vector_uint64_destroy(&autoreg_hostids);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

static int	housekeeping_events(int now)
{
#define ZBX_HK_EVENT_RULE	" and not exists (select null from problem where events.eventid=problem.eventid)" \
				" and not exists (select null from problem where events.eventid=problem.r_eventid)"

	static zbx_hk_rule_t	rules[] = {
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_TRIGGERS)
			" and events.object=" ZBX_STR(EVENT_OBJECT_TRIGGER)
			ZBX_HK_EVENT_RULE, HK_MIN_CLOCK_ALWAYS_RECHECK, &cfg.hk.events_trigger},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_INTERNAL)
			" and events.object=" ZBX_STR(EVENT_OBJECT_TRIGGER)
			ZBX_HK_EVENT_RULE, HK_MIN_CLOCK_ALWAYS_RECHECK, &cfg.hk.events_internal},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_INTERNAL)
			" and events.object=" ZBX_STR(EVENT_OBJECT_ITEM)
			ZBX_HK_EVENT_RULE, HK_MIN_CLOCK_ALWAYS_RECHECK, &cfg.hk.events_internal},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_INTERNAL)
			" and events.object=" ZBX_STR(EVENT_OBJECT_LLDRULE)
			ZBX_HK_EVENT_RULE, HK_MIN_CLOCK_ALWAYS_RECHECK, &cfg.hk.events_internal},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_DISCOVERY)
			" and events.object=" ZBX_STR(EVENT_OBJECT_DHOST), HK_MIN_CLOCK_UNDEFINED,
			&cfg.hk.events_discovery},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_DISCOVERY)
			" and events.object=" ZBX_STR(EVENT_OBJECT_DSERVICE), HK_MIN_CLOCK_UNDEFINED,
			&cfg.hk.events_discovery},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_AUTOREGISTRATION)
			" and events.object=" ZBX_STR(EVENT_OBJECT_ZABBIX_ACTIVE), HK_MIN_CLOCK_UNDEFINED,
			&cfg.hk.events_autoreg},
		{"events", "eventid", "events.source=" ZBX_STR(EVENT_SOURCE_SERVICE)
			" and events.object=" ZBX_STR(EVENT_OBJECT_SERVICE)
			ZBX_HK_EVENT_RULE, HK_MIN_CLOCK_ALWAYS_RECHECK, &cfg.hk.events_service},
		{NULL}
	};

	int		deleted = 0;
	zbx_hk_rule_t	*rule;

	if (ZBX_HK_OPTION_ENABLED != cfg.hk.events_mode)
		return 0;

	for (rule = rules; NULL != rule->table; rule++)
		deleted += housekeeping_process_rule(now, rule);

	return deleted;
#undef ZBX_HK_EVENT_RULE
}

static int	housekeeping_problems(int now)
{
	int	deleted = 0, rc;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() now:%d", __func__, now);

	rc = DBexecute("delete from problem where r_clock<>0 and r_clock<%d", now - SEC_PER_DAY);

	if (ZBX_DB_OK <= rc)
		deleted = rc;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

static int	housekeeping_proxy_dhistory(int now)
{
	int	deleted = 0, rc;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() now:%d", __func__, now);

	rc = DBexecute("delete from proxy_dhistory where clock<%d", now - SEC_PER_DAY);

	if (ZBX_DB_OK <= rc)
		deleted = rc;

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%d", __func__, deleted);

	return deleted;
}

static int	get_housekeeping_period(double time_slept)
{
	if (SEC_PER_HOUR > time_slept)
		return SEC_PER_HOUR;
	else if (24 * SEC_PER_HOUR < time_slept)
		return 24 * SEC_PER_HOUR;
	else
		return (int)time_slept;
}

#if defined(HAVE_POSTGRESQL)
static void	hk_tsdb_check_config(void)
{
	if (cfg.hk.history_global == ZBX_HK_OPTION_DISABLED && cfg.hk.history_mode == ZBX_HK_OPTION_ENABLED &&
			1 == db_version_info.history_compressed_chunks)
	{
		zabbix_log(LOG_LEVEL_WARNING, "Incorrect configuration. Override item history period is disabled, but "
				"historical data is compressed. Housekeeper may skip deleting this data.");
	}

	if (cfg.hk.trends_global == ZBX_HK_OPTION_DISABLED && cfg.hk.trends_mode == ZBX_HK_OPTION_ENABLED &&
			1 == db_version_info.trends_compressed_chunks)
	{
		zabbix_log(LOG_LEVEL_WARNING, "Incorrect configuration. Override item trends period is disabled, but "
				"trends data is compressed. Housekeeper may skip deleting this data.");
	}
}
#endif

ZBX_THREAD_ENTRY(housekeeper_thread, args)
{
	int			now, d_history_and_trends, d_cleanup, d_events, d_problems, d_sessions, d_services,
				d_audit, d_autoreg_host, sleeptime, records;
	double			sec, time_slept, time_now;
	char			sleeptext[25];
	zbx_ipc_async_socket_t	rtc;

	process_type = ((zbx_thread_args_t *)args)->process_type;
	server_num = ((zbx_thread_args_t *)args)->server_num;
	process_num = ((zbx_thread_args_t *)args)->process_num;

	zabbix_log(LOG_LEVEL_INFORMATION, "%s #%d started [%s #%d]", get_program_type_string(program_type),
			server_num, get_process_type_string(process_type), process_num);

	update_selfmon_counter(ZBX_PROCESS_STATE_BUSY);

	if (0 == CONFIG_HOUSEKEEPING_FREQUENCY)
	{
		sleeptime = ZBX_IPC_WAIT_FOREVER;
		zbx_setproctitle("%s [waiting for user command]", get_process_type_string(process_type));
		zbx_snprintf(sleeptext, sizeof(sleeptext), "waiting for user command");
	}
	else
	{
		sleeptime = HOUSEKEEPER_STARTUP_DELAY * SEC_PER_MIN;
		zbx_setproctitle("%s [startup idle for %d minutes]", get_process_type_string(process_type),
				HOUSEKEEPER_STARTUP_DELAY);
		zbx_snprintf(sleeptext, sizeof(sleeptext), "idle for %d hour(s)", CONFIG_HOUSEKEEPING_FREQUENCY);
	}

	hk_history_compression_init();

	zbx_rtc_subscribe(&rtc, process_type, process_num);

#if defined(HAVE_POSTGRESQL)

	if (ZBX_TSDB_VERSION > 0)
	{
		zbx_config_get(&cfg, ZBX_CONFIG_FLAGS_HOUSEKEEPER);
		hk_tsdb_check_config();
	}
#endif

	while (ZBX_IS_RUNNING())
	{
		zbx_uint32_t	rtc_cmd;
		unsigned char	*rtc_data;
		int		hk_execute = 0;

		sec = zbx_time();

		while (SUCCEED == zbx_rtc_wait(&rtc, &rtc_cmd, &rtc_data, sleeptime) && 0 != rtc_cmd)
		{
			switch (rtc_cmd)
			{
				case ZBX_RTC_HOUSEKEEPER_EXECUTE:
					if (0 == hk_execute)
					{
						zabbix_log(LOG_LEVEL_WARNING, "forced execution of the housekeeper");
						hk_execute = 1;
					}
					else
						zabbix_log(LOG_LEVEL_WARNING, "housekeeping procedure is already in"
								" progress");
					break;
				case ZBX_RTC_SHUTDOWN:
					goto out;
				default:
					continue;
			}

			sleeptime = 0;
		}

		if (!ZBX_IS_RUNNING())
			break;

		if (0 == CONFIG_HOUSEKEEPING_FREQUENCY)
			sleeptime = ZBX_IPC_WAIT_FOREVER;
		else
			sleeptime = CONFIG_HOUSEKEEPING_FREQUENCY * SEC_PER_HOUR;

		time_now = zbx_time();
		time_slept = time_now - sec;
		zbx_update_env(get_process_type_string(process_type), time_now);

		hk_period = get_housekeeping_period(time_slept);

		zabbix_log(LOG_LEVEL_WARNING, "executing housekeeper");

		now = time(NULL);

		zbx_setproctitle("%s [connecting to the database]", get_process_type_string(process_type));
		DBconnect(ZBX_DB_CONNECT_NORMAL);

		zbx_config_get(&cfg, ZBX_CONFIG_FLAGS_HOUSEKEEPER | ZBX_CONFIG_FLAGS_DB_EXTENSION);

		if (0 == strcmp(cfg.db.extension, ZBX_DB_EXTENSION_TIMESCALEDB))
		{
			zbx_setproctitle("%s [synchronizing history and trends compression settings]",
					get_process_type_string(process_type));
			hk_history_compression_update(&cfg.db);
		}

		zbx_setproctitle("%s [removing old history and trends]",
				get_process_type_string(process_type));
		sec = zbx_time();
		d_history_and_trends = housekeeping_history_and_trends(now);

		zbx_setproctitle("%s [removing old problems]", get_process_type_string(process_type));
		d_problems = housekeeping_problems(now);

		zbx_setproctitle("%s [removing old events]", get_process_type_string(process_type));
		d_events = housekeeping_events(now);

		zbx_setproctitle("%s [removing old sessions]", get_process_type_string(process_type));
		d_sessions = housekeeping_sessions(now);

		zbx_setproctitle("%s [removing old service alarms]", get_process_type_string(process_type));
		d_services = housekeeping_services(now);

		zbx_setproctitle("%s [removing old audit log items]", get_process_type_string(process_type));
		d_audit = housekeeping_audit(now);

		zbx_setproctitle("%s [removing old autoreg_hosts]", get_process_type_string(process_type));
		d_autoreg_host = housekeeping_autoreg_host();

		zbx_setproctitle("%s [removing old records]", get_process_type_string(process_type));
		records = housekeeping_proxy_dhistory(now);

		zbx_setproctitle("%s [removing deleted items data]", get_process_type_string(process_type));
		d_cleanup = housekeeping_cleanup();
		sec = zbx_time() - sec;

		zabbix_log(LOG_LEVEL_WARNING, "%s [deleted %d hist/trends, %d items/triggers, %d events, %d problems,"
				" %d sessions, %d alarms, %d audit, %d autoreg_host, %d records in " ZBX_FS_DBL
				" sec, %s]",
				get_process_type_string(process_type), d_history_and_trends, d_cleanup, d_events,
				d_problems, d_sessions, d_services, d_audit, d_autoreg_host, records, sec, sleeptext);

		zbx_config_clean(&cfg);

		DBclose();

		zbx_dc_cleanup_data_sessions();
		zbx_vc_housekeeping_value_cache();

		zbx_setproctitle("%s [deleted %d hist/trends, %d items/triggers, %d events, %d sessions, %d alarms,"
				" %d audit items, %d autoreg_host, %d records in " ZBX_FS_DBL " sec, %s]",
				get_process_type_string(process_type), d_history_and_trends, d_cleanup, d_events,
				d_sessions, d_services, d_audit, d_autoreg_host, records, sec, sleeptext);
	}
out:
	zbx_setproctitle("%s #%d [terminated]", get_process_type_string(process_type), process_num);

	while (1)
		zbx_sleep(SEC_PER_MIN);
}
