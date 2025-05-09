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

class CWidgetFieldCheckBox extends CWidgetField {

	private $caption;

	/**
	 * Check box widget field.
	 *
	 * @param string $name     Field name in form.
	 * @param string $label    Label for the field in form.
	 * @param string $caption  Text after checkbox.
	 */
	public function __construct($name, $label, $caption = null) {
		parent::__construct($name, $label);

		$this->setSaveType(ZBX_WIDGET_FIELD_TYPE_INT32);
		$this->setDefault(0);
		$this->caption = $caption;
	}

	public function setValue($value) {
		return parent::setValue((int) $value);
	}

	public function getCaption() {
		return $this->caption;
	}
}
