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
		unique_ptr<DFSM> fsm;

		TEST_METHOD(TestTransitionsDFSM)
		{
			fsm = make_unique<DFSM>();
			tSetTransition();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestOutputDFSM)
		{
			fsm = make_unique<DFSM>();
			tSetOutput();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, STOUT_INPUT, 1, STOUT_INPUT, 0, STOUT_INPUT, 
				2, STOUT_INPUT, 2, STOUT_INPUT, 1, STOUT_INPUT, 0, STOUT_INPUT };
			sequence_out_t expectedOut = { 0, 1, 0, 0, 1, 1, DEFAULT_OUTPUT, 1, 0, DEFAULT_OUTPUT, 0, 0, 1, 0, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestRemoveDFSM)
		{
			fsm = make_unique<DFSM>();
			tRemoveState();
			tRemoveTransition();
		}

		TEST_METHOD(TestTransitionsMealy)
		{
			fsm = make_unique<Mealy>();
			tSetTransition();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestOutputMealy)
		{
			fsm = make_unique<Mealy>();
			tSetOutput();
			tGetOutput();
			sequence_in_t seq = { 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 1, 0, 1, 1, DEFAULT_OUTPUT, 0, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestRemoveMealy)
		{
			fsm = make_unique<Mealy>();
			tRemoveState();
			tRemoveTransition();
		}

		TEST_METHOD(TestTransitionsMoore)
		{
			fsm = make_unique<Moore>();
			tSetTransition();
			tGetNextState();
			tGetEndPathState();
		}

		TEST_METHOD(TestOutputMoore)
		{
			fsm = make_unique<Moore>();
			tSetOutput();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 0, 0, 1, DEFAULT_OUTPUT, 0, 0, 1, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestRemoveMoore)
		{
			fsm = make_unique<Moore>();
			tRemoveState();
			tRemoveTransition();
		}

		TEST_METHOD(TestTransitionsDFA)
		{
			fsm = make_unique<DFA>();
			tSetTransition();
			tGetNextState();
			tGetEndPathState();
		}
		
		TEST_METHOD(TestOutputDFA)
		{
			fsm = make_unique<DFA>();
			tSetOutput();
			tGetOutput();
			sequence_in_t seq = { STOUT_INPUT, 0, 1, 0, 2, 2, 1, 0 };
			sequence_out_t expectedOut = { 0, 0, 1, DEFAULT_OUTPUT, 0, 0, 1, 0 };
			tGetOutputAlongPath(seq, expectedOut);
		}

		TEST_METHOD(TestRemoveDFA)
		{
			fsm = make_unique<DFA>();
			tRemoveState();
			tRemoveTransition();
		}

		/// includes tests for incNumberOfInputs(), addState(), setTransition()
		void tSetTransition() {
			fsm->create(4, 2, 2);
#define ERR_MSG_SET_TRANSITION_FAIL "Transition from %d on %d to %d was not added"
			ARE_EQUAL(true, fsm->setTransition(0, 0, 0), ERR_MSG_SET_TRANSITION_FAIL, 0, 0, 0);
			ARE_EQUAL(true, fsm->setTransition(0, 1, 1), ERR_MSG_SET_TRANSITION_FAIL, 0, 1, 1);

			// a repeated setting without problem
			ARE_EQUAL(true, fsm->setTransition(0, 1, 1), ERR_MSG_SET_TRANSITION_FAIL, 0, 1, 1);

#define ERR_MSG_SET_TRANSITION_SHOULD_FAIL "Transition from %d on %d to %d cannot be added"			
			// wrong input -> ERR messages:
			/// <type>::setTransition - bad input
			ARE_EQUAL(false, fsm->setTransition(0, 2, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, 2, 2);
			/// <type>::setTransition - use setOutput() to set an output instead
			ARE_EQUAL(false, fsm->setTransition(0, STOUT_INPUT, 0), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, STOUT_INPUT, 0);
			
			// bad state id -> 4 ERR messages:
			/// <type>::setTransition - bad state To
			ARE_EQUAL(false, fsm->setTransition(2, 1, 4), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 2, 1, 4);
			ARE_EQUAL(false, fsm->setTransition(3, 0, NULL_STATE), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, NULL_STATE);
			/// <type>::setTransition - bad state From
			ARE_EQUAL(false, fsm->setTransition(4, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 4, 0, 2);
			ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, NULL_STATE, 0, 2);
		
			fsm->incNumberOfInputs(1);
			ARE_EQUAL(true, fsm->setTransition(0, 2, 2), ERR_MSG_SET_TRANSITION_FAIL, 0, 2, 2);
			
			// wrong input -> ERR messages:
			/// <type>::setTransition - bad input
			ARE_EQUAL(false, fsm->setTransition(0, 3, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, 3, 2);
			/// <type>::setTransition - use setOutput() to set an output instead
			ARE_EQUAL(false, fsm->setTransition(0, STOUT_INPUT, 0), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, STOUT_INPUT, 0);
			// bad state id -> 4 ERR messages:
			/// <type>::setTransition - bad state To
			ARE_EQUAL(false, fsm->setTransition(2, 1, 4), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 2, 1, 4);
			ARE_EQUAL(false, fsm->setTransition(3, 0, NULL_STATE), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, NULL_STATE);
			/// <type>::setTransition - bad state From
			ARE_EQUAL(false, fsm->setTransition(4, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 4, 0, 2);
			ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, NULL_STATE, 0, 2);

			ARE_EQUAL(true, fsm->setTransition(1, 0, 3), ERR_MSG_SET_TRANSITION_FAIL, 1, 0, 3);

			ARE_EQUAL(state_t(4), fsm->addState(), "IDs of new states are not equal.");

			ARE_EQUAL(true, fsm->setTransition(2, 1, 4), ERR_MSG_SET_TRANSITION_FAIL, 2, 1, 4);
			ARE_EQUAL(true, fsm->setTransition(3, 2, 0), ERR_MSG_SET_TRANSITION_FAIL, 3, 2, 0);
			ARE_EQUAL(true, fsm->setTransition(4, 0, 2), ERR_MSG_SET_TRANSITION_FAIL, 4, 0, 2);
			
			// wrong input -> ERR messages:
			/// <type>::setTransition - bad input
			ARE_EQUAL(false, fsm->setTransition(0, 3, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, 3, 2);
			/// <type>::setTransition - use setOutput() to set an output instead
			ARE_EQUAL(false, fsm->setTransition(0, STOUT_INPUT, 0), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, STOUT_INPUT, 0);
			// bad state id -> 5 ERR messages:
			/// <type>::setTransition - bad state To
			ARE_EQUAL(false, fsm->setTransition(3, 0, 5), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, 5);
			ARE_EQUAL(false, fsm->setTransition(3, 0, NULL_STATE), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, NULL_STATE);
			/// <type>::setTransition - bad state From
			ARE_EQUAL(false, fsm->setTransition(5, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 5, 0, 2);
			ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, 2), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, NULL_STATE, 0, 2);
			ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, NULL_STATE), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, NULL_STATE, 0, NULL_STATE);
			// wrong input -> ERR messages:
			/// <type>::setTransition - use setOutput() to set an output instead
			ARE_EQUAL(false, fsm->setTransition(NULL_STATE, STOUT_INPUT, NULL_STATE), ERR_MSG_SET_TRANSITION_SHOULD_FAIL,
				NULL_STATE, STOUT_INPUT, NULL_STATE);
		}

		/// includes tests for getNextState()
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

		/// includes tests for getEndPathState()
		void tGetEndPathState() {
#define ERR_MSG_END_PATH_STATE_WRONG "The last state from %d on %s is wrong"
			sequence_in_t seq = { 0, 1, 0, 2, 2, 1, 0 };
			ARE_EQUAL(state_t(2), fsm->getEndPathState(0, seq), ERR_MSG_END_PATH_STATE_WRONG,
				0, FSMmodel::getInSequenceAsString(seq).c_str());
			seq.pop_back();
			ARE_EQUAL(state_t(4), fsm->getEndPathState(0, seq), ERR_MSG_END_PATH_STATE_WRONG,
				0, FSMmodel::getInSequenceAsString(seq).c_str());
			ARE_EQUAL(WRONG_STATE, fsm->getEndPathState(1, seq), ERR_MSG_END_PATH_STATE_WRONG,
				1, FSMmodel::getInSequenceAsString(seq).c_str());
		}

		/// includes tests for incNumberOfOutputs(), addState(out), setTransition(-,-,-,out), setOutput(out)
		void tSetOutput() {
			fsm->create(3, 3, 1);

			// wrong output -> ERR messages:
			/// <type>::addState - bad output
			ARE_EQUAL(NULL_STATE, fsm->addState(1), "A new states was created even with wrong output.");

			ARE_EQUAL(state_t(3), fsm->addState(), "IDs of new states are not equal.");

#define ERR_MSG_SET_STATE_OUTPUT_WRONG "State %d cannot have an output %d"
#define ERR_MSG_SET_TRANSITION_OUTPUT_WRONG "Transition from %d cannot have an output %d on input %d"
			// wrong output -> ERR messages:
			/// <type>::setOutput - bad output
			ARE_EQUAL(false, fsm->setOutput(0, 1), ERR_MSG_SET_STATE_OUTPUT_WRONG, 0, 1);
			// wrong state -> ERR messages:
			/// <type>::setOutput - bad state
			ARE_EQUAL(false, fsm->setOutput(NULL_STATE, 0), ERR_MSG_SET_STATE_OUTPUT_WRONG, NULL_STATE, 0);

			if (fsm->isOutputTransition()) {// DFSM, Mealy
				// wrong output -> ERR messages:
				/// <type>::setOutput/setTransition - bad output
				ARE_EQUAL(false, fsm->setOutput(0, 1, 0), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 0, 1, 0);
				ARE_EQUAL(false, fsm->setTransition(0, 2, 2, 1), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 0, 1, 2);
			}

			fsm->incNumberOfOutputs(1);

			/// DFA::incNumberOfOutputs - the number of outputs cannot be increased over 2
			fsm->incNumberOfOutputs(1);
			if (fsm->getNumberOfOutputs() > 2) 
				fsm->incNumberOfOutputs(-1);
			//DEBUG_MSG("%d", fsm->getNumberOfOutputs());
		
			if (fsm->isOutputState()) {// DFSM, Moore, DFA	
#define ERR_MSG_SET_STATE_OUTPUT "Output %d of state %d was not set"
				ARE_EQUAL(true, fsm->setOutput(0, 0), ERR_MSG_SET_STATE_OUTPUT, 0, 0);
				ARE_EQUAL(true, fsm->setOutput(1, 1), ERR_MSG_SET_STATE_OUTPUT, 1, 1);
				ARE_EQUAL(true, fsm->setOutput(2, 0), ERR_MSG_SET_STATE_OUTPUT, 2, 0);
				///fsm->setOutput(4, 1);
				ARE_EQUAL(state_t(4), fsm->addState(1), "IDs of new states are not equal.");
			}
			else {// Mealy
				// wrong output -> ERR messages:
				/// <type>::addState - set output of a state is not permitted
				ARE_EQUAL(NULL_STATE, fsm->addState(0), "A new states was created even with wrong output.");
				
				ARE_EQUAL(state_t(4), fsm->addState(), "IDs of new states are not equal.");
			}
			
			if (fsm->isOutputTransition()) {// DFSM, Mealy
				// wrong transition -> ERR messages:
				/// <type>::setOutput - there is no such transition
				ARE_EQUAL(false, fsm->setOutput(0, 1, 0), "Output %d set to nonexisted transition from %d on %d", 1, 0, 0);
#define ERR_MSG_SET_TRANSITION_FAIL "Transition from %d on %d to %d was not added"
				ARE_EQUAL(true, fsm->setTransition(0, 0, 0), ERR_MSG_SET_TRANSITION_FAIL, 0, 0, 0);
#define ERR_MSG_SET_TRANSITION_OUTPUT "Output %d was not set to the transition from %d on %d"
				ARE_EQUAL(true, fsm->setOutput(0, 1, 0), ERR_MSG_SET_TRANSITION_OUTPUT, 1, 0, 0);

				ARE_EQUAL(true, fsm->setTransition(0, 1, 1), ERR_MSG_SET_TRANSITION_FAIL, 0, 1, 1);
				ARE_EQUAL(true, fsm->setOutput(0, 0, 1), ERR_MSG_SET_TRANSITION_OUTPUT, 0, 0, 1);
				ARE_EQUAL(true, fsm->setTransition(0, 2, 2), ERR_MSG_SET_TRANSITION_FAIL, 0, 2, 2);
				//fsm->setOutput(0, 0, 2);
				ARE_EQUAL(true, fsm->setTransition(1, 0, 3), ERR_MSG_SET_TRANSITION_FAIL, 1, 0, 3);
				ARE_EQUAL(true, fsm->setOutput(1, 1, 0), ERR_MSG_SET_TRANSITION_OUTPUT, 1, 1, 0);
#define ERR_MSG_SET_TRANSITION_OUTPUT_FAIL "Transition from %d on %d to %d with output %d was not added"
				ARE_EQUAL(true, fsm->setTransition(2, 1, 4, 0), ERR_MSG_SET_TRANSITION_OUTPUT_FAIL, 2, 1, 4, 0);
				ARE_EQUAL(true, fsm->setTransition(3, 2, 0, 1), ERR_MSG_SET_TRANSITION_OUTPUT_FAIL, 3, 2, 0, 1);
				ARE_EQUAL(true, fsm->setTransition(4, 0, 2, 0), ERR_MSG_SET_TRANSITION_OUTPUT_FAIL, 4, 0, 2, 0);

				// wrong output -> ERR messages:
				/// <type>::setTransition - bad output
				ARE_EQUAL(false, fsm->setTransition(0, 2, 2, 2), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 0, 2, 2);
				// wrong input -> ERR messages:
				/// <type>::setTransition - bad input
				ARE_EQUAL(false, fsm->setTransition(0, 3, 2, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, 3, 2);
				/// <type>::setTransition - use setOutput() to set an output instead/STOUT_INPUT is not allowed
				ARE_EQUAL(false, fsm->setTransition(0, STOUT_INPUT, 0, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 0, STOUT_INPUT, 0);
				// bad state id -> 5 ERR messages:
				/// <type>::setTransition - bad state To
				ARE_EQUAL(false, fsm->setTransition(3, 0, 5, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, 5);
				ARE_EQUAL(false, fsm->setTransition(3, 0, NULL_STATE, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 3, 0, NULL_STATE);
				/// <type>::setTransition - bad state From
				ARE_EQUAL(false, fsm->setTransition(5, 0, 2, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, 5, 0, 2);
				ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, 2, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL, NULL_STATE, 0, 2);
				ARE_EQUAL(false, fsm->setTransition(NULL_STATE, 0, NULL_STATE, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL,
					NULL_STATE, 0, NULL_STATE);
				// wrong input -> ERR messages:
				/// <type>::setTransition - use setOutput() to set an output instead/STOUT_INPUT is not allowed
				ARE_EQUAL(false, fsm->setTransition(NULL_STATE, STOUT_INPUT, NULL_STATE, 1), ERR_MSG_SET_TRANSITION_SHOULD_FAIL,
					NULL_STATE, STOUT_INPUT, NULL_STATE);

				// wrong state -> ERR messages:
				/// <type>::setOutput - bad state
				ARE_EQUAL(false, fsm->setOutput(NULL_STATE, 0, 1), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, NULL_STATE, 0, 1);
				// wrong output -> ERR messages:
				/// <type>::setOutput - bad output
				ARE_EQUAL(false, fsm->setOutput(4, 5, 0), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 4, 5, 0);
			}
			else {// Moore, DFA
				// wrong input -> ERR messages:
				/// <type>::addState - bad input
				ARE_EQUAL(false, fsm->setOutput(0, 1, 2), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 0, 1, 2);
				createTransitions();
				ARE_EQUAL(false, fsm->setOutput(0, 1, 2), ERR_MSG_SET_TRANSITION_OUTPUT_WRONG, 0, 1, 2);
			}
		}		

		/// includes tests for getOutput
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

#define ERR_MSG_OUTPUT_AFTER_NO_TRANSITION "There is output after undefined transition from %d on %d"
					// the others should not exist -> ERRs:
					/// <type>::getOutput - there is no such transition
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 1), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 1, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 2), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 1, 2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 0), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 2, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 2), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 2, 2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 0), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 3, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 1), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 3, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 1), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 4, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(4, 2), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 4, 2);

					// wrong state -> ERR messages:
					/// <type>::getOutput - bad state id
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 0), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, NULL_STATE, 0);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 1), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, -2, 1);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 2), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 5, 2);
					// wrong input -> ERR messages:
					/// <type>::getOutput - bad input
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, -2), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 1, -2);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(2, 3), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 2, 3);
					// wrong state and input -> ERR messages:
					/// <type>::getOutput - bad state id
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(NULL_STATE, 3), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, NULL_STATE, 3);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(-2, 3), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, -2, 3);
					ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(5, 3), ERR_MSG_OUTPUT_AFTER_NO_TRANSITION, 5, 3);
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
		
		/// includes tests for getOutputAlongPath()
		void tGetOutputAlongPath(sequence_in_t& seq, sequence_out_t& expectedOut) {
#define ERR_MSG_OUTPUT_SEQUENCE "The output sequence %s from %d on %s is different from the correct one %s"
			sequence_out_t outSeq = fsm->getOutputAlongPath(0, seq);
			ARE_EQUAL(expectedOut == outSeq, true, ERR_MSG_OUTPUT_SEQUENCE,
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 0, FSMmodel::getInSequenceAsString(seq).c_str(),
				FSMmodel::getOutSequenceAsString(expectedOut).c_str());
			seq.pop_back();
			expectedOut.pop_back();
			outSeq = fsm->getOutputAlongPath(0, seq);
			ARE_EQUAL(expectedOut == outSeq, true, ERR_MSG_OUTPUT_SEQUENCE,
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 0, FSMmodel::getInSequenceAsString(seq).c_str(),
				FSMmodel::getOutSequenceAsString(expectedOut).c_str());
			// seq from state 1 produces ERR:
			/// <type>::getOutput - there is no such transition (3, 1)
			outSeq = fsm->getOutputAlongPath(1, seq);
#define ERR_MSG_OUTPUT_SEQUENCE_FAIL "The output sequence %s from %d on %s does not contaion only WRONG_OUTPUT"
			ARE_EQUAL(1, int(outSeq.size()), ERR_MSG_OUTPUT_SEQUENCE_FAIL,
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 1, FSMmodel::getInSequenceAsString(seq).c_str());
			ARE_EQUAL(WRONG_OUTPUT, outSeq.front(), ERR_MSG_OUTPUT_SEQUENCE_FAIL,
				FSMmodel::getOutSequenceAsString(outSeq).c_str(), 1, FSMmodel::getInSequenceAsString(seq).c_str());
		}

		void tRemoveState() {
			create();
			auto states = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");

			/// <type>::removeState - the initial state cannot be removed
			ARE_EQUAL(false, fsm->removeState(0), "The initial state was removed");
			/// <type>::removeState - bad state
			ARE_EQUAL(false, fsm->removeState(5), "Non-existent state 5 was removed");
			ARE_EQUAL(true, fsm->removeState(3), "State 3 cannot be removed");

			ARE_EQUAL(NULL_STATE, fsm->getNextState(1, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, 1, 0);
			/// <type>::getOutput - there is no such transition
			ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 0);
			/// <type>::getNextState - bad state id
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(3, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, 3, 0);
			/// <type>::getOutput - bad state id
			ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 0);
			/// <type>::getOutput - bad state id
			ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 1), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 1);
			if (fsm->isOutputState()) {
				/// <type>::getOutput - bad state id
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, STOUT_INPUT), ERR_MSG_OUTPUT_STATE, 3);
			}
			auto states2 = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states2.size()), "The numbers of states are not equal.");
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size() - 1), "The numbers of states do not correspond.");
			bool present = true;
			for (state_t& state : states)
			{
				if (find(states2.begin(), states2.end(), state) == states2.end()) {
					ARE_EQUAL(state_t(3), state, "State %d was removed unintentionally", state);
					present = false;
				}
			}
			ARE_EQUAL(false, present, "State 3 was not removed");

			/// <type>::removeState - bad state
			ARE_EQUAL(false, fsm->removeState(3), "Non-existent state 3 was removed");

			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states2.size()), "The numbers of states are not equal.");
		}

		void tRemoveTransition() {
			create();
#define ERR_MSG_WRONG_TRANSITION "Wrong transition from %d on %d was removed"
			/// <type>::removeTransition - bad input
			ARE_EQUAL(false, fsm->removeTransition(0, STOUT_INPUT), ERR_MSG_WRONG_TRANSITION, 0, STOUT_INPUT);
			ARE_EQUAL(false, fsm->removeTransition(0, 3), ERR_MSG_WRONG_TRANSITION, 0, 3);
			/// <type>::removeTransition - bad state From
			ARE_EQUAL(false, fsm->removeTransition(NULL_STATE, STOUT_INPUT), ERR_MSG_WRONG_TRANSITION, NULL_STATE, STOUT_INPUT);
			ARE_EQUAL(false, fsm->removeTransition(5, 1), ERR_MSG_WRONG_TRANSITION, 5, 1);
			/// <type>::removeTransition - there is no such transition
			ARE_EQUAL(false, fsm->removeTransition(1, 2), ERR_MSG_WRONG_TRANSITION, 1, 2);

#define ERR_MSG_REMOVE_TRANSITION "Transition from %d on %d cannot be removed"
			ARE_EQUAL(true, fsm->removeTransition(3, 2), ERR_MSG_REMOVE_TRANSITION, 3, 2);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(3, 2), ERR_MSG_NEXT_STATE_NOT_EXISTS, 3, 2);
			if (fsm->isOutputTransition()) {
				/// <type>::getOutput - there is no such transition
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 2), ERR_MSG_OUTPUT_NO_TRANSITION, 3, 2);
			}
			
			// removes the transition even the next state and output are wrong
			/// <type>::removeTransition - state To has no effect here
			/// <type>::removeTransition - the output has no effect here
			ARE_EQUAL(true, fsm->removeTransition(1, 0, 5, 4), ERR_MSG_REMOVE_TRANSITION, 1, 0);
			ARE_EQUAL(NULL_STATE, fsm->getNextState(1, 0), ERR_MSG_NEXT_STATE_NOT_EXISTS, 1, 0);
			if (fsm->isOutputTransition()) {
				/// <type>::getOutput - there is no such transition
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 0), ERR_MSG_OUTPUT_NO_TRANSITION, 1, 0);
			}
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