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

#include "CppUnitTest.h"
#include "../FSMlib/FSMlib.h"

using namespace std;

#define INIT_SUITE \
	char msg[250]; \
	void writeErrorMsg(const char* errorMsg) {\
		Logger::WriteMessage(errorMsg);\
	}\
	TEST_MODULE_INITIALIZE(ModuleInitialize) {\
		FSMlib::setErrorMsgHandler(writeErrorMsg);\
	}

#define ARE_EQUAL(expected, actual, format, ...) \
	_snprintf_s(msg, 250, format, ##__VA_ARGS__); \
	Assert::AreEqual(expected, actual, ToString(msg).c_str(), LINE_INFO());

#define DEBUG_MSG(format, ...) \
	_snprintf_s(msg, 250, format, ##__VA_ARGS__); \
	Logger::WriteMessage(msg);

