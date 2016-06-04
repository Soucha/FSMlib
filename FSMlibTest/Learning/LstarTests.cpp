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

#define OUTPUT_GV string(string(DATA_PATH) + "tmp/output.gv").c_str()
#define PTRandSTR(f) f, #f

bool showConjecture(const unique_ptr<DFSM>& conjecture) {
	auto fn = conjecture->writeDOTfile(string(DATA_PATH) + "tmp/");
	//char c;	cin >> c;
	remove(OUTPUT_GV);
	rename(fn.c_str(), OUTPUT_GV);
	return true;
}

namespace FSMlibTest
{
	TEST_CLASS(LstarTests)
	{
	public:
		unique_ptr<DFSM> fsm;
		vector<pair<function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)>, string>>	ceFunc;

		// TODO: incomplete machines

		LstarTests()
		{
			ceFunc.emplace_back(PTRandSTR(addAllPrefixesToS));
			ceFunc.emplace_back(PTRandSTR(addSuffixAfterLastStateToE));
			ceFunc.emplace_back(PTRandSTR(addAllSuffixesAfterLastStateToE));
			ceFunc.emplace_back(PTRandSTR(addSuffix1by1ToE));
			ceFunc.emplace_back(PTRandSTR(addSuffixToE_binarySearch));
		}

		TEST_METHOD(TestLStar_DFSM)
		{
			fsm = make_unique<DFSM>();
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestLStar_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			//*/
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestLStar_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testLStarAllVariants(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			//*/
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestLStar_DFA)
		{
			fsm = make_unique<DFA>();
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testLStarAllVariants(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testLstar(const unique_ptr<Teacher>& teacher,
				function<void(const sequence_in_t& ce, ObservationTable& ot, const unique_ptr<Teacher>& teacher)> processCE,
				string fnName, string teacherName, string filename, bool checkConsistency = false) {
			auto model = Lstar(teacher, processCE, showConjecture, checkConsistency);
			DEBUG_MSG("Reset: %d,\tOQ: %d,\tsymbols: %d,\tEQ: %d,\t%s\t%s\t%s%s\n", teacher->getAppliedResetCount(),
				teacher->getOutputQueryCount(), teacher->getQueriedSymbolsCount(), teacher->getEquivalenceQueryCount(),
				fnName.c_str(), teacherName.c_str(), filename.c_str(), (FSMmodel::areIsomorphic(fsm, model) ? "" : "\tNOT LEARNED"));
			//ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, model), "Learned model is different to the specification.");
		}
		
		void testLStarAllVariants(string filename) {
			fsm->load(filename);
			
			for (size_t i = 0; i < ceFunc.size() - fsm->isOutputTransition(); i++) {
				unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
				testLstar(teacher, ceFunc[i].first, ceFunc[i].second, "TeacherDFSM", filename, (i == 0));
			}

			for (size_t i = 0; i < ceFunc.size() - fsm->isOutputTransition(); i++) {
				unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
				testLstar(teacher, ceFunc[i].first, ceFunc[i].second, "TeacherRL", filename, (i == 0));
			}

			for (size_t i = 0; i < ceFunc.size() - fsm->isOutputTransition(); i++) {
				shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
				unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method);
				testLstar(teacher, ceFunc[i].first, ceFunc[i].second, "BlackBoxDFSM, TeacherBB:SPY_method (3 extra states)", filename, (i == 0));
			}
		}
	};
}
