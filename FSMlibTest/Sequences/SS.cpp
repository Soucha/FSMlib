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

using namespace FSMsequence;

namespace FSMlibTest
{
	TEST_CLASS(SS)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSS_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestSS_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			//*
			//testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");// too hard
			//testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");// too hard
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm", false);
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm", false);
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			//*/
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm", false);
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestSS_Moore)
		{
			Moore moore;
			fsm = &moore;
			//*
			//testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");// too hard
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm", false); 
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm", false);
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm", false);
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGetSynchronizingS(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			//*/
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm", false);
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestSS_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm", false);
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGetSynchronizingS(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");

		}

		void testGetSynchronizingS(string filename, bool hasSS = true) {
			fsm->load(filename);
			auto sS = getSynchronizingSequence(fsm);
			if (!sS.empty()) {
				DEBUG_MSG("SS of %s: %s\n", filename.c_str(), FSMmodel::getInSequenceAsString(sS).c_str());
				ARE_EQUAL(true, hasSS, "FSM has not SS but it was found.");

				set<state_t> actBlock, nextBlock;
				for (state_t state : fsm->getStates()) {
					nextBlock.insert(state);
				}
				for (sequence_in_t::iterator sIt = sS.begin(); sIt != sS.end(); sIt++) {
					ARE_EQUAL(true, ((*sIt < fsm->getNumberOfInputs()) || (*sIt == STOUT_INPUT)),
						"Input %d is invalid", *sIt);
					if (nextBlock.size() > 1) {
						actBlock = nextBlock;
						nextBlock.clear();
					}
					else {
						sequence_in_t endSS(sIt, sS.end());
						ARE_EQUAL(true, false, "Suffix %s is extra", FSMmodel::getInSequenceAsString(endSS).c_str());
					}
					for (set<state_t>::iterator stateIt = actBlock.begin(); stateIt != actBlock.end(); stateIt++) {
						state_t nextState = fsm->getNextState(*stateIt, *sIt);
						ARE_EQUAL(true, nextState != NULL_STATE, "There is no transition from %d on %d", *stateIt, *sIt);
						ARE_EQUAL(true, nextState != WRONG_STATE, "Transition from %d on %d is invalid", *stateIt, *sIt);
						nextBlock.insert(nextState);
					}
				}
				if (nextBlock.size() > 1) {
					set<state_t>::iterator stateIt = actBlock.begin();
					string notSynchStates = FSMlib::Utils::toString(*stateIt);
					for (++stateIt; stateIt != actBlock.end(); stateIt++) {
						notSynchStates += ", " + FSMlib::Utils::toString(*stateIt);
					}
					ARE_EQUAL(true, false, "States %s are not synchronized.", notSynchStates.c_str());
				}
			}
			else {
				ARE_EQUAL(false, hasSS, "FSM has SS but it was not found.");
				ARE_EQUAL(true, sS.empty(), "FSM has not SS but sequence %s was returned.",
					FSMmodel::getInSequenceAsString(sS).c_str());
				DEBUG_MSG("SS of %s: NO\n", filename.c_str());
			}
		}

	};
}
