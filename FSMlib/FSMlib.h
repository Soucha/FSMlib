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

#ifdef _WIN32
	#ifdef FSMLIB_EXPORTS
	#define FSMLIB_API __declspec(dllexport)
	#else
	#define FSMLIB_API __declspec(dllimport)

	#ifdef __CUDA_ARCH__
	#define PARALLEL_COMPUTING
	#endif

	#include "Model\FSMmodel.h"
	#include "Sequences\FSMsequence.h"
	#include "Testing\FSMtesting.h"
	#include "Testing\FaultCoverageChecker.h"
	#include "Learning\FSMlearning.h"

	#include "PrefixSet.h"
	#include "UnionFind.h"
	#endif
#else
	#define FSMLIB_API
#endif

namespace FSMlib {
#define ERROR_MESSAGE_MAX_LEN	250
#ifdef _WIN32
#define ERROR_MESSAGE(format, ...) {\
	char msg[ERROR_MESSAGE_MAX_LEN];\
	_snprintf_s(msg, ERROR_MESSAGE_MAX_LEN, _TRUNCATE, format, ##__VA_ARGS__); \
	FSMlib::noticeListeners(msg); }
#else
#define ERROR_MESSAGE(format, ...) {\
	char msg[ERROR_MESSAGE_MAX_LEN];\
	snprintf(msg, ERROR_MESSAGE_MAX_LEN, format, ##__VA_ARGS__); \
	FSMlib::noticeListeners(msg); }
#endif	
	/**
	* Writes given error message in std::cerr stream.
	*/
	FSMLIB_API void displayErrorMsgOnCerr(const char* msg);

	/**
	* Handles a function that displays error messages.
	*/
	static void(*errorMsgHandler)(const char*) = displayErrorMsgOnCerr;

	/**
	* Calls errorMsgHandler with given message.
	*/
	FSMLIB_API void noticeListeners(const char*);

	/**
	* Sets errorMsgHandler to point on arbitrary function that takes
	* a string as an argument. The function is called with a particular
	* error message if an error occurs.
	*/
	FSMLIB_API void setErrorMsgHandler(void(*userErrorMsgHandler)(const char*));

	namespace Utils {
		/**
		* Generates random sequence of alpha-numeric characters.
		* @param length of sequence
		* @return generated string
		*/
		FSMLIB_API std::string hashCode(int length);

		/**
		* Concatenates given name with generated hash and given suffix
		* so that new filename will be unique in given path.
		* @param name by which new file name begins
		* @param suffix of new file name
		* @param path to folder where new file will be
		* @return new unique file name
		*/
		FSMLIB_API std::string getUniqueName(std::string name, std::string suffix, std::string path = "");

		/**
		* Converts number to string.
		* @param number - integer value
		* @return number as string
		*/
		FSMLIB_API std::string toString(int number);
	}

}
