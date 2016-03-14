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

#include "FSM.h"

/**
* Class derived from FSM.<br>
* <br>
* Output function is defined for each state and for each transition.
*/
class FSMLIB_API DFSM : public FSM {
public:

	DFSM() :
		FSM() {
		_isReduced = false;
		_type = TYPE_DFSM;
		_transition.clear();
		_outputState.clear();
		_outputTransition.clear();
		_isOutputState = true;
		_isOutputTransition = true;
	}

	DFSM(const DFSM& other) :
		FSM(other), _isReduced(other._isReduced), _transition(other._transition),
		_isOutputState(other._isOutputState), _isOutputTransition(other._isOutputTransition),
		_outputTransition(other._outputTransition), _outputState(other._outputState) {
	}

	/**
	* @return True if this FSM is in minimal form
	*/
	bool isReduced() const {
		return _isReduced;
	}

	/**
	* @return True if states are labelled by outputs
	*/
	bool isOutputState() const {
		return _isOutputState;
	}

	/**
	* @return True if transitions are labelled by outputs
	*/
	bool isOutputTransition() const {
		return _isOutputTransition;
	}

	//<-- TRANSITION SYSTEM -->//

	/**
	* Gets identification of next state after applying given input from given state.
	* @param state
	* @param input
	* @return Next state or WRONG_STATE if given state or input is not valid
	*/
	virtual state_t getNextState(state_t state, input_t input);

	/**
	* Gets identification of last state after applying given input sequence from given state.
	* @param state
	* @param path
	* @return end state or WRONG_STATE if given state or path is not valid
	*/
	state_t getEndPathState(state_t state, sequence_in_t path);

	//<-- OUTPUT BEHAVIOUR -->//

	/**
	* Gets output observed after applying given input from given state.
	* @param state
	* @param input
	* @return Output or WRONG_OUTPUT if given state or input is not valid
	*/
	virtual output_t getOutput(state_t state, input_t input);

	/**
	* Gets output sequence observed after applying given input sequence from given state.
	* @param state
	* @param path
	* @return output sequence or WRONG_OUTPUT if given state or path is not valid
	*/
	sequence_out_t getOutputAlongPath(state_t state, sequence_in_t path);

	//<-- MODEL INITIALIZATION -->//

	void create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs);
	void generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs);
	bool load(string fileName);

	//<-- STORING -->//

	string save(string path);
	string writeDOTfile(string path);

	//<-- EDITING THE MODEL -->//

	state_t addState(output_t stateOutput = DEFAULT_OUTPUT);
	bool setOutput(state_t state, output_t output, input_t input = STOUT_INPUT);
	bool setTransition(state_t from, input_t input, state_t to, output_t output = DEFAULT_OUTPUT);
	bool removeState(state_t state);
	bool removeTransition(state_t from, input_t input, state_t to = NULL_STATE, output_t output = DEFAULT_OUTPUT);

	void incNumberOfInputs(input_t byNum);
	void incNumberOfOutputs(output_t byNum);

	/**
	* Transforms given DFSM into the form without unreachable states.
	* @param dfsm
	* @return True if unreachable states were removed (or given had no unreachable state),
	*         false if an error occured while unreachable states were removing
	*/
	bool removeUnreachableStates();

	/**
	* Eliminates unused rows from transition table and
	* sets the number of inputs and outputs to the greatest
	* input or output occurred in this model.
	*
	* Note that some states change their identification.
	*/
	void makeCompact();

	/*
	* Reduces this FSM into its minimal form.
	* @return True on success, False if an error occurred
	*/
	bool mimimize();

protected:
	bool _isReduced;
	bool _isOutputState;
	bool _isOutputTransition;
	vector< vector< state_t > > _transition;
	vector< vector< output_t > > _outputTransition;
	vector< output_t > _outputState;

	bool distinguishByStateOutputs(queue< vector< state_t > >& blocks);
	bool distinguishByTransitionOutputs(queue< vector< state_t > >& blocks);
	bool distinguishByTransitions(queue< vector< state_t > >& blocks);
	void mergeEquivalentStates(queue< vector< state_t > >& equivalentStates);
	
	void clearStateOutputs();
	void clearTransitionOutputs();
	void clearTransitions();
	
	void generateStateOutputs(output_t nOutputs);
	void generateTransitionOutputs(output_t nOutputs, output_t firstOutput);
	// generates coherent transition system
	void generateTransitions();
	
	bool loadStateOutputs(ifstream& file);
	bool loadTransitionOutputs(ifstream& file);
	bool loadTransitions(ifstream& file);
	
	string getFilename();
	void saveInfo(ofstream& file);
	void saveStateOutputs(ofstream& file);
	void saveTransitionOutputs(ofstream& file);
	void saveTransitions(ofstream& file);
	
	void writeDotStart(ofstream& file);
	void writeDotStates(ofstream& file);
	void writeDotTransitions(ofstream& file);
	void writeDotEnd(ofstream& file);
};
