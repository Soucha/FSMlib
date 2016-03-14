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
#include "../../FSMlib/FSMlib.h"
#include <iostream>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

#define CHECK_CERR() \
	if (int(strCout.tellp()) > 0) {\
		Logger::WriteMessage(ToString(strCout.str()).c_str());\
		strCout.str("");\
	}

namespace FSMlibTest
{
	streambuf * backup = cerr.rdbuf();
	ostringstream strCout;
	

	TEST_MODULE_INITIALIZE(ModuleInitialize)
	{
		cerr.rdbuf(strCout.rdbuf());
	}

	TEST_MODULE_CLEANUP(ModuleCleanup)
	{
		cerr.rdbuf(backup);
	}


	TEST_CLASS(IOtests)
	{
	public:
		DFSM * fsm, * fsm2;
		wchar_t message[200];


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
			Logger::WriteMessage(machineTypeNames[fsm->getType()]);
			fsm->create(0, 0, 0);// = create(1,1,1) is minimum
			CHECK_CERR()
			int n = fsm->getNumberOfStates();
			Assert::AreEqual(n, 1, L"The number of states is not correct.", LINE_INFO());
			tSaveLoad();

			fsm->create(5, 3, 2);
			CHECK_CERR()
			n = fsm->getNumberOfStates();
			Assert::AreEqual(n, 5, L"The number of states is not correct.", LINE_INFO());
			int p = fsm->getNumberOfInputs();
			Assert::AreEqual(p, 3, L"The number of inputs is not correct.", LINE_INFO());
			int q = fsm->getNumberOfOutputs();
			Assert::AreEqual(q, 2, L"The number of outputs is not correct.", LINE_INFO());
			tSaveLoad();
		}

		void tSaveLoad() {
			string path = "../../data/tmp/";
			string filename = fsm->save(path);
			CHECK_CERR()
			_swprintf(message, L"This %s cannot be saved into path '%s'.", 
				ToString(machineTypeNames[fsm->getType()]).c_str(), ToString(path).c_str());
			Assert::IsTrue(!filename.empty(), message, LINE_INFO());
			_swprintf(message, L"File '%s' cannot be loaded.", ToString(filename).c_str());
			Assert::IsTrue(fsm2->load(filename), message, LINE_INFO());
			CHECK_CERR()
			Logger::WriteMessage(filename.c_str());
			

			//Assert::IsTrue(FSMmodel::areIsomorphic(fsm, fsm2), L"Save FSM and load FSM are not same.", LINE_INFO());
			Assert::AreEqual(fsm->getNumberOfStates(), fsm2->getNumberOfStates(), 
				L"The numbers of states are not equal.", LINE_INFO());
			Assert::AreEqual(fsm->getNumberOfInputs(), fsm2->getNumberOfInputs(),
				L"The numbers of inputs are not equal.", LINE_INFO());
			Assert::AreEqual(fsm->getNumberOfOutputs(), fsm2->getNumberOfOutputs(),
				L"The numbers of outputs are not equal.", LINE_INFO());

			if (fsm->getNumberOfStates() == 0) return;

			vector<state_t> states;
			state_t stop = 0;
			states.push_back(0);
			while (stop < states.size()) {
				if (fsm->isOutputState()) {
					_swprintf(message, L"The outputs of state %d are different.", states[stop]);
					Assert::AreEqual(fsm->getOutput(states[stop], STOUT_INPUT), fsm2->getOutput(states[stop], STOUT_INPUT),
						message, LINE_INFO());
					CHECK_CERR()
				}
				for (input_t i = 0; i < fsm->getNumberOfInputs(); i++) {
					if (fsm->isOutputTransition()) {
						_swprintf(message, L"The outputs on input %d from state %d are different.", i, states[stop]);
						Assert::AreEqual(fsm->getOutput(states[stop], i), fsm2->getOutput(states[stop], i),
							message, LINE_INFO());
						CHECK_CERR()
					}
					state_t nextState = fsm->getNextState(states[stop], i);
					CHECK_CERR()
					_swprintf(message, L"%d", nextState);
					//L
					Logger::WriteMessage(message);
					_swprintf(message, L"The next states on input %d from state %d are different.", i, states[stop]);
					Assert::AreEqual(nextState, fsm2->getNextState(states[stop], i),
						message, LINE_INFO());
					CHECK_CERR()
					_swprintf(message, L"The next state on input %d from state %d is wrong.", i, states[stop]);
					Assert::IsTrue(nextState != WRONG_STATE,
						message, LINE_INFO());
					if ((nextState != NULL_STATE) && (find(states.begin(), states.end(), nextState) == states.end())) {
						states.push_back(nextState);
					}
				}
				stop++;
			}
			if (fsm->isReduced()) {
				Assert::AreEqual(fsm->getNumberOfStates(), state_t(states.size()),
					L"The numbers of states are not equal.", LINE_INFO());
			}
		}
	};
}