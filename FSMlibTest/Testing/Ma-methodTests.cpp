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
	TEST_CLASS(Mamethod)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestMamethod_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMamethod_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMamethod_Moore)
		{
			Moore moore;
			fsm = &moore;
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", false);
		}

		TEST_METHOD(TestMamethod_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testMamethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", false);
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void testMamethod(string filename, bool hasDS = true) {
			fsm->load(filename);
			sequence_set_t TS;
			sequence_in_t CS;
			int extraStates = 0;
			if (Ma_method(fsm, CS, extraStates)) {
				TS.insert(CS);
				printTS(TS, filename);
				ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but a TS was obtained.");
				ARE_EQUAL(false, CS.empty(), "Obtained TS is empty.");
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
			if (Mra_method(fsm, TS, extraStates)) {
				printTS(TS, filename);
				ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but a TS was obtained.");
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
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
