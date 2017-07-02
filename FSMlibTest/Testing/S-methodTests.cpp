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
	TEST_CLASS(Smethod)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSmethod_DFSM)
		{
			fsm = make_unique<DFSM>();
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSmethod_Mealy)
		{
			fsm = make_unique<Mealy>();
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSmethod_Moore)
		{
			fsm = make_unique<Moore>();
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
		}

		TEST_METHOD(TestSmethod_DFA)
		{
			fsm = make_unique<DFA>();
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
			testSmethod(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
		}

		void printTS(sequence_set_t & TS, string filename) {
			DEBUG_MSG("Set of %d test sequences (%s):\n", TS.size(), filename.c_str());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		void setAssumedState(const shared_ptr<OTreeNode>& node) {
			node->assumedState = node->state;
			for (auto& n : node->next) {
				if (n) setAssumedState(n);
			}
		}

		void testSmethod(string filename) {
			fsm->load(filename);
			for (int extraStates = 0; extraStates < 3; extraStates++) {
				auto TS = S_method(fsm, extraStates);
				printTS(TS, filename);
				ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
				auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, extraStates);
				ARE_EQUAL(1, int(indistinguishable.size()), "The S-method (%d extra states) has not complete fault coverage,"
					" it produces %d indistinguishable FSMs.", extraStates, indistinguishable.size());
				ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable.front()), "FCC found a machine different from the specification.");
			}

			OTree ot;
			ot.es = 0;
			StateCharacterization sc;
			auto TS = S_method_ext(fsm, ot, sc);
			printTS(TS, filename);
			ARE_EQUAL(false, TS.empty(), "Obtained TS is empty.");
			auto indistinguishable = FaultCoverageChecker::getFSMs(fsm, TS, ot.es);
			ARE_EQUAL(1, int(indistinguishable.size()), "The S-method (%d extra states) has not complete fault coverage,"
				" it produces %d indistinguishable FSMs.", ot.es, indistinguishable.size());
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable.front()), "FCC found a machine different from the specification.");

			setAssumedState(ot.rn[0]->convergent.front());

			ot.es = 1;
			auto TS1 = S_method_ext(fsm, ot, sc);
			printTS(TS1, filename);
			TS1.insert(TS.begin(), TS.end());
			printTS(TS1, filename);
			ARE_EQUAL(false, TS1.empty(), "Obtained TS is empty.");
			auto indistinguishable1 = FaultCoverageChecker::getFSMs(fsm, TS1, ot.es);
			ARE_EQUAL(1, int(indistinguishable1.size()), "The S-method (%d extra states) has not complete fault coverage,"
				" it produces %d indistinguishable FSMs.", ot.es, indistinguishable1.size());
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, indistinguishable1.front()), "FCC found a machine different from the specification.");

		}
	};
}