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
 * @var array $data
 */

$this->includeJsFile('administration.proxy.list.js.php');

if ($data['uncheck']) {
	uncheckTableRows('proxy');
}

$widget = (new CWidget())
	->setTitle(_('Proxies'))
	->setControls((new CTag('nav', true,
		(new CList())
			->addItem(new CRedirectButton(_('Create proxy'), 'zabbix.php?action=proxy.edit'))
		))
			->setAttribute('aria-label', _('Content controls'))
	)
	->addItem((new CFilter())
		->setResetUrl((new CUrl('zabbix.php'))->setArgument('action', 'proxy.list'))
		->setProfile($data['profileIdx'])
		->setActiveTab($data['active_tab'])
		->addFilterTab(_('Filter'), [
			(new CFormList())->addRow(_('Name'),
				(new CTextBox('filter_name', $data['filter']['name']))
					->setWidth(ZBX_TEXTAREA_FILTER_SMALL_WIDTH)
					->setAttribute('autofocus', 'autofocus')
			),
			(new CFormList())->addRow(_('Mode'),
				(new CRadioButtonList('filter_status', (int) $data['filter']['status']))
					->addValue(_('Any'), -1)
					->addValue(_('Active'), HOST_STATUS_PROXY_ACTIVE)
					->addValue(_('Passive'), HOST_STATUS_PROXY_PASSIVE)
					->setModern(true)
			)
		])
		->addVar('action', 'proxy.list')
	);

// create form
$proxyForm = (new CForm('get'))->setName('proxyForm');

// create table
$proxyTable = (new CTableInfo())
	->setHeader([
		(new CColHeader(
			(new CCheckBox('all_hosts'))
				->onClick("checkAll('".$proxyForm->getName()."', 'all_hosts', 'proxyids');")
		))->addClass(ZBX_STYLE_CELL_WIDTH),
		make_sorting_header(_('Name'), 'host', $data['sort'], $data['sortorder'],
			(new CUrl('zabbix.php'))
				->setArgument('action', 'proxy.list')
				->getUrl()
		),
		_('Mode'),
		_('Encryption'),
		_('Compression'),
		_('Last seen (age)'),
		_('Host count'),
		_('Item count'),
		_('Required performance (vps)'),
		_('Hosts')
	]);

foreach ($data['proxies'] as $proxy) {
	$hosts = [];
	$i = 0;

	foreach ($proxy['hosts'] as $host) {
		if (++$i > $data['config']['max_in_table']) {
			$hosts[] = [' ', HELLIP()];

			break;
		}

		switch ($host['status']) {
			case HOST_STATUS_MONITORED:
				$style = null;
				break;
			case HOST_STATUS_TEMPLATE:
				$style = ZBX_STYLE_GREY;
				break;
			default:
				$style = ZBX_STYLE_RED;
		}

		if ($hosts) {
			$hosts[] = ', ';
		}

		$hosts[] = $data['allowed_ui_conf_hosts']
			? (new CLink($host['name'], (new CUrl('zabbix.php'))
				->setArgument('action', 'host.edit')
				->setArgument('hostid', $host['hostid'])
			))
				->addClass($style)
				->onClick('view.editHost(event, '.json_encode($host['hostid']).')')
			: (new CSpan($host['name']))->addClass($style);
	}

	$name = (new CLink($proxy['host'], (new CUrl('zabbix.php'))
		->setArgument('action', 'proxy.edit')
		->setArgument('proxyid', $proxy['proxyid'])
	));

	// encryption
	$in_encryption = '';
	$out_encryption = '';

	if ($proxy['status'] == HOST_STATUS_PROXY_PASSIVE) {
		// input encryption
		if ($proxy['tls_connect'] == HOST_ENCRYPTION_NONE) {
			$in_encryption = (new CSpan(_('None')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}
		elseif ($proxy['tls_connect'] == HOST_ENCRYPTION_PSK) {
			$in_encryption = (new CSpan(_('PSK')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}
		else {
			$in_encryption = (new CSpan(_('CERT')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}
	}
	else {
		// output encryption
		$out_encryption_array = [];
		if (($proxy['tls_accept'] & HOST_ENCRYPTION_NONE) == HOST_ENCRYPTION_NONE) {
			$out_encryption_array[] = (new CSpan(_('None')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}
		if (($proxy['tls_accept'] & HOST_ENCRYPTION_PSK) == HOST_ENCRYPTION_PSK) {
			$out_encryption_array[] = (new CSpan(_('PSK')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}
		if (($proxy['tls_accept'] & HOST_ENCRYPTION_CERTIFICATE) == HOST_ENCRYPTION_CERTIFICATE) {
			$out_encryption_array[] = (new CSpan(_('CERT')))->addClass(ZBX_STYLE_STATUS_GREEN);
		}

		$out_encryption = (new CDiv($out_encryption_array))->addClass(ZBX_STYLE_STATUS_CONTAINER);
	}

	$proxyTable->addRow([
		new CCheckBox('proxyids['.$proxy['proxyid'].']', $proxy['proxyid']),
		(new CCol($name))->addClass(ZBX_STYLE_WORDBREAK),
		$proxy['status'] == HOST_STATUS_PROXY_ACTIVE ? _('Active') : _('Passive'),
		$proxy['status'] == HOST_STATUS_PROXY_ACTIVE ? $out_encryption : $in_encryption,
		($proxy['auto_compress'] == HOST_COMPRESSION_ON)
			? (new CSpan(_('On')))->addClass(ZBX_STYLE_STATUS_GREEN)
			: (new CSpan(_('Off')))->addClass(ZBX_STYLE_STATUS_GREY),
		($proxy['lastaccess'] == 0)
			? (new CSpan(_('Never')))->addClass(ZBX_STYLE_RED)
			: zbx_date2age($proxy['lastaccess']),
		array_key_exists('host_count', $proxy) ? $proxy['host_count'] : '',
		array_key_exists('item_count', $proxy) ? $proxy['item_count'] : '',
		array_key_exists('vps_total', $proxy) ? $proxy['vps_total'] : '',
		$hosts ? (new CCol($hosts))->addClass(ZBX_STYLE_WORDBREAK) : ''
	]);
}

// append table to form
$proxyForm->addItem([
	$proxyTable,
	$data['paging'],
	new CActionButtonList('action', 'proxyids', [
		'proxy.hostenable' => ['name' => _('Enable hosts'),
			'confirm' => _('Enable hosts monitored by selected proxies?')
		],
		'proxy.hostdisable' => ['name' => _('Disable hosts'),
			'confirm' => _('Disable hosts monitored by selected proxies?')
		],
		'proxy.delete' => ['name' => _('Delete'), 'confirm' => _('Delete selected proxies?')]
	], 'proxy')
]);

// append form to widget
$widget->addItem($proxyForm)->show();
