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
	}

	DFSM(const DFSM& other) :
		FSM(other), _isReduced(other._isReduced), _transition(other._transition),
		_outputTransition(other._outputTransition), _outputState(other._outputState) {
	}

	/**
	* @return True if this FSM is in minimal form
	*/
	bool isReduced() const {
		return _isReduced;
	}

	//<-- TRANSITION SYSTEM -->//

	/**
	* Get identification of next state after applying given input from given state.
	* @param state
	* @param input
	* @return Next state
	* @throw Exception if given state or input is not valid
	*/
	virtual state_t getNextState(state_t state, input_t input);

	/**
	* Get identification of last state after applying given input sequence from given state.
	* @param state
	* @param path
	* @return end state
	* @throw Exception if given state or input is not valid
	*/
	state_t getEndPathState(state_t state, sequence_in_t path) {
		for (sequence_in_t::iterator inputIt = path.begin(); inputIt != path.end(); inputIt++) {
			state = this->getNextState(state, *inputIt);
		}
		return state;
	}

	//<-- OUTPUT BEHAVIOUR -->//

	/**
	* Get output observed after applying given input from given state.
	* @param state
	* @param input
	* @return Output
	* @throw Exception if given state or input is not valid
	*/
	virtual output_t getOutput(state_t state, input_t input);

	/**
	* Get output sequence observed after applying given input sequence from given state.
	* @param state
	* @param path
	* @return output sequence
	* @throw Exception if given state or input is not valid
	*/
	sequence_out_t getOutputAlongPath(state_t state, sequence_in_t path) {
		sequence_out_t sOut;
		for (sequence_in_t::iterator inputIt = path.begin(); inputIt != path.end(); inputIt++) {
			sOut.push_back(this->getOutput(state, *inputIt));
			state = this->getNextState(state, *inputIt);
		}
		return sOut;
	}

	//<-- MODEL INITIALIZATION -->//

	void create(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs);
	void generate(num_states_t numberOfStates, num_inputs_t numberOfInputs, num_outputs_t numberOfOutputs);
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

	void incNumberOfInputs(num_inputs_t byNum);
	void incNumberOfOutputs(num_outputs_t byNum);

protected:
	bool _isReduced;
	vector< vector< state_t > > _transition;
	vector< vector< output_t > > _outputTransition;
	vector< output_t > _outputState;

	void clearStateOutputs();
	void clearTransitionOutputs();
	void clearTransitions();
	
	void generateStateOutputs(num_outputs_t nOutputs);
	void generateTransitionOutputs(num_outputs_t nOutputs, num_outputs_t firstOutput);
	// generate coherent transition system
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
	void writeDotStates(ofstream& file, bool withOutputs = true);
	void writeDotTransitions(ofstream& file, bool withOutputs = true);
	void writeDotEnd(ofstream& file);

private:
	void setEquivalence(queue< vector<state_t> >& equivalentStates);
	void removeUnreachableStates(vector<state_t>& unreachableStates);

};