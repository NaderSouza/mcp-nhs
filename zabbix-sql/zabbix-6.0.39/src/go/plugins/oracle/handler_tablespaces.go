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

package oracle

import (
	"context"
	"strings"

	"golang.zabbix.com/sdk/errs"
	"golang.zabbix.com/sdk/zbxerr"
)

const (
	temp          = "TEMPORARY"
	perm          = "PERMANENT"
	undo          = "UNDO"
	defaultPERMTS = "USERS"
	defaultTEMPTS = "TEMP"
)

func tablespacesHandler(ctx context.Context, conn OraClient, params map[string]string,
	_ ...string) (interface{}, error) {
	var tablespaces string

	query, args, err := getTablespacesQuery(params)
	if err != nil {
		return nil, errs.Wrap(err, "failed to retrieve tablespace query")
	}

	row, err := conn.QueryRow(ctx, query, args...)
	if err != nil {
		return nil, errs.WrapConst(err, zbxerr.ErrorCannotFetchData)
	}

	err = row.Scan(&tablespaces)
	if err != nil {
		return nil, errs.WrapConst(err, zbxerr.ErrorCannotFetchData)
	}

	// Add leading zeros for floats: ".03" -> "0.03".
	// Oracle JSON functions are not RFC 4627 compliant.
	// There should be a better way to do that, but I haven't come up with it ¯\_(ツ)_/¯
	tablespaces = strings.ReplaceAll(tablespaces, "\":.", "\":0.")

	return tablespaces, nil
}

func getTablespacesQuery(params map[string]string) (string, []any, error) {
	ts := params["Tablespace"]
	t := params["Type"]

	if ts == "" && t == "" {
		return getFullQuery(), nil, nil
	}

	var err error
	ts, t, err = prepValues(ts, t)
	if err != nil {
		return "", nil, errs.WrapConst(err, zbxerr.ErrorInvalidParams)
	}

	switch t {
	case perm, undo:
		return getPermQueryPart(), []any{ts}, nil
	case temp:
		return getTempQueryPart(), []any{ts}, nil
	default:
		return "", nil, errs.Wrapf(zbxerr.ErrorInvalidParams, "incorrect table-space type %s", t)
	}
}

func prepValues(ts, t string) (outTableSpace string, outType string, err error) {
	if ts != "" && t != "" {
		return ts, t, nil
	}

	if ts == "" {
		if t == perm {
			return defaultPERMTS, t, nil
		}

		if t == temp {
			return defaultTEMPTS, t, nil
		}

		return "", "", errs.Errorf("incorrect type %s", t)
	}

	return ts, perm, nil
}

