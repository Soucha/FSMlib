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
	TEST_CLASS(IOtests)
	{
	public:
		unique_ptr<DFSM> fsm, fsm2;

		TEST_METHOD(TestCreateSaveLoadDFSM)
		{
			fsm = make_unique<DFSM>();
			fsm2 = make_unique<DFSM>();
			tCreateSaveLoad();
		}

		TEST_METHOD(TestGenerateSaveLoadDFSM)
		{
			fsm = make_unique<DFSM>();
			fsm2 = make_unique<DFSM>();
			tGenerateSaveLoad();
		}

		TEST_METHOD(TestCreateSaveLoadMealy)
		{
			fsm = make_unique<Mealy>();
			fsm2 = make_unique<Mealy>();
			tCreateSaveLoad();
		}

		TEST_METHOD(TestGenereateSaveLoadMealy)
		{
			fsm = make_unique<Mealy>();
			fsm2 = make_unique<Mealy>();
			tGenerateSaveLoad();
		}

		TEST_METHOD(TestCreateSaveLoadMoore)
		{
			fsm = make_unique<Moore>();
			fsm2 = make_unique<Moore>();
			tCreateSaveLoad();
		}

		TEST_METHOD(TestGenerateSaveLoadMoore)
		{
			fsm = make_unique<Moore>();
			fsm2 = make_unique<Moore>();
			tGenerateSaveLoad();
		}
		
		TEST_METHOD(TestCreateSaveLoadDFA)
		{
			fsm = make_unique<DFA>();
			fsm2 = make_unique<DFA>();
			tCreateSaveLoad();
		}

		TEST_METHOD(TestGenerateSaveLoadDFA)
		{
			fsm = make_unique<DFA>();
			fsm2 = make_unique<DFA>();
			tGenerateSaveLoad();
		}

		/// includes tests for getNumberOfStates/Inputs/Outputs()
		/// and confirms generate(), removeTransition(), removeState()
		void tGenerateSaveLoad() {
			DEBUG_MSG("Generate: %s", machineTypeNames[fsm->getType()]);
			fsm->generate(0, 0, 0);// = create(1,1,1) is minimum
			/// ERR: <type>::generate - the number of states needs to be greater than 0 (set to 1)
			/// ERR: <type>::generate - the number of inputs needs to be greater than 0 (set to 1)
			/// ERR: <type>::generate - the number of outputs needs to be greater than 0 (set to 1)
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(1), "The number of states is not correct.");
			ARE_EQUAL(fsm->getNumberOfInputs(), input_t(1), "The number of inputs is not correct.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), output_t(1), "The number of outputs is not correct.");
			tSaveLoad();

			fsm->generate(5, 3, 2);
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(5), "The number of states is not correct.");
			ARE_EQUAL(fsm->getNumberOfInputs(), input_t(3), "The number of inputs is not correct.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), output_t(2), "The number of outputs is not correct.");
			tSaveLoad();

			fsm->removeTransition(0, 0);
			fsm->removeTransition(1, 1);
			fsm->removeTransition(2, 2);
			/// 3 transitions * 2 machines = 6 ERRs if fsm->isOutputTransition()
			/// <type>::getOutput - there is no such transition
			fsm->removeState(3);
			/// if fsm->isOutputTransition() && there was a transition to state 3, then 2 ERRs:
			/// <type>::getOutput - there is no such transition
			tSaveLoad();
		}

		/// includes tests for getNumberOfStates/Inputs/Outputs()
		/// and confirms create(), removeTransition()
		void tCreateSaveLoad() {
			DEBUG_MSG("Create: %s", machineTypeNames[fsm->getType()]);
			fsm->create(0, 0, 0);// = create(1,0,0) is minimum
			/// ERR: <type>::create - the number of states needs to be greater than 0 (set to 1)
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(1), "The number of states is not correct.");
			ARE_EQUAL(fsm->getNumberOfInputs(), input_t(0), "The number of inputs is not correct.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), output_t(0), "The number of outputs is not correct.");
			tSaveLoad();

			fsm->create(5, 3, 2);
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(5), "The number of states is not correct.");
			ARE_EQUAL(fsm->getNumberOfInputs(), input_t(3), "The number of inputs is not correct.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), output_t(2), "The number of outputs is not correct.");
			/// 5 states * 3 inputs * 2 machines = 30 ERRs if fsm->isOutputTransition()
			/// <type>::getOutput - there is no such transition
			tSaveLoad();

			fsm->setTransition(0, 0, 0);
			fsm->setTransition(0, 1, 1);
			fsm->setTransition(0, 2, 2);
			fsm->setTransition(1, 0, 3);
			fsm->setTransition(2, 1, 4);
			fsm->setTransition(3, 2, 0);
			fsm->setTransition(4, 0, 2);
			/// (5 states * 3 inputs - 7 transitions) * 2 machines = 16 ERRs if fsm->isOutputTransition()
			/// <type>::getOutput - there is no such transition
			tSaveLoad();
		}

		/// includes tests for getNumberOfStates/Inputs/Outputs(), getType(),
		///		isReduced(), isOutputState/Transition(), getStates(), addState(), removeState()
		///		save(), load()
		/// confirms getOutput(), getNextState()
		void tSaveLoad() {
			string path = DATA_PATH;
			path += "tmp/";
			string filename = fsm->save(path);
			ARE_EQUAL(!filename.empty(), true, "This %s cannot be saved into path '%s'.",
				machineTypeNames[fsm->getType()], path.c_str());
			ARE_EQUAL(fsm2->load(filename), true, "File '%s' cannot be loaded.", filename.c_str());
			DEBUG_MSG(filename.c_str());
			
			ARE_EQUAL(fsm->getType(), fsm2->getType(), "Types of machines are not equal.");
			ARE_EQUAL(fsm->getNumberOfStates(), fsm2->getNumberOfStates(), "The numbers of states are not equal.");
			ARE_EQUAL(fsm->getNumberOfInputs(), fsm2->getNumberOfInputs(), "The numbers of inputs are not equal.");
			ARE_EQUAL(fsm->getNumberOfOutputs(), fsm2->getNumberOfOutputs(), "The numbers of outputs are not equal.");
			ARE_EQUAL(fsm->isReduced(), fsm2->isReduced(), "The indicators of minimal form are not equal.");
			ARE_EQUAL(fsm->isOutputState(), fsm2->isOutputState(), "The indicators of state outputs are not equal.");
			ARE_EQUAL(fsm->isOutputTransition(), fsm2->isOutputTransition(), "The indicators of transition outputs are not equal.");
			
			auto states = fsm->getStates();
			ARE_EQUAL(fsm->getNumberOfStates(), state_t(states.size()), "The numbers of states are not equal.");
			auto states2 = fsm2->getStates();
			ARE_EQUAL(states == states2, true, "The sets of states are not equal.");
			ARE_EQUAL((find(states.begin(), states.end(), 0) != states.end()), true, "The initial state is missing.");

			for (state_t& state : states) {
				if (fsm->isOutputState()) {
					ARE_EQUAL(fsm->getOutput(state, STOUT_INPUT), fsm2->getOutput(state, STOUT_INPUT), 
						"The outputs of state %d are different.", state);
				}
				for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
					state_t nextState = fsm->getNextState(state, i);
					//DEBUG_MSG("%d", nextState);
					if (fsm->isOutputTransition()) {
						output_t output = fsm->getOutput(state, i);
						ARE_EQUAL(output, fsm2->getOutput(state, i),
							"The outputs on input %d from state %d are different.", i, state);
						if (nextState == NULL_STATE) {
							ARE_EQUAL(WRONG_OUTPUT, output,
								"There is no transition on input %d from state %d but output %d is set.",
								i, state, output);
						}
					}
					ARE_EQUAL(nextState, fsm2->getNextState(state, i),
						"The next states on input %d from state %d are different.", i, state);
					ARE_EQUAL(nextState != WRONG_STATE, true, 
						"The next state on input %d from state %d is wrong.", i, state);
					if (nextState != NULL_STATE) {
						ARE_EQUAL((find(states.begin(), states.end(), nextState) != states.end()),
							true, "The next state is wrong.");
					}
				}
			}
			auto newState = fsm->addState();
			//DEBUG_MSG("%d", newState);
			ARE_EQUAL(fsm->getNumberOfStates(), fsm2->getNumberOfStates() + 1, "The numbers of states are to be different by one.");
			ARE_EQUAL(newState, fsm2->addState(), "IDs of new states are not equal.");
			ARE_EQUAL(fsm->getNumberOfStates(), fsm2->getNumberOfStates(), "The numbers of states are not equal.");
			fsm->removeState(newState);
			ARE_EQUAL(fsm->getNumberOfStates(), fsm2->getNumberOfStates() - 1, "The numbers of states are to be different by one.");
		}
	};
}