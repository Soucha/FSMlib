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

namespace FSMlibTest
{
	TEST_CLASS(BasicOpsTests)
	{
	public:
		DFSM * fsm;

		TEST_METHOD(TestGetNextStateDFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			create();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestGetOutputDFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			create();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, STOUT_INPUT, 1, STOUT_INPUT, 0, STOUT_INPUT, 
				2, STOUT_INPUT, 2, STOUT_INPUT, 1, STOUT_INPUT, 0, STOUT_INPUT };
			sequence_out_t expectedOut = { 0, 1, 0, 0, 1, 1, DEFAULT_OUTPUT, 1, 0, DEFAULT_OUTPUT, 0, 0, 1, 0, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestGetNextStateMealy)
		{
			Mealy mealy;
			fsm = &mealy;
			create();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestGetOutputMealy)
		{
			Mealy mealy;
			fsm = &mealy;
			create();
			tGetOutput();
			sequence_in_t seq = { 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 1, 0, 1, 1, DEFAULT_OUTPUT, 0, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestGetNextStateMoore)
		{
			Moore moore;
			fsm = &moore;
			create();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestGetOutputMoore)
		{
			Moore moore;
			fsm = &moore;
			create();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 0, 0, 1, DEFAULT_OUTPUT, 0, 0, 1, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestGetNextStateDFA)
		{
			DFA dfa;
			fsm = &dfa;
			create();
			tGetNextState();
			tGetEndPathState();
		}
		
		TEST_METHOD(TestGetOutputDFA)
		{
			DFA dfa;
			fsm = &dfa;
			create();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 0, 0, 1, DEFAULT_OUTPUT, 0, 0, 1, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		void tGetNextState() {
#define ERR_MSG_NEXT_STATE_WRONG "Next state from %d on %d is wrong"
			// given transitions:
			ARE_EQUAL(state_t(0), fsm->getNextState(0, 0), ERR_MSG_NEXT_STATE_WRONG, 0, 0);
			ARE_EQUAL(state_t(1), fsm->getNextState(0, 1), ERR_MSG_NEXT_STATE_WRONG, 0, 1);
			ARE_EQUAL(state_t(2), fsm->getNextState(0, 2), ERR_MSG_NEXT_STATE_WRONG, 0, 2);
			ARE_EQUAL(state_t(3), fsm->getNextState(1, 0), ERR_MSG_NEXT_STATE_WRONG, 1, 0);
			ARE_EQUAL(state_t(4), fsm->getNextState(2, 1), ERR_MSG_NEXT_STATE_WRONG, 2, 1);
			ARE_EQUAL(state_t(0), fsm->getNextState(3, 2), ERR_MSG_NEXT_STATE_WRONG, 3, 2);
			ARE_EQUAL(state_t(2), fsm->getNextState(4, 0), ERR_MSG_NEXT_STATE_WRONG, 4, 0);

			// self loops
			ARE_EQUAL(state_t(0), fsm->getNextState(0, STOUT_INPUT), ERR_MSG_NEXT_STATE_WRONG, 0, STOUT_INPUT);
			ARE_EQUAL(state_t(1), fsm->getNextState(1, STOUT_INPUT), ERR_MSG_NEXT_STATE_WRONG, 1, STOUT_INPUT);
			ARE_EQUAL(state_t(2), fsm->getNextState(2, STOUT_INPUT), ERR_MSG_NEXT_STATE_WRONG, 2, STOUT_INPUT);
			ARE_EQUAL(state_t(3), fsm->getNextState(3, STOUT_INPUT), ERR_MSG_NEXT_STATE_WRONG, 3, STOUT_INPUT);
			ARE_EQUAL(state_t(4), fsm->getNextState(4, STOUT_INPUT), ERR_MSG_NEXT_STATE_WRONG, 4, STOUT_INPUT);

#define ERR_MSG_NEXT_STATE_NOT_EXISTS "There is no next state from %d on %d"
			// the others should not exist
			ARE_EQUAL(NULL_STATE, fsm->getNextState(1, 1), ERR_MSG_NEXT_STATE_NOT_EXISTS, 1, 1);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(1, 2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 1, 2);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(2, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, 2, 0);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(2, 2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 2, 2);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(3, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, 3, 0);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(3, 1), ERR_MSG_NEXT_STATE_NOT_EXISTS, 3, 1);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(4, 1), ERR_MSG_NEXT_STATE_NOT_EXISTS, 4, 1);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(4, 2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 4, 2);

			// wrong state -> ERR messages:
			/// <type>::getNextState - bad state id
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(NULL_STATE, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, NULL_STATE, 0);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(-2, 1), ERR_MSG_NEXT_STATE_NOT_EXISTS, -2, 1);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(5, 2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 5, 2);
			// wrong input -> ERR messages:
			/// <type>::getNextState - bad input
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(1, -2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 1, -2);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(2, 3), ERR_MSG_NEXT_STATE_NOT_EXISTS, 2, 3);
			// wrong state and input -> ERR messages:
			/// <type>::getNextState - bad state id
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(NULL_STATE, STOUT_INPUT), ERR_MSG_NEXT_STATE_NOT_EXISTS, NULL_STATE, STOUT_INPUT);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(-2, STOUT_INPUT), ERR_MSG_NEXT_STATE_NOT_EXISTS, -2, STOUT_INPUT);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(5, STOUT_INPUT), ERR_MSG_NEXT_STATE_NOT_EXISTS, 5, STOUT_INPUT);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(NULL_STATE, 3), ERR_MSG_NEXT_STATE_NOT_EXISTS, NULL_STATE, 3);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(-2, 3), ERR_MSG_NEXT_STATE_NOT_EXISTS, -2, 3);
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(5, 3), ERR_MSG_NEXT_STATE_NOT_EXISTS, 5, 3);
		}

		void tGetOutput() {
			if (fsm->isOutputState()) {
#define ERR_MSG_OUTPUT_STATE "Wrong output of state %d"
				ARE_EQUAL(output_t(0), fsm->getOutput(0, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 0);
				ARE_EQUAL(output_t(1), fsm->getOutput(1, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 1);
				ARE_EQUAL(output_t(0), fsm->getOutput(2, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 2);
				ARE_EQUAL(DEFAULT_OUTPUT, fsm->getOutput(3, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 3);
				ARE_EQUAL(output_t(1), fsm->getOutput(4, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 4);

				// wrong state -> ERR messages:
				/// <type>::getOutput - bad state id
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, NULL_STATE);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, -2);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 5);

				if (!fsm->isOutputTransition()) {// Moore and DFA -> output of the next state
#define ERR_MSG_OUTPUT_NEXT_STATE "Wrong output of the next state of transition from %d on %d"
					// outputs on defined transitions
					ARE_EQUAL(output_t(0), fsm->getOutput(0, 0), ERR_MSG_OUTPUT_NEXT_STATE, 0, 0);
					ARE_EQUAL(output_t(1), fsm->getOutput(0, 1), ERR_MSG_OUTPUT_NEXT_STATE, 0, 1);
					ARE_EQUAL(output_t(0), fsm->getOutput(0, 2), ERR_MSG_OUTPUT_NEXT_STATE, 0, 2);
					ARE_EQUAL(DEFAULT_OUTPUT, fsm->getOutput(1, 0), ERR_MSG_OUTPUT_NEXT_STATE, 1, 0);
					ARE_EQUAL(output_t(1), fsm->getOutput(2, 1), ERR_MSG_OUTPUT_NEXT_STATE, 2, 1);
					ARE_EQUAL(output_t(0), fsm->getOutput(3, 2), ERR_MSG_OUTPUT_NEXT_STATE, 3, 2);
					ARE_EQUAL(output_t(0), fsm->getOutput(4, 0), ERR_MSG_OUTPUT_NEXT_STATE, 4, 0);

#define ERR_MSG_OUTPUT_NO_TRANSITION "There is output after undefined transition from %d on %d"
					// the others should not exist -> ERRs:
					/// <type>::getOutput - there is no such transition
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 4, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 4, 2);

					// wrong state -> ERR messages:
					/// <type>::getOutput - bad state id
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 0), ERR_MSG_OUTPUT_NO_TRANSITION, NULL_STATE, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 1), ERR_MSG_OUTPUT_NO_TRANSITION, -2, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 5, 2);
					// wrong input -> ERR messages:
					/// <type>::getOutput - bad input
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, -2), ERR_MSG_OUTPUT_NO_TRANSITION, 1, -2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 3), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 3);
					// wrong state and input -> ERR messages:
					/// <type>::getOutput - bad state id
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 3), ERR_MSG_OUTPUT_NO_TRANSITION, NULL_STATE, 3);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 3), ERR_MSG_OUTPUT_NO_TRANSITION, -2, 3);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 3), ERR_MSG_OUTPUT_NO_TRANSITION, 5, 3);
				}
			}
			if (fsm->isOutputTransition()) {
#define ERR_MSG_OUTPUT_TRANSITION "Wrong output of transition from %d on %d"
				// outputs on defined transitions
				ARE_EQUAL(output_t(1), fsm->getOutput(0, 0), ERR_MSG_OUTPUT_TRANSITION, 0, 0);
				ARE_EQUAL(output_t(0), fsm->getOutput(0, 1), ERR_MSG_OUTPUT_TRANSITION, 0, 1);
				ARE_EQUAL(DEFAULT_OUTPUT, fsm->getOutput(0, 2), ERR_MSG_OUTPUT_TRANSITION, 0, 2);
				ARE_EQUAL(output_t(1), fsm->getOutput(1, 0), ERR_MSG_OUTPUT_TRANSITION, 1, 0);
				ARE_EQUAL(output_t(0), fsm->getOutput(2, 1), ERR_MSG_OUTPUT_TRANSITION, 2, 1);
				ARE_EQUAL(output_t(1), fsm->getOutput(3, 2), ERR_MSG_OUTPUT_TRANSITION, 3, 2);
				ARE_EQUAL(output_t(0), fsm->getOutput(4, 0), ERR_MSG_OUTPUT_TRANSITION, 4, 0);

#define ERR_MSG_OUTPUT_NO_TRANSITION "There is output on undefined transition from %d on %d"
				// the others should not exist -> ERRs:
				/// <type>::getOutput - there is no such transition
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 1);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 2);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 0);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 2);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 0);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 1);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 4, 1);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 4, 2);

				// wrong state -> ERR messages:
				/// <type>::getOutput - bad state id
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 0), ERR_MSG_OUTPUT_NO_TRANSITION, NULL_STATE, 0);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 1), ERR_MSG_OUTPUT_NO_TRANSITION, -2, 1);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 5, 2);
				// wrong input -> ERR messages:
				/// <type>::getOutput - bad input
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, -2), ERR_MSG_OUTPUT_NO_TRANSITION, 1, -2);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 3), ERR_MSG_OUTPUT_NO_TRANSITION, 2, 3);
				// wrong state and input -> ERR messages:
				/// <type>::getOutput - bad state id
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 3), ERR_MSG_OUTPUT_NO_TRANSITION, NULL_STATE, 3);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 3), ERR_MSG_OUTPUT_NO_TRANSITION, -2, 3);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 3), ERR_MSG_OUTPUT_NO_TRANSITION, 5, 3);
				if (!fsm->isOutputState()) {// Mealy
#define ERR_MSG_OUTPUT_WRONG_INPUT "Output of state %d is defined"
					// wrong input -> ERR messages:
					/// <type>::getOutput - bad input
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(0, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 3);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 4);
					// wrong state -> ERR messages:
					/// <type>::getOutput - bad state id
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, NULL_STATE);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, -2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, STOUT_INPUT), ERR_MSG_OUTPUT_WRONG_INPUT, 5);
				}
			}
		}

		void tGetEndPathState() {
			sequence_in_t seq = { 0, 1, 0, 2, 2, 1, 0 };
			ARE_EQUAL(state_t(2), fsm->getEndPathState(0, seq), "The last state from %d on %s is wrong",
				0, FSMmodel::getInSequenceAsString(seq).c_str());
			seq.pop_back();
			ARE_EQUAL(state_t(4), fsm->getEndPathState(0, seq), "The last state from %d on %s is wrong",
				0, FSMmodel::getInSequenceAsString(seq).c_str());
			ARE_EQUAL(WRONG_STATE, fsm->getEndPathState(1, seq), "The last state from %d on %s is wrong",
				1, FSMmodel::getInSequenceAsString(seq).c_str());
		}

		void tGetOutputAlongPath(sequence_in_t& seq, sequence_out_t& expectedOut) {
			sequence_out_t outSeq = fsm->getOutputAlongPath(0, seq);
			ARE_EQUAL(expectedOut == outSeq, true, "The output sequence %s from %d on %s is different from the correct one %s",
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 0, FSMmodel::getInSequenceAsString(seq).c_str(),
				FSMmodel::getOutSequenceAsString(expectedOut).c_str());
			seq.pop_back();
			expectedOut.pop_back();
			outSeq = fsm->getOutputAlongPath(0, seq);
			ARE_EQUAL(expectedOut == outSeq, true, "The output sequence %s from %d on %s is different from the correct one %s",
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 0, FSMmodel::getInSequenceAsString(seq).c_str(),
				FSMmodel::getOutSequenceAsString(expectedOut).c_str());
			// seq from state 1 produces ERR:
			/// <type>::getOutput - there is no such transition (3, 1)
			outSeq = fsm->getOutputAlongPath(1, seq);
			ARE_EQUAL(1, int(outSeq.size()), "The output sequence %s from %d on %s does not contaion only WRONG_OUTPUT",
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 1, FSMmodel::getInSequenceAsString(seq).c_str());
			ARE_EQUAL(WRONG_OUTPUT, outSeq.front(), "The output sequence %s from %d on %s does not contaion only WRONG_OUTPUT",
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 1, FSMmodel::getInSequenceAsString(seq).c_str());
		}

		void create() {
			fsm->create(5, 3, 2);
			createTransitions();
			if (fsm->isOutputState()) createStateOutputs();
			if (fsm->isOutputTransition()) createTransitionOutputs();
		}

		void createTransitions() {
			fsm->setTransition(0, 0, 0);
			fsm->setTransition(0, 1, 1);
			fsm->setTransition(0, 2, 2);
			fsm->setTransition(1, 0, 3);
			fsm->setTransition(2, 1, 4);
			fsm->setTransition(3, 2, 0);
			fsm->setTransition(4, 0, 2);
		}
		
		void createStateOutputs() {
			fsm->setOutput(0, 0);
			fsm->setOutput(1, 1);
			fsm->setOutput(2, 0);
			//fsm->setOutput(3, 0);
			fsm->setOutput(4, 1);
		}

		void createTransitionOutputs() {
			fsm->setOutput(0, 1, 0);
			fsm->setOutput(0, 0, 1);
			//fsm->setOutput(0, 0, 2);
			fsm->setOutput(1, 1, 0);
			fsm->setOutput(2, 0, 1);
			fsm->setOutput(3, 1, 2);
			fsm->setOutput(4, 0, 0);
		}
	};
}