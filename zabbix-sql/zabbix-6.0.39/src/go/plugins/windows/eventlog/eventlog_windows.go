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

package eventlog

import (
	"fmt"
	"runtime"
	"time"
	"unsafe"

	"golang.zabbix.com/agent2/internal/agent"
	"golang.zabbix.com/agent2/pkg/glexpr"
	"golang.zabbix.com/agent2/pkg/itemutil"
	"golang.zabbix.com/agent2/pkg/zbxlib"
	"golang.zabbix.com/sdk/conf"
	"golang.zabbix.com/sdk/errs"
	"golang.zabbix.com/sdk/plugin"
	"golang.zabbix.com/sdk/zbxerr"
)

var impl Plugin

type Options struct {
	MaxLinesPerSecond int `conf:"range=1:1000,default=20"`
}

// Plugin -
type Plugin struct {
	plugin.Base
	options Options
}

type metadata struct {
	key       string
	params    []string
	blob      unsafe.Pointer
	lastcheck time.Time
}

func init() {
	err := plugin.RegisterMetrics(&impl, "WindowsEventlog", "eventlog", "Windows event log file monitoring.")
	if err != nil {
		panic(zbxerr.New("failed to register metrics").Wrap(err))
	}
}

func (p *Plugin) Configure(global *plugin.GlobalOptions, options interface{}) {
	err := conf.UnmarshalStrict(options, &p.options)
	if err != nil {
		p.Warningf("cannot unmarshal configuration options: %s", err)
	}

	zbxlib.SetEventlogMaxLinesPerSecond(p.options.MaxLinesPerSecond)
}

func (p *Plugin) Validate(options interface{}) error {
	var o Options

	err := conf.Unmarshal(options, &o)
	if err != nil {
		return errs.Wrap(err, "plugin config validation failed")
	}

	return nil
}

func (p *Plugin) Export(key string, params []string, ctx plugin.ContextProvider) (result interface{}, err error) {
	if ctx == nil || ctx.ClientID() <= agent.MaxBuiltinClientID {
		return nil, fmt.Errorf(`The "%s" key is not supported in test or single passive check mode`, key)
	}
	meta := ctx.Meta()
	var data *metadata
	if meta.Data == nil {
		data = &metadata{key: key, params: params}
		runtime.SetFinalizer(data, func(d *metadata) { zbxlib.FreeActiveMetric(d.blob) })
		if data.blob, err = zbxlib.NewActiveMetric(key, params, meta.LastLogsize(), meta.Mtime()); err != nil {
			return nil, err
		}
		meta.Data = data
	} else {
		data = meta.Data.(*metadata)
		if !itemutil.CompareKeysParams(key, params, data.key, data.params) {
			zbxlib.FreeActiveMetric(data.blob)
			data.key = key
			data.params = params
			// recreate if item key has been changed
			if data.blob, err = zbxlib.NewActiveMetric(key, params, meta.LastLogsize(), meta.Mtime()); err != nil {
				return nil, err
			}
		}
	}

	if ctx.Output().PersistSlotsAvailable() == 0 {
		p.Warningf("buffer is full, cannot store persistent value")
		return nil, nil
	}

	// with flexible checks there are no guaranteed refresh time,
	// so using number of seconds elapsed since last check
	now := time.Now()
	refresh := zbxlib.GetCheckIntervalSeconds(ctx.ItemID(), ctx.Delay(), now, data.lastcheck)
	logitem := zbxlib.EventLogItem{Results: make([]*zbxlib.EventLogResult, 0), Output: ctx.Output()}
	grxp := ctx.GlobalRegexp().(*glexpr.Bundle)
	zbxlib.ProcessEventLogCheck(data.blob, &logitem, refresh, grxp.Cblob)
	runtime.KeepAlive(grxp)
	data.lastcheck = now

	if len(logitem.Results) != 0 {
		results := make([]plugin.Result, len(logitem.Results))
		for i, r := range logitem.Results {
			results[i].Itemid = ctx.ItemID()
			results[i].Value = r.Value
			results[i].EventSource = r.EventSource
			results[i].EventID = r.EventID
			results[i].EventSeverity = r.EventSeverity
			results[i].EventTimestamp = r.EventTimestamp
			results[i].Error = r.Error
			results[i].Ts = r.Ts
			results[i].LastLogsize = &r.LastLogsize
			results[i].Persistent = true
		}
		return results, nil
	}
	return nil, nil
}
