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
#define PTRandSTR(f) f, #f

extern bool showConjecture(const unique_ptr<DFSM>& conjecture);

namespace FSMlibTest
{
	TEST_CLASS(OPalgTests)
	{
	public:
		unique_ptr<DFSM> fsm;
		vector<pair<OP_CEprocessing, string>>	ceFunc;


		// TODO: incomplete machines

		OPalgTests()
		{
			ceFunc.emplace_back(PTRandSTR(AllGlobally));
			ceFunc.emplace_back(PTRandSTR(OneGlobally));
			ceFunc.emplace_back(PTRandSTR(OneLocally));
		}


		TEST_METHOD(TestOP_DFSM)
		{
			fsm = make_unique<DFSM>();
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestOP_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			/*/
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestOP_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm"); // too hard
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testOPalg(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			/*/
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestOP_DFA)
		{
			fsm = make_unique<DFA>();
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testOPalg(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testOPalgorithm(const unique_ptr<Teacher>& teacher, string teacherName, string filename, OP_CEprocessing processCE, string strCE) {
			auto model = ObservationPackAlgorithm(teacher, processCE, showConjecture);
			DEBUG_MSG("Reset: %d,\tOQ: %d,\tsymbols: %d,\tEQ: %d,\tCE: %s,\t%s\t%s%s\n", teacher->getAppliedResetCount(),
				teacher->getOutputQueryCount(), teacher->getQueriedSymbolsCount(), teacher->getEquivalenceQueryCount(), strCE.c_str(),
				teacherName.c_str(), filename.c_str(), (FSMmodel::areIsomorphic(fsm, model) ? "" : "\tNOT LEARNED"));
			//ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, model), "Learned model is different to the specification.");
		}

		void testOPalg(string filename) {
			fsm->load(filename);

			for (size_t i = 0; i < ceFunc.size(); i++) {
				unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
				testOPalgorithm(teacher, "TeacherDFSM", filename, ceFunc[i].first, ceFunc[i].second);
			}

			for (size_t i = 0; i < ceFunc.size(); i++) {
				unique_ptr<Teacher> teacher = make_unique<TeacherRL>(fsm);
				testOPalgorithm(teacher, "TeacherRL", filename, ceFunc[i].first, ceFunc[i].second);
			}

			for (size_t i = 0; i < ceFunc.size(); i++) {
				shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
				unique_ptr<Teacher> teacher = make_unique<TeacherBB>(bb, bind(FSMtesting::HSI_method, placeholders::_1, placeholders::_2,
					bind(FSMsequence::getHarmonizedStateIdentifiersFromSplittingTree, placeholders::_1,
					bind(FSMsequence::getSplittingTree, placeholders::_1, true, false), false)), 3);
				testOPalgorithm(teacher, "BlackBoxDFSM, TeacherBB:HSI_method (3 extra states)", filename, ceFunc[i].first, ceFunc[i].second);
			}
		}
	};
}
