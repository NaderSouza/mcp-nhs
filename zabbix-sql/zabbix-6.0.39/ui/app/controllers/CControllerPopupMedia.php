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


class CControllerPopupMedia extends CController {
	protected function init() {
		$this->disableSIDvalidation();
	}

	protected function checkInput() {
		$fields = [
			'dstfrm' =>			'required|string',
			'media' =>			'int32',
			'mediatypeid' =>	'db media_type.mediatypeid',
			'sendto' =>			'string',
			'sendto_emails'	=>	'array',
			'period' =>			'time_periods',
			'active' =>			'in '.implode(',', [MEDIA_STATUS_ACTIVE, MEDIA_STATUS_DISABLED]),
			'severity' =>		'',
			'add' =>			'in 1'
		];

		$ret = $this->validateInput($fields);

		if (!$ret) {
			$output = [];
			if (($messages = getMessages()) !== null) {
				$output['errors'] = $messages->toString();
			}

			$this->setResponse(
				(new CControllerResponseData(['main_block' => json_encode($output)]))->disableView()
			);
		}

		return $ret;
	}

	protected function checkPermissions() {
		if (CWebUser::getType() < USER_TYPE_ZABBIX_ADMIN
				|| (CWebUser::isGuest() && CWebUser::getType() < USER_TYPE_SUPER_ADMIN)) {
			return false;
		}

		return true;
	}

	protected function doAction() {
		$page_options = [
			'dstfrm' => $this->getInput('dstfrm'),
			'media' => $this->getInput('media', -1),
			'sendto' => $this->getInput('sendto', ''),
			'mediatypeid' => $this->getInput('mediatypeid', 0),
			'active' => $this->getInput('active', MEDIA_STATUS_ACTIVE),
			'period' => $this->getInput('period', ZBX_DEFAULT_INTERVAL),
			'sendto_emails' => array_values($this->getInput('sendto_emails', ['']))
		];

		// Validation before adding Media to user's Media tab.
		if ($this->hasInput('add')) {
			$output = [];

			$db_mediatypes = API::MediaType()->get([
				'output' => ['type'],
				'mediatypeids' => $page_options['mediatypeid']
			]);

			if (!$db_mediatypes) {
				error(_s('Media type with ID "%1$s" is not available.', $page_options['mediatypeid']));
			}
			else {
				$type = $db_mediatypes[0]['type'];

				if ($type == MEDIA_TYPE_EMAIL) {
					$email_validator = new CEmailValidator();

					$page_options['sendto_emails'] = array_values(array_filter($page_options['sendto_emails']));
					if (!$page_options['sendto_emails']) {
						error(_s('Incorrect value for field "%1$s": %2$s.', 'sendto_emails', _('cannot be empty')));
					}

					foreach ($page_options['sendto_emails'] as $email) {
						if (!$email_validator->validate($email)) {
							error($email_validator->getError());
							break;
						}
					}
				}
				elseif ($page_options['sendto'] === '') {
					error(_s('Incorrect value for field "%1$s": %2$s.', 'sendto', _('cannot be empty')));
				}
			}

			if (($messages = getMessages()) !== null) {
				$output['errors'] = $messages->toString();
			}
			else {
				$severity = 0;
				$input_severity = $this->getInput('severity', []);
				foreach ($input_severity as $id) {
					$severity |= 1 << $id;
				}

				$output = [
					'dstfrm' => $page_options['dstfrm'],
					'media' => $this->getInput('media', -1),
					'mediatypeid' => $page_options['mediatypeid'],
					'sendto' => ($type == MEDIA_TYPE_EMAIL)
									? $page_options['sendto_emails']
									: $page_options['sendto'],
					'period' => $page_options['period'],
					'active' => $this->getInput('active', MEDIA_STATUS_DISABLED),
					'severity' => $severity
				];
			}

			$this->setResponse(
				(new CControllerResponseData(['main_block' => json_encode($output)]))->disableView()
			);
		}
		else {
			$severities = [];

			for ($severity = TRIGGER_SEVERITY_NOT_CLASSIFIED; $severity < TRIGGER_SEVERITY_COUNT; $severity++) {
				$severities[$severity] = CSeverityHelper::getName($severity);
			}

			// Prepare data for view.
			if ($page_options['media'] != -1) {
				$severity_request = $this->getInput('severity', 63);

				$page_options['severities'] = [];
				foreach ($severities as $severity => $foo) {
					if ($severity_request & (1 << $severity)) {
						$page_options['severities'][$severity] = $severity;
					}
				}
			}
			else {
				$page_options['severities'] = $this->getInput('severity', array_keys($severities));
			}

			$db_mediatypes = API::MediaType()->get([
				'output' => ['name', 'type', 'status'],
				'preservekeys' => true
			]);
			CArrayHelper::sort($db_mediatypes, ['name']);

			$mediatypes = [];
			foreach ($db_mediatypes as $mediatypeid => $db_mediatype) {
				$mediatypes[$mediatypeid] = $db_mediatype['type'];
			}

			$data = [
				'title' => _('Media'),
				'options' => $page_options,
				'db_mediatypes' => $db_mediatypes,
				'mediatypes' => $mediatypes,
				'severities' => $severities,
				'user' => [
					'debug_mode' => $this->getDebugMode()
				]
			];

			$this->setResponse(new CControllerResponseData($data));
		}
	}
}
