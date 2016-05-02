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
	TEST_CLASS(Wpmethod)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestWpmethod_DFSM)
		{
			fsm = make_unique<DFSM>();
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
		}

		TEST_METHOD(TestWpmethod_Mealy)
		{
			fsm = make_unique<Mealy>();
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
		}

		TEST_METHOD(TestWpmethod_Moore)
		{
			fsm = make_unique<Moore>();
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
		}

		TEST_METHOD(TestWpmethod_DFA)
		{
			fsm = make_unique<DFA>();
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
			testWpmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testWpmethod(string filename) {
			fsm->load(filename);
			sequence_set_t TS;
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				Wp_method(fsm, TS, extraStates);
				printTS(TS, filename);
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The Wp-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			}
		}
	};
}
