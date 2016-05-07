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
	TEST_CLASS(SVS)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSVS_DFSM)
		{
			fsm = make_unique<DFSM>();
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", 1, 0);
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestSVS_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			/*/
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", 1, 0);
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestSVS_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm", 2, 58, 95);//2 states that have not SVS
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGetVerifyingSet(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			/*/
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", 1, 0);
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestSVS_DFA)
		{
			fsm = make_unique<DFA>();
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", 1, 0);
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGetVerifyingSet(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testGetStateVerifyingS(state_t state, sequence_in_t& sVS, bool hasVS = true) {
			DEBUG_MSG("%d: %s\n", state, FSMmodel::getInSequenceAsString(sVS).c_str());
			if (sVS.empty()) {
				ARE_EQUAL(false, hasVS, "FSM has State Verifying Sequence of state %d but it was not found.", state);
				auto sVS = getStateVerifyingSequence(fsm, state, true);
				ARE_EQUAL(true, sVS.empty(), 
					"FSM has State Verifying Sequence %s of state %d but it was not found.", 
					FSMmodel::getInSequenceAsString(sVS).c_str(), state);
				return;
			}
			ARE_EQUAL(true, hasVS, "FSM has not State Verifying Sequence of state %d but it was found.", state);
			
			set<state_t> actBlock, nextBlock;
			output_t output;
			for (state_t i : fsm->getStates()) {
				nextBlock.insert(i);
			}
			for (sequence_in_t::iterator sIt = sVS.begin(); sIt != sVS.end(); sIt++) {
				ARE_EQUAL(true, ((*sIt < fsm->getNumberOfInputs()) || (*sIt == STOUT_INPUT)),
					"Input %d is invalid", *sIt);
				if (nextBlock.size() > 1) {
					actBlock = nextBlock;
					nextBlock.clear();
				}
				else {
					sequence_in_t endVS(sIt, sVS.end());
					ARE_EQUAL(true, false, "Suffix %s is extra", FSMmodel::getInSequenceAsString(endVS).c_str());
				}
				output = fsm->getOutput(state, *sIt);
				state_t nextState = fsm->getNextState(state, *sIt);
				for (set<state_t>::iterator stateIt = actBlock.begin(); stateIt != actBlock.end(); stateIt++) {
					if (output == fsm->getOutput(*stateIt, *sIt)) {
						ARE_EQUAL(nextState == fsm->getNextState(*stateIt, *sIt), state == *stateIt,
							"State %d translates into the same state %d on input %d as the reference state %d",
							*stateIt, nextState, *sIt, state);
						nextBlock.insert(fsm->getNextState(*stateIt, *sIt));
					}
				}
				state = nextState;
			}
			if (nextBlock.size() > 1) {
				set<state_t>::iterator stateIt = actBlock.begin();
				string undistStates = FSMlib::Utils::toString(*stateIt);
				for (++stateIt; stateIt != actBlock.end(); stateIt++) {
					undistStates += ", " + FSMlib::Utils::toString(*stateIt);
				}
				ARE_EQUAL(true, false, "States %s are not distinguished.", undistStates.c_str());
			}
		}

		void testGetVerifyingSet(string filename, int n = 0, ...) {
			fsm->load(filename);
			auto sVSet = getVerifyingSet(fsm, true);
			DEBUG_MSG("Verifying Set of %s:\n", filename.c_str());
			ARE_EQUAL(state_t(sVSet.size()), fsm->getNumberOfStates(),
				"FSM has %d states but only %d sequences were received.",
				fsm->getNumberOfStates(), sVSet.size());
			va_list vl;
			va_start(vl, n);
			state_t stateWithoutSVS = NULL_STATE;
			if (n > 0) {
				stateWithoutSVS = va_arg(vl, state_t);
				n--;
			}
			for (state_t state : fsm->getStates()) {
				if (state == stateWithoutSVS) {
					testGetStateVerifyingS(state, sVSet[state], false);
					if (n > 0) {
						stateWithoutSVS = va_arg(vl, state_t);
						n--;
					}
				}
				else {
					testGetStateVerifyingS(state, sVSet[state]);
				}
			}
			va_end(vl);
		}
	};
}
