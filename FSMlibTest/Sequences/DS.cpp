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
	TEST_CLASS(DS)
	{
	public:
		DFSM * fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestDS_DFSM)
		{
			DFSM dfsm;
			fsm = &dfsm;
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm", ADS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", CSet_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm", SVS_FOUND);
		}

		TEST_METHOD(TestDS_Mealy)
		{
			Mealy mealy;
			fsm = &mealy;
			/*
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm", SVS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm", SVS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm", ADS_FOUND);
			//*/
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm", ADS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm", SVS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", CSet_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm", SVS_FOUND);
			//*/
		}
		
		TEST_METHOD(TestDS_Moore)
		{
			Moore moore;
			fsm = &moore;
			/*
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm", CSet_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm", ADS_FOUND);
			//*/
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm", ADS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", CSet_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm", SVS_FOUND);
			//*/
		}

		TEST_METHOD(TestDS_DFA)
		{
			DFA dfa;
			fsm = &dfa;
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm", ADS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", CSet_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm", PDS_FOUND);
			testGetDistinguishingSequences(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm", SVS_FOUND);
		}

		void testPDS(sequence_in_t& pDS) {
			DEBUG_MSG("Preset DS (len %d): %s\n", pDS.size(), FSMmodel::getInSequenceAsString(pDS).c_str());
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

		void printADS(const unique_ptr<AdaptiveDS>& node, int base = 0) {
			if (node->currentStates.size() == 1) {
				DEBUG_MSG(": %u => %u\n", node->initialStates[0], node->currentStates[0]);
			}
			else {
				DEBUG_MSG(" -> %s\n", FSMmodel::getInSequenceAsString(node->input).c_str());
				for (map<output_t, unique_ptr<AdaptiveDS>>::iterator it = node->decision.begin();
					it != node->decision.end(); it++) {
					DEBUG_MSG("%*u", base + 3, it->first);
					printADS(it->second, base + 3);
				}
			}
		}

		void testADS(const unique_ptr<AdaptiveDS>& ads) {
			DEBUG_MSG("Adaptive DS:\n");
			printADS(ads);
			set<state_t> states;
			vector<bool> dist(fsm->getNumberOfStates(), false);
			// check initial and current states of root
			ARE_EQUAL(state_t(ads->initialStates.size()), fsm->getNumberOfStates(),
				"Root node of ADS has not exactly the number of states in the set of initial states.");
			for (state_t i = 0; i < ads->initialStates.size(); i++) {
				ARE_EQUAL(true, states.insert(ads->initialStates[i]).second,
					"Root node of ADS has state %d in the initial states set more times.", ads->initialStates[i]);
			}

			states.clear();
			ARE_EQUAL(state_t(ads->currentStates.size()), fsm->getNumberOfStates(),
				"Root node of ADS has not exactly the number of states in the set of current states.");
			for (state_t i = 0; i < ads->currentStates.size(); i++) {
				ARE_EQUAL(true, states.insert(ads->currentStates[i]).second,
					"Root node of ADS has state %d in the initial states set more times.", ads->currentStates[i]);
			}

			queue<AdaptiveDS*> fifo;
			fifo.push(ads.get());
			while (!fifo.empty()) {
				auto ads = fifo.front();
				fifo.pop();
				if (ads->currentStates.size() == 1) continue;
				states.clear();
				for (map<output_t, unique_ptr<AdaptiveDS>>::iterator it = ads->decision.begin(); it != ads->decision.end(); it++) {
					it->second->input.insert(it->second->input.begin(), ads->input.begin(), ads->input.end());
					ARE_EQUAL(it->second->currentStates.size(), it->second->initialStates.size(),
						"Node with entire input %s has current and initial sets of different size.",
						FSMmodel::getInSequenceAsString(it->second->input).c_str());
					for (state_t i = 0; i < it->second->initialStates.size(); i++) {
						ARE_EQUAL(it->second->currentStates[i], fsm->getEndPathState(it->second->initialStates[i], ads->input),
							"Node with entire input %s, initial state %d does not go to current state %d",
							FSMmodel::getInSequenceAsString(it->second->input).c_str(), it->second->initialStates[i],
							it->second->currentStates[i]);
						ARE_EQUAL(fsm->getOutputAlongPath(it->second->initialStates[i], ads->input).back(), it->first,
							"Input sequence %S and initial state %d do not produce last output symbol %d",
							FSMmodel::getInSequenceAsString(ads->input).c_str(), it->second->initialStates[i], it->first);
						ARE_EQUAL(true, states.insert(it->second->initialStates[i]).second,
							"Node with entire input %s has more initial states with id %d in its children.",
							FSMmodel::getInSequenceAsString(ads->input).c_str(), it->second->initialStates[i]);
					}
					if (it->second->currentStates.size() != 1) {
						fifo.push(it->second.get());
					}
					else {
						ARE_EQUAL(false, bool(dist[it->second->initialStates[0]]),
							"Initial state %d has already been distinguished.", it->second->initialStates[0]);
						dist[it->second->initialStates[0]] = true;
					}
				}
				for (state_t i = 0; i < ads->initialStates.size(); i++) {
					ARE_EQUAL(1, int(states.count(ads->initialStates[i])),
						"Node with entire input %s has not distinguished its initial state %d",
						FSMmodel::getInSequenceAsString(ads->input).c_str(), ads->initialStates[i]);
				}
				ARE_EQUAL(states.size(), ads->initialStates.size(),
					"Node with entire input %s has less initial states then its children.",
					FSMmodel::getInSequenceAsString(ads->input).c_str());
			}
			for (state_t i : fsm->getStates()) {
				ARE_EQUAL(true, bool(dist[i]), "State %d was not distinguished", i);
			}
		}

		void testSVS(state_t state, sequence_in_t& sVS, bool hasVS = true) {
			DEBUG_MSG("%d: %s\n", state, FSMmodel::getInSequenceAsString(sVS).c_str());
			if (sVS.empty()) {
				ARE_EQUAL(false, hasVS, "FSM has State Verifying Sequence of state %d but it was not found.", state);
				auto sVS = getStateVerifyingSequence(fsm, state);
				ARE_EQUAL(true, sVS.empty(),
					"FSM has State Verifying Sequence %s of state %d but it was not found.",
					FSMmodel::getInSequenceAsString(sVS).c_str(), state);
				return;
			}
			
			set<state_t> actBlock, nextBlock;
			output_t output;
			for (state_t i : fsm->getStates()) {
				nextBlock.insert(i);
			}
			for (sequence_in_t::iterator sIt = sVS.begin(); sIt != sVS.end(); sIt++) {
				ARE_EQUAL(true, ((*sIt < fsm->getNumberOfInputs()) || (*sIt == STOUT_INPUT)),
					"Input %d is invalid", *sIt);
				if (nextBlock.size() > 1) {
					actBlock = nextBlock;
					nextBlock.clear();
				}
				else {
					sequence_in_t endVS(sIt, sVS.end());
					ARE_EQUAL(true, false, "Suffix %s is extra", FSMmodel::getInSequenceAsString(endVS).c_str());
				}
				output = fsm->getOutput(state, *sIt);
				state_t nextState = fsm->getNextState(state, *sIt);
				for (set<state_t>::iterator stateIt = actBlock.begin(); stateIt != actBlock.end(); stateIt++) {
					if (output == fsm->getOutput(*stateIt, *sIt)) {
						ARE_EQUAL(nextState == fsm->getNextState(*stateIt, *sIt), state == *stateIt,
							"State %d translates into the same state %d on input %d as the reference state %d",
							*stateIt, nextState, *sIt, state);
						nextBlock.insert(fsm->getNextState(*stateIt, *sIt));
					}
				}
				state = nextState;
			}
			if (nextBlock.size() > 1) {
				set<state_t>::iterator stateIt = actBlock.begin();
				string undistStates = FSMlib::Utils::toString(*stateIt);
				for (++stateIt; stateIt != actBlock.end(); stateIt++) {
					undistStates += ", " + FSMlib::Utils::toString(*stateIt);
				}
				ARE_EQUAL(true, false, "States %s are not distinguished.", undistStates.c_str());
			}
		}

		void testVSet(sequence_vec_t& VSet, bool mustHasSVS = true) {
			DEBUG_MSG("State Verifying Set:\n");
			ARE_EQUAL(state_t(VSet.size()), fsm->getNumberOfStates(),
				"FSM has %d states but only %d sequences were received.",
				fsm->getNumberOfStates(), VSet.size());
			auto states = fsm->getStates();
			for (state_t state = 0; state < VSet.size(); state++) {
				testSVS(states[state], VSet[state], mustHasSVS);
			}
		}

		void testSCSet(state_t stateIdx, sequence_set_t& sCSet) {
			state_t N = fsm->getNumberOfStates();
			vector<bool> distinguished(N, false);
			vector<state_t> states = fsm->getStates();
			bool distinguishState, hasMinLen;
			sequence_out_t outS, outSWithoutLast, outI;
			//DEBUG_MSG("State Characterizing Set of state %d:\n", states[stateIdx]);
			DEBUG_MSG("%d:", states[stateIdx]);
			for (sequence_set_t::iterator sIt = sCSet.begin(); sIt != sCSet.end(); sIt++) {
				DEBUG_MSG(" %s\n", FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(false, sIt->empty(), "Empty sequence is not possible!");
				outSWithoutLast = outS = fsm->getOutputAlongPath(states[stateIdx], *sIt);
				outSWithoutLast.pop_back();
				distinguishState = hasMinLen = false;
				for (state_t i = 0; i < N; i++) {
					if (i != stateIdx) {
						outI = fsm->getOutputAlongPath(states[i], *sIt);
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
			distinguished[stateIdx] = true;
			for (state_t i = 0; i < N; i++) {
				ARE_EQUAL(true, bool(distinguished[i]), "State[%d] %d is not distinguished from fixed state[%d] %d.",
					i, states[i], stateIdx, states[stateIdx]);
			}
		}

		void testSCSets(vector<sequence_set_t>& SCSets) {
			DEBUG_MSG("State Characterizing Sets:\n");
			ARE_EQUAL(state_t(SCSets.size()), fsm->getNumberOfStates(),
				"FSM has %d states but only %d sets were received.",
				fsm->getNumberOfStates(), SCSets.size());
			for (state_t i = 0; i < SCSets.size(); i++) {
				testSCSet(i, SCSets[i]);
			}
		}

		void testCSet(sequence_set_t& CSet) {
			state_t N = fsm->getNumberOfStates();
			vector < vector<bool> > distinguished(N - 1);
			vector<state_t> states = fsm->getStates();
			bool distinguishState, hasMinLen;
			sequence_out_t outS, outSWithoutLast, outI;
			for (state_t state = 0; state < N - 1; state++) {
				distinguished[state].resize(N, false);
			}
			DEBUG_MSG("Characterizing Set:\n");
			for (sequence_set_t::iterator sIt = CSet.begin(); sIt != CSet.end(); sIt++) {
				DEBUG_MSG("%s\n", FSMmodel::getInSequenceAsString(*sIt).c_str());
				ARE_EQUAL(false, sIt->empty(), "Empty sequence is not possible!");

				distinguishState = hasMinLen = false;
				for (state_t state = 0; state < N - 1; state++) {
					outSWithoutLast = outS = fsm->getOutputAlongPath(states[state], *sIt);
					outSWithoutLast.pop_back();
					for (state_t i = state + 1; i < N; i++) {
						outI = fsm->getOutputAlongPath(states[i], *sIt);
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
					ARE_EQUAL(true, bool(distinguished[state][i]), "State[%d] %d is not distinguished from state[%d] %d.",
						i, states[i], state, states[state]);
				}
			}
		}


		void testGetDistinguishingSequences(string filename, int seqVal = CSet_FOUND) {
			fsm->load(filename);
			sequence_in_t pDS;
			unique_ptr<AdaptiveDS> aDS;
			sequence_vec_t SVS;
			vector<sequence_set_t> SCSets;
			sequence_set_t CSet;
			int retVal = getDistinguishingSequences(fsm, pDS, aDS, SVS, SCSets, CSet,
				getStatePairsShortestSeparatingSequences, false, reduceSCSet_EqualLength, reduceCSet_EqualLength);
			DEBUG_MSG("Found DS type of %s: %d\n", filename.c_str(), retVal);
			if (retVal > seqVal) {
				switch (seqVal) {
				case PDS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has preset DS but it was not found.");
				}
				case ADS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has adaptive DS but it was not found.");
				}
				case SVS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has SVS for each state but they were not found.");
				}
				break;
				}
			}
			else if (retVal < seqVal) {
				switch (retVal) {
				case PDS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has not preset DS but it was found.");
				}
				case ADS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has not adaptive DS but it was found.");
				}
				case SVS_FOUND:
				{
					ARE_EQUAL(true, false, "FSM has not SVS for each state but they were found.");
				}
				break;
				}
			}
			switch (retVal) {
			case PDS_FOUND:
				testPDS(pDS);
			case ADS_FOUND:
				testADS(aDS);
			case SVS_FOUND:
				testVSet(SVS);
			case CSet_FOUND:
				if (retVal == CSet_FOUND) testVSet(SVS, false);
				testCSet(CSet);
				testSCSets(SCSets);
			}
		}
	};
}