func getFullQuery() string {
	return `SELECT JSON_ARRAYAGG(
               JSON_OBJECT(TABLESPACE_NAME VALUE
                           JSON_OBJECT(
                                   'contents' VALUE CONTENTS,
                                   'file_bytes' VALUE FILE_BYTES,
                                   'max_bytes' VALUE MAX_BYTES,
                                   'free_bytes' VALUE FREE_BYTES,
                                   'used_bytes' VALUE USED_BYTES,
                                   'used_pct_max' VALUE USED_PCT_MAX,
                                   'used_file_pct' VALUE USED_FILE_PCT,
                                   'used_from_max_pct' VALUE USED_FROM_MAX_PCT,
                                   'status' VALUE STATUS
                               )
                   ) RETURNING CLOB
           )
FROM (SELECT df.TABLESPACE_NAME          AS TABLESPACE_NAME,
             df.CONTENTS                 AS CONTENTS,
             NVL(SUM(df.BYTES), 0)       AS FILE_BYTES,
             NVL(SUM(df.MAX_BYTES), 0)   AS MAX_BYTES,
             NVL(SUM(f.FREE), 0)         AS FREE_BYTES,
             SUM(df.BYTES) - SUM(f.FREE) AS USED_BYTES,
             ROUND(CASE SUM(df.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(df.BYTES) / SUM(df.MAX_BYTES) * 100)
                       END, 2)           AS USED_PCT_MAX,
             ROUND(CASE SUM(df.BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(df.BYTES) - SUM(f.FREE)) / SUM(df.BYTES) * 100
                       END, 2)           AS USED_FILE_PCT,
             ROUND(CASE SUM(df.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE NVL(SUM(df.BYTES) - SUM(f.FREE), 0) / SUM(df.MAX_BYTES) * 100
                       END, 2)           AS USED_FROM_MAX_PCT,
             CASE df.STATUS
                 WHEN 'ONLINE' THEN 1
                 WHEN 'OFFLINE' THEN 2
                 WHEN 'READ ONLY' THEN 3
                 ELSE 0
                 END                     AS STATUS
      FROM (SELECT ddf.FILE_ID,
                   dt.CONTENTS,
                   dt.STATUS,
                   ddf.FILE_NAME,
                   ddf.TABLESPACE_NAME,
                   TRUNC(ddf.BYTES)                         AS BYTES,
                   TRUNC(GREATEST(ddf.BYTES, ddf.MAXBYTES)) AS MAX_BYTES
            FROM DBA_DATA_FILES ddf,
                 DBA_TABLESPACES dt
            WHERE ddf.TABLESPACE_NAME = dt.TABLESPACE_NAME) df,
           (SELECT TRUNC(SUM(BYTES)) AS FREE,
                   FILE_ID
            FROM DBA_FREE_SPACE
            GROUP BY FILE_ID) f
      WHERE df.FILE_ID = f.FILE_ID (+)
      GROUP BY df.TABLESPACE_NAME, df.CONTENTS, df.STATUS
      UNION ALL
      SELECT Y.NAME                            AS TABLESPACE_NAME,
             Y.CONTENTS                        AS CONTENTS,
             NVL(SUM(Y.BYTES), 0)              AS FILE_BYTES,
             NVL(SUM(Y.MAX_BYTES), 0)          AS MAX_BYTES,
             NVL(MAX(NVL(Y.FREE_BYTES, 0)), 0) AS FREE,
             SUM(Y.BYTES) - MAX(Y.FREE_BYTES)  AS USED_BYTES,
             ROUND(CASE SUM(Y.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(Y.BYTES) / SUM(Y.MAX_BYTES) * 100)
                       END, 2)                 AS USED_PCT_MAX,
             ROUND(CASE SUM(Y.BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(Y.BYTES) - MAX(Y.FREE_BYTES)) / SUM(Y.BYTES) * 100
                       END, 2)                 AS USED_FILE_PCT,
             ROUND(CASE SUM(Y.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE NVL(SUM(Y.BYTES) - MAX(Y.FREE_BYTES), 0) / SUM(Y.MAX_BYTES) * 100
                       END, 2)                        AS USED_FROM_MAX_PCT,
             CASE Y.TBS_STATUS
                 WHEN 'ONLINE' THEN 1
                 WHEN 'OFFLINE' THEN 2
                 WHEN 'READ ONLY' THEN 3
                 ELSE 0
                 END                           AS STATUS
      FROM (SELECT dtf.TABLESPACE_NAME                             AS NAME,
                   dt.CONTENTS,
                   dt.STATUS                                       AS TBS_STATUS,
                   dtf.STATUS                                      AS STATUS,
                   dtf.BYTES                                       AS BYTES,
                   (SELECT ((f.TOTAL_BLOCKS - s.TOT_USED_BLOCKS) * vp.VALUE)
                    FROM (SELECT TABLESPACE_NAME,
                                 SUM(USED_BLOCKS) TOT_USED_BLOCKS
                          FROM GV$SORT_SEGMENT
                          WHERE TABLESPACE_NAME != 'DUMMY'
                          GROUP BY TABLESPACE_NAME) s,
                         (SELECT TABLESPACE_NAME,
                                 SUM(BLOCKS) TOTAL_BLOCKS
                          FROM DBA_TEMP_FILES
                          WHERE TABLESPACE_NAME != 'DUMMY'
                          GROUP BY TABLESPACE_NAME) f,
                         (SELECT VALUE
                          FROM V$PARAMETER
                          WHERE NAME = 'db_block_size') vp
                    WHERE f.TABLESPACE_NAME = s.TABLESPACE_NAME
                      AND f.TABLESPACE_NAME = dtf.TABLESPACE_NAME) AS FREE_BYTES,
                   CASE
                       WHEN dtf.MAXBYTES = 0 THEN dtf.BYTES
                       ELSE dtf.MAXBYTES
                       END                                         AS MAX_BYTES
            FROM sys.DBA_TEMP_FILES dtf,
                 sys.DBA_TABLESPACES dt
            WHERE dtf.TABLESPACE_NAME = dt.TABLESPACE_NAME) Y
      GROUP BY Y.NAME, Y.CONTENTS, Y.TBS_STATUS)
`
}

