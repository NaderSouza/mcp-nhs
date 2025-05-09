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
 * Clock widget form view.
 *
 * @var array $data
 */

$fields = $data['dialogue']['fields'];

$form = CWidgetHelper::createForm();

$rf_rate_field = ($data['templateid'] === null) ? $fields['rf_rate'] : null;

$form_list = CWidgetHelper::createFormList($data['dialogue']['name'], $data['dialogue']['type'],
	$data['dialogue']['view_mode'], $data['known_widget_types'], $rf_rate_field
);

$scripts = [$this->readJsFile('../../../include/classes/widgets/views/js/widget.clock.form.view.js.php')];

// Time type.
$form_list->addRow(CWidgetHelper::getLabel($fields['time_type']), CWidgetHelper::getSelect($fields['time_type']));

// Item.
$field_itemid = CWidgetHelper::getItem($fields['itemid'], $data['captions']['items']['itemid'], $form->getName());
$form_list->addRow(CWidgetHelper::getMultiselectLabel($fields['itemid']), $field_itemid, null, 'js-row-itemid');
$scripts[] = $field_itemid->getPostJS();

$form
	->addItem($form_list)
	->addItem(
		(new CScriptTag('
			widget_clock_form.init('.json_encode([
				'form_id' => $form->getId()
			]).');
		'))->setOnDocumentReady()
	);

return [
	'form' => $form,
	'scripts' => $scripts
];
