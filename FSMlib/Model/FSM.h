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

#include "FSMtypes.h"

/**
* An abstract class for common access to various types of Finite State Machines (FSM).<br>
* <br>
* States are labelled from 0 to N-1, where N is the number of states.<br>
* Inputs are labelled from 0 to In-1, where In is the number of inputs.<br>
* Outputs are labelled from 0 to Out-1, where Out is the number of outputs.<br>
* Transition and Output functions depend on the type of a FSM.<br>
* &nbsp;    - Moore Machine defines an output for each state. (Out &le; N)<br>
* &nbsp;    - Mealy Machine defines an output for each transition. (Out &le; N * In)<br>
*
* Recommended usage:<br>
* Use a derived class DFSM or NFSM according to determinism of targeted FSM.
* &nbsp;  auto fsm = make_unique&lt;derived_class&gt;();<br>
* &nbsp;  where &lt;derived_class&gt; is DFSM, Moore, Mealy or DFA
*/
class FSMLIB_API FSM {
public:

	FSM() :
		_numberOfStates(0),
		_numberOfInputs(0),
		_numberOfOutputs(0),
		_type(TYPE_NONE) {
		_usedStateIDs.clear();
	}

	virtual ~FSM() {}

	// implicit copy and move constructors and assignment operators

	//<-- GENERAL INFORMATION -->//

	/**
	* @return The number of states
	*/
	state_t getNumberOfStates() const {
		return _numberOfStates;
	}

	/**
	* @return The greatest state ID increased by 1
	*/
	state_t getGreatestStateId() const {
		return state_t(_usedStateIDs.size());
	}

	/**
	* @return True if all states 0 - (N-1) are used, False otherwise.
	*/
	bool isCompact() const {
		return (_numberOfStates == _usedStateIDs.size());
	}

	/**
	* @return The number of inputs
	*/
	input_t getNumberOfInputs() const {
		return _numberOfInputs;
	}

	/**
	* @return The number of outputs
	*/
	output_t getNumberOfOutputs() const {
		return _numberOfOutputs;
	}

	/**
	* Gets type of this FSM.
	* Possible types are listed in FSMtypes.h.
	* @return The type of this FSM
	*/
	machine_type_t getType() const {
		return _type;
	}

	//<-- MODEL INITIALIZATION -->//

	/**
	* Creates an empty model with the given number of states and
	* expected numbers of inputs and outputs.
	* @param numberOfStates
	* @param numberOfInputs
	* @param numberOfOutputs
	*/
	virtual void create(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) = 0;

	/**
	* Generates a connected FSM with the given number of states and the number of inputs.
	* The given number of outputs can be scaled down depending to the type of FSM,
	* the number of states and the number of inputs.
	* @param numberOfStates
	* @param numberOfInputs
	* @param numberOfOutputs
	*/
	virtual void generate(state_t numberOfStates, input_t numberOfInputs, output_t numberOfOutputs) = 0;

	/**
	* Tries to load FSM from file.
	* @param fileName of a FSM to load
	* @return True on success. Otherwise, an error is written on standard error output.
	*/
	virtual bool load(string fileName) = 0;

	//<-- STORING -->//

	/**
	* Tries to save the FSM to a file.
	* @param path to an existing folder where new file should be stored
	* @return The name of a file with the saved FSM, or the empty string if there is an error
	*/
	virtual string save(string path) = 0;

	/**
	* Tries to write the FSM to DOT file which can be displayed using Graphviz.
	* @param path to an existing folder where new file should be stored
	* @return The name of a file with the saved FSM in DOT format, or the empty string if there is an error
	*/
	virtual string writeDOTfile(string path) = 0;

	//<-- EDITING THE MODEL -->//

	/**
	* Adds a new state to this FSM.
	* @param output of the state (Optional for some FSM types)
	* @return Identification of new state, or NULL_STATE if there is an error.
	*/
	virtual state_t addState(output_t stateOutput = DEFAULT_OUTPUT) = 0;

	/**
	* Updates the output of an existing state or transition identified by given state and input.
	* @param state 
	* @param output
	* @param input (Optional for some FSM types)
	* @return True if the output was updated
	*/
	virtual bool setOutput(state_t state, output_t output, input_t input = STOUT_INPUT) = 0;

	/**
	* Adds or updates a transition identified by given state From and input.
	* @param from 
	* @param input
	* @param to
	* @param output (Optional for some FSM types)
	* @return True if the transition was set
	*/
	virtual bool setTransition(state_t from, input_t input, state_t to, output_t output = DEFAULT_OUTPUT) = 0;

	/**
	* Removes given state (and all related transitions).<br>
	* Note that a successive call of addState can return given identifier of a new state.
	* @param state 
	* @return True if the state was removed
	*/
	virtual bool removeState(state_t state) = 0;

	/**
	* Removes given transition.
	* The transition is identified by given state From and input
	* if this FSM is deterministic.
	* Otherwise, the identification requires the next state To (and output).
	* @param from 
	* @param input
	* @param to (Optional for some FSM types)
	* @param output (Optional for some FSM types)
	* @return True if the transition was removed
	*/
	virtual bool removeTransition(state_t from, input_t input, state_t to = NULL_STATE, output_t output = DEFAULT_OUTPUT) = 0;

	/**
	* Increases the number of inputs by given number.
	* @param positive increment
	*/
	virtual void incNumberOfInputs(input_t byNum) = 0;

	/**
	* Increases the number of outputs by given number.
	* @param positive increment
	*/
	virtual void incNumberOfOutputs(output_t byNum) = 0;

protected:
	state_t _numberOfStates;
	input_t _numberOfInputs;
	output_t _numberOfOutputs;
	machine_type_t _type;
#pragma warning (disable : 4251)
	vector<bool> _usedStateIDs;
};

