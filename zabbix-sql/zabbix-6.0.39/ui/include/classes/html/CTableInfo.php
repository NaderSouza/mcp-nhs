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


class CTableInfo extends CTable {

	protected $message;

	public function __construct() {
		parent::__construct();

		$this->addClass(ZBX_STYLE_LIST_TABLE);
		$this->setNoDataMessage(_('No data found.'));
	}

	public function toString($destroy = true) {
		$tableid = $this->getId();

		if (!$tableid) {
			$tableid = uniqid('t', true);
			$tableid = str_replace('.', '', $tableid);
			$this->setId($tableid);
		}

		return parent::toString($destroy);
	}

	public function setNoDataMessage($message) {
		$this->message = $message;

		return $this;
	}

	protected function endToString() {
		$ret = '';
		if ($this->rownum == 0 && $this->message !== null) {
			$ret .= $this->prepareRow(new CCol($this->message), ZBX_STYLE_NOTHING_TO_SHOW)->toString();
		}
		$ret .= parent::endToString();

		return $ret;
	}
}
