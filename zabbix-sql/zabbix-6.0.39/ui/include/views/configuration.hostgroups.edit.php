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

$widget = (new CWidget())->setTitle(_('Host groups'));

$form = (new CForm())
	->addItem((new CVar('form_refresh', $data['form_refresh'] + 1))->removeId())
	->setName('hostgroupForm')
	->setAttribute('aria-labelledby', ZBX_STYLE_PAGE_TITLE)
	->addVar('groupid', $data['groupid'])
	->addVar('form', $data['form']);

$form_list = (new CFormList('hostgroupFormList'));

if ($data['groupid'] != 0 && $data['group']['flags'] == ZBX_FLAG_DISCOVERY_CREATED) {
	$name = (new CSpan(_('Inaccessible discovery rule')))->addClass(ZBX_STYLE_GREY);

	if ($data['group']['discoveryRule']) {
		if ($data['allowed_ui_conf_hosts'] && $data['group']['is_discovery_rule_editable']) {
			$name = (new CLink($data['group']['discoveryRule']['name'],
					(new CUrl('host_prototypes.php'))
						->setArgument('form', 'update')
						->setArgument('parent_discoveryid', $data['group']['discoveryRule']['itemid'])
						->setArgument('hostid', $data['group']['hostPrototype']['hostid'])
						->setArgument('context', 'host')
				));
		}
		else {
			$name = new CSpan($data['group']['discoveryRule']['name']);
		}
	}

	$form_list->addRow(_('Discovered by'), $name);
}

$form_list->addRow(
		(new CLabel(_('Group name'), 'name'))->setAsteriskMark(),
		(new CTextBox('name', $data['name'], $data['groupid'] && $data['group']['flags'] == ZBX_FLAG_DISCOVERY_CREATED))
			->setAttribute('autofocus', 'autofocus')
			->setWidth(ZBX_TEXTAREA_STANDARD_WIDTH)
			->setAriaRequired()
	);

if ($data['groupid'] != 0 && CWebUser::getType() == USER_TYPE_SUPER_ADMIN) {
	$form_list->addRow(null,
		(new CCheckBox('subgroups'))
			->setLabel(_('Apply permissions and tag filters to all subgroups'))
			->setChecked($data['subgroups'])
	);
}

$tab = (new CTabView())->addTab('hostgroupTab', _('Host group'), $form_list);

if ($data['groupid'] == 0) {
	$tab->setFooter(makeFormFooter(
		new CSubmit('add', _('Add')),
		[new CButtonCancel()]
	));
}
else {
	$tab->setFooter(makeFormFooter(
		new CSubmit('update', _('Update')), [
			(new CSubmit('clone', _('Clone')))->setEnabled(CWebUser::getType() == USER_TYPE_SUPER_ADMIN),
			(new CButtonDelete(_('Delete selected group?'), url_param('form').url_param('groupid'))),
			new CButtonCancel()
		]
	));
}

$form->addItem($tab);

$widget->addItem($form);

$widget->show();
