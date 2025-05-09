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
 * @var CView $this
 */

$widget = (new CWidget())
	->setTitle(_('Maintenance periods'))
	->setControls(
		(new CTag('nav', true,
			(new CRedirectButton(_('Create maintenance period'), (new CUrl('maintenance.php'))
				->removeArgument('maintenanceid')
				->setArgument('form', 'create')
				->getUrl()
			))->setEnabled($data['allowed_edit']))
		)->setAttribute('aria-label', _('Content controls'))
	)
	->addItem((new CFilter())
		->setResetUrl(new CUrl('maintenance.php'))
		->setProfile($data['profileIdx'])
		->setActiveTab($data['active_tab'])
		->addFilterTab(_('Filter'), [
			(new CFormList())
				->addRow(
					(new CLabel(_('Host groups'), 'filter_groups__ms')),
					(new CMultiSelect([
						'name' => 'filter_groups[]',
						'object_name' => 'hostGroup',
						'data' => $data['filter']['groups'],
						'popup' => [
							'parameters' => [
								'srctbl' => 'host_groups',
								'srcfld1' => 'groupid',
								'dstfrm' => 'zbx_filter',
								'dstfld1' => 'filter_groups_',
								'editable' => 1
							]
						]
					]))->setWidth(ZBX_TEXTAREA_MEDIUM_WIDTH)
				)
				->addRow(_('Name'),
					(new CTextBox('filter_name', $data['filter']['name']))->setWidth(ZBX_TEXTAREA_MEDIUM_WIDTH)
				),
			(new CFormList())->addRow(_('State'),
				(new CRadioButtonList('filter_status', (int) $data['filter']['status']))
					->addValue(_('Any'), -1)
					->addValue(_x('Active', 'maintenance status'), MAINTENANCE_STATUS_ACTIVE)
					->addValue(_x('Approaching', 'maintenance status'), MAINTENANCE_STATUS_APPROACH)
					->addValue(_x('Expired', 'maintenance status'), MAINTENANCE_STATUS_EXPIRED)
					->setModern(true)
			)
		])
	);

// create form
$maintenanceForm = (new CForm())->setName('maintenanceForm');

// create table
$url = (new CUrl('maintenance.php'))->getUrl();

$maintenanceTable = (new CTableInfo())
	->setHeader([
		(new CColHeader(
			(new CCheckBox('all_maintenances'))->onClick("checkAll('".$maintenanceForm->getName()."', 'all_maintenances', 'maintenanceids');")
		))->addClass(ZBX_STYLE_CELL_WIDTH),
		make_sorting_header(_('Name'), 'name', $this->data['sort'], $this->data['sortorder'], $url),
		make_sorting_header(_('Type'), 'maintenance_type', $this->data['sort'], $this->data['sortorder'], $url),
		make_sorting_header(_('Active since'), 'active_since', $this->data['sort'], $this->data['sortorder'], $url),
		make_sorting_header(_('Active till'), 'active_till', $this->data['sort'], $this->data['sortorder'], $url),
		_('State'),
		_('Description')
	]);

foreach ($this->data['maintenances'] as $maintenance) {
	$maintenanceid = $maintenance['maintenanceid'];

	switch ($maintenance['status']) {
		case MAINTENANCE_STATUS_EXPIRED:
			$maintenanceStatus = (new CSpan(_x('Expired', 'maintenance status')))->addClass(ZBX_STYLE_RED);
			break;
		case MAINTENANCE_STATUS_APPROACH:
			$maintenanceStatus = (new CSpan(_x('Approaching', 'maintenance status')))->addClass(ZBX_STYLE_ORANGE);
			break;
		case MAINTENANCE_STATUS_ACTIVE:
			$maintenanceStatus = (new CSpan(_x('Active', 'maintenance status')))->addClass(ZBX_STYLE_GREEN);
			break;
	}

	$maintenanceTable->addRow([
		new CCheckBox('maintenanceids['.$maintenanceid.']', $maintenanceid),
		(new CCol(
			new CLink($maintenance['name'], 'maintenance.php?form=update&maintenanceid='.$maintenanceid)
		))->addClass(ZBX_STYLE_WORDBREAK),
		$maintenance['maintenance_type'] ? _('No data collection') : _('With data collection'),
		zbx_date2str(DATE_TIME_FORMAT, $maintenance['active_since']),
		zbx_date2str(DATE_TIME_FORMAT, $maintenance['active_till']),
		$maintenanceStatus,
		(new CCol($maintenance['description']))
			->addClass(ZBX_STYLE_WORDBREAK)
			->addStyle('max-width: '.ZBX_TEXTAREA_BIG_WIDTH.'px;')
	]);
}

// append table to form
$maintenanceForm->addItem([
	$maintenanceTable,
	$this->data['paging'],
	new CActionButtonList('action', 'maintenanceids', [
		'maintenance.massdelete' => ['name' => _('Delete'), 'confirm' => _('Delete selected maintenance periods?'),
			'disabled' => $data['allowed_edit'] ? null : 'disabled'
		]
	])
]);

// append form to widget
$widget->addItem($maintenanceForm);

$widget->show();
