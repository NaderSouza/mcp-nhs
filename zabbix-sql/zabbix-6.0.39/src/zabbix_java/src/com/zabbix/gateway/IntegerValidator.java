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

package com.zabbix.gateway;

class IntegerValidator implements InputValidator
{
	private int lo;
	private int hi;

	IntegerValidator(int lo, int hi)
	{
		if (lo > hi)
			throw new IllegalArgumentException("bad validation bounds: " + lo + " and " + hi);

		this.lo = lo;
		this.hi = hi;
	}

	@SuppressWarnings("removal")
	protected final void finalize() throws Throwable
	{
	}

	@Override
	public boolean validate(Object value)
	{
		if (value instanceof Integer)
		{
			Integer integer = (Integer)value;

			if (!(Integer.valueOf(lo).compareTo(integer) <= 0))
				return false;

			if (!(integer.compareTo(Integer.valueOf(hi)) <= 0))
				return false;

			return true;
		}
		else
			return false;
	}
}