func getPermQueryPart() string {
	return `
SELECT JSON_ARRAYAGG(
               JSON_OBJECT(
                       TABLESPACE_NAME VALUE JSON_OBJECT(
                       'contents' VALUE CONTENTS,
                       'file_bytes' VALUE FILE_BYTES,
                       'max_bytes' VALUE MAX_BYTES,
                       'free_bytes' VALUE FREE_BYTES,
                       'used_bytes' VALUE USED_BYTES,
                       'used_pct_max' VALUE USED_PCT_MAX,
                       'used_file_pct' VALUE USED_FILE_PCT,
                       'used_from_max_pct' VALUE USED_FROM_MAX_PCT,
                       'status' VALUE STATUS
                   )
                   ) RETURNING CLOB
           )
FROM (SELECT df.TABLESPACE_NAME          AS TABLESPACE_NAME,
             df.CONTENTS                 AS CONTENTS,
             NVL(SUM(df.BYTES), 0)       AS FILE_BYTES,
             NVL(SUM(df.MAX_BYTES), 0)   AS MAX_BYTES,
             NVL(SUM(f.FREE), 0)         AS FREE_BYTES,
             SUM(df.BYTES) - SUM(f.FREE) AS USED_BYTES,
             ROUND(CASE SUM(df.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(df.BYTES) / SUM(df.MAX_BYTES) * 100)
                       END, 2)           AS USED_PCT_MAX,
             ROUND(CASE SUM(df.BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(df.BYTES) - SUM(f.FREE)) / SUM(df.BYTES) * 100
                       END, 2)           AS USED_FILE_PCT,
             ROUND(CASE SUM(df.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE NVL(SUM(df.BYTES) - SUM(f.FREE), 0) / SUM(df.MAX_BYTES) * 100
                       END, 2)           AS USED_FROM_MAX_PCT,
             CASE df.STATUS
                 WHEN 'ONLINE' THEN 1
                 WHEN 'OFFLINE' THEN 2
                 WHEN 'READ ONLY' THEN 3
                 ELSE 0
                 END                     AS STATUS
      FROM (SELECT ddf.FILE_ID,
                   dt.CONTENTS,
                   dt.STATUS,
                   ddf.FILE_NAME,
                   ddf.TABLESPACE_NAME,
                   TRUNC(ddf.BYTES)                         AS BYTES,
                   TRUNC(GREATEST(ddf.BYTES, ddf.MAXBYTES)) AS MAX_BYTES
            FROM DBA_DATA_FILES ddf,
                 DBA_TABLESPACES dt
            WHERE ddf.TABLESPACE_NAME = dt.TABLESPACE_NAME) df,
           (SELECT TRUNC(SUM(BYTES)) AS FREE,
                   FILE_ID
            FROM DBA_FREE_SPACE
            GROUP BY FILE_ID) f
      WHERE df.FILE_ID = f.FILE_ID (+)
        AND df.TABLESPACE_NAME = :1
      GROUP BY df.TABLESPACE_NAME,
               df.CONTENTS,
               df.STATUS)`
}

