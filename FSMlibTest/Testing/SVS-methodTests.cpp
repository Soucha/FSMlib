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
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSVSmethod_DFSM)
		{
			fsm = make_unique<DFSM>();
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_Mealy)
		{
			fsm = make_unique<Mealy>();
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_Moore)
		{
			fsm = make_unique<Moore>();
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm"); 
			testSVSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", 1);
		}

		TEST_METHOD(TestSVSmethod_DFA)
		{
			fsm = make_unique<DFA>();
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
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				auto TS = SVS_method(fsm, extraStates);
				printTS(TS, filename);
				DEBUG_MSG("The number of states without SVS: %d\n", statesWithoutSVS);
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The SVS-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
				ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable.front()), "FCC found a machine different from the specification.");
			}
		}
	};
}
