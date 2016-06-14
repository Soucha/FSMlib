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
	TEST_CLASS(TTTalgTests)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestTTT_DFSM)
		{
			fsm = make_unique<DFSM>();
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestTTT_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			/*/
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestTTT_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testTTTalg(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			/*/
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestTTT_DFA)
		{
			fsm = make_unique<DFA>();
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testTTTalg(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testTTTalgorithm(const unique_ptr<Teacher>& teacher, string teacherName, string filename) {
			auto model = TTT(teacher, showConjecture);
			DEBUG_MSG("Reset: %d,\tOQ: %d,\tsymbols: %d,\tEQ: %d,\t%s\t%s%s\n", teacher->getAppliedResetCount(),
				teacher->getOutputQueryCount(), teacher->getQueriedSymbolsCount(), teacher->getEquivalenceQueryCount(),
				teacherName.c_str(), filename.c_str(), (FSMmodel::areIsomorphic(fsm, model) ? "" : "\tNOT LEARNED"));
			//ARE_EQUAL(true, FSMmodel::areIsomorphic(fsm, model), "Learned model is different to the specification.");
		}

		void testTTTalg(string filename) {
			fsm->load(filename);

			unique_ptr<Teacher> teacher = make_unique<TeacherDFSM>(fsm, true);
			testTTTalgorithm(teacher, "TeacherDFSM", filename);

			teacher = make_unique<TeacherRL>(fsm);
			testTTTalgorithm(teacher, "TeacherRL", filename);

			shared_ptr<BlackBox> bb = make_shared<BlackBoxDFSM>(fsm, true);
			teacher = make_unique<TeacherBB>(bb, FSMtesting::SPY_method, 3);
			testTTTalgorithm(teacher, "BlackBoxDFSM, TeacherBB:SPY_method (3 extra states)", filename);
		}
	};
}
