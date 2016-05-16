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

#include "BlackBoxDFSM.h"
#include "BlackBox.h"
#include "TeacherBB.h"
#include "TeacherDFSM.h"
#include "Teacher.h"

namespace FSMlearning {
	FSMLIB_API void addSuffixToE(const sequence_in_t& ce, const map<sequence_in_t, vector<sequence_out_t>>& ot,
		vector<sequence_in_t>& S, vector<sequence_in_t>& E);

	FSMLIB_API unique_ptr<DFSM> Lstar(const unique_ptr<Teacher>& teacher, 
		function<void(const sequence_in_t& ce, const map<sequence_in_t, vector<sequence_out_t>>& ot,
			vector<sequence_in_t>& S, vector<sequence_in_t>& E)> processCounterexample,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel = nullptr,
		bool checkConsistency = false);
}
