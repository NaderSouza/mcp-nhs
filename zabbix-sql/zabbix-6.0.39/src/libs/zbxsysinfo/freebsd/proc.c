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

#include "common.h"
#include "sysinfo.h"
#include "zbxregexp.h"
#include "log.h"

#if (__FreeBSD_version) < 500000
#	define ZBX_COMMLEN	MAXCOMLEN
#	define ZBX_PROC_PID	kp_proc.p_pid
#	define ZBX_PROC_COMM	kp_proc.p_comm
#	define ZBX_PROC_STAT	kp_proc.p_stat
#	define ZBX_PROC_TSIZE	kp_eproc.e_vm.vm_tsize
#	define ZBX_PROC_DSIZE	kp_eproc.e_vm.vm_dsize
#	define ZBX_PROC_SSIZE	kp_eproc.e_vm.vm_ssize
#	define ZBX_PROC_RSSIZE	kp_eproc.e_vm.vm_rssize
#	define ZBX_PROC_VSIZE	kp_eproc.e_vm.vm_map.size
#else
#	define ZBX_COMMLEN	COMMLEN
#	define ZBX_PROC_PID	ki_pid
#	define ZBX_PROC_COMM	ki_comm
#	define ZBX_PROC_STAT	ki_stat
#	define ZBX_PROC_TSIZE	ki_tsize
#	define ZBX_PROC_DSIZE	ki_dsize
#	define ZBX_PROC_SSIZE	ki_ssize
#	define ZBX_PROC_RSSIZE	ki_rssize
#	define ZBX_PROC_VSIZE	ki_size
#endif

#if (__FreeBSD_version) < 500000
#	define ZBX_PROC_FLAG 	kp_proc.p_flag
#	define ZBX_PROC_MASK	P_INMEM
#elif (__FreeBSD_version) < 700000
#	define ZBX_PROC_FLAG 	ki_sflag
#	define ZBX_PROC_MASK	PS_INMEM
#else
#	define ZBX_PROC_TDFLAG	ki_tdflags
#	define ZBX_PROC_FLAG	ki_flag
#	define ZBX_PROC_MASK	P_INMEM
#endif

#define ARGV_START_SIZE	64
static char	*get_commandline(struct kinfo_proc *proc)
{
	int		mib[4], i;
	size_t		sz;
	static char	*args = NULL;
#if (__FreeBSD_version >= 802510)
	static int	args_alloc = 0;
#else
	int		argv_max, err = -1;
	static int	args_alloc = ARGV_START_SIZE;
#endif

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	mib[2] = KERN_PROC_ARGS;
	mib[3] = proc->ZBX_PROC_PID;

#if (__FreeBSD_version >= 802510)
	if (-1 == sysctl(mib, 4, NULL, &sz, NULL, 0))
		return NULL;

	if (NULL == args)
	{
		args = zbx_malloc(args, sz);
		args_alloc = sz;
	}
	else if (sz > args_alloc)
	{
		args = zbx_realloc(args, sz);
		args_alloc = sz;
	}

	if (-1 == sysctl(mib, 4, args, &sz, NULL, 0))
		return NULL;
#else
	/*
	 * Before FreeBSD 8.3 sysctl() API for kern.proc.args didn't follow the regular convention
	 * that a user can query the needed size for results by passing in a NULL old pointer
	 * and a valid oldsize, given that we have to estimate the required output buffer size manually:
	 *
	 * https://github.com/freebsd/freebsd-src/commit/9f688f2ce3c01f30b0c98d17c6ce057660819c8c
	*/

	if (NULL == args)
		args = zbx_malloc(args, args_alloc);

	if (-1 == (argv_max = sysconf(_SC_ARG_MAX)))
		return NULL;

	while (0 != err && args_alloc < argv_max)
	{
		sz = (size_t)args_alloc;

		if (-1 == (err = sysctl(mib, 4, args, &sz, NULL, 0)))
		{
			if (ENOMEM == errno)
			{
				args_alloc *= 2;
				args = zbx_realloc(args, args_alloc);
			}
			else
				return NULL;
		}
	}

	if (-1 == err)
		return NULL;
#endif
	for (i = 0; i < (int)(sz - 1); i++)
		if (args[i] == '\0')
			args[i] = ' ';

	if (0 == sz)
		zbx_strlcpy(args, proc->ZBX_PROC_COMM, args_alloc);

	return args;
}
#undef ARGV_START_SIZE

