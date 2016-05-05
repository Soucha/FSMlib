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
	TEST_CLASS(ADSmethod)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestADSmethod_DFSM)
		{
			fsm = make_unique<DFSM>();
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestADSmethod_Mealy)
		{
			fsm = make_unique<Mealy>();
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestADSmethod_Moore)
		{
			fsm = make_unique<Moore>();
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestADSmethod_DFA)
		{
			fsm = make_unique<DFA>();
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testADSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", false);
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testADSmethod(string filename, bool hasDS = true) {
			fsm->load(filename);
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				auto TS = ADS_method(fsm, extraStates);
				if (!TS.empty()) {
					printTS(TS, filename);
					ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but a TS was obtained.");
					ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
					auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
					ARE_EQUAL(1, int(indistinguishable.size()), "The ADS-method (%d extra states) has not complete fault coverage,"
						" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
					ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable.front()), "FCC found a machine different from the specification.");
				}
				else {
					ARE_EQUAL(false, hasDS, "FSM has adaptive DS so a TS can be created but it was not obtained.");
					if (!TS.empty()) printTS(TS, filename);
					else {
						DEBUG_MSG("ADS-method on %s: no ADS, no TS\n", filename.c_str());
					}
					ARE_EQUAL(true, TS.empty(), "FSM has not adaptive DS but obtained TS has %d sequences.",
						TS.size());
				}
			}
		}
	};
}
