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

package zbxlib

/* cspell:disable */

/*
#cgo CFLAGS: -I${SRCDIR}/../../../../../include

#include "common.h"
#include "sysinfo.h"
#include "module.h"

int	SYSTEM_LOCALTIME(AGENT_REQUEST *request, AGENT_RESULT *result);
int	PROC_MEM(AGENT_REQUEST *request, AGENT_RESULT *result);
int	PROC_NUM(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_BOOTTIME(AGENT_REQUEST *request, AGENT_RESULT *result);
int	NET_TCP_LISTEN(AGENT_REQUEST *request, AGENT_RESULT *result);
int	CHECK_SERVICE(AGENT_REQUEST *request, AGENT_RESULT *result);
int	CHECK_SERVICE_PERF(AGENT_REQUEST *request, AGENT_RESULT *result);
int	NET_UDP_LISTEN(AGENT_REQUEST *request, AGENT_RESULT *result);
int	GET_SENSOR(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_CPU_LOAD(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_CPU_SWITCHES(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_CPU_INTR(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_HW_CPU(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_HW_MACADDR(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_SW_OS(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_SW_PACKAGES(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_SWAP_IN(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_SWAP_OUT(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_SWAP_SIZE(AGENT_REQUEST *request, AGENT_RESULT *result);
int	SYSTEM_USERS_NUM(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VFS_DIR_GET(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VFS_FS_DISCOVERY(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VFS_FS_INODE(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VFS_FS_SIZE(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VFS_FS_GET(AGENT_REQUEST *request, AGENT_RESULT *result);
int	VM_MEMORY_SIZE(AGENT_REQUEST *request, AGENT_RESULT *result);
*/
import "C"

import (
	"unsafe"
)

func resolveMetric(key string) (cfunc unsafe.Pointer) {
	switch key {
	case "system.localtime":
		cfunc = unsafe.Pointer(C.SYSTEM_LOCALTIME)
	case "system.boottime":
		cfunc = unsafe.Pointer(C.SYSTEM_BOOTTIME)
	case "net.tcp.listen":
		cfunc = unsafe.Pointer(C.NET_TCP_LISTEN)
	case "net.udp.listen":
		cfunc = unsafe.Pointer(C.NET_UDP_LISTEN)
	case "sensor":
		cfunc = unsafe.Pointer(C.GET_SENSOR)
	case "system.cpu.load":
		cfunc = unsafe.Pointer(C.SYSTEM_CPU_LOAD)
	case "system.cpu.switches":
		cfunc = unsafe.Pointer(C.SYSTEM_CPU_SWITCHES)
	case "system.cpu.intr":
		cfunc = unsafe.Pointer(C.SYSTEM_CPU_INTR)
	case "system.hw.cpu":
		cfunc = unsafe.Pointer(C.SYSTEM_HW_CPU)
	case "system.hw.macaddr":
		cfunc = unsafe.Pointer(C.SYSTEM_HW_MACADDR)
	case "system.sw.os":
		cfunc = unsafe.Pointer(C.SYSTEM_SW_OS)
	case "vfs.dir.get":
		cfunc = unsafe.Pointer(C.VFS_DIR_GET)
	}
	return
}
