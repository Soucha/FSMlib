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

#include "FSMsequence.h"

namespace FSMsequence {
	typedef set<state_t> block_t;

	struct ss_node_t {
		block_t collapsedStates;
		sequence_in_t ss;

		ss_node_t(block_t states, sequence_in_t ss) :
			collapsedStates(states), ss(ss) {
		}
	};

	sequence_in_t getSynchronizingSequence(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getSynchronizingSequence", sequence_in_t());
		sequence_in_t outSS;
		queue<unique_ptr<ss_node_t>> fifo;
		set<block_t> used;
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		block_t states;
		for (state_t state = 0; state < fsm->getNumberOfStates(); state++) {
			states.emplace(state);
		}
		fifo.emplace(make_unique<ss_node_t>(states, sequence_in_t()));
		used.emplace(move(states));
		while (!fifo.empty()) {
			auto act = move(fifo.front());
			fifo.pop();
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				block_t states;
				for (const auto& state : act->collapsedStates) {
					auto nextState = fsm->getNextState(state, input);
					// nextState is not WRONG_STATE because *blockIt and input are valid
					if (nextState == NULL_STATE) {// all states need to have transition on input
						states.clear();
						break;
					}
					states.emplace(nextState);
				}
				if (states.empty()) {// all states need to have transition on input
					continue;
				}
				// SS found
				if (states.size() == 1) {
					outSS.swap(act->ss);
					outSS.push_back(input);
					if (useStout) outSS.push_back(STOUT_INPUT);
					return outSS;
				}
				if (used.find(states) == used.end()) {
					sequence_in_t s(act->ss);
					s.push_back(input);
					if (useStout) s.push_back(STOUT_INPUT);
					fifo.emplace(make_unique<ss_node_t>(states, s));
					used.emplace(move(states));
				}
			}
		}
		return outSS;
	}
}