int     PROC_MEM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
#define ZBX_SIZE	1
#define ZBX_RSS		2
#define ZBX_VSIZE	3
#define ZBX_PMEM	4
#define ZBX_TSIZE	5
#define ZBX_DSIZE	6
#define ZBX_SSIZE	7
	char		*procname, *proccomm, *param, *args, *mem_type = NULL, *rxp_error = NULL;
	int		do_task, pagesize, count, i, proccount = 0, invalid_user = 0, mem_type_code, mib[4],
			ret = SYSINFO_RET_OK;
	unsigned int	mibs;
	zbx_uint64_t	mem_size = 0, byte_value = 0;
	double		pct_size = 0.0, pct_value = 0.0;
#if (__FreeBSD_version) < 500000
	int		mem_pages;
#else
	unsigned long 	mem_pages;
#endif
	size_t	sz;
	zbx_regexp_t	*proccomm_rxp = NULL;

	struct kinfo_proc	*proc = NULL;
	struct passwd		*usrinfo;

	if (5 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	procname = get_rparam(request, 0);
	param = get_rparam(request, 1);

	if (NULL != param && '\0' != *param)
	{
		errno = 0;

		if (NULL == (usrinfo = getpwnam(param)))
		{
			if (0 != errno)
			{
				SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain user information: %s",
						zbx_strerror(errno)));
				ret = SYSINFO_RET_FAIL;
				goto clean;
			}

			invalid_user = 1;
		}
	}
	else
		usrinfo = NULL;

	param = get_rparam(request, 2);

	if (NULL == param || '\0' == *param || 0 == strcmp(param, "sum"))
		do_task = ZBX_DO_SUM;
	else if (0 == strcmp(param, "avg"))
		do_task = ZBX_DO_AVG;
	else if (0 == strcmp(param, "max"))
		do_task = ZBX_DO_MAX;
	else if (0 == strcmp(param, "min"))
		do_task = ZBX_DO_MIN;
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	proccomm = get_rparam(request, 3);

	if (NULL != proccomm && '\0' != *proccomm)
	{
		if (SUCCEED != zbx_regexp_compile(proccomm, &proccomm_rxp, &rxp_error))
		{
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Invalid regular expression in fourth parameter: "
					"%s", rxp_error));

			zbx_free(rxp_error);
			ret = SYSINFO_RET_FAIL;
			goto clean;
		}
	}

	mem_type = get_rparam(request, 4);

	if (NULL == mem_type || '\0' == *mem_type || 0 == strcmp(mem_type, "size"))
	{
		mem_type_code = ZBX_SIZE;		/* size of process (code + data + stack) */
	}
	else if (0 == strcmp(mem_type, "rss"))
	{
		mem_type_code = ZBX_RSS;		/* resident set size */
	}
	else if (0 == strcmp(mem_type, "vsize"))
	{
		mem_type_code = ZBX_VSIZE;		/* virtual size */
	}
	else if (0 == strcmp(mem_type, "pmem"))
	{
		mem_type_code = ZBX_PMEM;		/* percentage of real memory used by process */
	}
	else if (0 == strcmp(mem_type, "tsize"))
	{
		mem_type_code = ZBX_TSIZE;		/* text size */
	}
	else if (0 == strcmp(mem_type, "dsize"))
	{
		mem_type_code = ZBX_DSIZE;		/* data size */
	}
	else if (0 == strcmp(mem_type, "ssize"))
	{
		mem_type_code = ZBX_SSIZE;		/* stack size */
	}
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid fifth parameter."));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	if (1 == invalid_user)	/* handle 0 for non-existent user after all parameters have been parsed and validated */
		goto out;

	pagesize = getpagesize();

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	if (NULL != usrinfo)
	{
		mib[2] = KERN_PROC_UID;
		mib[3] = usrinfo->pw_uid;
		mibs = 4;
	}
	else
	{
#if (__FreeBSD_version) < 500000
		mib[2] = KERN_PROC_ALL;
#else
		mib[2] = KERN_PROC_PROC;
#endif
		mib[3] = 0;
		mibs = 3;
	}

	if (ZBX_PMEM == mem_type_code)
	{
		sz = sizeof(mem_pages);

		if (0 != sysctlbyname("hw.availpages", &mem_pages, &sz, NULL, (size_t)0))
		{
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain number of physical pages: %s",
					zbx_strerror(errno)));
			ret = SYSINFO_RET_FAIL;
			goto clean;
		}
	}

	sz = 0;
	if (0 != sysctl(mib, mibs, NULL, &sz, NULL, (size_t)0))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain necessary buffer size from system: %s",
				zbx_strerror(errno)));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	proc = (struct kinfo_proc *)zbx_malloc(proc, sz);
	if (0 != sysctl(mib, mibs, proc, &sz, NULL, (size_t)0))
	{
		zbx_free(proc);
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain process information: %s",
				zbx_strerror(errno)));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	count = sz / sizeof(struct kinfo_proc);

	for (i = 0; i < count; i++)
	{
		if (NULL != procname && '\0' != *procname && 0 != strcmp(procname, proc[i].ZBX_PROC_COMM))
			continue;

		if (NULL != proccomm && '\0' != *proccomm)
		{
			if (NULL == (args = get_commandline(&proc[i])))
				continue;

			if (0 != zbx_regexp_match_precompiled(args, proccomm_rxp))
				continue;
		}

		switch (mem_type_code)
		{
			case ZBX_SIZE:
				byte_value = (proc[i].ZBX_PROC_TSIZE + proc[i].ZBX_PROC_DSIZE + proc[i].ZBX_PROC_SSIZE)
						* pagesize;
				break;
			case ZBX_RSS:
				byte_value = proc[i].ZBX_PROC_RSSIZE * pagesize;
				break;
			case ZBX_VSIZE:
				byte_value = proc[i].ZBX_PROC_VSIZE;
				break;
			case ZBX_PMEM:
				if (0 != (proc[i].ZBX_PROC_FLAG & ZBX_PROC_MASK))
#if (__FreeBSD_version) < 500000
					pct_value = ((float)(proc[i].ZBX_PROC_RSSIZE + UPAGES) / mem_pages) * 100.0;
#else
					pct_value = ((float)proc[i].ZBX_PROC_RSSIZE / mem_pages) * 100.0;
#endif
				else
					pct_value = 0.0;
				break;
			case ZBX_TSIZE:
				byte_value = proc[i].ZBX_PROC_TSIZE * pagesize;
				break;
			case ZBX_DSIZE:
				byte_value = proc[i].ZBX_PROC_DSIZE * pagesize;
				break;
			case ZBX_SSIZE:
				byte_value = proc[i].ZBX_PROC_SSIZE * pagesize;
				break;
		}

		if (ZBX_PMEM != mem_type_code)
		{
			if (0 != proccount++)
			{
				if (ZBX_DO_MAX == do_task)
					mem_size = MAX(mem_size, byte_value);
				else if (ZBX_DO_MIN == do_task)
					mem_size = MIN(mem_size, byte_value);
				else
					mem_size += byte_value;
			}
			else
				mem_size = byte_value;
		}
		else
		{
			if (0 != proccount++)
			{
				if (ZBX_DO_MAX == do_task)
					pct_size = MAX(pct_size, pct_value);
				else if (ZBX_DO_MIN == do_task)
					pct_size = MIN(pct_size, pct_value);
				else
					pct_size += pct_value;
			}
			else
				pct_size = pct_value;
		}
	}

	zbx_free(proc);
