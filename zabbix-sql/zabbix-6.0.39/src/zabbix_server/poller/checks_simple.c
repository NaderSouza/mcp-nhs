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

#include "checks_simple.h"

#include "checks_simple_vmware.h"
#include "simple.h"
#include "log.h"
#include "zbxself.h"

typedef int	(*vmfunc_t)(AGENT_REQUEST *, const char *, const char *, AGENT_RESULT *);

#define ZBX_VMWARE_PREFIX	"vmware."

typedef struct
{
	const char	*key;
	vmfunc_t	func;
}
zbx_vmcheck_t;

#if defined(HAVE_LIBXML2) && defined(HAVE_LIBCURL)
#	define VMCHECK_FUNC(func)	func
#else
#	define VMCHECK_FUNC(func)	NULL
#endif

static zbx_vmcheck_t	vmchecks[] =
{
	{"cluster.discovery", VMCHECK_FUNC(check_vcenter_cluster_discovery)},
	{"cluster.status", VMCHECK_FUNC(check_vcenter_cluster_status)},
	{"version", VMCHECK_FUNC(check_vcenter_version)},
	{"fullname", VMCHECK_FUNC(check_vcenter_fullname)},
	{"datastore.discovery", VMCHECK_FUNC(check_vcenter_datastore_discovery)},
	{"datastore.read", VMCHECK_FUNC(check_vcenter_datastore_read)},
	{"datastore.size", VMCHECK_FUNC(check_vcenter_datastore_size)},
	{"datastore.write", VMCHECK_FUNC(check_vcenter_datastore_write)},
	{"datastore.hv.list", VMCHECK_FUNC(check_vcenter_datastore_hv_list)},

	{"hv.cluster.name", VMCHECK_FUNC(check_vcenter_hv_cluster_name)},
	{"hv.cpu.usage", VMCHECK_FUNC(check_vcenter_hv_cpu_usage)},
	{"hv.cpu.usage.perf", VMCHECK_FUNC(check_vcenter_hv_cpu_usage_perf)},
	{"hv.cpu.utilization", VMCHECK_FUNC(check_vcenter_hv_cpu_utilization)},
	{"hv.datacenter.name", VMCHECK_FUNC(check_vcenter_hv_datacenter_name)},
	{"hv.datastore.discovery", VMCHECK_FUNC(check_vcenter_hv_datastore_discovery)},
	{"hv.datastore.read", VMCHECK_FUNC(check_vcenter_hv_datastore_read)},
	{"hv.datastore.size", VMCHECK_FUNC(check_vcenter_hv_datastore_size)},
	{"hv.datastore.write", VMCHECK_FUNC(check_vcenter_hv_datastore_write)},
	{"hv.datastore.list", VMCHECK_FUNC(check_vcenter_hv_datastore_list)},
	{"hv.datastore.multipath", VMCHECK_FUNC(check_vcenter_hv_datastore_multipath)},
	{"hv.discovery", VMCHECK_FUNC(check_vcenter_hv_discovery)},
	{"hv.fullname", VMCHECK_FUNC(check_vcenter_hv_fullname)},
	{"hv.hw.cpu.num", VMCHECK_FUNC(check_vcenter_hv_hw_cpu_num)},
	{"hv.hw.cpu.freq", VMCHECK_FUNC(check_vcenter_hv_hw_cpu_freq)},
	{"hv.hw.cpu.model", VMCHECK_FUNC(check_vcenter_hv_hw_cpu_model)},
	{"hv.hw.cpu.threads", VMCHECK_FUNC(check_vcenter_hv_hw_cpu_threads)},
	{"hv.hw.memory", VMCHECK_FUNC(check_vcenter_hv_hw_memory)},
	{"hv.hw.model", VMCHECK_FUNC(check_vcenter_hv_hw_model)},
	{"hv.hw.uuid", VMCHECK_FUNC(check_vcenter_hv_hw_uuid)},
	{"hv.hw.vendor", VMCHECK_FUNC(check_vcenter_hv_hw_vendor)},
	{"hv.memory.size.ballooned", VMCHECK_FUNC(check_vcenter_hv_memory_size_ballooned)},
	{"hv.memory.used", VMCHECK_FUNC(check_vcenter_hv_memory_used)},
	{"hv.network.in", VMCHECK_FUNC(check_vcenter_hv_network_in)},
	{"hv.network.out", VMCHECK_FUNC(check_vcenter_hv_network_out)},
	{"hv.perfcounter", VMCHECK_FUNC(check_vcenter_hv_perfcounter)},
	{"hv.power", VMCHECK_FUNC(check_vcenter_hv_power)},
	{"hv.sensor.health.state", VMCHECK_FUNC(check_vcenter_hv_sensor_health_state)},
	{"hv.status", VMCHECK_FUNC(check_vcenter_hv_status)},
	{"hv.maintenance", VMCHECK_FUNC(check_vcenter_hv_maintenance)},
	{"hv.uptime", VMCHECK_FUNC(check_vcenter_hv_uptime)},
	{"hv.version", VMCHECK_FUNC(check_vcenter_hv_version)},
	{"hv.sensors.get", VMCHECK_FUNC(check_vcenter_hv_sensors_get)},
	{"hv.vm.num", VMCHECK_FUNC(check_vcenter_hv_vm_num)},

	{"vm.cluster.name", VMCHECK_FUNC(check_vcenter_vm_cluster_name)},
	{"vm.cpu.num", VMCHECK_FUNC(check_vcenter_vm_cpu_num)},
	{"vm.cpu.ready", VMCHECK_FUNC(check_vcenter_vm_cpu_ready)},
	{"vm.cpu.usage", VMCHECK_FUNC(check_vcenter_vm_cpu_usage)},
	{"vm.cpu.usage.perf", VMCHECK_FUNC(check_vcenter_vm_cpu_usage_perf)},
	{"vm.cpu.latency", VMCHECK_FUNC(check_vcenter_vm_cpu_latency)},
	{"vm.cpu.readiness", VMCHECK_FUNC(check_vcenter_vm_cpu_readiness)},
	{"vm.cpu.swapwait", VMCHECK_FUNC(check_vcenter_vm_cpu_swapwait)},
	{"vm.datacenter.name", VMCHECK_FUNC(check_vcenter_vm_datacenter_name)},
	{"vm.discovery", VMCHECK_FUNC(check_vcenter_vm_discovery)},
	{"vm.guest.osuptime", VMCHECK_FUNC(check_vcenter_vm_guest_uptime)},
	{"vm.hv.name", VMCHECK_FUNC(check_vcenter_vm_hv_name)},
	{"vm.memory.size", VMCHECK_FUNC(check_vcenter_vm_memory_size)},
	{"vm.memory.size.ballooned", VMCHECK_FUNC(check_vcenter_vm_memory_size_ballooned)},
	{"vm.memory.size.compressed", VMCHECK_FUNC(check_vcenter_vm_memory_size_compressed)},
	{"vm.memory.size.swapped", VMCHECK_FUNC(check_vcenter_vm_memory_size_swapped)},
	{"vm.memory.size.usage.guest", VMCHECK_FUNC(check_vcenter_vm_memory_size_usage_guest)},
	{"vm.memory.size.usage.host", VMCHECK_FUNC(check_vcenter_vm_memory_size_usage_host)},
	{"vm.memory.size.private", VMCHECK_FUNC(check_vcenter_vm_memory_size_private)},
	{"vm.memory.size.shared", VMCHECK_FUNC(check_vcenter_vm_memory_size_shared)},
	{"vm.memory.size.consumed", VMCHECK_FUNC(check_vcenter_vm_memory_size_consumed)},
	{"vm.memory.usage", VMCHECK_FUNC(check_vcenter_vm_memory_usage)},
	{"vm.guest.memory.size.swapped", VMCHECK_FUNC(check_vcenter_vm_guest_memory_size_swapped)},
	{"vm.net.if.discovery", VMCHECK_FUNC(check_vcenter_vm_net_if_discovery)},
	{"vm.net.if.in", VMCHECK_FUNC(check_vcenter_vm_net_if_in)},
	{"vm.net.if.out", VMCHECK_FUNC(check_vcenter_vm_net_if_out)},
	{"vm.net.if.usage", VMCHECK_FUNC(check_vcenter_vm_net_if_usage)},
	{"vm.perfcounter", VMCHECK_FUNC(check_vcenter_vm_perfcounter)},
	{"vm.powerstate", VMCHECK_FUNC(check_vcenter_vm_powerstate)},
	{"vm.storage.committed", VMCHECK_FUNC(check_vcenter_vm_storage_committed)},
	{"vm.storage.unshared", VMCHECK_FUNC(check_vcenter_vm_storage_unshared)},
	{"vm.storage.uncommitted", VMCHECK_FUNC(check_vcenter_vm_storage_uncommitted)},
	{"vm.storage.readoio", VMCHECK_FUNC(check_vcenter_vm_storage_readoio)},
	{"vm.storage.writeoio", VMCHECK_FUNC(check_vcenter_vm_storage_writeoio)},
	{"vm.storage.totalwritelatency", VMCHECK_FUNC(check_vcenter_vm_storage_totalwritelatency)},
	{"vm.storage.totalreadlatency", VMCHECK_FUNC(check_vcenter_vm_storage_totalreadlatency)},
	{"vm.uptime", VMCHECK_FUNC(check_vcenter_vm_uptime)},
	{"vm.vfs.dev.discovery", VMCHECK_FUNC(check_vcenter_vm_vfs_dev_discovery)},
	{"vm.vfs.dev.read", VMCHECK_FUNC(check_vcenter_vm_vfs_dev_read)},
	{"vm.vfs.dev.write", VMCHECK_FUNC(check_vcenter_vm_vfs_dev_write)},
	{"vm.vfs.fs.discovery", VMCHECK_FUNC(check_vcenter_vm_vfs_fs_discovery)},
	{"vm.vfs.fs.size", VMCHECK_FUNC(check_vcenter_vm_vfs_fs_size)},

	{"dc.discovery", VMCHECK_FUNC(check_vcenter_dc_discovery)},

	{"cl.perfcounter", VMCHECK_FUNC(check_vcenter_cl_perfcounter)},

	{NULL, NULL}
};

