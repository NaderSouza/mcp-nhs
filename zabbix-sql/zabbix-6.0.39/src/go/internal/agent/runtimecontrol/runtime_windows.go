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

package runtimecontrol

import (
	"fmt"
	"io/ioutil"
	"net"
	"os"
	"time"

	"github.com/Microsoft/go-winio"
	"golang.zabbix.com/sdk/log"
)

func New(path string, timeout time.Duration) (conn *Conn, err error) {
	c := Conn{}
	if path != "" {
		if _, tmperr := os.Stat(path); !os.IsNotExist(tmperr) {
			if _, err = SendCommand(path, "version", timeout); err == nil {
				return nil, fmt.Errorf("An agent is already using control pipe %s", path)
			}
			if err = os.Remove(path); err != nil {
				return
			}
		}
		if c.listener, err = winio.ListenPipe(path, nil); err != nil {
			return
		}
		c.sink = make(chan *Client)
		log.Debugf("listening for control connections on %s", path)
	}
	return &c, nil
}

func SendCommand(path, command string, timeout time.Duration) (reply string, err error) {
	var conn net.Conn
	if conn, err = winio.DialPipe(path, &timeout); err != nil {
		return
	}
	defer conn.Close()

	if err = conn.SetDeadline(time.Now().Add(timeout)); err != nil {
		return
	}
	if _, err = conn.Write([]byte(command + "\n")); err != nil {
		return
	}
	var b []byte
	if b, err = ioutil.ReadAll(conn); err != nil {
		return
	}
	return string(b), nil
}
