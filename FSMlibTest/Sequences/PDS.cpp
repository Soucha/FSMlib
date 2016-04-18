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
	TEST_CLASS(PDS)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestPDS_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm", false);
		}

		TEST_METHOD(TestPDS_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			//*
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm", false);
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm", false);
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm", false);
			/*/
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm",false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm", false);
			//*/
		}

		TEST_METHOD(TestPDS_Moore)
		{
			Moore moore;
			fsm = &moore;
			//*
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm", false);
			//testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm"); // too hard
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGetPresetDS(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm", false);
			/*/
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm",false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm", false);
			//*/
		}

		TEST_METHOD(TestPDS_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", false);
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGetPresetDS(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm", false);
		}

		void testGetPresetDS(string filename, bool hasDS = true) {
			fsm->load(filename);
			sequence_in_t pDS;
			if (getPresetDistinguishingSequence(fsm, pDS)) {
				DEBUG_MSG("Preset DS of %s: %s\n", filename.c_str(), FSMmodel::getInSequenceAsString(pDS).c_str());
				ARE_EQUAL(true, hasDS, "FSM has not preset DS but it was found.");
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
				for (sequence_in_t::iterator sIt = pDS.begin(); sIt != pDS.end(); sIt++) {
					ARE_EQUAL(true, ((*sIt < fsm->getNumberOfInputs()) || (*sIt == STOUT_INPUT)),
						"Input %d is invalid", *sIt);
					blocksCount = int(fifo.size());
					if (blocksCount == 0) {
						sequence_in_t endDS(sIt, pDS.end());
						ARE_EQUAL(true, false, "Suffix %s is extra", FSMmodel::getInSequenceAsString(endDS).c_str());
					}
					while (blocksCount > 0) {
						actBlock = fifo.front();
						fifo.pop();
						state_t noTransitionFrom = NULL_STATE;
						for (state_t state = 0; state < actBlock.size(); state++) {
							output = fsm->getOutput(actBlock[state].second, *sIt);
							if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
							if (output == WRONG_OUTPUT) {
								ARE_EQUAL(NULL_STATE, noTransitionFrom, "States %d and %d have no transition on %d",
									noTransitionFrom, actBlock[state].second, *sIt);
								noTransitionFrom = actBlock[state].second;
							}
							else {
								sameOutput[output].push_back(
									make_pair(actBlock[state].first, fsm->getNextState(actBlock[state].second, *sIt)));
								outputUsed.push_back(output);
							}
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
				if (!fifo.empty()) {
					int blocks = int(fifo.size());
					while (!fifo.empty()) {
						actBlock = fifo.front();
						fifo.pop();
						string undistinguishedStates = FSMlib::Utils::toString(actBlock[0].first);
						for (state_t state = 1; state < actBlock.size(); state++) {
							undistinguishedStates += ", " + FSMlib::Utils::toString(actBlock[state].first);
						}
						DEBUG_MSG("States %s were not distinguished\n", undistinguishedStates.c_str());
					}
					ARE_EQUAL(true, false, "%d blocks of states were not distinguished", blocks);
				}
			}
			else {
				ARE_EQUAL(false, hasDS, "FSM has preset DS but it was not found.");
				ARE_EQUAL(true, pDS.empty(), "FSM has not preset DS but sequence %s was returned.",
					FSMmodel::getInSequenceAsString(pDS).c_str());
				DEBUG_MSG("PDS of %s: NO\n", filename.c_str());
			}
		}
	};
}