/******************************************************************************
 *                                                                            *
 * Purpose: Retrieves a handler of the item key                               *
 *                                                                            *
 * Parameters: key    - [IN] an item key (without parameters)                 *
 *             vmfunc - [OUT] a handler of the item key; can be NULL if       *
 *                            libxml2 or libcurl is not compiled in           *
 *                                                                            *
 * Return value: SUCCEED if key is a valid VMware key, FAIL - otherwise       *
 *                                                                            *
 ******************************************************************************/
static int	get_vmware_function(const char *key, vmfunc_t *vmfunc)
{
	zbx_vmcheck_t	*check;

	if (0 != strncmp(key, ZBX_VMWARE_PREFIX, ZBX_CONST_STRLEN(ZBX_VMWARE_PREFIX)))
		return FAIL;

	for (check = vmchecks; NULL != check->key; check++)
	{
		if (0 == strcmp(key + ZBX_CONST_STRLEN(ZBX_VMWARE_PREFIX), check->key))
		{
			*vmfunc = check->func;
			return SUCCEED;
		}
	}

	return FAIL;
}

int	get_value_simple(const DC_ITEM *item, AGENT_RESULT *result, zbx_vector_ptr_t *add_results)
{
	AGENT_REQUEST	request;
	vmfunc_t	vmfunc;
	int		ret = NOTSUPPORTED;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s() key_orig:'%s' addr:'%s'", __func__, item->key_orig, item->interface.addr);

	init_request(&request);

	if (SUCCEED != parse_item_key(item->key, &request))
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid item key format."));
		goto out;
	}

	request.lastlogsize = item->lastlogsize;

	if (0 == strcmp(request.key, "net.tcp.service") || 0 == strcmp(request.key, "net.udp.service"))
	{
		if (SYSINFO_RET_OK == check_service(&request, item->interface.addr, result, 0))
			ret = SUCCEED;
	}
	else if (0 == strcmp(request.key, "net.tcp.service.perf") || 0 == strcmp(request.key, "net.udp.service.perf"))
	{
		if (SYSINFO_RET_OK == check_service(&request, item->interface.addr, result, 1))
			ret = SUCCEED;
	}
	else if (SUCCEED == get_vmware_function(request.key, &vmfunc))
	{
		if (NULL != vmfunc)
		{
			if (0 == get_process_type_forks(ZBX_PROCESS_TYPE_VMWARE))
			{
				SET_MSG_RESULT(result, zbx_strdup(NULL, "No \"vmware collector\" processes started."));
				goto out;
			}

			if (SYSINFO_RET_OK == vmfunc(&request, item->username, item->password, result))
				ret = SUCCEED;
		}
		else
			SET_MSG_RESULT(result, zbx_strdup(NULL, "Support for VMware checks was not compiled in."));
	}
	else if (0 == strcmp(request.key, ZBX_VMWARE_PREFIX "eventlog"))
	{
#if defined(HAVE_LIBXML2) && defined(HAVE_LIBCURL)
		if (SYSINFO_RET_OK == check_vcenter_eventlog(&request, item, result, add_results))
			ret = SUCCEED;
#else
		ZBX_UNUSED(add_results);
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Support for VMware checks was not compiled in."));
#endif
	}
	else
	{
		/* it will execute item from a loadable module if any */
		if (SUCCEED == process(item->key, PROCESS_MODULE_COMMAND, result))
			ret = SUCCEED;
	}

	if (NOTSUPPORTED == ret && !ISSET_MSG(result))
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Simple check is not supported."));

out:
	free_request(&request);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return ret;
}
