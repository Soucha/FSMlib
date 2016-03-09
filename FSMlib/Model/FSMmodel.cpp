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

namespace Utils {
	string hashCode(int length) {
		string hash = "";
		char c;
		for (int i = 0; i < length; i++) {
			do {
				c = char((rand() % 75) + 48);
			} while (!isalnum(c));
			hash += c;
		}
		return hash;
	}

	string getUniqueName(string name, string suffix, string path) {
		string newName = path + name + "." + suffix;
		ifstream file(newName.c_str());
		while (file.is_open()) {
			newName = path + name + "_" + hashCode(5) + "." + suffix;
			file.close();
			file.open(newName.c_str());
		}
		return newName;
	}

	string toString(int number) {
		ostringstream str;
		str << number;
		return str.str();
	}
}

namespace FSMmodel {
	bool areIsomorphic(DFSM* fsm1, DFSM* fsm2) {
		if (!fsm1->isReduced()) {
			fsm1->mimimize();
		}
		if (!fsm2->isReduced()) {
			fsm2->mimimize();
		}
		if ((fsm1->getNumberOfStates() != fsm2->getNumberOfStates()) ||
			(fsm1->getNumberOfInputs() != fsm2->getNumberOfInputs()) ||
			(fsm1->getNumberOfOutputs() != fsm2->getNumberOfOutputs()) ||
			(fsm1->getType() != fsm2->getType())) {
			return false;
		}
		num_states_t state;
		state_t nextState1, nextState2;
		vector<state_t> stateEq(fsm1->getNumberOfStates());
		vector<bool> used(fsm1->getNumberOfStates(), false);
		queue<num_states_t> fifo;

		stateEq[0] = 0;
		used[0] = true;
		fifo.push(0);
		while (!fifo.empty()) {
			state = fifo.front();
			fifo.pop();
			if (fsm1->getOutput(state_t(state), STOUT_INPUT) != fsm2->getOutput(stateEq[state], STOUT_INPUT)) {
				return false;
			}
			for (num_inputs_t input = 0; input < fsm1->getNumberOfInputs(); input++) {
				if (fsm1->getOutput(state_t(state), input_t(input)) != fsm2->getOutput(stateEq[state], input_t(input))) {
					return false;
				}
				nextState1 = fsm1->getNextState(state_t(state), input_t(input));
				nextState2 = fsm2->getNextState(stateEq[state], input_t(input));
				if ((nextState1 == WRONG_STATE) || (nextState2 == WRONG_STATE)) {
					return false;
				} else if ((nextState1 == NULL_STATE) || (nextState2 == NULL_STATE)) {
					if (nextState1 != nextState2) return false;
				} else if (used[num_states_t(nextState1)]) {
					if (stateEq[num_states_t(nextState1)] != nextState2) {
						return false;
					}
				} else {
					stateEq[num_states_t(nextState1)] = nextState2;
					used[num_states_t(nextState1)] = true;
					fifo.push(num_states_t(nextState1));
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
			((sequence.front() == EPSILON_INPUT) ? EPSILON_SYMBOL : Utils::toString(num_inputs_t(sequence.front()))));
		sequence_in_t::iterator inputIt = sequence.begin();
		for (++inputIt; inputIt != sequence.end(); inputIt++) {
			s += SEQUENCE_SEPARATOR + ((*inputIt == STOUT_INPUT) ? STOUT_SYMBOL :
				((*inputIt == EPSILON_INPUT) ? EPSILON_SYMBOL : Utils::toString(num_inputs_t(*inputIt))));
		}
		return s;
	}

	string getOutSequenceAsString(sequence_out_t sequence) {
		if (sequence.empty()) return "EMPTY";
		string s = ((sequence.front() == DEFAULT_OUTPUT) ? DEFAULT_OUTPUT_SYMBOL :
			((sequence.front() == WRONG_OUTPUT) ? WRONG_OUTPUT_SYMBOL : Utils::toString(num_outputs_t(sequence.front()))));
		sequence_out_t::iterator outputIt = sequence.begin();
		for (++outputIt; outputIt != sequence.end(); outputIt++) {
			s += SEQUENCE_SEPARATOR + ((*outputIt == DEFAULT_OUTPUT) ? DEFAULT_OUTPUT_SYMBOL :
				((*outputIt == WRONG_OUTPUT) ? WRONG_OUTPUT_SYMBOL : Utils::toString(num_outputs_t(*outputIt))));
		}
		return s;
	}
}