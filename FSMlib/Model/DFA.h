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

#include "Moore.h"

/**
* Class derived from DFSM.<br>
* <br>
* Each state is either accepting (1) or rejecting (0).
*/
class FSMLIB_API DFA : public Moore {
public:

	DFA() :
		Moore() {
		_type = TYPE_MOORE;
	}

	DFA(const DFA& other) :
		Moore(other) {
	}

	/// inherited
	//state_t getNextState(state_t state, input_t input);

	/// inherited
	//output_t getOutput(state_t state, input_t input);

	//<-- MODEL INITIALIZATION -->//

	void create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs);
	void generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs);
	bool load(string fileName);

	//<-- STORING -->//

	/// inherited
	//string save(string path);
	/// inherited
	//string writeDOTfile(string path);

	//<-- EDITING THE MODEL -->//

	/// inherited
	//state_t addState(output_t stateOutput = DEFAULT_OUTPUT);
	/// inherited
	//bool setOutput(state_t state, output_t output, input_t input = STOUT_INPUT);
	/// inherited
	//bool setTransition(state_t from, input_t input, state_t to, output_t output = DEFAULT_OUTPUT);

	/// inherited
	//bool removeState(state_t state);
	/// inherited
	//bool removeTransition(state_t from, input_t input, state_t to = NULL_STATE, output_t output = DEFAULT_OUTPUT);

	/// inherited
	//void incNumberOfInputs(input_t byNum);

	/// it is not possible to increase the number of outputs for DFA
	void incNumberOfOutputs(output_t byNum);

	/// inherited
	//bool removeUnreachableStates();
	/// inherited
	//void makeCompact();
	/// inherited
	//bool mimimize();
};