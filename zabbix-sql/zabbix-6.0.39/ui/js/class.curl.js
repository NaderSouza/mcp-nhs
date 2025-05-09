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
 * WARNING: the class doesn't support parsing query strings with multi-dimentional arrays.
 *
 * @param url
 * @param add_sid	boolean	Add SID to given URL. Default: true.
 */
var Curl = function(url, add_sid) {
	url = url || location.href;
	if (typeof add_sid === 'undefined') {
		add_sid = true;
	}

	this.url = url;
	this.args = {};

	this.query = (this.url.indexOf('?') >= 0) ? this.url.substring(this.url.indexOf('?') + 1) : '';
	if (this.query.indexOf('#') >= 0) {
		this.query = this.query.substring(0, this.query.indexOf('#'));
	}

	var protocolSepIndex = this.url.indexOf('://');
	if (protocolSepIndex >= 0) {
		this.protocol = this.url.substring(0, protocolSepIndex).toLowerCase();
		this.host = this.url.substring(protocolSepIndex + 3);

		if (this.host.indexOf('/') >= 0) {
			this.host = this.host.substring(0, this.host.indexOf('/'));
		}

		var atIndex = this.host.indexOf('@');
		if (atIndex >= 0) {
			var credentials = this.host.substring(0, atIndex);
			var colonIndex = credentials.indexOf(':');

			if (colonIndex >= 0) {
				this.username = credentials.substring(0, colonIndex);
				this.password = credentials.substring(colonIndex);
			}
			else {
				this.username = credentials;
			}
			this.host = this.host.substring(atIndex + 1);
		}

		var host_ipv6 = this.host.indexOf(']');
		if (host_ipv6 >= 0) {
			if (host_ipv6 < (this.host.length - 1)) {
				host_ipv6++;
				var host_less = this.host.substring(host_ipv6);

				var portColonIndex = host_less.indexOf(':');
				if (portColonIndex >= 0) {
					this.port = host_less.substring(portColonIndex + 1);
					this.host = this.host.substring(0, host_ipv6);
				}
			}
		}
		else {
			var portColonIndex = this.host.indexOf(':');
			if (portColonIndex >= 0) {
				this.port = this.host.substring(portColonIndex + 1);
				this.host = this.host.substring(0, portColonIndex);
			}
		}
		this.file = this.url.substring(protocolSepIndex + 3);
		this.file = this.file.substring(this.file.indexOf('/'));

		if (this.file == this.host) {
			this.file = '';
		}
	}
	else {
		this.file = this.url;
	}

	if (this.file.indexOf('?') >= 0) {
		this.file = this.file.substring(0, this.file.indexOf('?'));
	}

	var refSepIndex = this.file.indexOf('#');
	if (refSepIndex >= 0) {
		this.reference = this.file.substring(refSepIndex + 1);
		this.file = this.file.substring(0, refSepIndex);
	}

	this.path = this.file;
	if (this.query.length > 0) {
		this.file += '?' + this.query;
	}
	if (this.query.length > 0) {
		this.formatArguments();
	}

	if (add_sid) {
		this.addSID();
	}
};

