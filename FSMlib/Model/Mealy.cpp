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

#include "Mealy.h"
/*
output_t Mealy::getOutput(state_t state, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::getOutput - bad state id (%d)", machineTypeNames[_type], state);
		return WRONG_OUTPUT;
	}
	if ((input >= _numberOfInputs) || (input == STOUT_INPUT)) {
		ERROR_MESSAGE("%s::getOutput - bad input (%d)", machineTypeNames[_type], input);
		return WRONG_OUTPUT;
	}
	state_t& nextState = _transition[state][input];
	if ((nextState == NULL_STATE) || (nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState])) {
		ERROR_MESSAGE("%s::getOutput - there is no such transition (%d, %d)", machineTypeNames[_type], state, input); 
		return WRONG_OUTPUT;
	}
	return _outputTransition[state][input];
}
*/
bool Mealy::setOutput(state_t state, output_t output, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		ERROR_MESSAGE("%s::setOutput - bad state (%d)", machineTypeNames[_type], state);
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setOutput - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], output);
		return false;
	}
	if ((input >= _numberOfInputs) || (input == STOUT_INPUT)) {
		ERROR_MESSAGE("%s::setOutput - bad input (%d)", machineTypeNames[_type], input);
		return false;
	}
	if (_transition[state][input] == NULL_STATE) {
		ERROR_MESSAGE("%s::setOutput - there is no such transition (%d, %d)", machineTypeNames[_type], state, input);
		return false;
	}
	_outputTransition[state][input] = output;
	_isReduced = false;
	return true;
}

bool Mealy::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if (input == STOUT_INPUT) {
		ERROR_MESSAGE("%s::setTransition - STOUT_INPUT is not allowed", machineTypeNames[_type]);
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
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		ERROR_MESSAGE("%s::setTransition - bad output (%d) (increase the number of outputs first)", machineTypeNames[_type], output);
		return false;
	}
	_transition[from][input] = to;
	_outputTransition[from][input] = output;
	_isReduced = false;
	return true;
}

output_t Mealy::getMaxOutputs(state_t numberOfStates, input_t numberOfInputs) {
	return (numberOfStates * numberOfInputs);
}
