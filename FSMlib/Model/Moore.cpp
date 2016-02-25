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

#include <fstream>
#include <iostream>
#include <time.h>

#include "Moore.h"

output_t Moore::getOutput(state_t state, input_t input) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		throw "Moore::getOutput - bad state id";
	}
	if (input == STOUT_INPUT) {
		return _outputState[num_states_t(state)];
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		throw "Moore::getOutput - bad input";
	}
	num_states_t nextState = num_states_t(_transition[num_states_t(state)][num_inputs_t(input)]);
	if ((state_t(nextState) == NULL_STATE) || (nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState])) {
		throw "Moore::getOutput - there is no such transition";
	}
	return _outputState[nextState];
}

void Moore::create(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs) {
	if (numberOfOutputs > numberOfStates) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of " << numberOfStates << endl;
		numberOfOutputs = numberOfStates;
	}
	if (numberOfInputs == 0) {
		cerr << typeNames[_type] << "::create - the number of inputs needs to be greater than 0 (set to 1)" << endl;
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::create - the number of outputs needs to be greater than 0 (set to 1)" << endl;
		numberOfOutputs = 1;
	}

	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_MOORE;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	clearTransitions();
	clearStateOutputs();
}

void Moore::generate(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs) {
	if (numberOfInputs == 0) {
		cerr << typeNames[_type] << "::generate - the number of inputs needs to be greater than 0 (set to 1)" << endl;
		numberOfInputs = 1;
	}
	if (numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::generate - the number of outputs needs to be greater than 0 (set to 1)" << endl;
		numberOfOutputs = 1;
	}
	if (numberOfStates == 0) {
		cerr << typeNames[_type] << "::generate - the number of states needs to be greater than 0 (set to 1)" << endl;
		numberOfStates = 1;
	}
	if (numberOfOutputs > numberOfStates) {
		cerr << typeNames[_type] << "::generate - the number of outputs reduced to maximum of " << numberOfStates << endl;
		numberOfOutputs = numberOfStates;
	}
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_MOORE;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	srand(time(NULL));

	generateTransitions();
	generateStateOutputs(_numberOfOutputs); 
}

bool Moore::load(string fileName) {
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::load - unable to open file" << endl;
		return false;
	}
	file >> _type >> _isReduced >> _numberOfStates >> _numberOfInputs >> _numberOfOutputs;
	if (_type != TYPE_MOORE) {
		cerr << typeNames[_type] << "::load - bad type of FSM" << endl;
		file.close();
		return false;
	}
	if (_numberOfInputs == 0) {
		cerr << typeNames[_type] << "::load - the number of inputs needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfOutputs == 0) {
		cerr << typeNames[_type] << "::load - the number of outputs needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfStates == 0) {
		cerr << typeNames[_type] << "::load - the number of states needs to be greater than 0" << endl;
		file.close();
		return false;
	}
	if (_numberOfOutputs > _numberOfStates) {
		cerr << typeNames[_type] << "::load - the number of outputs cannot be greater than the maximum value of "
			<< _numberOfStates << endl;
		file.close();
		return false;
	}
	if (!loadStateOutputs(file) || !loadTransitions(file)) {
		file.close();
		return false;
	}
	file.close();
	return true;
}

string Moore::save(string path) {
	string filename = getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::save - unable to open file" << endl;
		return "";
	}
	saveInfo(file);
	saveStateOutputs(file);
	saveTransitions(file);
	file.close();
	return filename;
}

string Moore::writeDOTfile(string path) {
	string filename("DOT");
	filename += getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::writeDOTfile - unable to open file" << endl;
		return "";
	}
	writeDotStart(file);
	writeDotStates(file, true);
	writeDotTransitions(file, false);
	writeDotEnd(file);
	file.close();
	return filename;
}

bool Moore::setOutput(state_t state, output_t output, input_t input) {
	if ((num_states_t(state) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(state)])) {
		cerr << typeNames[_type] << "::setOutput - bad state" << endl;
		return false;
	}
	if ((num_outputs_t(output) >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setOutput - bad output (increase the number of outputs first)" << endl;
		return false;
	}
	if (input == STOUT_INPUT) {
		_outputState[num_states_t(state)] = output;
		return true;
	}
	cerr << typeNames[_type] << "::setOutput - bad input (only STOUT_INPUT allowed)" << endl;
	return false;
}

bool Moore::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if ((input == STOUT_INPUT) || (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setTransition - use setOutput to set an output instead" << endl;
		return false;
	}
	if ((num_states_t(from) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(from)])) {
		cerr << typeNames[_type] << "::setTransition - bad state From" << endl;
		return false;
	}
	if (num_inputs_t(input) >= _numberOfInputs) {
		cerr << typeNames[_type] << "::setTransition - bad input" << endl;
		return false;
	}
	if ((num_states_t(to) >= _usedStateIDs.size()) || (!_usedStateIDs[num_states_t(to)])) {
		cerr << typeNames[_type] << "::setTransition - bad state To" << endl;
		return false;
	}
	_transition[num_states_t(from)][num_inputs_t(input)] = to;
	return true;
}

void Moore::setEquivalence(queue<vector<state_t> >& equivalentStates) {
	state_t state;
	vector<state_t> actBlock, removedStates, stateEquiv(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		stateEquiv[state] = state;
	}
	while (!equivalentStates.empty()) {
		actBlock = equivalentStates.front();
		equivalentStates.pop();
		for (state = 1; state < actBlock.size(); state++) {
			stateEquiv[actBlock[state]] = actBlock[0];
			removedStates.push_back(actBlock[state]);
		}
	}
	sort(removedStates.begin(), removedStates.end());
	state = _numberOfStates - 1;
	for (state_t i = 0; i < removedStates.size(); i++) {
		while (stateEquiv[state] != state) state--;
		if (state <= removedStates[i]) break;
		_transition[removedStates[i]] = _transition[state];
		_outputState[removedStates[i]] = _outputState[state];
		stateEquiv[state] = removedStates[i];
	}
	_numberOfStates -= removedStates.size();
	_transition.resize(_numberOfStates);
	_outputState.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		for (input_t input = 0; input < _numberOfInputs; input++) {
			_transition[state][input] = stateEquiv[_transition[state][input]];
		}
	}
}

void Moore::removeUnreachableStates(vector<state_t>& unreachableStates) {
	state_t state;
	vector<state_t> stateEquiv(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		stateEquiv[state] = state;
	}
	for (state = 0; state < unreachableStates.size(); state++) {
		stateEquiv[unreachableStates[state]] = 0;
	}
	state = _numberOfStates - 1;
	for (state_t i = 0; i < unreachableStates.size(); i++) {
		while (stateEquiv[state] != state) state--;
		if (state <= unreachableStates[i]) break;
		_transition[unreachableStates[i]] = _transition[state];
		_outputState[unreachableStates[i]] = _outputState[state];
		stateEquiv[state] = unreachableStates[i];
	}
	_numberOfStates -= unreachableStates.size();
	_transition.resize(_numberOfStates);
	_outputState.resize(_numberOfStates);
	for (state = 0; state < _numberOfStates; state++) {
		for (input_t input = 0; input < _numberOfInputs; input++) {
			_transition[state][input] = stateEquiv[_transition[state][input]];
		}
	}
}

