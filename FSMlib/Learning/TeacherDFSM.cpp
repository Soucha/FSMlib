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

#include "TeacherDFSM.h"

bool TeacherDFSM::isBlackBoxResettable() {
	return _isResettable;
}

void TeacherDFSM::resetBlackBox() {
	if (!_isResettable) {
		ERROR_MESSAGE("TeacherDFSM::reset - cannot be reset!");
		return;
	}
	_resetCounter++;
	_currState = 0;
}

output_t TeacherDFSM::outputQuery(input_t input) {
	_outputQueryCounter++;
	_querySymbolCounter++;
	auto ns = _fsm->getNextState(_currState, input);
	auto output = WRONG_OUTPUT; 
	if ((ns != NULL_STATE) && (ns != WRONG_STATE)) {
		output = _fsm->getOutput(_currState, input);
		_currState = ns;
	}
	return output;
}

sequence_out_t TeacherDFSM::outputQuery(sequence_in_t inputSequence) {
	_outputQueryCounter++;
	if (inputSequence.empty()) return sequence_out_t();
	sequence_out_t output;
	for (const auto& input : inputSequence) {
		output.emplace_back(outputQuery(input));
		_outputQueryCounter--;
	}
	return output;
}

output_t TeacherDFSM::resetAndOutputQuery(input_t input) {
	resetBlackBox();
	return outputQuery(input);
}

sequence_out_t TeacherDFSM::resetAndOutputQuery(sequence_in_t inputSequence) {
	resetBlackBox();
	return outputQuery(inputSequence);
}

sequence_in_t TeacherDFSM::equivalenceQuery(const unique_ptr<DFSM>& model) {
	_equivalenceQueryCounter++;
	
	vector<set<state_t>> stateEq(_fsm->getNumberOfStates());
	queue<pair<pair<state_t,state_t>,sequence_in_t>> fifo;
	fifo.emplace(make_pair(0, 0), sequence_in_t());
	while (!fifo.empty()) {
		auto p = move(fifo.front());
		fifo.pop();
		auto & bbState = p.first.first;
		auto & state = p.first.second;
		if (!stateEq[bbState].insert(state).second) continue;
		if (_fsm->isOutputState() &&
			(_fsm->getOutput(bbState, STOUT_INPUT) != model->getOutput(state, STOUT_INPUT))) {
			p.second.push_back(STOUT_INPUT);
			return p.second;
		}
		for (input_t input = 0; input < _fsm->getNumberOfInputs(); input++) {
			auto nextBbState = _fsm->getNextState(bbState, input);
			auto nextState = model->getNextState(state, input);
			if ((nextBbState == WRONG_STATE) || (nextState == WRONG_STATE)) {
				p.second.push_back(input);
				return p.second;
			}
			else if ((nextBbState == NULL_STATE) || (nextState == NULL_STATE)) {
				if (nextBbState != nextState) {
					p.second.push_back(input);
					return p.second;
				}
			}
			else if (_fsm->isOutputTransition() &&
				(_fsm->getOutput(bbState, input) != model->getOutput(state, input))) {
				p.second.push_back(input);
				return p.second;
			}
			else if (!stateEq[nextBbState].count(nextState)) {
				sequence_in_t s(p.second);
				s.push_back(input);
				fifo.emplace(make_pair(move(nextBbState), move(nextState)), move(s));
			}
		}
	}
	return sequence_in_t();
}