func getTempQueryPart() string {
	return `
SELECT JSON_ARRAYAGG(
               JSON_OBJECT(
                       TABLESPACE_NAME VALUE JSON_OBJECT(
                       'contents' VALUE CONTENTS,
                       'file_bytes' VALUE FILE_BYTES,
                       'max_bytes' VALUE MAX_BYTES,
                       'free_bytes' VALUE FREE_BYTES,
                       'used_bytes' VALUE USED_BYTES,
                       'used_pct_max' VALUE USED_PCT_MAX,
                       'used_file_pct' VALUE USED_FILE_PCT,
                       'used_from_max_pct' VALUE USED_FROM_MAX_PCT,
                       'status' VALUE STATUS
                   )
                   ) RETURNING CLOB
           )
FROM (SELECT Y.NAME                            AS TABLESPACE_NAME,
             Y.CONTENTS                        AS CONTENTS,
             NVL(SUM(Y.BYTES), 0)              AS FILE_BYTES,
             NVL(SUM(Y.MAX_BYTES), 0)          AS MAX_BYTES,
             NVL(MAX(NVL(Y.FREE_BYTES, 0)), 0) AS FREE_BYTES,
             SUM(Y.BYTES) - MAX(Y.FREE_BYTES)  AS USED_BYTES,
             ROUND(CASE SUM(Y.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(Y.BYTES) / SUM(Y.MAX_BYTES) * 100)
                       END, 2)                 AS USED_PCT_MAX,
             ROUND(CASE SUM(Y.BYTES)
                       WHEN 0 THEN 0
                       ELSE (SUM(Y.BYTES) - MAX(Y.FREE_BYTES)) / SUM(Y.BYTES) * 100
                       END, 2)                 AS USED_FILE_PCT,
             ROUND(CASE SUM(Y.MAX_BYTES)
                       WHEN 0 THEN 0
                       ELSE NVL(SUM(Y.BYTES) - MAX(Y.FREE_BYTES), 0) / SUM(Y.MAX_BYTES) * 100
                       END, 2)                 AS USED_FROM_MAX_PCT,
             CASE Y.TBS_STATUS
                 WHEN 'ONLINE' THEN 1
                 WHEN 'OFFLINE' THEN 2
                 WHEN 'READ ONLY' THEN 3
                 ELSE 0
                 END                           AS STATUS
      FROM (SELECT dtf.TABLESPACE_NAME                             AS NAME,
                   dt.CONTENTS,
                   dt.STATUS                                       AS TBS_STATUS,
                   dtf.STATUS                                      AS STATUS,
                   dtf.BYTES                                       AS BYTES,
                   (SELECT ((f.TOTAL_BLOCKS - s.TOT_USED_BLOCKS) * vp.VALUE)
                    FROM (SELECT TABLESPACE_NAME,
                                 SUM(USED_BLOCKS) TOT_USED_BLOCKS
                          FROM GV$SORT_SEGMENT
                          WHERE TABLESPACE_NAME != 'DUMMY'
                          GROUP BY TABLESPACE_NAME) s,
                         (SELECT TABLESPACE_NAME,
                                 SUM(BLOCKS) TOTAL_BLOCKS
                          FROM DBA_TEMP_FILES
                          WHERE TABLESPACE_NAME != 'DUMMY'
                          GROUP BY TABLESPACE_NAME) f,
                         (SELECT VALUE
                          FROM V$PARAMETER
                          WHERE NAME = 'db_block_size') vp
                    WHERE f.TABLESPACE_NAME = s.TABLESPACE_NAME
                      AND f.TABLESPACE_NAME = dtf.TABLESPACE_NAME) AS FREE_BYTES,
                   CASE
                       WHEN dtf.MAXBYTES = 0 THEN dtf.BYTES
                       ELSE dtf.MAXBYTES
                       END                                         AS MAX_BYTES
            FROM sys.DBA_TEMP_FILES dtf,
                 sys.DBA_TABLESPACES dt
            WHERE dtf.TABLESPACE_NAME = dt.TABLESPACE_NAME
              AND dtf.TABLESPACE_NAME = :1) Y
      GROUP BY Y.NAME,
               Y.CONTENTS,
               Y.TBS_STATUS)`
}
