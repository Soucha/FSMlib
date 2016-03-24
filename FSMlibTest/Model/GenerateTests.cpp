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
	TEST_CLASS(GenerateTests)
	{
	public:
		DFSM * fsm;

		TEST_METHOD(TestGenerateDFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			tGenerate(5, 4, 3);
			
			// reduction of the number of output
			/// <type>::generate - the number of outputs reduced to the maximum of 16
			tGenerate(4, 3, 30);
		}

		TEST_METHOD(TestGenerateMealy)
		{
			Mealy mealy;
			fsm = &mealy;
			tGenerate(5, 4, 3);

			// reduction of the number of output
			/// <type>::generate - the number of outputs reduced to the maximum of 12
			tGenerate(4, 3, 30);
		}

		TEST_METHOD(TestGenerateMoore)
		{
			Moore moore;
			fsm = &moore;
			tGenerate(5, 4, 3);

			// reduction of the number of output
			/// <type>::generate - the number of outputs reduced to the maximum of 4
			tGenerate(4, 3, 30);
		}

		TEST_METHOD(TestGenerateDFA)
		{
			DFA dfa;
			fsm = &dfa;
			tGenerate(5, 4, 2);

			// reduction of the number of output
			/// <type>::generate - the number of outputs reduced to the maximum of 2
			tGenerate(4, 3, 30);
		}

		void tGenerate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
			fsm->generate(numberOfStates, numberOfInputs, numberOfOutputs);

			ARE_EQUAL(numberOfStates, fsm->getNumberOfStates(), "The numbers of states are not equal.");
			ARE_EQUAL(numberOfInputs, fsm->getNumberOfInputs(), "The numbers of inputs are not equal.");
			auto states = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");
			ARE_EQUAL(true, find(states.begin(), states.end(), 0) != states.end(), "The initial state 0 does not exist");
			vector<bool> usedOutputs(fsm->getNumberOfOutputs(), false);
			set<state_t> coveredStates;
			queue<state_t> fifo;
			fifo.push(0);
			coveredStates.insert(0);
			while (!fifo.empty()) {
				auto state = fifo.front();
				fifo.pop();
				if (fsm->isOutputState()) {
					auto output = fsm->getOutput(state, STOUT_INPUT);
					ARE_EQUAL(true, output != DEFAULT_OUTPUT, "Output of state %d is not set", state);
					ARE_EQUAL(true, output < fsm->getNumberOfOutputs(), "Output %d of state %d is wrong", output, state);
					usedOutputs[output] = true;
				}
				for (input_t i = 0; i < fsm->getNumberOfInputs(); i++)
				{
					auto nextState = fsm->getNextState(state, i);
					ARE_EQUAL(true, nextState != NULL_STATE, "Transition from %d on %d is not set", state, i);
					ARE_EQUAL(true, nextState < fsm->getNumberOfStates(), "Transition from %d on %d leads to wrong state %d", state, i, nextState);
					if (coveredStates.insert(nextState).second) {// new state
						fifo.push(nextState);
					}
					if (fsm->isOutputTransition()) {
						auto output = fsm->getOutput(state, i);
						ARE_EQUAL(true, output != DEFAULT_OUTPUT, "Output of transition from %d on %d is not set", state, i);
						ARE_EQUAL(true, output < fsm->getNumberOfOutputs(), "Output %d of transition from %d on %d is wrong", output, state, i);
						usedOutputs[output] = true;
					}
				}
			}
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(coveredStates.size()), "Machine is not connected");
			for (output_t o = 0; o < fsm->getNumberOfOutputs(); o++)
			{
				ARE_EQUAL(true, (bool)usedOutputs[o], "Output %d is not used", o);
			}
		}
	};
}
