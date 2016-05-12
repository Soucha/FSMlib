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

using namespace FSMtesting;

namespace FSMlibTest
{
	TEST_CLASS(BB)
	{
	public:
		unique_ptr<BlackBox> bb;

		TEST_METHOD(TestBlackBoxDFSM)
		{
			auto fsm = make_unique<DFSM>();
			create(fsm);
			bb = make_unique<BlackBoxDFSM>(FSMmodel::duplicateFSM(fsm), true);
			testBB(true);
			bb->reset();
			testBB_DFSM(true);
		}

		void testBB(bool resettable) {
			ARE_EQUAL(seq_len_t(0), bb->getAppliedResetCount(), "The initial value of reset counter is not 0.");
			ARE_EQUAL(seq_len_t(0), bb->getQueriedSymbolsCount(), "The initial value of query symbol counter is not 0.");
			ARE_EQUAL(resettable, bb->isResettable(), "Mismatch of reset availability");

			bb->reset();// an error message if not resettable
			ARE_EQUAL(seq_len_t(resettable), bb->getAppliedResetCount(), "Reset counter does not match");
			auto out = bb->query(STOUT_INPUT);
			ARE_EQUAL(seq_len_t(1), bb->getQueriedSymbolsCount(), "Query symbol counter does not match");

			auto outSeq = bb->query({ 0, STOUT_INPUT, 1, 0, STOUT_INPUT, 2, 2 });
			ARE_EQUAL(seq_len_t(1 + 7), bb->getQueriedSymbolsCount(), "Query symbol counter does not match");

			outSeq = bb->query({ STOUT_INPUT, 0, STOUT_INPUT, 2, 1 });
			ARE_EQUAL(seq_len_t(8 + 5), bb->getQueriedSymbolsCount(), "Query symbol counter does not match");

			outSeq = bb->resetAndQuery({ 2, 1, 0 });
			ARE_EQUAL(seq_len_t(resettable ? 2 : 0), bb->getAppliedResetCount(), "Reset counter does not match");
			ARE_EQUAL(seq_len_t(13 + 3), bb->getQueriedSymbolsCount(), "Query symbol counter does not match");

			out = bb->resetAndQuery(STOUT_INPUT);
			ARE_EQUAL(seq_len_t(resettable ? 3 : 0), bb->getAppliedResetCount(), "Reset counter does not match");
			ARE_EQUAL(seq_len_t(16 + 1), bb->getQueriedSymbolsCount(), "Query symbol counter does not match");
		}

		void testBB_DFSM(bool resettable) {
			auto out = bb->query(STOUT_INPUT);
			ARE_EQUAL(output_t(0), out, "Received output does not match");
			auto outSeq = bb->query({0, STOUT_INPUT, 1, 0, STOUT_INPUT, 2, 2});
			sequence_out_t expOutSeq({1, 0, 0, 1, DEFAULT_OUTPUT, 1, DEFAULT_OUTPUT});
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 2
			outSeq = bb->query({ STOUT_INPUT, 0, STOUT_INPUT, 2, 1 });
			expOutSeq = {0, WRONG_OUTPUT, 0, WRONG_OUTPUT, 0};
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 4
			outSeq = bb->resetAndQuery({ 2, 1, 0 });
			if (resettable) expOutSeq = { DEFAULT_OUTPUT, 0, 0 };
			else expOutSeq = {WRONG_OUTPUT, WRONG_OUTPUT, 0};
			ARE_EQUAL(expOutSeq, outSeq, "Received output does not match");
			// current state is 2
			out = bb->resetAndQuery(STOUT_INPUT);
			ARE_EQUAL(output_t(0), out, "Received output does not match");
			out = bb->query(0);
			ARE_EQUAL(output_t(resettable ? 1 : WRONG_OUTPUT), out, "Received output does not match");
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
