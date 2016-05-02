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
	TEST_CLASS(CoverTests)
	{
	public:
		unique_ptr<DFSM> fsm;
		
		// TODO: more tests, incomplete machines

		TEST_METHOD(TestCoversDFSM)
		{
			fsm = make_unique<DFSM>();
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "DFSM_U7.fsm", 2);
			testGetTransitionCover(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");

			testGetTraversalSet(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
		}

		TEST_METHOD(TestCoversMealy)
		{
			fsm = make_unique<Mealy>();
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "Mealy_U7.fsm", 2);
			testGetTransitionCover(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");

			testGetTraversalSet(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
		}

		TEST_METHOD(TestCoversMoore)
		{
			fsm = make_unique<Moore>();
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "Moore_U7.fsm", 2);
			testGetTransitionCover(DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");

			testGetTraversalSet(DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");
		}

		TEST_METHOD(TestCoversDFA)
		{
			fsm = make_unique<DFA>();
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
			testGetStateCover(DATA_PATH + MINIMIZATION_DIR + "DFA_U7.fsm", 2);
			testGetTransitionCover(DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");

			testGetTraversalSet(DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
		}

		void testGetStateCover(string filename, int unreachableStates = 0) {
			fsm->load(filename);
			set<state_t> coveredStates;
			auto stateCover = getStateCover(fsm);
			coveredStates.clear();
			DEBUG_MSG("State cover of %s\n", filename.c_str());
			for (sequence_in_t seq : stateCover) {
				auto state = fsm->getEndPathState(0, seq);
				ARE_EQUAL(true, state != WRONG_STATE, "Sequence %s is not valid", FSMmodel::getInSequenceAsString(seq).c_str());
				ARE_EQUAL(true, (coveredStates.insert(state)).second, "State %d is again covered with path %s", state,
						FSMmodel::getInSequenceAsString(seq).c_str());
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(seq).c_str());
			}
			//DEBUG_MSG("\n");
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(coveredStates.size() + unreachableStates),
				"%d states are not covered", (fsm->getNumberOfStates() - coveredStates.size() - unreachableStates));
		}

		void testGetTransitionCover(string filename) {
			fsm->load(filename);
			auto transitionCover = getTransitionCover(fsm);
			set< pair<state_t, input_t> > covered;
			bool isEmptySeq = false;
			state_t state;
			input_t input;
			DEBUG_MSG("Transition cover of %s\n", filename.c_str());
			for (sequence_in_t seq : transitionCover) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(seq).c_str());
				if (seq.empty()) {
					ARE_EQUAL(false, isEmptySeq, "There is extra empty sequence.");
					isEmptySeq = true;
				}
				else {
					if (fsm->isOutputState()) seq.pop_back();
					input = seq.back();
					seq.pop_back();
					state = fsm->getEndPathState(0, seq);
					ARE_EQUAL(true, state != WRONG_STATE, "Sequence %s is not valid", FSMmodel::getInSequenceAsString(seq).c_str());
					ARE_EQUAL(true, fsm->getNextState(state, input) != WRONG_STATE, "Transition (%d, %d) aplied after %s is invalid",
						state, input, FSMmodel::getInSequenceAsString(seq).c_str());
					ARE_EQUAL(true, fsm->getNextState(state, input) != NULL_STATE, "There is no transition (%d, %d) aplied after %s",
						state, input, FSMmodel::getInSequenceAsString(seq).c_str());
					ARE_EQUAL(true, (covered.insert(make_pair(state, input))).second, "Transition (%d, %d) is again covered in path %s,%d",
						state, input, FSMmodel::getInSequenceAsString(seq).c_str(), input);
				}
			}
			//DEBUG_MSG("\n");
			for (auto state : fsm->getStates()) {
				for (input = 0; input < fsm->getNumberOfInputs(); input++) {
					if (fsm->getNextState(state, input) != NULL_STATE) {
						ARE_EQUAL(true, covered.find(make_pair(state, input)) != covered.end(), "Transition (%d, %d) is not covered",
							state, input);
					}
				}
			}
		}

		void testGetTraversalSet(string filename) {
			fsm->load(filename);
			
			auto traversalSet = getTraversalSet(fsm, 0);
			ARE_EQUAL(true, traversalSet.empty(), "Traversal set is not empty by depth of zero.");
			
			traversalSet = getTraversalSet(fsm, 1);
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				sequence_in_t seq;
				seq.push_back(input);
				if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
				ARE_EQUAL(1, int(traversalSet.count(seq)), "Traversal set (1) contains %d-times sequence %s",
					traversalSet.count(seq), FSMmodel::getInSequenceAsString(seq).c_str());
				traversalSet.erase(seq);
			}
			if (!traversalSet.empty()) {
				for (sequence_in_t seq : traversalSet) {
					DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(seq).c_str());
				}
				ARE_EQUAL(true, traversalSet.empty(), "Traversal set (1) contains %d extra sequences", traversalSet.size());
			}

			traversalSet = getTraversalSet(fsm, 2);
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				sequence_in_t seq;
				seq.push_back(input);
				if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
				ARE_EQUAL(1, int(traversalSet.count(seq)), "Traversal set (2) contains %d-times sequence %s",
					traversalSet.count(seq), FSMmodel::getInSequenceAsString(seq).c_str());
				traversalSet.erase(seq);
				for (input_t input2 = 0; input2 < fsm->getNumberOfInputs(); input2++) {
					seq.push_back(input2);
					if (fsm->isOutputState()) seq.push_back(STOUT_INPUT);
					ARE_EQUAL(1, int(traversalSet.count(seq)), "Traversal set (2) contains %d-times sequence %s",
						traversalSet.count(seq), FSMmodel::getInSequenceAsString(seq).c_str());
					traversalSet.erase(seq);
					seq.pop_back();
					if (fsm->isOutputState()) seq.pop_back();
				}
			}
			if (!traversalSet.empty()) {
				for (sequence_in_t seq : traversalSet) {
					DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(seq).c_str());
				}
				ARE_EQUAL(true, traversalSet.empty(), "Traversal set (2) contains %d extra sequences", traversalSet.size());
			}
			
			DEBUG_MSG("Traversal set (3) of %s\n", filename.c_str());
			traversalSet = getTraversalSet(fsm, 3);
			for (sequence_in_t seq : traversalSet) {
				DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(seq).c_str());
			}
		}
	};
}