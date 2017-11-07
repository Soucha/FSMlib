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
#pragma warning (disable:4503)

#include <stdio.h>
#include <iostream>
#include <chrono>
#include <fstream>
#include <cstring>
#include <math.h>
#ifdef _WIN32
#include <filesystem>
using namespace std::tr2::sys;
#else
#include <experimental/filesystem>
using namespace std::experimental::filesystem;
#endif

#include "../FSMlib/FSMlib.h"
#include "../FSMlib/Learning/FSMlearning.h"
#include "../FSMlib/Testing/FaultCoverageChecker.h"
#include "../FSMlib/PrefixSet.h"

using namespace FSMsequence;
using namespace FSMtesting;
using namespace FSMlearning;

#define DATA_PATH			string("../data/")
#define MINIMIZATION_DIR	string("tests/minimization/")
#define SEQUENCES_DIR		string("tests/sequences/")
#define EXAMPLES_DIR		string("examples/")
#define EXPERIMENTS_DIR		string("experiments/")
#define OUTPUT_GV			string(DATA_PATH + "tmp/output.gv").c_str()