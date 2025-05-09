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

if ($data['uncheck']) {
	uncheckTableRows('userrole');
}

$widget = (new CWidget())
	->setTitle(_('User roles'))
	->setControls((new CTag('nav', true,
		(new CList())
			->addItem(new CRedirectButton(_('Create user role'),
				(new CUrl('zabbix.php'))->setArgument('action', 'userrole.edit'))
			)
		))->setAttribute('aria-label', _('Content controls'))
	)
	->addItem((new CFilter())
		->setResetUrl((new CUrl('zabbix.php'))->setArgument('action', 'userrole.list'))
		->addVar('action', 'userrole.list')
		->setProfile($data['profileIdx'])
		->setActiveTab($data['active_tab'])
		->addFilterTab(_('Filter'), [
			(new CFormList())->addRow(_('Name'),
				(new CTextBox('filter_name', $data['filter']['name']))
					->setWidth(ZBX_TEXTAREA_FILTER_SMALL_WIDTH)
					->setAttribute('autofocus', 'autofocus')
			)
		])
	);

$form = (new CForm())
	->setName('userroles_form')
	->setId('userroles');

$table = (new CTableInfo())
	->setHeader([
		(new CColHeader((new CCheckBox('all_roles'))->onClick(sprintf(
			'checkAll(\'%s\',\'all_roles\',\'roleids\');', $form->getName()
		))))->addClass(ZBX_STYLE_CELL_WIDTH),
		make_sorting_header(_('Name'), 'name', $data['sort'], $data['sortorder'],
			(new CUrl('zabbix.php'))
				->setArgument('action', 'userrole.list')
				->getUrl()
		),
		'#',
		_('Users')
	]);

foreach ($this->data['roles'] as $role) {
	$users = [];

	foreach ($role['users'] as $user) {
		if ($users) {
			$users[] = ', ';
		}

		$user_has_access = ($user['gui_access'] != GROUP_GUI_ACCESS_DISABLED
			&& $user['users_status'] != GROUP_STATUS_DISABLED
		);

		$user = $data['allowed_ui_users']
			? (new CLink(getUserFullname($user), (new CUrl('zabbix.php'))
				->setArgument('action', 'user.edit')
				->setArgument('userid', $user['userid'])
				->getUrl()
			))
				->addClass(ZBX_STYLE_LINK_ALT)
			: new CSpan(getUserFullname($user));

		$users[] = $user->addClass($user_has_access ? ZBX_STYLE_GREEN : ZBX_STYLE_RED);
	}

	if (count($role['users']) != $role['user_cnt']) {
		$users[] = [' ', HELLIP()];
	}

	$name = new CLink($role['name'], (new CUrl('zabbix.php'))
		->setArgument('action', 'userrole.edit')
		->setArgument('roleid', $role['roleid'])
		->getUrl()
	);

	$table->addRow([
		(new CCheckBox('roleids['.$role['roleid'].']', $role['roleid']))->setEnabled($role['readonly'] ? false : true),
		(new CCol($name))->addClass(ZBX_STYLE_WORDBREAK),
		[
			$data['allowed_ui_users']
				? new CLink(_('Users'), (new CUrl('zabbix.php'))
					->setArgument('action', 'user.list')
					->setArgument('filter_roles[]', $role['roleid'])
					->setArgument('filter_set', 1)
					->getUrl()
				)
				: _('Users'),
			CViewHelper::showNum($role['user_cnt'])
		],
		$users
	]);
}

$form->addItem([
	$table,
	$this->data['paging'],
	new CActionButtonList('action', 'roleids', [
		'userrole.delete' => ['name' => _('Delete'), 'confirm' => _('Delete selected roles?')]
	], 'userrole')
]);

$widget->addItem($form);
$widget->show();
