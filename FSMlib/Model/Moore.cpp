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

#include "Moore.h"

bool Moore::setOutput(state_t state, output_t output, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::setOutput - bad state (%d)", machineTypeNames[_type], state);
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setOutput - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], output);
		return false;
	}
	if (input != STOUT_INPUT) {
		ERROR_MESSAGE("%s::setOutput - bad input (%d) (only STOUT_INPUT allowed)", machineTypeNames[_type], input);
		return false;
	}
	_outputState[state] = output;
	_isReduced = false;
	return true;	
}

bool Moore::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if ((input == STOUT_INPUT) || (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setTransition - use setOutput() to set an output instead", machineTypeNames[_type]);
		return false;
	}
	if ((from >= _usedStateIDs.size()) || (!_usedStateIDs[from])) {
		ERROR_MESSAGE("%s::setTransition - bad state From (%d)", machineTypeNames[_type], from);
		return false;
	}
	if (input >= _numberOfInputs) {
		ERROR_MESSAGE("%s::setTransition - bad input (%d)", machineTypeNames[_type], input);
		return false;
	}
	if ((to >= _usedStateIDs.size()) || (!_usedStateIDs[to])) {
		ERROR_MESSAGE("%s::setTransition - bad state To (%d)", machineTypeNames[_type], to);
		return false;
	}
	_transition[from][input] = to;
	_isReduced = false;
	return true;
}

output_t Moore::getMaxOutputs(state_t numberOfStates, input_t numberOfInputs) {
	return numberOfStates;
}


