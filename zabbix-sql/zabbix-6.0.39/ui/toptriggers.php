<?php
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


require_once dirname(__FILE__).'/include/config.inc.php';
require_once dirname(__FILE__).'/include/triggers.inc.php';

$page['title'] = _('100 busiest triggers');
$page['file'] = 'toptriggers.php';
$page['scripts'] = ['class.calendar.js', 'gtlc.js'];

require_once dirname(__FILE__).'/include/page_header.php';

//	VAR					TYPE	OPTIONAL	FLAGS	VALIDATION	EXCEPTION
$fields = [
	'groupids' =>	[T_ZBX_INT,			O_OPT,	P_SYS|P_ONLY_ARRAY,	DB_ID,	null],
	'hostids' =>	[T_ZBX_INT,			O_OPT,	P_SYS|P_ONLY_ARRAY,	DB_ID,	null],
	'severities' =>	[T_ZBX_INT,			O_OPT,	P_SYS|P_ONLY_ARRAY,	null,	null],
	'from' =>		[T_ZBX_RANGE_TIME,	O_OPT,	P_SYS,	null,	null],
	'to' =>			[T_ZBX_RANGE_TIME,	O_OPT,	P_SYS,	null,	null],
	'filter_rst' =>	[T_ZBX_STR,			O_OPT,	P_SYS,	null,	null],
	'filter_set' =>	[T_ZBX_STR,			O_OPT,	P_SYS,	null,	null]
];
check_fields($fields);
validateTimeSelectorPeriod(getRequest('from'), getRequest('to'));

/*
 * Filter
 */
if (hasRequest('filter_set')) {
	CProfile::updateArray('web.toptriggers.filter.severities', getRequest('severities', []), PROFILE_TYPE_STR);
	CProfile::updateArray('web.toptriggers.filter.groupids', getRequest('groupids', []), PROFILE_TYPE_STR);
	CProfile::updateArray('web.toptriggers.filter.hostids', getRequest('hostids', []), PROFILE_TYPE_STR);
}
elseif (hasRequest('filter_rst')) {
	DBstart();
	CProfile::deleteIdx('web.toptriggers.filter.severities');
	CProfile::deleteIdx('web.toptriggers.filter.groupids');
	CProfile::deleteIdx('web.toptriggers.filter.hostids');
	DBend();
}

$timeselector_options = [
	'profileIdx' => 'web.toptriggers.filter',
	'profileIdx2' => 0,
	'from' => getRequest('from'),
	'to' => getRequest('to')
];
updateTimeSelectorPeriod($timeselector_options);

$data['filter'] = [
	'severities' => CProfile::getArray('web.toptriggers.filter.severities', []),
	'timeline' => getTimeSelectorPeriod($timeselector_options),
	'active_tab' => CProfile::get('web.toptriggers.filter.active', 1)
];

// multiselect host groups
$data['multiSelectHostGroupData'] = [];
$groupids = CProfile::getArray('web.toptriggers.filter.groupids', []);

if ($groupids) {
	$groupids = getSubGroups($groupids, $data['multiSelectHostGroupData']);
}

// multiselect hosts
$data['multiSelectHostData'] = [];
$hostids = CProfile::getArray('web.toptriggers.filter.hostids', []);

if ($hostids) {
	$filterHosts = API::Host()->get([
		'output' => ['hostid', 'name'],
		'hostids' => $hostids
	]);

	foreach ($filterHosts as $filterHost) {
		$data['multiSelectHostData'][] = [
			'id' => $filterHost['hostid'],
			'name' => $filterHost['name']
		];
	}
}

// data generation
$triggersEventCount = [];

// get 100 triggerids with max event count
$sql = 'SELECT e.objectid,count(distinct e.eventid) AS cnt_event'.
		' FROM triggers t,events e'.
		' WHERE t.triggerid=e.objectid'.
			' AND e.source='.EVENT_SOURCE_TRIGGERS.
			' AND e.object='.EVENT_OBJECT_TRIGGER.
			' AND e.clock>='.zbx_dbstr($data['filter']['timeline']['from_ts']).
			' AND e.clock<='.zbx_dbstr($data['filter']['timeline']['to_ts']);

if ($data['filter']['severities']) {
	$sql .= ' AND '.dbConditionInt('t.priority', $data['filter']['severities']);
}

if ($hostids) {
	$inHosts = ' AND '.dbConditionInt('i.hostid', $hostids);
}

if ($groupids) {
	$inGroups = ' AND '.dbConditionInt('hgg.groupid', $groupids);
}

if (CWebUser::getType() == USER_TYPE_SUPER_ADMIN && ($groupids || $hostids)) {
	$sql .= ' AND EXISTS ('.
				'SELECT NULL'.
				' FROM functions f,items i,hosts_groups hgg'.
				' WHERE t.triggerid=f.triggerid'.
					' AND f.itemid=i.itemid'.
					' AND i.hostid=hgg.hostid'.
					($hostids ? $inHosts : '').
					($groupids ? $inGroups : '').
			')';
}
elseif (CWebUser::getType() != USER_TYPE_SUPER_ADMIN) {
	// add permission filter
	$userId = CWebUser::$data['userid'];
	$userGroups = getUserGroupsByUserId($userId);
	$sql .= ' AND EXISTS ('.
				'SELECT NULL'.
				' FROM functions f,items i,hosts_groups hgg'.
				' JOIN rights r'.
					' ON r.id=hgg.groupid'.
						' AND '.dbConditionInt('r.groupid', $userGroups).
				' WHERE t.triggerid=f.triggerid'.
					' AND f.itemid=i.itemid'.
					' AND i.hostid=hgg.hostid'.
					($hostids ? $inHosts : '').
					($groupids ? $inGroups : '').
				' GROUP BY f.triggerid'.
				' HAVING MIN(r.permission)>'.PERM_DENY.
			')';
}
$sql .= ' AND '.dbConditionInt('t.flags', [ZBX_FLAG_DISCOVERY_NORMAL, ZBX_FLAG_DISCOVERY_CREATED]).
		' GROUP BY e.objectid'.
		' ORDER BY cnt_event DESC';
$result = DBselect($sql, 100);
while ($row = DBfetch($result)) {
	$triggersEventCount[$row['objectid']] = $row['cnt_event'];
}

$data['triggers'] = API::Trigger()->get([
	'output' => ['triggerid', 'description', 'expression', 'priority', 'lastchange'],
	'selectHosts' => ['hostid', 'status', 'name'],
	'triggerids' => array_keys($triggersEventCount),
	'expandDescription' => true,
	'preservekeys' => true
]);

$trigger_hostids = [];

foreach ($data['triggers'] as $triggerId => $trigger) {
	$hostId = $trigger['hosts'][0]['hostid'];
	$trigger_hostids[$hostId] = $hostId;

	$data['triggers'][$triggerId]['cnt_event'] = $triggersEventCount[$triggerId];
}

CArrayHelper::sort($data['triggers'], [
	['field' => 'cnt_event', 'order' => ZBX_SORT_DOWN],
	'host', 'description', 'priority'
]);

$data['hosts'] = API::Host()->get([
	'output' => ['hostid', 'status'],
	'hostids' => $trigger_hostids,
	'preservekeys' => true
]);

// render view
echo (new CView('reports.toptriggers', $data))->getOutput();

require_once dirname(__FILE__).'/include/page_footer.php';