Curl.prototype = {

	url:		'', // actually, it's deprecated/private variable
	port:		-1,
	host:		'',
	protocol:	'',
	username:	'',
	password:	'',
	file:		'',
	reference:	'',
	path:		'',
	query:		'',
	args:		null,

	addSID: function() {
		var sid = '';
		var position = parseInt(location.href.indexOf('sid='));

		if (position > -1) {
			sid = location.href.substr(position + 4, 16);
		}
		else {
			sid = jQuery('meta[name="csrf-token"]').attr('content');
		}

		if ((/[0-9a-z]+/i).test(sid)) {
			this.setArgument('sid', sid);
		}
	},

	formatQuery: function() {
		this.query = jQuery.param(this.args);
	},

	formatArguments: function() {
		this.args = {};
		var args = this.query.split('&');

		if (args.length < 1) {
			return;
		}

		var keyval = '',
			array_values = {};

		for (var i = 0; i < args.length; i++) {
			keyval = args[i].split('=');

			if (keyval.length > 1) {
				try {
					var tmp = keyval[1].replace(/\+/g, '%20');
					keyval[0] = decodeURIComponent(keyval[0]);
					var matches = keyval[0].match(/(.*)\[\]$/);

					// Find all parameters with non-indexed arrays like "groupids[]" and store them for later use.
					if (matches) {
						if (!(matches[1] in array_values)) {
							array_values[matches[1]] = [];
						}

						array_values[matches[1]].push(decodeURIComponent(tmp));
					}
					else {
						this.args[keyval[0]] = decodeURIComponent(tmp);
					}
				}
				catch(exc) {
					this.args[keyval[0]] = keyval[1];
				}
			}
			else {
				this.args[keyval[0]] = '';
			}
		}

		// Set non-indexed array parameters with values.
		for (var key in array_values) {
			this.setArgument(key, array_values[key]);
		}
	},

	setArgument: function(key, value) {
		this.args[key] = value;
		this.formatQuery();
	},

	unsetArgument: function(key) {
		delete(this.args[key]);
		this.formatQuery();
	},

	getArgument: function(key) {
		if (typeof(this.args[key]) != 'undefined') {
			return this.args[key];
		}
		else {
			return null;
		}
	},

	getArguments: function() {
		return this.args;
	},

	/**
	 * Get query parameters as javascript object, support indexed names up to 3 levels only.
	 */
	getArgumentsObject: function() {
		let result = {},
			parts;

		for (const name in this.args) {
			parts = name.replace(/\]/g, '').split('[');

			switch (parts.length) {
				case 1:
					result[name] = this.args[name];
					break;

				case 2:
					// name[0]
					if (!(parts[0] in result)) {
						result[parts[0]] = [];
					}

					result[parts[0]][parts[1]] = this.args[name];
					break;

				case 3:
					// name[0][property]
					if (!(parts[0] in result)) {
						result[parts[0]] = [];
					}

					if (!(parts[1] in result[parts[0]])) {
						result[parts[0]][parts[1]] = {}
					}

					result[parts[0]][parts[1]][parts[2]] = this.args[name];
					break;
			}
		}

		return result;
	},

	getUrl: function() {
		this.formatQuery();

		var url = this.protocol.length > 0 ? this.protocol + '://' : '';
		url +=  this.username.length > 0 ? encodeURI(this.username) : '';
		url +=  this.password.length > 0 ? encodeURI(':' + this.password) : '';
		url +=  this.host.length > 0 ? this.host : '';
		url +=  this.port.length > 0 ? ':' + this.port : '';
		url +=  this.path.length > 0 ? encodeURI(this.path) : '';
		url +=  this.query.length > 0 ? '?' + this.query : '';
		url +=  this.reference.length > 0 ? encodeURI('#' + this.reference) : '';

		return url;
	},

	setPort: function(port) {
		this.port = port;
	},

	getPort: function() {
		return this.port;
	},

	// returns the protocol of this url, i.e. 'http' in the url 'http://server/'
	getProtocol: function() {
		return this.protocol;
	},

	setProtocol: function(protocol) {
		this.protocol = protocol;
	},

	// returns the host name of this url, i.e. 'server.com' in the url 'http://server.com/'
	getHost: function() {
		return this.host;
	},

	setHost: function(host) {
		this.host = host;
	},

	// returns the user name part of this url, i.e. 'joe' in the url 'http://joe@server.com/'
	getUserName: function() {
		return this.username;
	},

	setUserName: function(username) {
		this.username = username;
	},

	// returns the password part of this url, i.e. 'secret' in the url 'http://joe:secret@server.com/'
	getPassword: function() {
		return this.password;
	},

	setPassword: function(password) {
		this.password = password;
	},

	// returns the file part of this url, i.e. everything after the host name.
	getFile: function() {
		return this.file;
	},

	// returns the reference of this url, i.e. 'bookmark' in the url 'http://server/file.html#bookmark'
	getReference: function() {
		return this.reference;
	},

	setReference: function(reference) {
		this.reference = reference;
	},

	// returns the file path of this url, i.e. '/dir/file.html' in the url 'http://server/dir/file.html'
	getPath: function() {
		return this.path;
	},

	setPath: function(path) {
		this.path = path;
	}
};
