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

#include "../Sequences/FSMsequence.h"

namespace FSMtesting {
	//<-- Resettable machines -->// 

	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended Preset Distinguishing Sequence.
	* 
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return true if a test suite is designed, false if there is no PDS or negative extraStates
	*/
	FSMLIB_API bool PDS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended Adaptive Distinguishing Sequence.
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return true if a test suite is designed, false if there is no ADS or negative extraStates
	*/
	FSMLIB_API bool ADS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states are confirmed
	* using Verifying Set of State Verifying Sequences of each state
	* and all last transitions are confirmed
	* using appended State Verifying Sequence of the end state, or
	* using its State Characterizing Set if the state has no SVS.
	* SCSet is also in place of SVS in VSet if such SVS does not exist.
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	* @return the number of states without SVS, or NULL_STATE if extraStates is negative
	*/
	FSMLIB_API state_t SVS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using appended Characterizing Set.
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences, 
	*		 TS will be empty if extraStates is negative
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	*/
	FSMLIB_API void W_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	
	/**
	* Designs a test suite in which all transitions are confirmed
	* using appended State Characterizing Set of the end state
	* and all states are confirmed using Characterizing Set
	* that consists of all used SCSets.
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences,
	*		 TS will be empty if extraStates is negative
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	*/
	FSMLIB_API void Wp_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	
	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using appended Harmonized State Identifier of the related state.
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences,
	*		 TS will be empty if extraStates is negative
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	*/
	FSMLIB_API void HSI_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/**
	* Designs a test suite in which all state pairs are distinguished and thus confirmed
	* using a separating sequence that is chosen adaptively to minimize the length of TS.
	* Transitions are confirmed in the same manner, i.e. end states are distinguished from all states.
	*
	* Source:
	* InCollection (dorofeeva2005improved)
	* Dorofeeva, R.; El-Fakih, K. & Yevtushenko, N. 
	* An improved conformance testing method 
	* Formal Techniques for Networked and Distributed Systems-FORTE 2005, Springer, 2005, 204-218
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences,
	*		 TS will be empty if extraStates is negative
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	*/
	FSMLIB_API void H_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/**
	* Designs a test suite in which all states and transitions are confirmed
	* using Harmonized State Identifiers that are appended to already confirmed states
	*
	* Source:
	* Article (simao2012reducing)
	* Simao, A.; Petrenko, A. & Yevtushenko, N.
	* On reducing test length for fsms with extra states 
	* Software Testing, Verification and Reliability, Wiley Online Library, 2012, 22, 435-454
	*
	* @param fsm - Deterministic FSM
	* @param TS - Test Suite to fill up with test sequences,
	*		 TS will be empty if extraStates is negative
	* @param extraStates - how many extra states shall be considered,
	*		 default is no extra state, needs to be positive or 0
	*/
	FSMLIB_API void SPY_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/// TODO
	//FSMLIB_API void FF_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	//FSMLIB_API void SC_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	//FSMLIB_API void P_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/// Attempts
	//FSMLIB_API void GSPY_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);// is it correct?
	//FSMLIB_API void SPYH_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);// is it correct?
	FSMLIB_API bool Mra_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API bool Mrstar_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0, std::string fileName = "");
	FSMLIB_API bool Mrg_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	
	//<-- Checking sequence methods -->//

	/// TODO
	//FSMLIB_API bool HrADS_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API bool D_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API bool AD_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API void DW_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API void DWp_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API int UIO_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API bool CSP_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	FSMLIB_API bool C_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	//FSMLIB_API bool K_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);

	// Attempts
	//FSMLIB_API void MHSI_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);// is it correct?
	FSMLIB_API bool Ma_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	FSMLIB_API bool Mstar_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0, std::string fileName = "");
	FSMLIB_API bool Mg_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	FSMLIB_API bool MgGA_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
}
