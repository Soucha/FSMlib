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
	TEST_CLASS(Mstarmethod)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestMstarmethod_DFSM)
		{
			fsm = make_unique<DFSM>();
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMstarmethod_Mealy)
		{
			fsm = make_unique<Mealy>();
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMstarmethod_Moore)
		{
			fsm = make_unique<Moore>();
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMstarmethod_DFA)
		{
			fsm = make_unique<DFA>();
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testMstarmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", false);
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testMstarmethod(string filename, bool hasDS = true) {
			fsm->load(filename);
			sequence_set_t TS;
			sequence_in_t CS;
			int extraStates = 0;
			if (Mstar_method(fsm, CS, extraStates)) {
				TS.insert(CS);
				printTS(TS, filename);
				ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but a TS was obtained.");
				ARE_EQUAL(false, CS.empty(), "Obtained TS is empty.");
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The Mstar-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			}
			else {
				ARE_EQUAL(false, hasDS, "FSM has adaptive DS so a TS can be created but it was not obtained.");
				if (!TS.empty()) printTS(TS, filename);
				else {
					DEBUG_MSG("Ma-method on %s: no ADS, no TS\n", filename.c_str());
				}
				ARE_EQUAL(true, TS.empty(), "FSM has not adaptive DS but obtained TS has %d sequences.",
					TS.size());
			}
			TS.clear();
			if (Mrstar_method(fsm, TS, extraStates)) {
				printTS(TS, filename);
				ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but a TS was obtained.");
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The Mrstar-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
			}
			else {
				ARE_EQUAL(false, hasDS, "FSM has adaptive DS so a TS can be created but it was not obtained.");
				if (!TS.empty()) printTS(TS, filename);
				else {
					DEBUG_MSG("Mra-method on %s: no ADS, no TS\n", filename.c_str());
				}
				ARE_EQUAL(true, TS.empty(), "FSM has not adaptive DS but obtained TS has %d sequences.",
					TS.size());
			}
		}
	};
}
