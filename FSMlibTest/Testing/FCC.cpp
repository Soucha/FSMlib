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
#include "../TestUtils.h"

using namespace FSMtesting;

namespace FSMlibTest
{
	TEST_CLASS(FCCTests)
	{
	public:

		TEST_METHOD(FCCTest)
		{
			testFCC();
		}

		void printTS(sequence_set_t & TS) {
			DEBUG_MSG("Set of %d test sequences:\n", TS.size());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testFCC() {
			Mealy mealy;
			DFSM * fsm = &mealy;
			fsm->create(2, 2, 2);
			fsm->setTransition(0, 0, 0, 0);
			fsm->setTransition(0, 1, 1, 0);
			fsm->setTransition(1, 0, 0, 1);
			fsm->setTransition(1, 1, 1, 0);
			sequence_set_t TS;
			sequence_in_t test1{ 0, 0 };
			sequence_in_t test2{ 1, 0, 0 };
			sequence_in_t test3{ 1, 1, 0 };
			vector<DFSM*> indistinguishable;
			int extraStates = 0;
			//W_method(fsm, TS, extraStates);
			
			FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
			ARE_EQUAL(0, int(indistinguishable.size()), "A machine found in an empty TS.");

			TS.clear();
			TS.insert(test1);
			TS.insert(test2);
			TS.insert(test3);
			printTS(TS);
			indistinguishable.clear();
			FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
			ARE_EQUAL(1, int(indistinguishable.size()), "TS has not complete fault coverage.");

			TS.clear();
			//TS.insert(test1);
			TS.insert(test2);
			TS.insert(test3);
			indistinguishable.clear();
			FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
			ARE_EQUAL(4, int(indistinguishable.size()), "There are 4 indistiguishable machines.");

			TS.clear();
			TS.insert(test1);
			TS.insert(test2);
			test3.pop_back();// the last transition is not confirmed
			TS.insert(test3);
			indistinguishable.clear();
			FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
			ARE_EQUAL(2, int(indistinguishable.size()), "There are only two indistiguishable machines.");

		}

	};
}
