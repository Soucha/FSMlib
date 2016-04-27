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
#include "TestUtils.h"

using namespace FSMlib;

namespace FSMlibTest
{
	TEST_CLASS(UnionFindTests)
	{
	public:

		TEST_METHOD(UnionFindTest)
		{
			testUnionFind();
		}

		void testUnionFind() {
			UnionFind uf(5);
			vector<int> labels(5);
			for (int i = 0; i < 5; i++)
			{
				ARE_EQUAL(i, uf.doFind(i), "Initial label is different from the element id.");
				labels[i] = i;
			}
			uf.doUnion(1, 3);
			int label = uf.doFind(1);
			DEBUG_MSG("Label of 1 and 3: %d\n", label);
			labels[1] = labels[3] = label;
			for (int i = 0; i < 5; i++) {
				ARE_EQUAL(labels[i], uf.doFind(i), "Labels do not correspond.");
			}
			uf.doUnion(0, 4);
			label = uf.doFind(0);
			DEBUG_MSG("Label of 0 and 4: %d\n", label);
			labels[0] = labels[4] = label;
			for (int i = 0; i < 5; i++) {
				ARE_EQUAL(labels[i], uf.doFind(i), "Labels do not correspond.");
			}
			uf.doUnion(2, 0);
			label = uf.doFind(2);
			DEBUG_MSG("Label of 2 and 0: %d\n", label);
			labels[2] = labels[0] = label;
			ARE_EQUAL(label, uf.doFind(4), "Labels do not correspond.");
			for (int i = 0; i < 5; i++) {
				ARE_EQUAL(labels[i], uf.doFind(i), "Labels do not correspond.");
			}
			uf.doUnion(4, 2);// nothing happens
			label = uf.doFind(2);
			DEBUG_MSG("Label of 2 and 4: %d\n", label);
			labels[2] = labels[4] = label;
			ARE_EQUAL(label, uf.doFind(0), "Labels do not correspond.");
			for (int i = 0; i < 5; i++) {
				ARE_EQUAL(labels[i], uf.doFind(i), "Labels do not correspond.");
			}
			uf.doUnion(1, 4);
			label = uf.doFind(4);
			DEBUG_MSG("Label of all 5 elements: %d\n", label);
			ARE_EQUAL(label, uf.doFind(4), "Labels do not correspond.");
			for (int i = 0; i < 5; i++) {
				ARE_EQUAL(label, uf.doFind(i), "Labels do not correspond.");
			}

		}
	};
}
