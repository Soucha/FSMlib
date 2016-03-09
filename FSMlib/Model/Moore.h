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

#include "DFSM.h"

/**
* Class derived from DFSM.<br>
* <br>
* Output function is defined for each state.
*/
class FSMLIB_API Moore : public DFSM {
public:

	Moore() :
		DFSM() {
		_type = TYPE_MOORE;
	}
	
	Moore(const Moore& other) :
		DFSM(other) {
	}

	/// inherited
	//state_t getNextState(state_t state, input_t input);
	
	output_t getOutput(state_t state, input_t input);

	//<-- MODEL INITIALIZATION -->//

	void create(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs);
	void generate(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs);
	bool load(string fileName);

	//<-- STORING -->//

	string save(string path);
	string writeDOTfile(string path);

	//<-- EDITING THE MODEL -->//

	/// inherited
	//state_t addState(output_t stateOutput = DEFAULT_OUTPUT);
	bool setOutput(state_t state, output_t output, input_t input = STOUT_INPUT);
	bool setTransition(state_t from, input_t input, state_t to, output_t output = DEFAULT_OUTPUT);
	
	/// inherited
	//bool removeState(state_t state);
	/// inherited
	//bool removeTransition(state_t from, input_t input, state_t to = NULL_STATE, output_t output = DEFAULT_OUTPUT);

	/// inherited
	//void incNumberOfInputs(num_inputs_t byNum);
	/// inherited
	//void incNumberOfOutputs(num_outputs_t byNum);

	/// inherited
	//bool removeUnreachableStates();
	/// inherited
	//void makeCompact();
	/// inherited
	//bool mimimize();
};
