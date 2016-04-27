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

#include "DFA.h"
#include "Moore.h"
#include "Mealy.h"
#include "DFSM.h"
#include "FSM.h"

namespace FSMmodel {
	/**
	* Creates a finite state machine of give type.
	*
	* @param machineType
	* @param numberOfStates
	* @param numberOfInputs
	* @param numberOfOutputs
	* @return a pointer to a new FSM
	*/
	FSMLIB_API FSM * createFSM(machine_type_t machineType, state_t numberOfStates = 1, input_t numberOfInputs = 0, output_t numberOfOutputs = 0);

	/**
	* Creates a copy of provided finite state machine.
	*
	* @param fsm
	* @return a pointer to the copy of given FSM
	*/
	FSMLIB_API FSM * duplicateFSM(FSM * fsm);

	/**
	* Compares two DFSMs with same input and output alphabet and initial state 0.
	* FSMs are isomorphic only if they differ in permutation of states.
	*
	* Function minimizes given FSMs if they are not in reduced form.
	* @param fsm1
	* @param fsm2
	* @return True if given machines are isomorphic
	*/
	FSMLIB_API bool areIsomorphic(DFSM * dfsm1, DFSM * dfsm2);

	/**
	* Checks if each state is reacheable from each other.
	* @param dfsm
	* @return true if the given DFSM is strongly connected
	*/
	FSMLIB_API bool isStronglyConnected(DFSM * dfsm);

	/**
	* Creates GIF file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if system should show GIF file
	* @return True if GIF was created (and shown). 
	*		  System can run dot unless false is returned
	*/
	FSMLIB_API bool createGIF(string fileName, bool show = false);

	/**
	* Creates JPG file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if cygwin should show JPG file
	* @return True if JPG was created (and shown). 
	*		  System can run dot unless false is returned
	*/
	FSMLIB_API bool createJPG(string fileName, bool show = false);

	/**
	* Creates PNG file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if cygwin should show PNG file
	* @return True if PNG was created (and shown). 
	*		  System can run dot unless false is returned
	*/
	FSMLIB_API bool createPNG(string fileName, bool show = false);

	/**
	* Prints given sequence into a string
	* @param input sequence
	* @return sequence as string
	*/
	FSMLIB_API string getInSequenceAsString(sequence_in_t sequence);

	/**
	* Prints given sequence into a string
	* @param output sequence
	* @return sequence as string
	*/
	FSMLIB_API string getOutSequenceAsString(sequence_out_t sequence);

	/**
	* Creates a table of shortest paths between all states.
	* 
	* @param fsm - Deterministic FSM
	* @param shortestPaths - resulting table 
	*/
	FSMLIB_API void createAllShortestPaths(DFSM * fsm, shortest_paths_t & shortestPaths);
	
	/**
	* Designs a shortest path from given start state to given end state
	* based on given shortest paths table.
	*
	* @param fsm - Deterministic FSM
	* @param from - start state
	* @param to - end state
	* @param shortestPaths - the result of call createAllShortestPaths
	* @param shortestPath - a shortest path from given start to end state
	* @return true if there is a path from start state to end state, false otherwise
	*/
	FSMLIB_API bool getShortestPath(DFSM * fsm, state_t from, state_t to,
		shortest_paths_t & shortestPaths, sequence_in_t & shortestPath);

}
