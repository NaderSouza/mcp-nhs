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
?>

<script type="text/x-jquery-tmpl" id="row-expression-tmpl">
	<?= (new CRow([
			(new CSelect('expressions[#{rowNum}][expression_type]'))
				->addClass('js-expression-type-select')
				->setId('expressions_#{rowNum}_expression_type')
				->addOptions(CSelect::createOptionsFromArray(CRegexHelper::expression_type2str())),
			(new CTextBox('expressions[#{rowNum}][expression]', '', false, 255))
				->setWidth(ZBX_TEXTAREA_MEDIUM_WIDTH)
				->setAriaRequired(),
			(new CSelect('expressions[#{rowNum}][exp_delimiter]'))
				->addOptions(CSelect::createOptionsFromArray(CRegexHelper::expressionDelimiters()))
				->setId('expressions_#{rowNum}_exp_delimiter')
				->addClass('js-expression-delimiter-select')
				->setDisabled(true)
				->addStyle('display: none;'),
			new CCheckBox('expressions[#{rowNum}][case_sensitive]'),
			(new CCol(
				(new CButton('expressions[#{rowNum}][remove]', _('Remove')))
					->addClass(ZBX_STYLE_BTN_LINK)
					->addClass('element-table-remove')
			))->addClass(ZBX_STYLE_NOWRAP)
		]))
			->addClass('form_row')
			->setAttribute('data-index', '#{rowNum}')
			->toString()
	?>
</script>

<script type="text/x-jquery-tmpl" id="test-table-row-tmpl">
	<?= (new CRow([
			'#{type}', '#{expression}', (new CSpan('#{result}'))->addClass('#{resultClass}')
		]))
			->addClass('test_row')
			->toString()
	?>
</script>

<script type="text/x-jquery-tmpl" id="test-combined-table-row-tmpl">
	<?= (new CRow([
			(new CCol(_('Combined result')))->setColspan(2), (new CSpan('#{result}'))->addClass('#{resultClass}')
		]))
			->addClass('test_row')
			->toString()
	?>
</script>

<script>
	(function($) {
		/**
		 * Object to manage expression related GUI elements.
		 * @type {Object}
		 */
		window.zabbixRegExp = {

			/**
			 * Template for expression row of testing results table.
			 * @type {String}
			 */
			testTableRowTpl: new Template($('#test-table-row-tmpl').html()),

			/**
			 * Template for combined result row in testing results table.
			 * @type {String}
			 */
			testCombinedTableRowTpl: new Template($('#test-combined-table-row-tmpl').html()),

			/**
			 * Send all expressions data to server with test string.
			 *
			 * @param {string} string Test string to test expression against
			 *
			 * @return {jqXHR}
			 */
			testExpressions: function(string) {
				var ajaxData = {
					testString: string,
					expressions: {}
				};

				$('#testResultTable').css({opacity: 0.5});

				$('#tbl_expr .form_row').each(function() {
					var index = $(this).data('index');

					ajaxData.expressions[index] = {
						expression : $('#expressions_' + index + '_expression').val(),
						expression_type : $('#expressions_' + index + '_expression_type').val(),
						exp_delimiter : $('#expressions_' + index + '_exp_delimiter').val(),
						case_sensitive : $('#expressions_' + index + '_case_sensitive').is(':checked') ? '1' : '0'
					}
				});

				var url = new Curl('zabbix.php');
				url.setArgument('action', 'regex.test');

				return $.post(
					url.getUrl(),
					{ajaxdata: ajaxData},
					$.proxy(this.showTestResults, this),
					'json'
				);
			},

			/**
			 * Update test results table with data received form server.
			 *
			 * @param {Object} response ajax response
			 */
			showTestResults: function(response) {
				var tplData,
					hasErrors,
					obj = this,
					$expressions = $('#tbl_expr .form_row'),
					expression_type_str;

				$('#testResultTable .test_row').remove();
				hasErrors = ($expressions.length == 0);

				$expressions.each(function() {
					var index = $(this).data('index'),
						expr_result = response.data.expressions[index],
						result;

					if (response.data.errors[index]) {
						hasErrors = true;
						result = response.data.errors[index];
					}
					else {
						result = expr_result ? <?= json_encode(_('TRUE')) ?> : <?= json_encode(_('FALSE')) ?>;
					}

					switch ($('#expressions_' + index + '_expression_type').val()) {
						case '<?= EXPRESSION_TYPE_INCLUDED ?>':
							expression_type_str = <?= json_encode(_('Character string included')) ?>;
							break;

						case '<?= EXPRESSION_TYPE_ANY_INCLUDED ?>':
							expression_type_str = <?= json_encode(_('Any character string included')) ?>;
							break;

						case '<?= EXPRESSION_TYPE_NOT_INCLUDED ?>':
							expression_type_str = <?= json_encode(_('Character string not included')) ?>;
							break;

						case '<?= EXPRESSION_TYPE_TRUE ?>':
							expression_type_str = <?= json_encode(_('Result is TRUE')) ?>;
							break;

						case '<?= EXPRESSION_TYPE_FALSE ?>':
							expression_type_str = <?= json_encode(_('Result is FALSE')) ?>;
							break;

						default:
							expression_type_str = '';
					}

					$('#testResultTable').append(obj.testTableRowTpl.evaluate({
						expression: $('#expressions_' + index + '_expression').val(),
						type: expression_type_str,
						result: result,
						resultClass: expr_result ? '<?= ZBX_STYLE_GREEN ?>' : '<?= ZBX_STYLE_RED ?>'
					}));
				});

				if (hasErrors) {
					tplData = {
						resultClass: '<?= ZBX_STYLE_RED ?>',
						result: <?= json_encode(_('UNKNOWN')) ?>
					};
				}
				else {
					tplData = {
						resultClass: response.data.final ? '<?= ZBX_STYLE_GREEN ?>' : '<?= ZBX_STYLE_RED ?>',
						result: response.data.final ? <?= json_encode(_('TRUE')) ?> : <?= json_encode(_('FALSE')) ?>
					};
				}

				$('#testResultTable').append(this.testCombinedTableRowTpl.evaluate(tplData));
				$('#testResultTable').css({opacity: 1});
			}
		};
	}(jQuery));

	jQuery(function($) {
		var $form = $('form#regex'),
			$test_string = $('#test_string');
			$test_btn = $('#testExpression');

		$form.on('submit', function() {
			$form.trimValues(['#name']);
		});

		$('#testExpression, #tab_test').click(function() {
			$test_btn.addClass('is-loading');
			$test_btn.prop('disabled', true);
			$test_string.prop('disabled', true);

			zabbixRegExp
				.testExpressions($test_string.val())
				.always(function() {
					$test_btn.removeClass('is-loading');
					$test_btn.prop('disabled', false);
					$test_string.prop('disabled', false);
				});
		});

		$('#tbl_expr')
			.dynamicRows({
				template: '#row-expression-tmpl'
			})
			.on('change', '.js-expression-type-select', (e) => {
				$(e.target)
					.closest('[data-index]')
					.find('.js-expression-delimiter-select')
					.prop('disabled', e.target.value !== '<?= EXPRESSION_TYPE_ANY_INCLUDED ?>')
					.toggle(e.target.value === '<?= EXPRESSION_TYPE_ANY_INCLUDED ?>');
			});

		$form.find('#clone').click(function() {
			var url = new Curl('zabbix.php?action=regex.edit');

			$form.serializeArray().forEach(function(field) {
				url.setArgument(field.name, field.value);
			});

			redirect(url.getUrl(), 'post', 'action', undefined, false, true);
		});
	});
</script>
