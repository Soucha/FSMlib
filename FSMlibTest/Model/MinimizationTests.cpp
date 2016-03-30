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
	TEST_CLASS(MinimizationTests)
	{
	public:
		DFSM * fsm, * fsm2;

		// TODO: test incomplete machines

		TEST_METHOD(TestRemoveUnreachableStatesDFSM)
		{
			DFSM dfsm, dfsm2;
			fsm = &dfsm;
			fsm2 = &dfsm2;
			create();
			tRemoveUnreachableStates();
			tMakeCompact();
		}

		TEST_METHOD(TestMinimizeDFSM)
		{
			DFSM dfsm, dfsm2;
			fsm = &dfsm;
			fsm2 = &dfsm2;
			tIsomorphismSameFSM(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
			tIsomorphismPermuteState(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4_P.fsm", DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
			tIsomorphismBadPermuteState(DATA_PATH + MINIMIZATION_DIR + "DFSM_R4_BP.fsm", DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
			tMinimize(DATA_PATH + MINIMIZATION_DIR + "DFSM_U7.fsm", DATA_PATH + MINIMIZATION_DIR + "DFSM_R4.fsm");
		}
		
		TEST_METHOD(TestRemoveUnreachableStatesMealy)
		{
			Mealy mealy, mealy2;
			fsm = &mealy;
			fsm2 = &mealy2;
			create();
			tRemoveUnreachableStates();
			tMakeCompact();
		}

		TEST_METHOD(TestMinimizeMealy)
		{
			Mealy mealy, mealy2;
			fsm = &mealy;
			fsm2 = &mealy2;
			tIsomorphismSameFSM(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
			tIsomorphismPermuteState(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4_P.fsm", DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
			tIsomorphismBadPermuteState(DATA_PATH + MINIMIZATION_DIR + "Mealy_R4_BP.fsm", DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
			tMinimize(DATA_PATH + MINIMIZATION_DIR + "Mealy_U7.fsm", DATA_PATH + MINIMIZATION_DIR + "Mealy_R4.fsm");
		
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_Zhu1993.fsm", DATA_PATH + EXAMPLES_DIR + "Mealy_R4_Zhu1993.fsm");
		}

		TEST_METHOD(TestRemoveUnreachableStatesMoore)
		{
			Moore moore, moore2;
			fsm = &moore;
			fsm2 = &moore2;
			create();
			tRemoveUnreachableStates();
			tMakeCompact();
		}

		TEST_METHOD(TestMinimizeMoore)
		{
			Moore moore, moore2;
			fsm = &moore;
			fsm2 = &moore2;
			tIsomorphismSameFSM(DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");
			tIsomorphismPermuteState(DATA_PATH + MINIMIZATION_DIR + "Moore_R4_P.fsm", DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");
			tIsomorphismBadPermuteState(DATA_PATH + MINIMIZATION_DIR + "Moore_R4_BP.fsm", DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");
			tMinimize(DATA_PATH + MINIMIZATION_DIR + "Moore_U7.fsm", DATA_PATH + MINIMIZATION_DIR + "Moore_R4.fsm");

			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm", DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
		}

		TEST_METHOD(TestRemoveUnreachableStatesDFA)
		{
			DFA dfa, dfa2;
			fsm = &dfa;
			fsm2 = &dfa2;
			create();
			tRemoveUnreachableStates();
			tMakeCompact();
		}

		TEST_METHOD(TestMinimizeDFA)
		{
			DFA dfa, dfa2;
			fsm = &dfa;
			fsm2 = &dfa2;
			tIsomorphismSameFSM(DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
			tIsomorphismPermuteState(DATA_PATH + MINIMIZATION_DIR + "DFA_R4_P.fsm", DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
			tIsomorphismBadPermuteState(DATA_PATH + MINIMIZATION_DIR + "DFA_R4_BP.fsm", DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
			tMinimize(DATA_PATH + MINIMIZATION_DIR + "DFA_U7.fsm", DATA_PATH + MINIMIZATION_DIR + "DFA_R4.fsm");
		
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			tMinimize(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm", DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void tRemoveUnreachableStates() {
			ARE_EQUAL(true, fsm->removeUnreachableStates(), "Removal of unreachable states failed");
			auto states = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");
			ARE_EQUAL(state_t(2), fsm->getNumberOfStates(), "The numbers of states are not equal.");
			if (states[0] == 0) {
				ARE_EQUAL(state_t(2), states[1], "State %d is wrong", states[1]);
			}
			else {
				ARE_EQUAL(state_t(2), states[0], "State %d is wrong", states[0]);
				ARE_EQUAL(state_t(0), states[1], "State %d is wrong", states[1]);
			}
			/// <type>::getNextState - bad state id
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(1, 2), "There is no next state from %d on %d", 1, 2);
			/// <type>::getOutput - bad state id
			ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, 2), "There is output on undefined transition from %d on %d", 1, 2);
			/// <type>::getNextState - bad state id
			ARE_EQUAL(WRONG_STATE, fsm->getNextState(3, 1), "There is no next state from %d on %d", 3, 1);
			/// <type>::getOutput - bad state id
			ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, 1), "There is output on undefined transition from %d on %d", 3, 1);
			if (fsm->isOutputState()) {
				/// <type>::getOutput - bad state id
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(1, STOUT_INPUT), "There is output non-existent state %d", 1);
				ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(3, STOUT_INPUT), "There is output non-existent state %d", 3);
			}
		}

		void tMakeCompact() {// checks only connected graphs properly
			*fsm2 = *fsm;
			fsm->makeCompact();
			DEBUG_MSG("Number of states: %d -> %d", fsm2->getNumberOfStates(), fsm->getNumberOfStates());
			ARE_EQUAL(fsm2->getNumberOfStates(), fsm->getNumberOfStates(), "The numbers of states are not equal.");
			DEBUG_MSG("Number of inputs: %d -> %d", fsm2->getNumberOfInputs(), fsm->getNumberOfInputs());
			DEBUG_MSG("Number of outputs: %d -> %d", fsm2->getNumberOfOutputs(), fsm->getNumberOfOutputs());
			auto states = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");

			for (state_t state = 0; state < fsm->getNumberOfStates(); state++)
			{
				ARE_EQUAL(state, states[state], "States are not sorted");
			}

			input_t maxInput = 0;
			output_t maxOutput = 0;
			vector<state_t> stateEquiv(fsm->getNumberOfStates(), NULL_STATE);
			stateEquiv[0] = 0;
			queue<state_t> fifo;
			fifo.push(0);
			while (!fifo.empty()) {
				state_t state = fifo.front();
				fifo.pop();
				input_t i;
				if (fsm->isOutputState()) {
					auto output = fsm->getOutput(state, STOUT_INPUT);
					ARE_EQUAL(true, WRONG_OUTPUT != output, "getOutput(%d, %d) failed", state, STOUT_INPUT);
					auto output2 = fsm2->getOutput(stateEquiv[state], STOUT_INPUT);
					ARE_EQUAL(output2, output, "Output of state %d (%d) were changed", stateEquiv[state], state);
					if ((output != DEFAULT_OUTPUT) && (output + 1 > maxOutput)) {
						maxOutput = output + 1;
					}
				}
				for (i = 0; i < fsm->getNumberOfInputs(); i++)
				{
					auto nextState = fsm->getNextState(state, i);
					auto nextState2 = fsm2->getNextState(stateEquiv[state], i);
					ARE_EQUAL(true, WRONG_STATE != nextState, "getNextState(%d, %d) failed", state, i);
					if (nextState != NULL_STATE) {
						ARE_EQUAL(true, NULL_STATE != nextState2, "Next state %d was not set", nextState2);
						ARE_EQUAL(true, WRONG_STATE != nextState2, "Next state %d was not set", nextState2);
						ARE_EQUAL(true, nextState < fsm->getNumberOfStates(), "Next state %d is wrong", nextState);
						if (stateEquiv[nextState] != NULL_STATE) {
							ARE_EQUAL(stateEquiv[nextState], nextState2, "Next states are not equal", nextState2);
						}
						else {
							stateEquiv[nextState] = nextState2;
							fifo.push(nextState);
						}
						if (i + 1 > maxInput) {
							maxInput = i + 1;
						}
						if (fsm->isOutputTransition()) {
							auto output = fsm->getOutput(state, i);
							ARE_EQUAL(true, WRONG_OUTPUT != output, "getOutput(%d, %d) failed", state, i);
							auto output2 = fsm2->getOutput(stateEquiv[state], i);
							ARE_EQUAL(output2, output, "Output of transition from %d (%d) on %d were changed",
								stateEquiv[state], state, i);
							if ((output != DEFAULT_OUTPUT) && (output + 1 > maxOutput)) {
								maxOutput = output + 1;
							}
						}
					}
					else {
						ARE_EQUAL(NULL_STATE, nextState2, "Next state %d was different", nextState2);
						if (fsm->isOutputTransition()) {
							/// <type>::getOutput - there is no such transition
							ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(state, i), "getOutput(%d, %d) failed", state, i);
							/// <type>::getOutput - there is no such transition
							ARE_EQUAL(WRONG_OUTPUT, fsm2->getOutput(stateEquiv[state], i), "getOutput(%d, %d) failed",
								stateEquiv[state], i);
						}
					}
				}
				for (; i < fsm2->getNumberOfInputs(); i++) {
					/// <type>::getNextState - bad input
					ARE_EQUAL(WRONG_STATE, fsm->getNextState(state, i), "There is no next state from %d on %d", state, i);
					ARE_EQUAL(NULL_STATE, fsm2->getNextState(stateEquiv[state], i),
						"There is no next state from %d on %d", stateEquiv[state], i);
					if (fsm->isOutputTransition()) {
						/// <type>::getOutput - bad input
						ARE_EQUAL(WRONG_OUTPUT, fsm->getOutput(state, i), "getOutput(%d, %d) failed", state, i);
						/// <type>::getOutput - there is no such transition
						ARE_EQUAL(WRONG_OUTPUT, fsm2->getOutput(stateEquiv[state], i), "getOutput(%d, %d) failed",
							stateEquiv[state], i);
					}
				}
			}
			ARE_EQUAL(fsm->getNumberOfInputs(), maxInput, "Greatest input %d does not correspond to the number of inputs %d",
				maxInput, fsm->getNumberOfInputs());
			ARE_EQUAL(fsm->getNumberOfOutputs(), maxOutput, "Greatest output %d does not correspond to the number of outputs %d",
				maxOutput, fsm->getNumberOfOutputs());
			
			auto states2 = fsm2->getStates();
			for each (state_t state in stateEquiv)
			{
				ARE_EQUAL(true, NULL_STATE != state, "The machine is not connected");
				auto it = find(states2.begin(), states2.end(), state);
				if (it == states2.end()) {
					ARE_EQUAL(state_t(3), state, "State %d was removed unintentionally", state);
				}
				states2.erase(it);
			}
			ARE_EQUAL(true, states2.empty(), "The machines are not isomorphic");
		}

		void create() {
			fsm->create(4, 3, 2);
			if (fsm->isOutputState()) {
				fsm->setOutput(0, 0);
				fsm->setOutput(1, 1);
				fsm->setOutput(2, 0);
				fsm->setOutput(3, 1);
			}
			fsm->setTransition(0, 0, 0);
			fsm->setTransition(0, 1, 2);
			fsm->setTransition(1, 2, 3);
			fsm->setTransition(2, 0, 0);
			fsm->setTransition(3, 1, 0);
			fsm->setTransition(3, 2, 3);
			if (fsm->isOutputTransition()) {
				fsm->setOutput(0, 0, 0);
				//fsm->setOutput(0, 0, 1);
				fsm->setOutput(1, 1, 2);
				fsm->setOutput(2, 0, 0);
				fsm->setOutput(3, 1, 1);
				//fsm->setOutput(3, 0, 2);
			}
		}

		void tIsomorphismSameFSM(string filename) {
			ARE_EQUAL(true, fsm->load(filename), "Unable to load file %s", filename.c_str());
			ARE_EQUAL(true, fsm2->load(filename), "Unable to load file %s", filename.c_str());
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, fsm2), "Same FSM are isomorphic");
		}

		void tIsomorphismPermuteState(string filename1, string filename2) {
			ARE_EQUAL(true, fsm->load(filename1), "Unable to load file %s", filename1.c_str());
			ARE_EQUAL(true, fsm2->load(filename2), "Unable to load file %s", filename2.c_str());
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, fsm2), "FSM with permute states is isomorphic with original FSM");
		}

		void tIsomorphismBadPermuteState(string filename1, string filename2) {
			ARE_EQUAL(true, fsm->load(filename1), "Unable to load file %s", filename1.c_str());
			ARE_EQUAL(true, fsm2->load(filename2), "Unable to load file %s", filename2.c_str());
			ARE_EQUAL(false, FSMmodel::areIsomorphic(fsm, fsm2), "FSMs are not isomorphic but they are identificated as isomorphic");
		}

		void tMinimize(string filename1, string filename2) {
			ARE_EQUAL(true, fsm->load(filename1), "Unable to load file %s", filename1.c_str());
			ARE_EQUAL(true, fsm2->load(filename2), "Unable to load file %s", filename2.c_str());
			ARE_EQUAL(true, fsm->minimize(), "FSM is not able to minimize");
			//fsm->save(DATA_PATH);
			ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, fsm2), "Wrong reduction.");
		}
	};
}