/* Copyright (c) Michal Soucha, 2016
*
* This file is part of FSMlib
*
* FSMlib is free software: you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation, either version 3 of the License, or (at your option) any later
* version.
*
* FSMlib is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with
* FSMlib. If not, see <http://www.gnu.org/licenses/>.
*/
#include "stdafx.h"

#include <string>
#include "UnionFind.h"

namespace FSMlib {
	UnionFind::UnionFind(int n) {
		v.resize(n);
		for (int i = 0; i < n; i++) {
			v[i].first = i; //parent
			v[i].second = 0; //rank
		}
	}

	int UnionFind::doFind(int x) {
		if (v[x].first != x) {
			v[x].first = doFind(v[x].first);
		}
		return v[x].first;
	}

	void UnionFind::doUnion(int x, int y) {
		x = doFind(x);
		y = doFind(y);
		if (x == y) return;

		// x and y are not already in same set. Merge them.
		if (v[x].second < v[y].second)
			v[x].first = y;
		else if (v[x].second > v[y].second)
			v[y].first = x;
		else {
			v[y].first = x;
			v[x].second++;
		}
	}
}
