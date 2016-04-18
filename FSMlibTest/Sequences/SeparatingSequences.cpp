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

using namespace FSMsequence;

namespace FSMlibTest
{
	TEST_CLASS(SepSeq)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSepSeq_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestSepSeq_Mealy)
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

		TEST_METHOD(TestSepSeq_Moore)
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

		TEST_METHOD(TestSepSeq_DFA)
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

#define PTRandSTR(f) f, #f

		void groupTest(string filename) {
			testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences));
#ifdef PARALLEL_COMPUTING
			testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences_ParallelSF));
			testGetSeparatingSequences(filename, PTRandSTR(getStatePairsShortestSeparatingSequences_ParallelQueue));
#endif
		}

		void testGetSeparatingSequences(string filename, void(*getSepSeq)(DFSM * dfsm, vector<sequence_in_t> & seq), string name) {
			if (!filename.empty()) fsm->load(filename);
			vector<sequence_in_t> seq;
			(*getSepSeq)(fsm, seq);
			DEBUG_MSG("Separating sequences (%s) of %s:\n", name.c_str(), filename.c_str());
			vector<state_t> states = fsm->getStates();
			state_t k = 0, N = fsm->getNumberOfStates();
			ARE_EQUAL(((N - 1) * N) / 2, state_t(seq.size()), "The number of state pairs is not equal to the number of separating sequences");
			for (state_t i = 0; i < N - 1; i++) {
				for (state_t j = i + 1; j < N; j++, k++) {
					ARE_EQUAL(true,
						fsm->getOutputAlongPath(states[i], seq[k]) != fsm->getOutputAlongPath(states[j], seq[k]),
						"States %d [%d] and %d [%d] are not separated by %s", states[i], i, states[j], j,
						FSMmodel::getInSequenceAsString(seq[k]).c_str());
					DEBUG_MSG("%d[%d] x %d[%d]: %s\n", states[i], i, states[j], j, FSMmodel::getInSequenceAsString(seq[k]).c_str());
				}
			}
		}
	};
}
