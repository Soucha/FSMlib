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

#include "BlackBox.h"

/**
* A derived class describing behaviour of a Black Box that is modelled by a given DFSM.
*/
class FSMLIB_API BlackBoxDFSM : public BlackBox {
public:
	BlackBoxDFSM(const unique_ptr<DFSM>& blackBoxModel, bool isResettable) :
		BlackBox(isResettable, blackBoxModel->getType()),
		_fsm(FSMmodel::duplicateFSM(blackBoxModel)),
		_currState(0) {
		_fsm->minimize();
	}

	vector<input_t> getNextPossibleInputs();

	input_t getNumberOfInputs() {
		return _fsm->getNumberOfInputs();
	}

	output_t getNumberOfOutputs() {
		return _fsm->getNumberOfOutputs();
	}

	void reset();

	output_t query(input_t input);

	sequence_out_t query(const sequence_in_t& inputSequence);

	output_t resetAndQuery(input_t input);

	sequence_out_t resetAndQuery(const sequence_in_t& inputSequence);

private:
	unique_ptr<DFSM> _fsm;
	state_t _currState;
};
