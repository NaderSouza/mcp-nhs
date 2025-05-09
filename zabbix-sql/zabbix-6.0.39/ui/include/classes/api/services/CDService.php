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


/**
 * Class containing methods for operations with discovery services.
 */
class CDService extends CApiService {

	public const ACCESS_RULES = [
		'get' => ['min_user_type' => USER_TYPE_ZABBIX_USER]
	];

	protected $tableName = 'dservices';
	protected $tableAlias = 'ds';
	protected $sortColumns = ['dserviceid', 'dhostid', 'ip'];

	/**
	 * Get discovery service data.
	 *
	 * @param array  $options
	 * @param array  $options['groupids']				ServiceGroup IDs
	 * @param array  $options['hostids']				Service IDs
	 * @param bool   $options['monitored_hosts']		only monitored Services
	 * @param bool   $options['templated_hosts']		include templates in result
	 * @param bool   $options['with_items']				only with items
	 * @param bool   $options['with_triggers']			only with triggers
	 * @param bool   $options['with_httptests']			only with http tests
	 * @param bool   $options['with_graphs']			only with graphs
	 * @param bool   $options['editable']				only with read-write permission. Ignored for SuperAdmins
	 * @param bool   $options['selectGroups']			select ServiceGroups
	 * @param bool   $options['selectTemplates']		select Templates
	 * @param bool   $options['selectItems']			select Items
	 * @param bool   $options['selectTriggers']			select Triggers
	 * @param bool   $options['selectGraphs']			select Graphs
	 * @param int    $options['count']					count Services, returned column name is rowscount
	 * @param string $options['pattern']				search hosts by pattern in Service name
	 * @param string $options['extendPattern']			search hosts by pattern in Service name, ip and DNS
	 * @param int    $options['limit']					limit selection
	 * @param string $options['sortfield']				field to sort by
	 * @param string $options['sortorder']				sort order
	 *
	 * @return array									service data as array or false if error
	 */
	public function get($options = []) {
		$result = [];

		$sqlParts = [
			'select'	=> ['dservices' => 'ds.dserviceid'],
			'from'		=> ['dservices' => 'dservices ds'],
			'where'		=> [],
			'group'		=> [],
			'order'		=> [],
			'limit'		=> null
		];

		$defOptions = [
			'dserviceids'				=> null,
			'dhostids'					=> null,
			'dcheckids'					=> null,
			'druleids'					=> null,
			'editable'					=> false,
			'nopermissions'				=> null,
			// filter
			'filter'					=> null,
			'search'					=> null,
			'searchByAny'				=> null,
			'startSearch'				=> false,
			'excludeSearch'				=> false,
			'searchWildcardsEnabled'	=> null,
			// output
			'output'					=> API_OUTPUT_EXTEND,
			'selectDRules'				=> null,
			'selectDHosts'				=> null,
			'selectHosts'				=> null,
			'countOutput'				=> false,
			'groupCount'				=> false,
			'preservekeys'				=> false,
			'sortfield'					=> '',
			'sortorder'					=> '',
			'limit'						=> null,
			'limitSelects'				=> null
		];
		$options = zbx_array_merge($defOptions, $options);

		if (self::$userData['type'] < USER_TYPE_ZABBIX_ADMIN) {
			return [];
		}

// dserviceids
		if (!is_null($options['dserviceids'])) {
			zbx_value2array($options['dserviceids']);
			$sqlParts['where']['dserviceid'] = dbConditionInt('ds.dserviceid', $options['dserviceids']);
		}

// dhostids
		if (!is_null($options['dhostids'])) {
			zbx_value2array($options['dhostids']);

			$sqlParts['where'][] = dbConditionInt('ds.dhostid', $options['dhostids']);

			if ($options['groupCount']) {
				$sqlParts['group']['dhostid'] = 'ds.dhostid';
			}
		}

// dcheckids
		if (!is_null($options['dcheckids'])) {
			zbx_value2array($options['dcheckids']);

			$sqlParts['where'][] = dbConditionInt('ds.dcheckid', $options['dcheckids']);

			if ($options['groupCount']) {
				$sqlParts['group']['dcheckid'] = 'ds.dcheckid';
			}
		}

// druleids
		if (!is_null($options['druleids'])) {
			zbx_value2array($options['druleids']);

			$sqlParts['from']['dhosts'] = 'dhosts dh';

			$sqlParts['where']['druleid'] = dbConditionInt('dh.druleid', $options['druleids']);
			$sqlParts['where']['dhds'] = 'dh.dhostid=ds.dhostid';

			if ($options['groupCount']) {
				$sqlParts['group']['druleid'] = 'dh.druleid';
			}
		}

// filter
		if (is_array($options['filter'])) {
			$this->dbFilter('dservices ds', $options, $sqlParts);
		}

// search
		if (is_array($options['search'])) {
			zbx_db_search('dservices ds', $options, $sqlParts);
		}

// limit
		if (zbx_ctype_digit($options['limit']) && $options['limit']) {
			$sqlParts['limit'] = $options['limit'];
		}
//-------

		$sqlParts = $this->applyQueryOutputOptions($this->tableName(), $this->tableAlias(), $options, $sqlParts);
		$sqlParts = $this->applyQuerySortOptions($this->tableName(), $this->tableAlias(), $options, $sqlParts);
		$res = DBselect(self::createSelectQueryFromParts($sqlParts), $sqlParts['limit']);
		while ($dservice = DBfetch($res)) {
			if ($options['countOutput']) {
				if ($options['groupCount']) {
					$result[] = $dservice;
				}
				else {
					$result = $dservice['rowscount'];
				}
			}
			else {
				$result[$dservice['dserviceid']] = $dservice;
			}
		}

		if ($options['countOutput']) {
			return $result;
		}

		if ($result) {
			$result = $this->addRelatedObjects($options, $result);
			$result = $this->unsetExtraFields($result, ['dhostid'], $options['output']);
		}

		// removing keys (hash -> array)
		if (!$options['preservekeys']) {
			$result = zbx_cleanHashes($result);
		}

		return $result;
	}

