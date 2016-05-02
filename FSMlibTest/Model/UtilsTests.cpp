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
#include "../TestUtils.h"

namespace FSMlibTest
{
	TEST_CLASS(UtilsTests)
	{
	public:

		unique_ptr<DFSM> fsm;
		
		TEST_METHOD(Utils_hashCode) {
			int len = 5;
			string s = FSMlib::Utils::hashCode(len);
			string t = FSMlib::Utils::hashCode(len);
			ARE_EQUAL(int(s.size()), len, "Hash code %s has not the lenght of %d.", s.c_str(), len);
			ARE_EQUAL(int(t.size()), len, "Hash code %s has not the lenght of %d.", s.c_str(), len);
			ARE_EQUAL(true, s != t, "Hash codes %s and %s are equal.", s.c_str(), t.c_str());
			len = 3;
			s = FSMlib::Utils::hashCode(len);
			ARE_EQUAL(int(s.size()), len, "Hash code %s has not the lenght of %d.", s.c_str(), len);
		}
	
		TEST_METHOD(GetInSeqStr) {
			sequence_in_t seq = { EPSILON_INPUT, 1, 2, STOUT_INPUT, 3 };
			string s = FSMmodel::getInSequenceAsString(seq);
			string e = EPSILON_SYMBOL;
			e += ",1,2,";
			e += STOUT_SYMBOL;
			e += ",3";
			ARE_EQUAL(e, s, "Strings are not equal");
			seq.pop_back();
			s = FSMmodel::getInSequenceAsString(seq);
			ARE_EQUAL(false, e == s, "Strings %s and %s are equal", s.c_str(), e.c_str());
			e.pop_back();
			e.pop_back();
			ARE_EQUAL(e, s, "Strings are not equal");
		}

		TEST_METHOD(GetOutSeqStr) {
			sequence_out_t seq = { DEFAULT_OUTPUT, 1, 2, WRONG_OUTPUT, 3 };
			string s = FSMmodel::getOutSequenceAsString(seq);
			string e = DEFAULT_OUTPUT_SYMBOL;
			e += ",1,2,";
			e += WRONG_OUTPUT_SYMBOL;
			e += ",3";
			ARE_EQUAL(e, s, "Strings are not equal");
			seq.pop_back();
			s = FSMmodel::getOutSequenceAsString(seq);
			ARE_EQUAL(false, e == s, "Strings %s and %s are equal", s.c_str(), e.c_str());
			e.pop_back();
			e.pop_back();
			ARE_EQUAL(e, s, "Strings are not equal");
		}

		TEST_METHOD(CreateImageDFSM) {
			fsm = make_unique<DFSM>();
			tCreateImage();
		}

		TEST_METHOD(CreateImageMealy) {
			fsm = make_unique<Mealy>();
			tCreateImage();
		}

		TEST_METHOD(CreateImageMoore) {
			fsm = make_unique<Moore>();
			tCreateImage();
		}

		TEST_METHOD(CreateImageDFA) {
			fsm = make_unique<DFA>();
			tCreateImage();
		}

		void tCreateImage() {
			fsm->generate(5, 3, 2);
			string path = DATA_PATH;
			path += "tmp/";
			auto filename = fsm->writeDOTfile(path);
			ARE_EQUAL(true, !filename.empty(), "This %s cannot be saved into path '%s' in DOT format.",
				machineTypeNames[fsm->getType()], path.c_str());
			ARE_EQUAL(true, FSMmodel::createGIF(filename, true), "GIF was not created/shown for %s", filename.c_str());
			ARE_EQUAL(true, FSMmodel::createJPG(filename, true), "JPG was not created/shown for %s", filename.c_str());
			ARE_EQUAL(true, FSMmodel::createPNG(filename, true), "PNG was not created/shown for %s", filename.c_str());
		}
	};
}
