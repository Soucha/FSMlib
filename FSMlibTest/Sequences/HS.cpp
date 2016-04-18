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
	TEST_CLASS(HS)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestHS_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestHS_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			//*
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			/*/
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestHS_Moore)
		{
			Moore moore;
			fsm = &moore;
			//*
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm");
			//testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm"); // too hard
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGetPresetHomingS(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			/*/
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestHS_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGetPresetHomingS(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void testGetPresetHomingS(string filename, bool hasHS = true) {
			fsm->load(filename);
			sequence_in_t pHS;
			if (getPresetHomingSequence(fsm, pHS)) {
				DEBUG_MSG("HS of %s: %s\n", filename.c_str(), FSMmodel::getInSequenceAsString(pHS).c_str());
				ARE_EQUAL(true, hasHS, "FSM has not preset HS but it was found.");
				
				vector<pair<state_t, state_t> > actBlock;
				vector<vector<pair<state_t, state_t> > > sameOutput(fsm->getNumberOfOutputs() + 1);// +1 for DEFAULT_OUTPUT
				vector<output_t> outputUsed;
				output_t output;
				queue< vector<pair<state_t, state_t> > > fifo;
				int blocksCount;
				for (state_t state : fsm->getStates()) {
					actBlock.push_back(make_pair(state, state));
				}
				fifo.push(actBlock);
				for (sequence_in_t::iterator sIt = pHS.begin(); sIt != pHS.end(); sIt++) {
					ARE_EQUAL(true, ((*sIt < fsm->getNumberOfInputs()) || (*sIt == STOUT_INPUT)),
						"Input %d is invalid", *sIt);
					blocksCount = int(fifo.size());
					if (blocksCount == 0) {
						sequence_in_t endHS(sIt, pHS.end());
						ARE_EQUAL(true, false, "Suffix %s is extra", FSMmodel::getInSequenceAsString(endHS).c_str());
					}
					while (blocksCount > 0) {
						actBlock = fifo.front();
						fifo.pop();
						for (state_t state = 0; state < actBlock.size(); state++) {
							output = fsm->getOutput(actBlock[state].second, *sIt);
							if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
							ARE_EQUAL(true, output != WRONG_OUTPUT, "There is no transition from %d on %d", actBlock[state].second, *sIt);
							sameOutput[output].push_back(
								make_pair(actBlock[state].first, fsm->getNextState(actBlock[state].second, *sIt)));
							outputUsed.push_back(output);
						}
						for (output = 0; output < outputUsed.size(); output++) {
							if (!sameOutput[outputUsed[output]].empty()) {
								if (sameOutput[outputUsed[output]].size() > 1) {
									vector<pair<state_t, state_t> > tmp(sameOutput[outputUsed[output]]);
									fifo.push(tmp);
								}
								sameOutput[outputUsed[output]].clear();
							}
						}
						outputUsed.clear();
						blocksCount--;
					}
				}
				int pairs = 0;
				while (!fifo.empty()) {
					actBlock = fifo.front();
					fifo.pop();
					string undistinguishedStates = FSMlib::Utils::toString(actBlock[0].first) +
						"->" + FSMlib::Utils::toString(actBlock[0].second);
					for (state_t state = 1; state < actBlock.size(); state++) {
						if (actBlock[state].second != actBlock[0].second) {
							DEBUG_MSG("States %d and %d were not distinguished, they lead to states %d and %d\n",
								actBlock[0].first, actBlock[state].first, actBlock[0].second, actBlock[state].second);
							pairs++;
						}
					}
				}
				ARE_EQUAL(0, pairs, "%d pairs of states were not distinguished", pairs);
			}
			else {
				ARE_EQUAL(false, hasHS, "FSM has preset DS but it was not found.");
				ARE_EQUAL(true, pHS.empty(), "FSM has not preset DS but sequence %s was returned.",
					FSMmodel::getInSequenceAsString(pHS).c_str());
				DEBUG_MSG("HS of %s: NO\n", filename.c_str());
			}
		}
	};
}
