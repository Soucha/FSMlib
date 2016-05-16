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

#include "../Model/FSMmodel.h"

/**
* An abstract class describing behaviour of a Black Box.
*/
class FSMLIB_API BlackBox {
public:
	BlackBox(bool isResettable, machine_type_t type = TYPE_DFSM) : 
		_isResettable(isResettable),
		_resetCounter(0),
		_querySymbolCounter(0),
		_type(type) {
	}

	/**
	* @return True if the BlackBox can be reset, False otherwise.
	*/
	bool isResettable() const {
		return _isResettable;
	}

	/**
	* Gets the type of the Black Box model.
	* Possible types are listed in FSMtypes.h.
	* @return The type of this Black Box model.
	*/
	machine_type_t getModelType() const {
		return _type;
	}

	/**
	* @return How many input symbols were queried.
	*/
	seq_len_t getQueriedSymbolsCount() const {
		return _querySymbolCounter;
	}

	/**
	* @return How many times reset was applied.
	*/
	seq_len_t getAppliedResetCount() const {
		return _resetCounter;
	}

	/**
	* Provides a collection of inputs that can be applied next (excluding STOUT_INPUT).
	* @return a collection of next inputs
	*/
	virtual vector<input_t> getNextPossibleInputs() = 0;

	/**
	* Provides the number of all possible inputs if it is available.
	* Assumes that all inputs are mapped from 0 to this number minus 1.
	* @return the number of inputs, or STOUT_INPUT if the number is not known
	*/
	virtual input_t getNumberOfInputs() = 0;

	/**
	* Provides the number of all possible outputs if it is available.
	* Assumes that all outputs are mapped from 0 to this number minus 1.
	* @return the number of ouputs, or WRONG_OUTPUT if the number is not known
	*/
	virtual output_t getNumberOfOutputs() = 0;

	/**
	* Resets the BlackBox to its initial state.
	*/
	virtual void reset() = 0;

	/**
	* Applies the given input symbol and provides related response.
	* The current state of the BlackBox is changed accordingly.
	* @param input to query
	* @return Output symbol as a response to the given input symbol in the current state
	*/
	virtual output_t query(input_t input) = 0;
	
	/**
	* Applies the given input sequence and provides related response.
	* The current state of the BlackBox is changed accordingly.
	* @param input sequence to query
	* @return Output sequence as a response to the given input sequence from the current state
	*/
	virtual sequence_out_t query(const sequence_in_t& inputSequence) = 0;

	/**
	* Applies reset, then the given input symbol and provides related response.
	* The current state of the BlackBox is changed accordingly.
	* @param input to query
	* @return Output symbol as a response to the given input symbol in the initial state
	*/
	virtual output_t resetAndQuery(input_t input) = 0;
	
	/**
	* Applies reset, then the given input sequence and provides related response.
	* The current state of the BlackBox is changed accordingly.
	* @param input sequence to query
	* @return Output sequence as a response to the given input sequence from the initial state
	*/
	virtual sequence_out_t resetAndQuery(const sequence_in_t& inputSequence) = 0;
protected:
	bool _isResettable;
	seq_len_t _querySymbolCounter;
	seq_len_t _resetCounter;
	machine_type_t _type;
};
