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
	TEST_CLASS(SPYmethod)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSPYmethod_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSPYmethod_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSPYmethod_Moore)
		{
			Moore moore;
			fsm = &moore;
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSPYmethod_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
			testSPYmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testSPYmethod(string filename) {
			fsm->load(filename);
			sequence_set_t TS;
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				SPY_method(fsm, TS, extraStates);
				printTS(TS, filename);
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				vector<DFSM*> indistinguishable;
				FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The SPY-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			}
		}
	};
}
