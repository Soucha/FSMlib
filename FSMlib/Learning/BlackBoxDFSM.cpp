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

#include "BlackBoxDFSM.h"

void BlackBoxDFSM::reset() {
	if (!_isResettable) {
		ERROR_MESSAGE("BlackBoxDFSM::reset - cannot be reset!");
		return;
	}
	_resetCounter++;
	_currState = 0;
}

output_t BlackBoxDFSM::query(input_t input) {
	_querySymbolCounter++;
	auto output = _fsm->getOutput(_currState, input);
	if (output != WRONG_OUTPUT) _currState = _fsm->getNextState(_currState, input);
	return output;
}

sequence_out_t BlackBoxDFSM::query(sequence_in_t inputSequence) {
	if (inputSequence.empty()) return sequence_out_t();
	auto output = _fsm->getOutputAlongPath(_currState, inputSequence);
	if (output.empty() || (output.front() == WRONG_OUTPUT)) {
		output.clear();
		for (const auto& input : inputSequence) {
			output.emplace_back(query(input));
		}
	}
	else {
		_querySymbolCounter += seq_len_t(inputSequence.size());
		_currState = _fsm->getEndPathState(_currState, inputSequence);
	}
	return output;
}

output_t BlackBoxDFSM::resetAndQuery(input_t input) {
	reset();
	return query(input);
}

sequence_out_t BlackBoxDFSM::resetAndQuery(sequence_in_t inputSequence) {
	reset();
	return query(inputSequence);
}
