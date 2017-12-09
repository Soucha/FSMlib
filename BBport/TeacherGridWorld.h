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
#pragma once

#include "BBport.h"

/**
* A derived class describing behaviour of a Teacher that possesses a model of BlackBox directly.
*/
class TeacherGridWorld : public Teacher {
public:
	TeacherGridWorld(bool isResettable, input_t numberOfInputs) :
		Teacher(),
		_isResettable(isResettable),
		_numOfInputs(numberOfInputs),
		_numOfObservedOutputs(0),
		_querySymbolCounterEQ(0),
		_resetCounterEQ(0) {
	}

	bool isProvidedOnlyMQ() {
		return false;
	}

	machine_type_t getBlackBoxModelType() {
		return TYPE_DFSM;
	}

	vector<input_t> getNextPossibleInputs();

	input_t getNumberOfInputs() {
		return _numOfInputs;
	}

	output_t getNumberOfOutputs() {
		return _numOfObservedOutputs;
	}

	bool isBlackBoxResettable() {
		return _isResettable;
	}

	void resetBlackBox();

	output_t outputQuery(input_t input);

	sequence_out_t outputQuery(const sequence_in_t& inputSequence);

	output_t resetAndOutputQuery(input_t input);

	sequence_out_t resetAndOutputQuery(const sequence_in_t& inputSequence);

	output_t resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, input_t input);

	sequence_out_t resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, const sequence_in_t& suffix);

	sequence_in_t equivalenceQuery(const unique_ptr<DFSM>& conjecture);

	size_t _querySymbolCounterEQ;
	size_t _resetCounterEQ;
protected:
	bool _isResettable;
	input_t _numOfInputs;
	output_t _numOfObservedOutputs;
};
