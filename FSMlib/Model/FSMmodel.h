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
	* Transform given FSM into its minimal (reduced) form.
	* @param dfsm for minimization
	* @return True if given machine was minimized
	*/
	//FSMLIB_API bool minimize(DFSM * dfsm);

	/**
	* Checks if each state is reacheable from each other.
	* @param dfsm
	* @return true if the given DFSM is strongly connected
	*/
	FSMLIB_API bool isStronglyConnected(DFSM * dfsm);

	/**
	* Creates GIF file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if cygwin should show GIF file
	* @throw Exception if system was not able to run appropriate commands.
	*/
	//void createGIF(string fileName, bool show = false);

	/**
	* Creates JPG file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if cygwin should show JPG file
	* @throw Exception if system was not able to run appropriate commands.
	*/
	//void createJPG(string fileName, bool show = false);

	/**
	* Creates PNG file from DOT file.
	* @param fileName of DOT file
	* @param show - set true if cygwin should show PNG file
	* @throw Exception if system was not able to run appropriate commands.
	*/
	//void createPNG(string fileName, bool show = false);

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
}
