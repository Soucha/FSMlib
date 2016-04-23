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

using namespace FSMtesting;

namespace FSMlibTest
{
	TEST_CLASS(TScomp)
	{
	public:
		DFSM * fsm;
		//FaultCoverageChecker * fcChecker;

		// TODO: incomplete machines

		TEST_METHOD(TScomp_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TScomp_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			/*
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			/*/
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TScomp_Moore)
		{
			Moore moore;
			fsm = &moore;
			/*
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			groupTest(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			/*/
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TScomp_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void groupTest(string filename) {
			testingTSmethodComp(filename);
			//testingTSmethodComp("", 1);
		}

		void printTS(sequence_set_t & TS) {
			DEBUG_MSG("Set of %d test sequences:\n", TS.size());
			for (sequence_in_t cSeq : TS) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(cSeq).c_str());
			}
		}

		seq_len_t printInfo(sequence_set_t & TS, string method, int extraStates = 0, bool printSeq = false) {
			seq_len_t len = 0;
			for (sequence_in_t seq : TS) {
				len += seq_len_t(seq.size());
			}
			vector<DFSM*> indistinguishable;
			//fcChecker->getFSMs(TS, indistinguishable, extraStates);
			DEBUG_MSG("%s\t%d\t%d\t%d\n", method.c_str(), TS.size(), len, indistinguishable.size());
			ARE_EQUAL(0, int(indistinguishable.size()), "%s has not complete fault coverage, it produces %d indistinguishable FSMs.",
				method.c_str(), indistinguishable.size());
			if (printSeq) printTS(TS);
			return len;
		}

		seq_len_t printInfo(sequence_in_t & CS, string method, int extraStates = 0, bool printSeq = false) {
			vector<FSM*> indistinguishable;
			//fcChecker->getFSMs(CS, indistinguishable, extraStates);
			DEBUG_MSG("%s\t%d\t%d\t%d\n", method.c_str(), int(!CS.empty()), CS.size(), indistinguishable.size());
			ARE_EQUAL(0, int(indistinguishable.size()), "%s has not complete fault coverage, it produces %d indistinguishable FSMs.",
				method.c_str(), indistinguishable.size());
			if (printSeq) DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(CS).c_str());
			return seq_len_t(CS.size());
		}

		void comp2methods(int len1, int len2, string met1, string met2, string comp) {
			ARE_EQUAL(true, len1 <= len2, "%s should be better than %s but %s are %d and %d",
					met1.c_str(), met2.c_str(), comp.c_str(), len1, len2);
		}

		void testingCSmethodComp(string filename, int extraStates = 0) {
			if (!filename.empty()) fsm->load(filename);

			if (!FSMmodel::isStronglyConnected(fsm)) {
				DEBUG_MSG("Not strongly connected\n");
				return;
			}

			sequence_in_t CS_M, CS_C;
			int len_M, len_C;
			//fcChecker = new FaultCoverageChecker(fsm);

			bool hasADS;

			hasADS = C_method(fsm, CS_C, extraStates);
			hasADS = Ma_method(fsm, CS_M, extraStates);

			DEBUG_MSG("Comparison of testing CS methods on %s with %d extra states:\n", filename.c_str(), extraStates);
			DEBUG_MSG("ADS: %d\n", int(hasADS));
			DEBUG_MSG("Method\tlength\t#indistFSM\n");
			len_C = printInfo(CS_C, "C", extraStates);
			len_M = printInfo(CS_M, "M", extraStates);

			//delete fcChecker;
		}

		void testingTSmethodComp(string filename, int extraStates = 0) {
			if (!filename.empty()) fsm->load(filename);

			sequence_set_t TS_W, TS_Wp, TS_HSI, TS_PDS, TS_SVS, TS_H, TS_SPY, TS_ADS;
			seq_len_t len_W, len_Wp, len_HSI, len_PDS, len_SVS, len_H, len_SPY, len_ADS;

			//fcChecker = new FaultCoverageChecker(fsm);

			W_method(fsm, TS_W, extraStates);
			Wp_method(fsm, TS_Wp, extraStates);
			HSI_method(fsm, TS_HSI, extraStates);
			H_method(fsm, TS_H, extraStates);
			SPY_method(fsm, TS_SPY, extraStates);
			//Testing::GSPY_method(fsm, TS_GSPY, extraStates);
			bool hasPDS = PDS_method(fsm, TS_PDS, extraStates);
			bool hasADS = ADS_method(fsm, TS_ADS, extraStates);
			int statesWithoutSVS = SVS_method(fsm, TS_SVS, extraStates);

			DEBUG_MSG("Comparison of testing methods on %s with %d extra states:\n", filename.c_str(), extraStates);
			DEBUG_MSG("PDS: %d, ADS: %d, States without SVS: %d\n", int(hasPDS), int(hasADS), statesWithoutSVS);
			DEBUG_MSG("Method\t#seqs\tlength\t#indistFSM\n");
			len_PDS = printInfo(TS_PDS, "PDS", extraStates);
			len_ADS = printInfo(TS_ADS, "ADS", extraStates);
			len_SVS = printInfo(TS_SVS, "SVS", extraStates);
			len_W = printInfo(TS_W, "W", extraStates);
			len_Wp = printInfo(TS_Wp, "Wp", extraStates);
			len_HSI = printInfo(TS_HSI, "HSI", extraStates);
			len_H = printInfo(TS_H, "H", extraStates);
			len_SPY = printInfo(TS_SPY, "SPY", extraStates);
			//len_GSPY = printInfo(TS_GSPY, "GSPY", extraStates);

			/*
			comp2methods(len_Wp, len_W, "Wp", "W", "lengths");
			comp2methods(len_HSI, len_W, "HSI", "W", "lengths");
			comp2methods(len_HSI, len_Wp, "HSI", "Wp", "lengths");
			comp2methods(len_HSI, len_Wp, "HSI", "Wp", "lengths");
			comp2methods(len_HSI, len_Wp, "HSI", "Wp", "lengths");

			comp2methods(TS_Wp.size(), TS_W.size(), "Wp", "W", "#sequences");
			comp2methods(TS_HSI.size(), TS_W.size(), "HSI", "W", "#sequences");
			comp2methods(TS_HSI.size(), TS_Wp.size(), "HSI", "Wp", "#sequences");
			* */

			//delete fcChecker;
		}

	};
}