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
#include <iostream>

using namespace std;
using namespace Microsoft::VisualStudio::CppUnitTestFramework;

#define DATA_PATH		"../../data/"
#define MSG_MAX_LEN		250

/// this is needed in one <tests>.cpp file -> TestUtils.cpp
#define INIT_SUITE \
	TEST_MODULE_INITIALIZE(ModuleInitialize) {\
		FSMlib::setErrorMsgHandler(writeErrorMsg);\
	}
	
#define ARE_EQUAL(expected, actual, format, ...) \
	_snprintf_s(FSMlibTest::msg, MSG_MAX_LEN, _TRUNCATE, format, ##__VA_ARGS__); \
	Assert::AreEqual(expected, actual, ToString(FSMlibTest::msg).c_str(), LINE_INFO());

#define DEBUG_MSG(format, ...) \
	_snprintf_s(FSMlibTest::msg, MSG_MAX_LEN, _TRUNCATE, format, ##__VA_ARGS__); \
	Logger::WriteMessage(FSMlibTest::msg);

namespace FSMlibTest
{
	static char msg[MSG_MAX_LEN];
	static void writeErrorMsg(const char* errorMsg) {
		Logger::WriteMessage(errorMsg); 
	}
}
