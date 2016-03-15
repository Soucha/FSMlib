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
#include "CppUnitTest.h"
#include "../TestUtils.h"
#include <iostream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace FSMlibTest
{
	INIT_SUITE

	TEST_CLASS(IOtests)
	{
	public:
		DFSM * fsm, * fsm2;

		TEST_METHOD(TestGenerate)
		{
			
		}

		TEST_METHOD(TestSaveLoadDFSM)
		{
			DFSM dfsm, dfsm2;
			fsm = &dfsm;
			fsm2 = &dfsm2; 
			tCreateSaveLoad();
		}

		TEST_METHOD(TestSaveLoadDFA)
		{
			DFA dfa, dfa2;
			fsm = &dfa;
			fsm2 = &dfa2;
			tCreateSaveLoad();
		}

		void tCreateSaveLoad() {
			DEBUG_MSG(machineTypeNames[fsm->getType()]);
			fsm->create(0, 0, 0);// = create(1,1,1) is minimum
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(1), "The number of states is not correct.");
			tSaveLoad();

			fsm->create(5, 3, 2);
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(5), "The number of states is not correct.");
			ARE_EQUAL(fsm->getNumberOfInputs(), input_t(3), "The number of inputs is not correct.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), output_t(2), "The number of outputs is not correct.");
			tSaveLoad();
		}

		void tSaveLoad() {
			string path = "../../data/tmp/";
			string filename = fsm->save(path);
			ARE_EQUAL(!filename.empty(), true, "This %s cannot be saved into path '%s'.",
				machineTypeNames[fsm->getType()], path);
			ARE_EQUAL(fsm2->load(filename), true, "File '%s' cannot be loaded.", filename);
			DEBUG_MSG(filename.c_str());
			

			//Assert::IsTrue(FSMmodel::areIsomorphic(fsm, fsm2), L"Save FSM and load FSM are not same.", LINE_INFO());
			ARE_EQUAL(fsm->getNumberOfStates(), fsm2->getNumberOfStates(), "The numbers of states are not equal.");
			ARE_EQUAL(fsm->getNumberOfInputs(), fsm2->getNumberOfInputs(), "The numbers of inputs are not equal.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), fsm2->getNumberOfOutputs(), "The numbers of outputs are not equal.");

			if (fsm->getNumberOfStates() == 0) return;

			vector<state_t> states;
			state_t stop = 0;
			states.push_back(0);
			while (stop < states.size()) {
				if (fsm->isOutputState()) {
					ARE_EQUAL(fsm->getOutput(states[stop], STOUT_INPUT), fsm2->getOutput(states[stop], STOUT_INPUT), 
						"The outputs of state %d are different.", states[stop]);
				}
				for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
					if (fsm->isOutputTransition()) {
						ARE_EQUAL(fsm->getOutput(states[stop], i), fsm2->getOutput(states[stop], i),
							"The outputs on input %d from state %d are different.", i, states[stop]);
					}
					state_t nextState = fsm->getNextState(states[stop], i);
					DEBUG_MSG("%d", nextState);
					ARE_EQUAL(nextState, fsm2->getNextState(states[stop], i),
						"The next states on input %d from state %d are different.", i, states[stop]);
					ARE_EQUAL(nextState != WRONG_STATE, true, 
						"The next state on input %d from state %d is wrong.", i, states[stop]);
					if ((nextState != NULL_STATE) && (find(states.begin(), states.end(), nextState) == states.end())) {
						states.push_back(nextState);
					}
				}
				stop++;
			}
			if (fsm->isReduced()) {
				ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");
			}
		}
	};
}