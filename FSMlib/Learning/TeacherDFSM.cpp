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
	auto output = _fsm->getOutput(_currState, input);
	if (output != WRONG_OUTPUT) _currState = _fsm->getNextState(_currState, input);
	return output;
}

sequence_out_t TeacherDFSM::outputQuery(sequence_in_t inputSequence) {
	_outputQueryCounter++;
	if (inputSequence.empty()) return sequence_out_t();
	auto output = _fsm->getOutputAlongPath(_currState, inputSequence);
	if (output.empty() || (output.front() == WRONG_OUTPUT)) {
		output.clear();
		for (const auto& input : inputSequence) {
			output.emplace_back(outputQuery(input));
			_outputQueryCounter--;
		}
	}
	else {
		_querySymbolCounter += seq_len_t(inputSequence.size());
		_currState = _fsm->getEndPathState(_currState, inputSequence);
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

sequence_in_t TeacherDFSM::equivalenceQuery(const unique_ptr<DFSM>& conjecture) {
	_equivalenceQueryCounter++;
	auto model = FSMmodel::duplicateFSM(conjecture);
	if (!model->isReduced()) model->minimize();
	if ((_fsm->getNumberOfStates() == conjecture->getNumberOfStates())
		&& (FSMmodel::areIsomorphic(_fsm, model)))
		return sequence_in_t();

	vector<state_t> stateEq(_fsm->getNumberOfStates(), NULL_STATE);
	vector<bool> used(_fsm->getNumberOfStates(), false);
	queue<pair<state_t,sequence_in_t>> fifo;

	stateEq[0] = 0;
	used[0] = true;
	fifo.emplace(0, sequence_in_t());
	while (!fifo.empty()) {
		auto p = move(fifo.front());
		fifo.pop();
		if (_fsm->isOutputState() &&
			(_fsm->getOutput(p.first, STOUT_INPUT) != model->getOutput(stateEq[p.first], STOUT_INPUT))) {
			p.second.push_back(STOUT_INPUT);
			return p.second;
		}
		for (input_t input = 0; input < _fsm->getNumberOfInputs(); input++) {
			if (_fsm->isOutputTransition() &&
				(_fsm->getOutput(p.first, input) != model->getOutput(stateEq[p.first], input))) {
				p.second.push_back(input);
				return p.second;
			}
			auto nextState1 = _fsm->getNextState(p.first, input);
			auto nextState2 = model->getNextState(stateEq[p.first], input);
			if ((nextState1 == WRONG_STATE) || (nextState2 == WRONG_STATE)) {
				p.second.push_back(input);
				return p.second;
			}
			else if ((nextState1 == NULL_STATE) || (nextState2 == NULL_STATE)) {
				if (nextState1 != nextState2) {
					p.second.push_back(input);
					return p.second;
				}
			}
			else if (used[nextState1]) {
				if (stateEq[nextState1] != nextState2) {
					p.second.push_back(input);
					return p.second;
				}
			}
			else {
				stateEq[nextState1] = nextState2;
				used[nextState1] = true;
				sequence_in_t s(p.second);
				s.push_back(input);
				fifo.emplace(move(nextState1), move(s));
			}
		}
	}
	return sequence_out_t();
}