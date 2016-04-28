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

		ss_node_t(block_t states, sequence_in_t ss) {
			this->collapsedStates = states;
			this->ss = ss;
		}
	};

	sequence_in_t getSynchronizingSequence(DFSM * fsm) {
		sequence_in_t outSS;
		queue<ss_node_t*> fifo;
		set<block_t> used;
		ss_node_t* act, *succ;
		block_t states;
		state_t nextState;
		sequence_in_t s;
		states.clear();
		for (state_t i : fsm->getStates()) {
			states.insert(i);
		}
		act = new ss_node_t(states, s);
		fifo.push(act);
		used.insert(states);
		while (!fifo.empty()) {
			act = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				states.clear();
				nextState = 0;
				for (block_t::iterator blockIt = act->collapsedStates.begin();
					blockIt != act->collapsedStates.end(); blockIt++) {
					nextState = fsm->getNextState(*blockIt, input);
					// nextState is not WRONG_STATE because *blockIt and input are valid
					if (nextState == NULL_STATE) {// all states need to have transition on input
						break;
					}
					states.insert(nextState);
				}
				if (nextState == NULL_STATE) {// all states need to have transition on input
					continue;
				}
				// SS found
				if (states.size() == 1) {
					outSS = act->ss;
					outSS.push_back(input);
					while (!fifo.empty()) {
						delete fifo.front();
						fifo.pop();
					}
					return outSS;
				}
				if (used.find(states) == used.end()) {
					s = act->ss;
					s.push_back(input);
					succ = new ss_node_t(states, s);
					fifo.push(succ);
					used.insert(states);
				}
			}
			delete act;
		}
		return outSS;
	}
}