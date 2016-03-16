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

#include "stdafx.h"

#include "DFA.h"

void DFA::incNumberOfOutputs(output_t byNum) {
	if (_numberOfOutputs + byNum > 2) {
		ERROR_MESSAGE("%s::incNumberOfOutputs - the number of outputs cannot be increased", machineTypeNames[_type]);
	} else {
		_numberOfOutputs += byNum;
	}
}

output_t DFA::getMaxOutputs(state_t numberOfStates, input_t numberOfInputs) {
	return 2;
}
