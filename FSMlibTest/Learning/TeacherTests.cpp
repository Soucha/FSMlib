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
	TEST_CLASS(TeacherTests)
	{
	public:
		unique_ptr<Teacher> teacher;

		TEST_METHOD(TestTeacherDFSM)
		{
			auto fsm = make_unique<DFSM>();
			create(fsm);
			teacher = make_unique<TeacherDFSM>(fsm, true);
			testTeacher(true);
			teacher->resetBlackBox();
			testTeacherDFSM(true);
			testEQ(fsm);
		}

		TEST_METHOD(TestTeacherRL)
		{
			auto fsm = make_unique<DFSM>();
			create(fsm);
			teacher = make_unique<TeacherRL>(fsm);
			testTeacher(true);
			teacher->resetBlackBox();
			//testTeacherDFSM(true);
			testEQ(fsm);
		}

		TEST_METHOD(TestTeacherBB)
		{
			auto fsm = make_unique<DFSM>();
			create(fsm);
			auto bb = make_shared<BlackBoxDFSM>(fsm, true);
			teacher = make_unique<TeacherBB>(bb, bind(FSMtesting::HSI_method, placeholders::_1, placeholders::_2,
				bind(FSMsequence::getHarmonizedStateIdentifiersFromSplittingTree, placeholders::_1,
				bind(FSMsequence::getSplittingTree, placeholders::_1, true, false), false)));
			testTeacher(true, true);
			teacher->resetBlackBox();
			testTeacherDFSM(true);
			testEQ(fsm);
		}

		void testTeacher(bool resettable, bool cached = false) {
			ARE_EQUAL(seq_len_t(0), teacher->getAppliedResetCount(), "The initial value of reset counter is not 0.");
			ARE_EQUAL(seq_len_t(0), teacher->getQueriedSymbolsCount(), "The initial value of query symbol counter is not 0.");
			ARE_EQUAL(seq_len_t(0), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");
			ARE_EQUAL(seq_len_t(0), teacher->getEquivalenceQueryCount(), "The initial value of equivalence query counter is not 0.");
			ARE_EQUAL(resettable, teacher->isBlackBoxResettable(), "Mismatch of reset availability");

			teacher->resetBlackBox();// an error message if not resettable
			ARE_EQUAL(seq_len_t(resettable), teacher->getAppliedResetCount(), "Reset counter does not match");
			auto out = teacher->outputQuery(STOUT_INPUT);
			ARE_EQUAL(seq_len_t(1), teacher->getQueriedSymbolsCount(), "Query symbol counter does not match");
			ARE_EQUAL(seq_len_t(1), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");

			auto outSeq = teacher->outputQuery({ 0, STOUT_INPUT, 1, 0, STOUT_INPUT, 2, 2 });
			ARE_EQUAL(seq_len_t(1 + 7), teacher->getQueriedSymbolsCount(), "Query symbol counter does not match");
			ARE_EQUAL(seq_len_t(2), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");

			outSeq = teacher->outputQuery({ STOUT_INPUT, 0, STOUT_INPUT, 2, 1 });
			seq_len_t symbolCount = cached ? (8 + 4) : (8 + 5);
			ARE_EQUAL(symbolCount, teacher->getQueriedSymbolsCount(), "Query symbol counter does not match");
			ARE_EQUAL(seq_len_t(3), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");

			outSeq = teacher->resetAndOutputQuery({ 2, 1, 0 });
			symbolCount += 3;
			ARE_EQUAL(seq_len_t(resettable ? 2 : 0), teacher->getAppliedResetCount(), "Reset counter does not match");
			ARE_EQUAL(symbolCount, teacher->getQueriedSymbolsCount(), "Query symbol counter does not match");
			ARE_EQUAL(seq_len_t(4), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");

			out = teacher->resetAndOutputQuery(STOUT_INPUT);
			if (!cached || !resettable) symbolCount++;
			ARE_EQUAL(seq_len_t(resettable ? 3 : 0), teacher->getAppliedResetCount(), "Reset counter does not match");
			ARE_EQUAL(symbolCount, teacher->getQueriedSymbolsCount(), "Query symbol counter does not match");
			ARE_EQUAL(seq_len_t(5), teacher->getOutputQueryCount(), "The initial value of output query counter is not 0.");
		}

		void testTeacherDFSM(bool resettable) {
			auto initAlphabet = teacher->getNextPossibleInputs();
			ARE_EQUAL(teacher->getNumberOfInputs(), input_t(initAlphabet.size()), "The initial state has not transition for each input.");
			for (input_t i = 0; i < initAlphabet.size(); i++) {
				ARE_EQUAL(i, initAlphabet[i], "Provided next inputs do not match");
			}
			
			auto out = teacher->outputQuery(STOUT_INPUT);
			ARE_EQUAL(output_t(0), out, "Received output does not match");
			ARE_EQUAL(true, initAlphabet == teacher->getNextPossibleInputs(), "Different collections of next inputs were obtained for the initial state");
			
			auto outSeq = teacher->outputQuery({ 0, STOUT_INPUT, 1, 0, STOUT_INPUT, 2, 2 });
			sequence_out_t expOutSeq({ 1, 0, 0, 1, DEFAULT_OUTPUT, 1, DEFAULT_OUTPUT });
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 2
			auto alphabet2 = teacher->getNextPossibleInputs();
			ARE_EQUAL(input_t(1), input_t(alphabet2.size()), "State 2 has transition on one input only.");
			ARE_EQUAL(input_t(1), alphabet2[0], "State 2 has transition on input 1 only.");

			outSeq = teacher->outputQuery({ STOUT_INPUT, 0, STOUT_INPUT, 2, 1 });
			expOutSeq = { 0, WRONG_OUTPUT, 0, WRONG_OUTPUT, 0 };
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 4
			auto alphabet4 = teacher->getNextPossibleInputs();
			ARE_EQUAL(input_t(1), input_t(alphabet4.size()), "State 4 has transition on one input only.");
			ARE_EQUAL(input_t(0), alphabet4[0], "State 4 has transition on input 0 only.");

			outSeq = teacher->resetAndOutputQuery({ 2, 1, 0 });
			if (resettable) expOutSeq = { DEFAULT_OUTPUT, 0, 0 };
			else expOutSeq = { WRONG_OUTPUT, WRONG_OUTPUT, 0 };
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 2
			out = teacher->resetAndOutputQuery(STOUT_INPUT);
			ARE_EQUAL(output_t(0), out, "Received output does not match");
			out = teacher->outputQuery(0);
			ARE_EQUAL(output_t(resettable ? 1 : WRONG_OUTPUT), out, "Received output does not match");

			ARE_EQUAL(input_t(3), teacher->getNumberOfInputs(), "The number of inputs cannot change.");
			ARE_EQUAL(output_t(2), teacher->getNumberOfOutputs(), "The number of outputs is not correct.");
		}

		void testEQ(const unique_ptr<DFSM>& model) {
			auto ce = teacher->equivalenceQuery(model);
			ARE_EQUAL(seq_len_t(1), teacher->getEquivalenceQueryCount(), "The number of equivalence queries does not match.");
			ARE_EQUAL(sequence_in_t(), ce, "A counterexample was found for the specification.");

			model->setTransition(4, 0, 3, 0);
			ce = teacher->equivalenceQuery(model);
			ARE_EQUAL(seq_len_t(2), teacher->getEquivalenceQueryCount(), "The number of equivalence queries does not match.");
			ARE_EQUAL(false, ce.empty(), "A counterexample was not found");
			DEBUG_MSG("CE: %s\n", FSMmodel::getInSequenceAsString(ce).c_str());
			teacher->resetBlackBox();
			bool covered = false;
			state_t state = 0;
			output_t output;
			for (const auto& input : ce) {
				auto ns = model->getNextState(state, input);
				ARE_EQUAL(false, ns == WRONG_STATE, "CE contains invalid input %d", input);
				if (ns != NULL_STATE) {
					output = model->getOutput(state, input);
					state = ns;
				}
				else output = WRONG_OUTPUT;
				if (output != teacher->outputQuery(input)) {
					covered = true;
					break;
				}				
			}
			ARE_EQUAL(true, covered, "Counterexample %s does not reveal the difference.", FSMmodel::getInSequenceAsString(ce).c_str());

			model->removeTransition(4, 0);
			ce = teacher->equivalenceQuery(model);
			ARE_EQUAL(seq_len_t(3), teacher->getEquivalenceQueryCount(), "The number of equivalence queries does not match.");
			ARE_EQUAL(false, ce.empty(), "A counterexample was not found");
			DEBUG_MSG("CE: %s\n", FSMmodel::getInSequenceAsString(ce).c_str());
			teacher->resetBlackBox();
			covered = false;
			state = 0;
			for (const auto& input : ce) {
				auto ns = model->getNextState(state, input);
				ARE_EQUAL(false, ns == WRONG_STATE, "CE contains invalid input %d", input);
				if (ns != NULL_STATE) {
					output = model->getOutput(state, input);
					state = ns;
				}
				else output = WRONG_OUTPUT;
				if (output != teacher->outputQuery(input)) {
					covered = true;
					break;
				}
			}
			ARE_EQUAL(true, covered, "Counterexample %s does not reveal the difference.", FSMmodel::getInSequenceAsString(ce).c_str());

			model->removeState(3);
			ce = teacher->equivalenceQuery(model);
			ARE_EQUAL(seq_len_t(4), teacher->getEquivalenceQueryCount(), "The number of equivalence queries does not match.");
			ARE_EQUAL(false, ce.empty(), "A counterexample was not found");
			DEBUG_MSG("CE: %s\n", FSMmodel::getInSequenceAsString(ce).c_str());
			teacher->resetBlackBox();
			covered = false;
			state = 0;
			for (const auto& input : ce) {
				auto ns = model->getNextState(state, input);
				ARE_EQUAL(false, ns == WRONG_STATE, "CE contains invalid input %d", input);
				if (ns != NULL_STATE) {
					output = model->getOutput(state, input);
					state = ns;
				}
				else output = WRONG_OUTPUT;
				if (output != teacher->outputQuery(input)) {
					covered = true;
					break;
				}
			}
			ARE_EQUAL(true, covered, "Counterexample %s does not reveal the difference.", FSMmodel::getInSequenceAsString(ce).c_str());
		}

		void create(const unique_ptr<DFSM>& fsm) {
			fsm->create(5, 3, 2);
			createTransitions(fsm);
			if (fsm->isOutputState()) createStateOutputs(fsm);
			if (fsm->isOutputTransition()) createTransitionOutputs(fsm);
		}

		void createTransitions(const unique_ptr<DFSM>& fsm) {
			fsm->setTransition(0, 0, 0);
			fsm->setTransition(0, 1, 1);
			fsm->setTransition(0, 2, 2);
			fsm->setTransition(1, 0, 3);
			fsm->setTransition(2, 1, 4);
			fsm->setTransition(3, 2, 0);
			fsm->setTransition(4, 0, 2);
		}

		void createStateOutputs(const unique_ptr<DFSM>& fsm) {
			fsm->setOutput(0, 0);
			fsm->setOutput(1, 1);
			fsm->setOutput(2, 0);
			//fsm->setOutput(3, 0);
			fsm->setOutput(4, 1);
		}

		void createTransitionOutputs(const unique_ptr<DFSM>& fsm) {
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
