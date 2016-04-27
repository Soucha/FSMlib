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
	TEST_CLASS(SVSmethod)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSVSmethod_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_Moore)
		{
			Moore moore;
			fsm = &moore;
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm"); 
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", 1);
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testSVSmethod(string filename, state_t statesWithoutSVS = 0) {
			fsm->load(filename);
			sequence_set_t TS;
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				ARE_EQUAL(statesWithoutSVS, SVS_method(fsm, TS, extraStates), "The number of states without SVS differ.");
				printTS(TS, filename);
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				vector<DFSM*> indistinguishable;
				FaultCoverageChecker::getFSMs(fsm, TS, indistinguishable, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The SVS-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			}
		}
	};
}
