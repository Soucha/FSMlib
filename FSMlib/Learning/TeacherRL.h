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

#include "TeacherDFSM.h"

/**
* A derived class describing behaviour of a Teacher that represents a regular language.
*/
class FSMLIB_API TeacherRL : public TeacherDFSM {
public:
	TeacherRL(const unique_ptr<DFSM>& blackBox, bool keepQueries = false) :
		TeacherDFSM(blackBox, true, keepQueries) {
	}

	virtual ~TeacherRL() {}

	bool isProvidedOnlyMQ() {
		return true;
	}
	
	sequence_out_t outputQuery(const sequence_in_t& inputSequence);
};
