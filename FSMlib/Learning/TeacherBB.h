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

#include "Teacher.h"
#include "../Testing/FSMtesting.h"

/**
* A derived class describing behaviour of a Teacher that interacts with a given BlackBox.
*/
class FSMLIB_API TeacherBB : public Teacher {
public:
	TeacherBB(const shared_ptr<BlackBox>& blackBox, 
		function<sequence_set_t(const unique_ptr<DFSM>& fsm, int extraStates)> testingMethod, state_t maxExtraStates = 3);

	bool isProvidedOnlyMQ() {
		return false;
	}

	machine_type_t getBlackBoxModelType() {
		return _bb->getModelType();
	}

	vector<input_t> getNextPossibleInputs();

	input_t getNumberOfInputs();

	output_t getNumberOfOutputs();

	bool isBlackBoxResettable();

	void resetBlackBox();

	output_t outputQuery(input_t input);

	sequence_out_t outputQuery(const sequence_in_t& inputSequence);

	output_t resetAndOutputQuery(input_t input);

	sequence_out_t resetAndOutputQuery(const sequence_in_t& inputSequence);

	sequence_out_t resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, const sequence_in_t& suffix);

	sequence_in_t equivalenceQuery(const unique_ptr<DFSM>& conjecture);

private:
	struct bb_node_t;
	shared_ptr<BlackBox> _bb;
	function<sequence_set_t(const unique_ptr<DFSM>& fsm, int extraStates)> _testingMethod;
	state_t _maxExtraStates;
	shared_ptr<bb_node_t> _initialState, _currState, _bbState;
	input_t _greatestInput;
	output_t _greatestOutput;
};
