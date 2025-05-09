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
 * Class for standard ajax response generation.
 */
class CAjaxResponse {

	private $_result = true;
	private $_data = [];
	private $_errors = [];

	public function __construct($data = null) {
		if ($data !== null) {
			$this->success($data);
		}
	}

	/**
	 * Add error to ajax response. All errors are returned as array in 'errors' part of response.
	 *
	 * @param string $error error text
	 */
	public function error($error) {
		$this->_result = false;
		$this->_errors[] = ['error' => $error];
	}

	/**
	 * Assigns data that is returned in 'data' part of ajax response.
	 * If any error was added previously, this method does nothing.
	 *
	 * @param array $data
	 */
	public function success(array $data) {
		if ($this->_result) {
			$this->_data = $data;
		}
	}

	/**
	 * Output ajax response. If any error was added, 'result' is false, otherwise true.
	 */
	public function send() {
		echo json_encode($this->_result
			? ['result' => true, 'data' => $this->_data]
			: ['result' => false, 'errors' => $this->_errors]
		);
	}
}
