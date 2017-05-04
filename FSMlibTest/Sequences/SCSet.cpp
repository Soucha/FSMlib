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
	TEST_CLASS(SCSet)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestSCSet_DFSM)
		{
			fsm = make_unique<DFSM>();
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestSCSet_Mealy)
		{
			fsm = make_unique<Mealy>();
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

		TEST_METHOD(TestSCSet_Moore)
		{
			fsm = make_unique<Moore>();
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

		TEST_METHOD(TestSCSet_DFA)
		{
			fsm = make_unique<DFA>();
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm");
		}

		void groupTest(string filename) {
			testGetCharacterizingSet(filename);
			testGetStatesCharacterizingSets("");
			testGetStateCharacterizingSet("", (rand() % fsm->getNumberOfStates()));
			testGetHarmonizedStateIdentifiers("");
			//testGetSeparatingSequences("");
		}

		void testSCSet(state_t state, sequence_set_t& sCSet) {
			state_t N = fsm->getNumberOfStates();
			vector<bool> distinguished(N, false);
			bool distinguishState, hasMinLen;
			sequence_out_t outS, outSWithoutLast, outI;
			//DEBUG_MSG("State Characterizing Set of state %d:\n", states[state]);
			DEBUG_MSG("%d:", state);
			for (sequence_set_t::iterator sIt = sCSet.begin(); sIt != sCSet.end(); sIt++) {
				DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(false, sIt->empty(), "Empty sequence is not possible!");
				outSWithoutLast = outS = fsm->getOutputAlongPath(state, *sIt);
				outSWithoutLast.pop_back();
				distinguishState = hasMinLen = false;
				for (state_t i = 0; i < N; i++) {
					if (i != state) {
						outI = fsm->getOutputAlongPath(i, *sIt);
						if (outS != outI) {
							if (!distinguished[i]) {
								distinguishState = distinguished[i] = true;
								outI.pop_back();
								if (outSWithoutLast == outI) {
									hasMinLen = true;
								}
							}
						}
					}
				}
				ARE_EQUAL(true, distinguishState, "Sequence %s does not distinguished any states.",
					FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(true, hasMinLen, "Sequence %s could be shorter.",
					FSMmodel::getInSequenceAsString(*sIt).c_str());
			}
			distinguished[state] = true;
			for (state_t i = 0; i < N; i++) {
				ARE_EQUAL(true, bool(distinguished[i]), "State %d is not distinguished from fixed state %d.", i, state);
			}
		}

		void testGetStateCharacterizingSet(string filename, state_t state) {
			if (!filename.empty()) fsm->load(filename);
			auto sCSet = getStateCharacterizingSet(fsm, state, getStatePairsShortestSeparatingSequences, false, reduceSCSet_EqualLength);
			testSCSet(state, sCSet);
		}

		void testGetStatesCharacterizingSets(string filename) {
			if (!filename.empty()) fsm->load(filename);
			auto sCSets = getStatesCharacterizingSets(fsm, getStatePairsShortestSeparatingSequences, true, reduceSCSet_LS_SL);
			for (state_t i = 0; i < fsm->getNumberOfStates(); i++) {
				testSCSet(i, sCSets[i]);
			}
		}

		void testGetCharacterizingSet(string filename) {
			if (!filename.empty()) fsm->load(filename);
			auto CSet = getCharacterizingSet(fsm, getStatePairsShortestSeparatingSequences, true, reduceCSet_LS_SL);
			state_t N = fsm->getNumberOfStates();
			vector < vector<bool> > distinguished(N - 1);
			bool distinguishState, hasMinLen;
			sequence_out_t outS, outSWithoutLast, outI;
			for (state_t state = 0; state < N - 1; state++) {
				distinguished[state].resize(N, false);
			}
			DEBUG_MSG("Characterizing Set of %s:\n", filename.c_str());
			for (sequence_set_t::iterator sIt = CSet.begin(); sIt != CSet.end(); sIt++) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(false, sIt->empty(), "Empty sequence is not possible!");

				distinguishState = hasMinLen = false;
				for (state_t state = 0; state < N - 1; state++) {
					outSWithoutLast = outS = fsm->getOutputAlongPath(state, *sIt);
					outSWithoutLast.pop_back();
					for (state_t i = state + 1; i < N; i++) {
						outI = fsm->getOutputAlongPath(i, *sIt);
						if (outS != outI) {
							//DEBUG_MSG("%d-%d %u %s: %s %s %s\n", state, i, (distinguished[state][i] ? 1 : 0),
								//FSMmodel::getInSequenceAsString(*sIt).c_str(), FSMmodel::getOutSequenceAsString(outS).c_str(),
								//FSMmodel::getOutSequenceAsString(outSWithoutLast).c_str(), FSMmodel::getOutSequenceAsString(outI).c_str());
							if (!distinguished[state][i]) {
								distinguishState = distinguished[state][i] = true;
								outI.pop_back();
								if (outSWithoutLast == outI) {
									hasMinLen = true;
								}
							}
						}
					}
				}
				ARE_EQUAL(true, distinguishState, "Sequence %s does not distinguished any states.",
					FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(true, hasMinLen, "Sequence %s could be shorter.",
					FSMmodel::getInSequenceAsString(*sIt).c_str());
			}
			for (state_t state = 0; state < N - 1; state++) {
				for (state_t i = state + 1; i < N; i++) {
					ARE_EQUAL(true, bool(distinguished[state][i]), "State %d is not distinguished from state %d.", i, state);
				}
			}
		}

		void testGetHarmonizedStateIdentifiers(string filename) {
			if (!filename.empty()) fsm->load(filename);
			auto HCSet = getHarmonizedStateIdentifiers(fsm);
			state_t N = fsm->getNumberOfStates();
			FSMlib::PrefixSet pset;
			seq_len_t len(0);
			sequence_in_t test;
			sequence_in_t::iterator sIt;
			DEBUG_MSG("HSI family of %s:\n", filename.c_str());
			for (state_t state = 0; state < N; state++) {
				DEBUG_MSG("%d:", state);
				pset.clear();
				for (sequence_in_t seq : HCSet[state]) {
					DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(seq).c_str());
					pset.insert(seq);
				}
				for (state_t i = state + 1; i < N; i++) {
					for (sequence_in_t seq : HCSet[i]) {
						len = pset.contains(seq);
						if ((len > 0) && (len != seq_len_t(-1))) {
							sIt = seq.begin();
							test.clear();
							for (seq_len_t l = 0; l < len; l++) {
								test.push_back(*sIt);
								sIt++;
							}
							if (fsm->getOutputAlongPath(state, test) != fsm->getOutputAlongPath(i, test)) {
								break;
							}
						}
						len = seq_len_t(-1);
					}
					ARE_EQUAL(true, len != seq_len_t(-1), "States %d and %d have not common distinguishing sequence.", i, state);
				}
			}
		}

		void testGetSeparatingSequences(string filename) {
			if (!filename.empty()) fsm->load(filename);
			auto seq = getSeparatingSequences(fsm);
			state_t N = fsm->getNumberOfStates(), idx, nextStateI, nextStateJ, nextIdx;

			for (state_t i = 0; i < N - 1; i++) {
				for (state_t j = i + 1; j < N; j++) {
					idx = i * N + j - 1 - (i * (i + 3)) / 2;
					DEBUG_MSG("%u x %u (%u) - %d:\n", i, j, idx, seq[idx].minLen);
					ARE_EQUAL(true, (seq[idx].minLen > 0), "MinLen of SepS of states %d and %d is not positive.", i, j);
					bool dist = false;
					if (fsm->isOutputState()) {
						if (fsm->getOutput(i, STOUT_INPUT) != fsm->getOutput(j, STOUT_INPUT)) {
							ARE_EQUAL(seq_len_t(1), seq[idx].minLen,
								"States %d and %d have different outputs but minLen is %d.", i, j, seq[idx].minLen);
							dist = true;
						}
					}
					seq_len_t minVal = fsm->getNumberOfStates();
					input_t minIn = STOUT_INPUT;
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						DEBUG_MSG("  %d -> %d\n", input, seq[idx].next[input]);
						if (fsm->getOutput(i, input) != fsm->getOutput(j, input)) {
							ARE_EQUAL(seq_len_t(1), seq[idx].minLen,
								"States %d and %d have different outputs on %d but minLen is %d.", i, j, input, seq[idx].minLen);
							ARE_EQUAL(idx, seq[idx].next[input],
								"States %d and %d have different outputs on %d but the link goes to %d instead of their index %d.",
								i, j, input, seq[idx].next[input], idx);
							dist = true;
						}
						else {
							nextStateI = fsm->getNextState(i, input);
							nextStateJ = fsm->getNextState(j, input);
							if (nextStateI != nextStateJ) {
								nextIdx = (nextStateI < nextStateJ) ?
									(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
									(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
								if (nextIdx != idx) {
									ARE_EQUAL(nextIdx, seq[idx].next[input],
										"Pair of states %d, %d goes on %d to %d instead of %d.", 
										i, j, input, seq[idx].next[input], nextIdx);
									if (minVal > seq[nextIdx].minLen) {
										minVal = seq[nextIdx].minLen;
										minIn = input;
									}
								}
								else {
									ARE_EQUAL(NULL_STATE, seq[idx].next[input],
										"Pair of states %d, %d stays in the same pair on %d but the link goes to %d instead of NULL_STATE.",
										i, j, input, seq[idx].next[input]);
								}
								
							}
							else {
								ARE_EQUAL(NULL_STATE, seq[idx].next[input],
									"Pair of states %d, %d goes in the same next state [%d] on %d but the link goes to %d instead of NULL_STATE.",
									i, j, nextStateI, input, seq[idx].next[input]);
							}
						}
					}
					if (seq[idx].minLen == 1) {
						ARE_EQUAL(true, dist, "States %d and %d have same outputs on all inputs but minLen is 1.", i, j);
					} else {
						ARE_EQUAL(seq_len_t(minVal + 1), seq[idx].minLen,
							"States %d and %d have incorrect minLen of %d, best next pair has only %d.", i, j, seq[idx].minLen, minVal);
					}
				}
			}
		}
	};
}
