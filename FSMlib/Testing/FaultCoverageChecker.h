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

#include "FSMtesting.h"

namespace FSMtesting {
	namespace FaultCoverageChecker {
		/**
		* Finds all machines that cannot be distinguished using provided checking sequence. 
		*
		* @param fsm - Deterministic FSM
		* @param CS - Checking Sequence which fault coverage is to be checked
		* @param indistinguishable - a collection to fill up with FSMs that cannot be distinguished by given CS
		* @param extraStates - how many extra states shall be considered,
		*		 default is no extra state, needs to be positive or 0
		*/ 
		FSMLIB_API void getFSMs(DFSM* dfsm, sequence_in_t & CS, vector<DFSM*>& indistinguishable, int extraStates = 0);
		
		/**
		* Finds all machines that cannot be distinguished using provided checking sequence.
		*
		* @param fsm - Deterministic FSM
		* @param CS - Checking Sequence which fault coverage is to be checked
		* @param indistinguishable - a collection to fill up with FSMs that cannot be distinguished by given CS
		* @param hint - a collection of sequences leading to reference states that can be distinguished from each other
		* @param extraStates - how many extra states shall be considered,
		*		 default is no extra state, needs to be positive or 0
		*/
		FSMLIB_API void getFSMs(DFSM* dfsm, sequence_in_t & CS, vector<DFSM*>& indistinguishable, sequence_vec_t hint, int extraStates = 0);
		
		/**
		* Finds all machines that cannot be distinguished using provided test suite.
		*
		* @param fsm - Deterministic FSM
		* @param TS - Test Suite which fault coverage is to be checked
		* @param indistinguishable - a collection to fill up with FSMs that cannot be distinguished by given TS
		* @param extraStates - how many extra states shall be considered,
		*		 default is no extra state, needs to be positive or 0
		*/
		FSMLIB_API void getFSMs(DFSM* dfsm, sequence_set_t & TS, vector<DFSM*>& indistinguishable, int extraStates = 0);
		
		/**
		* Finds all machines that cannot be distinguished using provided test suite.
		*
		* @param fsm - Deterministic FSM
		* @param TS - Test Suite which fault coverage is to be checked
		* @param indistinguishable - a collection to fill up with FSMs that cannot be distinguished by given TS
		* @param hint - a collection of sequences leading to reference states that can be distinguished from each other
		* @param extraStates - how many extra states shall be considered,
		*		 default is no extra state, needs to be positive or 0
		*/
		FSMLIB_API void getFSMs(DFSM* dfsm, sequence_set_t & TS, vector<DFSM*>& indistinguishable, sequence_vec_t hint, int extraStates = 0);
	}
}