out:
	if (ZBX_PMEM != mem_type_code)
	{
		if (ZBX_DO_AVG == do_task)
			SET_DBL_RESULT(result, 0 == proccount ? 0.0 : (double)mem_size / (double)proccount);
		else
			SET_UI64_RESULT(result, mem_size);
	}
	else
	{
		if (ZBX_DO_AVG == do_task)
			SET_DBL_RESULT(result, 0 == proccount ? 0.0 : pct_size / (double)proccount);
		else
			SET_DBL_RESULT(result, pct_size);
	}
clean:
	if (NULL != proccomm_rxp)
		zbx_regexp_free(proccomm_rxp);

	return ret;

#undef ZBX_SIZE
#undef ZBX_RSS
#undef ZBX_VSIZE
#undef ZBX_PMEM
#undef ZBX_TSIZE
#undef ZBX_DSIZE
#undef ZBX_SSIZE
}

int	PROC_NUM(AGENT_REQUEST *request, AGENT_RESULT *result)
{
	char			*procname, *proccomm, *param, *args, *rxp_error = NULL;
	int			proccount = 0, invalid_user = 0, zbx_proc_stat;
	int			count, i, proc_ok, stat_ok, comm_ok, mib[4], mibs, ret = SYSINFO_RET_OK;
	size_t			sz;
	struct kinfo_proc	*proc = NULL;
	struct passwd		*usrinfo;
	zbx_regexp_t		*proccomm_rxp = NULL;

	if (4 < request->nparam)
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Too many parameters."));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	procname = get_rparam(request, 0);
	param = get_rparam(request, 1);

	if (NULL != param && '\0' != *param)
	{
		errno = 0;

		if (NULL == (usrinfo = getpwnam(param)))
		{
			if (0 != errno)
			{
				SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain user information: %s",
						zbx_strerror(errno)));
				ret = SYSINFO_RET_FAIL;
				goto clean;
			}

			invalid_user = 1;
		}
	}
	else
		usrinfo = NULL;

	param = get_rparam(request, 2);

	if (NULL == param || '\0' == *param || 0 == strcmp(param, "all"))
		zbx_proc_stat = ZBX_PROC_STAT_ALL;
	else if (0 == strcmp(param, "run"))
		zbx_proc_stat = ZBX_PROC_STAT_RUN;
	else if (0 == strcmp(param, "sleep"))
		zbx_proc_stat = ZBX_PROC_STAT_SLEEP;
	else if (0 == strcmp(param, "zomb"))
		zbx_proc_stat = ZBX_PROC_STAT_ZOMB;
	else if (0 == strcmp(param, "disk"))
		zbx_proc_stat = ZBX_PROC_STAT_DISK;
	else if (0 == strcmp(param, "trace"))
		zbx_proc_stat = ZBX_PROC_STAT_TRACE;
	else
	{
		SET_MSG_RESULT(result, zbx_strdup(NULL, "Invalid third parameter."));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	proccomm = get_rparam(request, 3);

	if (NULL != proccomm && '\0' != *proccomm)
	{
		if (SUCCEED != zbx_regexp_compile(proccomm, &proccomm_rxp, &rxp_error))
		{
			SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Invalid regular expression in fourth parameter: "
					"%s", rxp_error));

			zbx_free(rxp_error);
			ret = SYSINFO_RET_FAIL;
			goto clean;
		}
	}

	if (1 == invalid_user)	/* handle 0 for non-existent user after all parameters have been parsed and validated */
		goto out;

	mib[0] = CTL_KERN;
	mib[1] = KERN_PROC;
	if (NULL != usrinfo)
	{
		mib[2] = KERN_PROC_UID;
		mib[3] = usrinfo->pw_uid;
		mibs = 4;
	}
	else
	{
#if (__FreeBSD_version) > 500000
		mib[2] = KERN_PROC_PROC;
#else
		mib[2] = KERN_PROC_ALL;
#endif
		mib[3] = 0;
		mibs = 3;
	}

	sz = 0;
	if (0 != sysctl(mib, mibs, NULL, &sz, NULL, 0))
	{
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain necessary buffer size from system: %s",
				zbx_strerror(errno)));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	proc = (struct kinfo_proc *)zbx_malloc(proc, sz);
	if (0 != sysctl(mib, mibs, proc, &sz, NULL, 0))
	{
		zbx_free(proc);
		SET_MSG_RESULT(result, zbx_dsprintf(NULL, "Cannot obtain process information: %s",
				zbx_strerror(errno)));
		ret = SYSINFO_RET_FAIL;
		goto clean;
	}

	count = sz / sizeof(struct kinfo_proc);

	for (i = 0; i < count; i++)
	{
		proc_ok = 0;
		stat_ok = 0;
		comm_ok = 0;

		if (NULL == procname || '\0' == *procname || 0 == strcmp(procname, proc[i].ZBX_PROC_COMM))
			proc_ok = 1;

		if (zbx_proc_stat != ZBX_PROC_STAT_ALL)
		{
			switch (zbx_proc_stat) {
			case ZBX_PROC_STAT_RUN:
				if (SRUN == proc[i].ZBX_PROC_STAT)
					stat_ok = 1;
				break;
			case ZBX_PROC_STAT_SLEEP:
				if (SSLEEP == proc[i].ZBX_PROC_STAT && 0 != (proc[i].ZBX_PROC_TDFLAG & TDF_SINTR))
					stat_ok = 1;
				break;
			case ZBX_PROC_STAT_ZOMB:
				if (SZOMB == proc[i].ZBX_PROC_STAT)
					stat_ok = 1;
				break;
			case ZBX_PROC_STAT_DISK:
				if (SSLEEP == proc[i].ZBX_PROC_STAT && 0 == (proc[i].ZBX_PROC_TDFLAG & TDF_SINTR))
					stat_ok = 1;
				break;
			case ZBX_PROC_STAT_TRACE:
				if (SSTOP == proc[i].ZBX_PROC_STAT)
					stat_ok = 1;
				break;
			}
		}
		else
			stat_ok = 1;

		if (NULL != proccomm && '\0' != *proccomm)
		{
			if (NULL != (args = get_commandline(&proc[i])))
				if (0 == zbx_regexp_match_precompiled(args, proccomm_rxp))
					comm_ok = 1;
		}
		else
			comm_ok = 1;

		if (proc_ok && stat_ok && comm_ok)
			proccount++;
	}
	zbx_free(proc);
out:
	SET_UI64_RESULT(result, proccount);
clean:
	if (NULL != proccomm_rxp)
		zbx_regexp_free(proccomm_rxp);

	return ret;
}
