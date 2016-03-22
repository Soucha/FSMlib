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

#ifdef FSMLIB_EXPORTS
#define FSMLIB_API __declspec(dllexport)
#else
#define FSMLIB_API __declspec(dllimport)

#include "Model\FSMmodel.h"
#endif

namespace FSMlib {
#define ERROR_MESSAGE_MAX_LEN	250
#define ERROR_MESSAGE(format, ...) {\
	char msg[ERROR_MESSAGE_MAX_LEN];\
	_snprintf_s(msg, ERROR_MESSAGE_MAX_LEN, _TRUNCATE, format, ##__VA_ARGS__); \
	FSMlib::noticeListeners(msg); }
	
	FSMLIB_API void displayErrorMsgOnCerr(const char* msg);
	static void(*errorMsgHandler)(const char*) = displayErrorMsgOnCerr;
	void noticeListeners(const char*);// calls errorMsgHandler with given message

	FSMLIB_API void setErrorMsgHandler(void(*userErrorMsgHandler)(const char*));
}
