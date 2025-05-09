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


if (version_compare(PHP_VERSION, '7.2.5', '<')) {
	echo sprintf('Minimum required PHP version is %1$s.', '7.2.5');
	exit;
}

require_once dirname(__FILE__).'/ZBase.php';

/**
 * A wrapper for the ZBase class.
 *
 * Feel free to modify and extend it to change the functionality of ZBase.
 */
class APP extends ZBase {

}
