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

#include "TeacherBB.h"

#define MAX_DEPTH 3

struct bb_node_t {
	output_t incomingOutput;
	output_t stateOutput = WRONG_OUTPUT;
	map<input_t, shared_ptr<bb_node_t>> succ;
	weak_ptr<bb_node_t> parent;
	input_t incomingInput;
};

bool TeacherBB::isBlackBoxResettable() {
	return _bb->isResettable();
}

void TeacherBB::resetBlackBox() {
	_bb->reset();
	if (isBlackBoxResettable()) {
		_resetCounter++;
		_currState = _bbState = _initialState;
	}
}

output_t TeacherBB::outputQuery(input_t input) {
	_outputQueryCounter++;
	if (input == STOUT_INPUT) {
		if (_currState->stateOutput != WRONG_OUTPUT)
			return _currState->stateOutput;
	} else {
		auto it = _currState->succ.find(input);
		if (it != _currState->succ.end()) {
			_currState = it->second;
			return _currState->incomingOutput;
		}
	}
	_querySymbolCounter++;
	if (_bbState != _currState) {
		auto state = _currState;
		sequence_in_t inSeq;
		while ((state != _initialState) && (state != _bbState)) {
			inSeq.push_front(state->incomingInput);
			state = state->parent.lock();
		}
		if (state != _bbState) _bb->reset();
		_bb->query(move(inSeq));
	}
	auto output = _bb->query(input);
	if (input == STOUT_INPUT) {
		_currState->stateOutput = output;
	} else {
		_currState->succ[input] = make_shared<bb_node_t>();
		_currState->succ[input]->parent = _currState;
		_currState->succ[input]->incomingInput = input;
		_currState->succ[input]->incomingOutput = output;
		if (output != WRONG_OUTPUT) _currState = _currState->succ[input];
	}
	_bbState = _currState;
	return output;
}

sequence_out_t TeacherBB::outputQuery(sequence_in_t inputSequence) {
	_outputQueryCounter++;
	if (inputSequence.empty()) return sequence_out_t();
	sequence_out_t output;
	for (const auto& input : inputSequence) {
		output.emplace_back(outputQuery(input));
		_outputQueryCounter--;
	}
	return output;
}

output_t TeacherBB::resetAndOutputQuery(input_t input) {
	resetBlackBox();
	return outputQuery(input);
}

sequence_out_t TeacherBB::resetAndOutputQuery(sequence_in_t inputSequence) {
	resetBlackBox();
	return outputQuery(inputSequence);
}

sequence_in_t TeacherBB::equivalenceQuery(const unique_ptr<DFSM>& conjecture) {
	_equivalenceQueryCounter++;
	auto tmp = _currState;
	for (int extraStates = 0; extraStates < MAX_DEPTH; extraStates++) {
		auto TS = _testingMethod(conjecture, extraStates);
		for (const auto& test : TS) {
			auto bbOut = resetAndOutputQuery(test);
			_outputQueryCounter--;
			auto modelOut = conjecture->getOutputAlongPath(0, test);
			if (modelOut.empty() || (modelOut.front() == WRONG_OUTPUT)) {
				modelOut.clear();
				state_t state = 0;
				for (const auto& input : test) {
					modelOut.emplace_back(conjecture->getOutput(state, input));
					if (modelOut.back() != WRONG_OUTPUT) state = conjecture->getNextState(state, input);
				}
			}
			if (bbOut != modelOut) {
				_currState = tmp;
				return test;
			}
		}
	}
	_currState = tmp;
	return sequence_out_t();
}