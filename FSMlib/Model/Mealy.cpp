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

#include "Mealy.h"

output_t Mealy::getOutput(state_t state, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		cerr << typeNames[_type] << "::getOutput - bad state id" << endl;
		return WRONG_OUTPUT;
	}
	if ((input >= _numberOfInputs) || (input == STOUT_INPUT)) {
		cerr << typeNames[_type] << "::getOutput - bad input" << endl;
		return WRONG_OUTPUT;
	}
	state_t& nextState = _transition[state][input];
	if ((nextState == NULL_STATE) || (nextState >= _usedStateIDs.size()) || (!_usedStateIDs[nextState])) {
		cerr << typeNames[_type] << "::getOutput - there is no such transition" << endl;
		return WRONG_OUTPUT;
	}
	return _outputTransition[state][input];
}

void Mealy::create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
	if (numberOfOutputs > (numberOfStates * numberOfInputs)) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of "
			<< (numberOfStates * numberOfInputs) << endl;
		numberOfOutputs = (numberOfStates * numberOfInputs);
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
	_type = TYPE_MEALY;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	clearTransitions();
	clearTransitionOutputs();
}

void Mealy::generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
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
	if (numberOfOutputs > (numberOfStates * numberOfInputs)) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of "
			<< (numberOfStates * numberOfInputs) << endl;
		numberOfOutputs = (numberOfStates * numberOfInputs);
	}
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_MEALY;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	srand(time(NULL));

	generateTransitions();
	generateTransitionOutputs(_numberOfOutputs, 0);
}

bool Mealy::load(string fileName) {
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::load - unable to open file" << endl;
		return false;
	}
	state_t maxState;
	file >> _type >> _isReduced >> _numberOfStates >> _numberOfInputs >> _numberOfOutputs >> maxState;
	if (_type != TYPE_MEALY) {
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
	if (_numberOfOutputs > (_numberOfStates * _numberOfInputs)) {
		cerr << typeNames[_type] << "::load - the number of outputs cannot be greater than the maximum value of "
			<< (_numberOfStates * _numberOfInputs) << ". Consider minimization!" << endl;
	}
	if (maxState < _numberOfStates) {
		cerr << typeNames[_type] << "::load - the number of states cannot be greater than the greatest state ID" << endl;
		file.close();
		return false;
	}
	_usedStateIDs.resize(maxState, false);
	if (!loadTransitionOutputs(file) || !loadTransitions(file)) {
		file.close();
		return false;
	}
	file.close();
	return true;
}

string Mealy::save(string path) {
	string filename = getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::save - unable to open file" << endl;
		return "";
	}
	saveInfo(file);
	saveTransitionOutputs(file);
	saveTransitions(file);
	file.close();
	return filename;
}

string Mealy::writeDOTfile(string path) {
	string filename("DOT");
	filename += getFilename();
	filename = Utils::getUniqueName(filename, "fsm", path);
	ofstream file(filename.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::writeDOTfile - unable to open file" << endl;
		return "";
	}
	writeDotStart(file);
	writeDotStates(file, false);
	writeDotTransitions(file, true);
	writeDotEnd(file);
	file.close();
	return filename;
}

bool Mealy::setOutput(state_t state, output_t output, input_t input) {
	if ((state >= _usedStateIDs.size()) || (!_usedStateIDs[state])) {
		cerr << typeNames[_type] << "::setOutput - bad state" << endl;
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setOutput - bad output (increase the number of outputs first)" << endl;
		return false;
	}
	if ((input >= _numberOfInputs) || (input == STOUT_INPUT)) {
		cerr << typeNames[_type] << "::setOutput - bad input" << endl;
		return false;
	}
	if (_transition[state][input] == NULL_STATE) {
		cerr << typeNames[_type] << "::setOutput - there is no such transition" << endl;
		return false;
	}
	_outputTransition[state][input] = output;
	_isReduced = false;
	return true;
}

bool Mealy::setTransition(state_t from, input_t input, state_t to, output_t output) {
	if (input == STOUT_INPUT) {
		cerr << typeNames[_type] << "::setTransition - STOUT_INPUT is not allowed" << endl;
		return false;
	}
	if ((from >= _usedStateIDs.size()) || (!_usedStateIDs[from])) {
		cerr << typeNames[_type] << "::setTransition - bad state From" << endl;
		return false;
	}
	if (input >= _numberOfInputs) {
		cerr << typeNames[_type] << "::setTransition - bad input" << endl;
		return false;
	}
	if ((to >= _usedStateIDs.size()) || (!_usedStateIDs[to])) {
		cerr << typeNames[_type] << "::setTransition - bad state To" << endl;
		return false;
	}
	if ((output >= _numberOfOutputs) && (output != DEFAULT_OUTPUT)) {
		cerr << typeNames[_type] << "::setTransition - bad output (increase the number of outputs first)" << endl;
		return false;
	}
	_transition[from][input] = to;
	_outputTransition[from][input] = output;
	_isReduced = false;
	return true;
}

