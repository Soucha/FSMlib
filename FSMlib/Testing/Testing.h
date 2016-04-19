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

	FSMLIB_API bool PDS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API bool ADS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API int SVS_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API void W_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API void Wp_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API void HSI_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API void H_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	FSMLIB_API void SPY_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/// TODO
	//FSMLIB_API void FF_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	//FSMLIB_API void SC_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);
	//FSMLIB_API void P_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);

	/// Attempts
	FSMLIB_API void GSPY_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);// is it correct?
	FSMLIB_API void SPYH_method(DFSM* fsm, sequence_set_t & TS, int extraStates = 0);// is it correct?
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
	FSMLIB_API void MHSI_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);// is it correct?
	FSMLIB_API bool Ma_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	FSMLIB_API bool Mstar_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0, std::string fileName = "");
	FSMLIB_API bool Mg_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
	FSMLIB_API bool MgGA_method(DFSM* fsm, sequence_in_t & CS, int extraStates = 0);
}
