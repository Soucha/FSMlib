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

using namespace FSMlearning;

//#define OUTPUT_GV string(string(DATA_PATH) + "tmp/output.gv").c_str()

extern bool showConjecture(const unique_ptr<DFSM>& conjecture);

namespace FSMlibTest
{
	TEST_CLASS(GoodSplitTests)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestGoodSplit_DFSM)
		{
			fsm = make_unique<DFSM>();
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestGoodSplit_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm", 99);
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm", 3);
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm", 4);
			/*/
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm", 3);
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", 3);
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");// could require len of 3
			//*/
		}

		TEST_METHOD(TestGoodSplit_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			//testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm", 15);// too hard
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm", 98);
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGoodSplit(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm", 3);
			/*/
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestGoodSplit_DFA)
		{
			fsm = make_unique<DFA>();
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGoodSplit(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testGoodSplitAlgorithm(const unique_ptr<Teacher>& teacher, string teacherName, string filename, size_t maxLen) {
			auto model = GoodSplit(teacher, maxLen, showConjecture);
			DEBUG_MSG("Reset: %d,\tOQ: %d,\tsymbols: %d,\tEQ: %d,\tMaxLen: %d,\t%s\t%s%s\n", teacher->getAppliedResetCount(),
				teacher->getOutputQueryCount(), teacher->getQueriedSymbolsCount(), teacher->getEquivalenceQueryCount(), maxLen,
				teacherName.c_str(), filename.c_str(), (FSMmodel::areIsomorphic(fsm, model) ? "" : "\tNOT LEARNED"));
			//ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, model), "Learned model is different to the specification.");
		}

		void testGoodSplit(string filename, size_t maxLen = 2) {
			fsm->load(filename);

			unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
			testGoodSplitAlgorithm(teacher, "TeacherDFSM", filename, maxLen);

			teacher = make_unique<TeacherRL>(fsm);
			testGoodSplitAlgorithm(teacher, "TeacherRL", filename, maxLen);

			shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
			teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method, 3);
			testGoodSplitAlgorithm(teacher, "BlackBoxDFSM, TeacherBB:SPY_method (3 extra states)", filename, maxLen);
		}
	};
}
