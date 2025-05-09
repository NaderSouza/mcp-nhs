<?php declare(strict_types = 0);
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


class CControllerCorrelationEnable extends CController {

	protected function checkInput() {
		$fields = [
			'correlationids' => 'required|array_db correlation.correlationid'
		];

		$ret = $this->validateInput($fields);

		if (!$ret) {
			$this->setResponse(new CControllerResponseFatal());
		}

		return $ret;
	}

	protected function checkPermissions() {
		if (!$this->checkAccess(CRoleHelper::UI_CONFIGURATION_EVENT_CORRELATION)) {
			return false;
		}

		$correlations = API::Correlation()->get([
			'output' => [],
			'countOutput' => true,
			'correlationids' => $this->getInput('correlationids'),
			'editable' => true
		]);

		return ($correlations == count($this->getInput('correlationids')));
	}

	protected function doAction() {
		$correlations = [];

		foreach ($this->getInput('correlationids') as $correlationid) {
			$correlations[] = [
				'correlationid' => $correlationid,
				'status' => ZBX_CORRELATION_ENABLED
			];
		}

		$result = API::Correlation()->update($correlations);
		$updated = count($correlations);

		$response = new CControllerResponseRedirect((new CUrl('zabbix.php'))
			->setArgument('action', 'correlation.list')
			->setArgument('page', CPagerHelper::loadPage('correlation.list', null))
		);

		if ($result) {
			$response->setFormData(['uncheck' => '1']);
			CMessageHelper::setSuccessTitle(_n('Correlation enabled', 'Correlations enabled', $updated));
		}
		else {
			CMessageHelper::setErrorTitle(_n('Cannot enable correlation', 'Cannot enable correlations', $updated));
		}

		$this->setResponse($response);
	}
}
