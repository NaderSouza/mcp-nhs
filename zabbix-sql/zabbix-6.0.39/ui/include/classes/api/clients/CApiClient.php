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
 * This class should be used for calling API services.
 */
abstract class CApiClient {

	/**
	 * Call the given API service method and return the response.
	 *
	 * @param string 	$api
	 * @param string 	$method
	 * @param array 	$params
	 * @param string	$auth
	 *
	 * @return CApiClientResponse
	 */
	abstract public function callMethod($api, $method, array $params, $auth);
}
