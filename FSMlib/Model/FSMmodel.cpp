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

#include <sstream>
#include "FSMmodel.h"

#define SEQUENCE_SEPARATOR       ","

namespace FSMmodel {
	bool areIsomorphic(DFSM* fsm1, DFSM* fsm2) {
		if (!fsm1->isReduced()) {
			fsm1->minimize();
		}
		if (!fsm2->isReduced()) {
			fsm2->minimize();
		}
		if ((fsm1->getNumberOfStates() != fsm2->getNumberOfStates()) ||
			(fsm1->getNumberOfInputs() != fsm2->getNumberOfInputs()) ||
			(fsm1->getNumberOfOutputs() != fsm2->getNumberOfOutputs()) ||
			(fsm1->getType() != fsm2->getType()) ||
			(fsm1->isOutputState() != fsm2->isOutputState()) ||
			(fsm1->isOutputTransition() != fsm2->isOutputTransition())) {
			return false;
		}
		state_t state, nextState1, nextState2;
		vector<state_t> stateEq(fsm1->getNumberOfStates());
		vector<bool> used(fsm1->getNumberOfStates(), false);
		queue<state_t> fifo;

		stateEq[0] = 0;
		used[0] = true;
		fifo.push(0);
		while (!fifo.empty()) {
			state = fifo.front();
			fifo.pop();
			if (fsm1->isOutputState() && 
				(fsm1->getOutput(state, STOUT_INPUT) != fsm2->getOutput(stateEq[state], STOUT_INPUT))) {
				return false;
			}
			for (input_t input = 0; input < fsm1->getNumberOfInputs(); input++) {
				if (fsm1->isOutputTransition() && 
					(fsm1->getOutput(state, input) != fsm2->getOutput(stateEq[state], input))) {
					return false;
				}
				nextState1 = fsm1->getNextState(state, input);
				nextState2 = fsm2->getNextState(stateEq[state], input);
				if ((nextState1 == WRONG_STATE) || (nextState2 == WRONG_STATE)) {
					return false;
				} else if ((nextState1 == NULL_STATE) || (nextState2 == NULL_STATE)) {
					if (nextState1 != nextState2) return false;
				} else if (used[nextState1]) {
					if (stateEq[nextState1] != nextState2) {
						return false;
					}
				} else {
					stateEq[nextState1] = nextState2;
					used[nextState1] = true;
					fifo.push(nextState1);
				}
			}
		}
		return true;
	}

	
	bool isStronglyConnected(DFSM * dfsm) {
		return true;
	}

	string getInSequenceAsString(sequence_in_t sequence) {
		if (sequence.empty()) return "EMPTY";
		string s = ((sequence.front() == STOUT_INPUT) ? STOUT_SYMBOL : 
			((sequence.front() == EPSILON_INPUT) ? EPSILON_SYMBOL : FSMlib::Utils::toString(sequence.front())));
		sequence_in_t::iterator inputIt = sequence.begin();
		for (++inputIt; inputIt != sequence.end(); inputIt++) {
			s += SEQUENCE_SEPARATOR + ((*inputIt == STOUT_INPUT) ? STOUT_SYMBOL :
				((*inputIt == EPSILON_INPUT) ? EPSILON_SYMBOL : FSMlib::Utils::toString(*inputIt)));
		}
		return s;
	}

	string getOutSequenceAsString(sequence_out_t sequence) {
		if (sequence.empty()) return "EMPTY";
		string s = ((sequence.front() == DEFAULT_OUTPUT) ? DEFAULT_OUTPUT_SYMBOL :
			((sequence.front() == WRONG_OUTPUT) ? WRONG_OUTPUT_SYMBOL : FSMlib::Utils::toString(sequence.front())));
		sequence_out_t::iterator outputIt = sequence.begin();
		for (++outputIt; outputIt != sequence.end(); outputIt++) {
			s += SEQUENCE_SEPARATOR + ((*outputIt == DEFAULT_OUTPUT) ? DEFAULT_OUTPUT_SYMBOL :
				((*outputIt == WRONG_OUTPUT) ? WRONG_OUTPUT_SYMBOL : FSMlib::Utils::toString(*outputIt)));
		}
		return s;
	}
}