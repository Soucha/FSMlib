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

#include "DFA.h"

void DFA::create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
	if (numberOfOutputs > 2) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of 2" << endl;
		numberOfOutputs = 2;
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
	_type = TYPE_DFA;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	clearTransitions();
	clearStateOutputs();
}

void DFA::generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) {
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
	if (numberOfOutputs > 2) {
		cerr << typeNames[_type] << "::create - the number of outputs reduced to maximum of 2" << endl;
		numberOfOutputs = 2;
	}
	_numberOfStates = numberOfStates;
	_numberOfInputs = numberOfInputs;
	_numberOfOutputs = numberOfOutputs;
	_type = TYPE_DFA;
	_isReduced = false;
	_usedStateIDs.resize(_numberOfStates, true);

	srand(time(NULL));

	generateTransitions();
	generateStateOutputs(_numberOfOutputs);
}

bool DFA::load(string fileName) {
	ifstream file(fileName.c_str());
	if (!file.is_open()) {
		cerr << typeNames[_type] << "::load - unable to open file" << endl;
		return false;
	}
	state_t maxState;
	file >> _type >> _isReduced >> _numberOfStates >> _numberOfInputs >> _numberOfOutputs >> maxState;
	if (_type != TYPE_DFA) {
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
	if (_numberOfOutputs > 2) {
		cerr << typeNames[_type] << "::load - the number of outputs cannot be greater than 2" << endl;
		file.close();
		return false;
	}
	if (maxState < _numberOfStates) {
		cerr << typeNames[_type] << "::load - the number of states cannot be greater than the greatest state ID" << endl;
		file.close();
		return false;
	}
	_usedStateIDs.resize(maxState, false);
	if (!loadStateOutputs(file) || !loadTransitions(file)) {
		file.close();
		return false;
	}
	file.close();
	return true;
}

void DFA::incNumberOfOutputs(output_t byNum) {
	cerr << typeNames[_type] << "::incNumberOfOutputs - the number of outputs cannot be increased" << endl;
}