	protected function applyQueryOutputOptions($tableName, $tableAlias, array $options, array $sqlParts) {
		$sqlParts = parent::applyQueryOutputOptions($tableName, $tableAlias, $options, $sqlParts);

		if (!$options['countOutput']) {
			if ($options['selectDHosts'] !== null) {
				$sqlParts = $this->addQuerySelect('ds.dhostid', $sqlParts);
			}
		}

		return $sqlParts;
	}

	protected function addRelatedObjects(array $options, array $result) {
		$result = parent::addRelatedObjects($options, $result);

		$dserviceIds = array_keys($result);

		// select_drules
		if ($options['selectDRules'] !== null && $options['selectDRules'] != API_OUTPUT_COUNT) {
			$drules = [];
			$relationMap = new CRelationMap();
			// discovered items
			$dbRules = DBselect(
				'SELECT ds.dserviceid,dh.druleid'.
					' FROM dservices ds,dhosts dh'.
					' WHERE '.dbConditionInt('ds.dserviceid', $dserviceIds).
					' AND ds.dhostid=dh.dhostid'
			);
			while ($rule = DBfetch($dbRules)) {
				$relationMap->addRelation($rule['dserviceid'], $rule['druleid']);
			}

			$related_ids = $relationMap->getRelatedIds();

			if ($related_ids) {
				$drules = API::DRule()->get([
					'output' => $options['selectDRules'],
					'druleids' => $related_ids,
					'preservekeys' => true
				]);
				if (!is_null($options['limitSelects'])) {
					order_result($drules, 'name');
				}
			}

			$result = $relationMap->mapMany($result, $drules, 'drules');
		}

		// selectDHosts
		if ($options['selectDHosts'] !== null && $options['selectDHosts'] != API_OUTPUT_COUNT) {
			$relationMap = $this->createRelationMap($result, 'dserviceid', 'dhostid');
			$dhosts = API::DHost()->get([
				'output' => $options['selectDHosts'],
				'dhosts' => $relationMap->getRelatedIds(),
				'preservekeys' => true
			]);
			if (!is_null($options['limitSelects'])) {
				order_result($dhosts, 'dhostid');
			}
			$result = $relationMap->mapMany($result, $dhosts, 'dhosts', $options['limitSelects']);
		}

		// selectHosts
		if ($options['selectHosts'] !== null) {
			foreach ($result as $dserviceid => $dservice) {
				$result[$dserviceid]['hosts'] = ($options['selectHosts'] == API_OUTPUT_COUNT) ? 0 : [];
			}

			$db_services = DBselect(
				'SELECT DISTINCT ds.dserviceid,h.hostid'.
				' FROM dservices ds,dchecks dc,drules dr,hosts h,interface i'.
				' WHERE ds.dcheckid=dc.dcheckid'.
					' AND dc.druleid=dr.druleid'.
					' AND (dr.proxy_hostid=h.proxy_hostid OR (dr.proxy_hostid IS NULL AND h.proxy_hostid IS NULL))'.
					' AND h.hostid=i.hostid'.
					' AND ds.ip=i.ip'.
					' AND '.dbConditionInt('ds.dserviceid', $dserviceIds)
			);

			$host_services = [];

			while ($db_service = DBfetch($db_services)) {
				$host_services[$db_service['hostid']][] = $db_service['dserviceid'];
			}

			$db_hosts = API::Host()->get([
				'output' => ($options['selectHosts'] == API_OUTPUT_COUNT) ? [] : $options['selectHosts'],
				'hostids' => array_keys($host_services),
				'sortfield' => 'hostid',
				'preservekeys' => true
			]);

			$db_hosts = $this->unsetExtraFields($db_hosts, ['hostid'], $options['selectHosts']);

			foreach ($db_hosts as $hostid => $db_host) {
				foreach ($host_services[$hostid] as $dserviceid) {
					if ($options['selectHosts'] == API_OUTPUT_COUNT) {
						$result[$dserviceid]['hosts']++;
					}
					elseif ($options['limitSelects'] === null
							|| count($result[$dserviceid]['hosts']) < $options['limitSelects']) {
						$result[$dserviceid]['hosts'][] = $db_host;
					}
				}
			}

			if ($options['selectHosts'] == API_OUTPUT_COUNT) {
				foreach ($result as $dserviceid => $dservice) {
					$result[$dserviceid]['hosts'] = (string) $dservice['hosts'];
				}
			}
		}

		return $result;
	}
}
