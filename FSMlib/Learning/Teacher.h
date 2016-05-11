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
* An abstract class describing behaviour of a Teacher.
*/
class FSMLIB_API Teacher {
public:
	Teacher() :
		_querySymbolCounter(0),
		_resetCounter(0),
		_outputQueryCounter(0),
		_equivalenceQueryCounter(0) {
	}

	/**
	* @return True if the BlackBox can be reset, False otherwise.
	*/
	virtual bool isBlackBoxResettable() = 0;

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
	* @return How many Output Queries were asked.
	*/
	seq_len_t getOutputQueryCount() const {
		return _outputQueryCounter;
	}
	
	/**
	* @return How many Equivalence Queries were asked.
	*/
	seq_len_t getEquivalenceQueryCount() const {
		return _equivalenceQueryCounter;
	}

	/**
	* Resets the BlackBox to its initial state.
	* An error message is produced and reset fails if isBlackBoxResettable() is false.
	*/
	virtual void resetBlackBox() = 0;

	/**
	* Provides a response of the BlackBox on the given input symbol in its current state.
	* @param input to query
	* @return Output symbol as a response to the given input symbol in the current state
	*/
	virtual output_t outputQuery(input_t input) = 0;

	/**
	* Provides a response of the BlackBox on the given input sequence from its current state.
	* @param input sequence to query
	* @return Output sequence as a response to the given input sequence from the current state
	*/
	virtual sequence_out_t outputQuery(sequence_in_t inputSequence) = 0;

	/**
	* Provides a response of the BlackBox on the given input symbol in its initial state.
	* @param input to query
	* @return Output symbol as a response to the given input symbol in the initial state
	*/
	virtual output_t resetAndOutputQuery(input_t input) = 0;

	/**
	* Provides a response of the BlackBox on the given input sequence in its initial state.
	* @param input sequence to query
	* @return Output sequence as a response to the given input sequence from the initial state
	*/
	virtual sequence_out_t resetAndOutputQuery(sequence_in_t inputSequence) = 0;
	
	/**
	* Checks the equivalance of the given model and the BlackBox.
	* @param conjecture - a hypothesis of the BlackBox
	* @return A counterexample that covers a discrepancy between the conjectured model and the BlackBox,
	*		or an empty sequence if both systems are equivalent
	*/
	virtual sequence_in_t equivalenceQuery(const unique_ptr<DFSM>& conjecture) = 0;
protected:
	seq_len_t _querySymbolCounter;
	seq_len_t _resetCounter;
	seq_len_t _outputQueryCounter;
	seq_len_t _equivalenceQueryCounter;
};
