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

/**
* A derived class describing behaviour of a Teacher that represents a regular language.
*/
class FSMLIB_API TeacherRL : public Teacher {
public:
	TeacherRL(const unique_ptr<DFSM>& blackBox) :
		Teacher(),
		_fsm(FSMmodel::duplicateFSM(blackBox)),
		_currState(0) {
	}

	bool isProvidedOnlyMQ() {
		return true;
	}

	machine_type_t getBlackBoxModelType() {
		return _fsm->getType();
	}

	vector<input_t> getNextPossibleInputs();

	input_t getNumberOfInputs() {
		return _fsm->getNumberOfInputs();
	}

	output_t getNumberOfOutputs() {
		return _fsm->getNumberOfOutputs();
	}

	bool isBlackBoxResettable() {
		return true;
	}

	void resetBlackBox();

	output_t outputQuery(input_t input);

	sequence_out_t outputQuery(const sequence_in_t& inputSequence);

	output_t resetAndOutputQuery(input_t input);

	sequence_out_t resetAndOutputQuery(const sequence_in_t& inputSequence);

	sequence_out_t resetAndOutputQueryOnSuffix(const sequence_in_t& prefix, const sequence_in_t& suffix);

	sequence_in_t equivalenceQuery(const unique_ptr<DFSM>& conjecture);

private:
	unique_ptr<DFSM> _fsm;
	state_t _currState;
};