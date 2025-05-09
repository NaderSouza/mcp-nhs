//go:build !windows
// +build !windows

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

package dns

import (
	"fmt"
	"os"
	"strings"
)

func (o *options) setDefaultIP() (err error) {
	data, err := os.ReadFile("/etc/resolv.conf")
	if err != nil {
		return
	}

	s := strings.Split(string(data), "\n")
	for _, tmp := range s {
		if strings.HasPrefix(tmp, "nameserver") {
			return o.setIP(strings.TrimSpace(strings.TrimPrefix(tmp, "nameserver")))
		}
	}

	return fmt.Errorf("cannot find default dns nameserver")
}
