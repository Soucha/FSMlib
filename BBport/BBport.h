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

#ifdef BBPORT_EXPORTS
#define BBPORT_API __declspec(dllexport)
#else
#define BBPORT_API __declspec(dllimport)
#endif

#include "../FSMlib/FSMlib.h"

#define RESET_INPUT			input_t(-3)
#define LEARNING_COMPLETED	input_t(-4)
#define RESPONSE_REQUESTED	input_t(-5)

extern "C" {
	BBPORT_API int initLearning(bool isResettable, input_t numberOfInputs, state_t maxExtraStates);

	//BBPORT_API input_t getNextInput(int id);
	//BBPORT_API void setResponse(int id, output_t output);
	BBPORT_API input_t updateAndGetNextInput(int id, output_t output);
}
