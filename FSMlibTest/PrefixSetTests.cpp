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
#include "TestUtils.h"

using namespace FSMlib;

namespace FSMlibTest
{
	TEST_CLASS(PrefixSetTests)
	{
	public:
		
		TEST_METHOD(PrefixSetTest)
		{
			testPrefixSet();
		}

		TEST_METHOD(PrefixSetTest2)
		{
			testPrefixSet2();
		}

		void printMaxSeq(sequence_set_t &MS) {
			DEBUG_MSG("Set of %d sequences:\n", MS.size());
			seq_len_t len = 0;
			for (sequence_in_t cSeq : MS) {
				len += seq_len_t(cSeq.size());
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
			DEBUG_MSG("Total length %d\n", len);
		}

		void testPrefixSet() {
			PrefixSet pset;
			input_t a[] = { 0, 1, 0 };
			input_t b[] = { 0, 1, 0, 1 };
			input_t c[] = { 0, 1 };
			input_t d[] = { 0, 1, 1 };
			//input_t a [] = {0,1,0, 1,0, 0,1,0, 1, 0,1, 0,1,0, 0,0,1, 1,0,1,0, 0,0,0,1,0, 0,1,0,0, 0,1, 1,1,0,1};
			sequence_in_t ta(a, a + sizeof(a) / sizeof(input_t));
			sequence_in_t tb(b, b + sizeof(b) / sizeof(input_t));
			sequence_in_t tc(c, c + sizeof(c) / sizeof(input_t));
			sequence_in_t td(d, d + sizeof(d) / sizeof(input_t));

			sequence_set_t out;
			bool ret;
			int count;

			ret = pset.insert(ta);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(ta).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(1, count, "%d maximal sequences were obtained instead of 1", count);
			ret = pset.insert(tb);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(tb).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(1, count, "%d maximal sequences were obtained instead of 1", count);
			
			count = pset.contains(tc);
			ARE_EQUAL(int(tc.size()), count, "PS contains only %d symbols of sequence %s", count,
				FSMmodel::getInSequenceAsString(tc).c_str());
			count = pset.contains(td);
			ARE_EQUAL(2, count, "PS contains only %d symbols of sequence %s", count,
				FSMmodel::getInSequenceAsString(td).c_str());
			
			ret = pset.insert(tc);
			ARE_EQUAL(false, ret, "%s was inserted even though PS contains it", FSMmodel::getInSequenceAsString(tc).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(1, count, "%d maximal sequences were obtained instead of 1", count);
			ret = pset.insert(td);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(td).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(2, count, "%d maximal sequences were obtained instead of 2", count);
		}

		void testPrefixSet2() {
			PrefixSet pset;
			input_t b[] = { 0, 1, 0, 1 };
			input_t c[] = { 1, 1 };
			input_t d[] = { 0, 1, 1 };
			//input_t a [] = {0,1,0, 1,0, 0,1,0, 1, 0,1, 0,1,0, 0,0,1, 1,0,1,0, 0,0,0,1,0, 0,1,0,0, 0,1, 1,1,0,1};
			sequence_in_t ta;// (a, a + sizeof(a) / sizeof(input_t) );
			ta.push_back(STOUT_INPUT);
			sequence_in_t tb(b, b + sizeof(b) / sizeof(input_t));
			sequence_in_t tc(c, c + sizeof(c) / sizeof(input_t));
			sequence_in_t td(d, d + sizeof(d) / sizeof(input_t));

			sequence_set_t out;
			sequence_in_t seqOut;
			bool ret;
			int count;

			ret = pset.insert(ta);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(ta).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(1, count, "%d maximal sequences were obtained instead of 1", count);
			ret = pset.insert(tb);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(tb).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(2, count, "%d maximal sequences were obtained instead of 2", count);
			ret = pset.insert(tc);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(tc).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(3, count, "%d maximal sequences were obtained instead of 3", count);

			ARE_EQUAL(false, pset.empty(), "PS is not empty");
			ret = pset.popMaximalSequenceWithGivenPrefix(td.begin(), td.end(), seqOut);
			ARE_EQUAL(false, ret, "Maximal sequence %s found with given prefix %s.",
				FSMmodel::getInSequenceAsString(seqOut).c_str(), FSMmodel::getInSequenceAsString(td).c_str());
			ARE_EQUAL(true, seqOut.empty(), "Returned sequence %s is not empty",
				FSMmodel::getInSequenceAsString(seqOut).c_str());
			
			td.pop_back();
			ret = pset.popMaximalSequenceWithGivenPrefix(td.begin(), td.end(), seqOut);
			ARE_EQUAL(true, ret, "No maximal sequence with prefix %s was found", FSMmodel::getInSequenceAsString(td).c_str());
			// tb = td.td (0,1, 0,1) => seqOut = (0,1) = td
			ARE_EQUAL(true, td == seqOut, "Returned sequence %s differs from %s",
				FSMmodel::getInSequenceAsString(seqOut).c_str(), FSMmodel::getInSequenceAsString(td).c_str());

			pset.popMaximalSequence(seqOut);
			if (seqOut == ta) {
				pset.popMaximalSequence(seqOut);
				ARE_EQUAL(true, tc == seqOut, "Returned sequence %s differs from %s",
					FSMmodel::getInSequenceAsString(seqOut).c_str(), FSMmodel::getInSequenceAsString(tc).c_str());
			}
			else {
				ARE_EQUAL(true, tc == seqOut, "Returned sequence %s differs from %s",
					FSMmodel::getInSequenceAsString(seqOut).c_str(), FSMmodel::getInSequenceAsString(tc).c_str());
				pset.popMaximalSequence(seqOut);
				ARE_EQUAL(true, ta == seqOut, "Returned sequence %s differs from %s",
					FSMmodel::getInSequenceAsString(seqOut).c_str(), FSMmodel::getInSequenceAsString(ta).c_str());
			}
			ARE_EQUAL(true, pset.empty(), "PS is empty");
			
			ret = pset.insert(td);
			ARE_EQUAL(true, ret, "%s was not inserted", FSMmodel::getInSequenceAsString(td).c_str());
			pset.getMaximalSequences(out);
			count = int(out.size());
			printMaxSeq(out);
			ARE_EQUAL(1, count, "%d maximal sequences were obtained instead of 1", count);
			pset.clear();
			ret = pset.empty();
			ARE_EQUAL(true, pset.empty(), "PS is empty");
			pset.getMaximalSequences(out);
			count = int(out.size());
			ARE_EQUAL(0, count, "%d maximal sequences were obtained instead of 0", count);
		}
	};
}
