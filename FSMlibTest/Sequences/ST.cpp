/* Copyright (c) Michal Soucha, 2017
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
	TEST_CLASS(ST)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestST_DFSM)
		{
			fsm = make_unique<DFSM>();
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm");
		}

		TEST_METHOD(TestST_Mealy)
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
			//*/
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestST_Moore)
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
			//*/
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			groupTest(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm");
			//*/
		}

		TEST_METHOD(TestST_DFA)
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
			testGetSplittingTree(filename);
			testGetStatePairsSepSeqsFromST("");
			testGetSepSeqFromST("", (rand() % fsm->getNumberOfStates()));
			testGetHSIsFromST("");
			//testGetSeparatingSequences("");
		}

		void checkSTnode(const shared_ptr<st_node_t>& node, const unique_ptr<SplittingTree>& st, int base = 0) {
			if (node->block.size() == 1) {
				DEBUG_MSG("%u\n", node->block[0]);
				ARE_EQUAL(size_t(0), node->nextStates.size(), "NextStates of the leaf is not empty.");
				ARE_EQUAL(size_t(0), node->succ.size(), "The leaf has a successor.");
				ARE_EQUAL(size_t(0), node->sequence.size(), "The leaf has assigned a sequence.");
				ARE_EQUAL(size_t(0), node->undistinguishedStates, "The leaf has assigned the number of undistinguished states.");
				ARE_EQUAL(true, node == st->curNode[node->block[0]], "The current node of %d does not point to its leaf.", node->block[0]);
			}
			else {
				DEBUG_MSG("%u", node->block[0]);
				for (state_t i = 1; i < node->block.size(); i++) {
					DEBUG_MSG(",%u", node->block[i]);
					ARE_EQUAL(true, node->block[i - 1] < node->block[i], "States of the block are not in ascending order.");
				}
				DEBUG_MSG(" -%s-> %u", FSMmodel::getInSequenceAsString(node->sequence).c_str(), node->nextStates[0]);
				for (state_t i = 1; i < node->block.size(); i++) {
					DEBUG_MSG(",%u", node->nextStates[i]);
				}
				DEBUG_MSG(" (%u)\n", node->undistinguishedStates);
				set<state_t> diffStates, succDiffStates;
				for (state_t i = 0; i < node->block.size(); i++) {
					ARE_EQUAL(true, diffStates.insert(node->block[i]).second,
						"State %d is not unique in the current block.", node->block[i]);
					ARE_EQUAL(fsm->getEndPathState(node->block[i], node->sequence), node->nextStates[i],
						"State %d transfers to different state than %d on %s.", node->block[i], node->nextStates[i],
						FSMmodel::getInSequenceAsString(node->sequence).c_str());
				}
				size_t undistinguished(0);
				for (const auto& p : node->succ) {
					set<state_t> nextStates;
					DEBUG_MSG("%*u: ", base + 3, p.first);
					for (const auto& state : p.second->block) {
						ARE_EQUAL(true, succDiffStates.insert(state).second,
							"State %d is not unique in the successors.", state);
						ARE_EQUAL(p.first, fsm->getOutputAlongPath(state, node->sequence).back(),
							"State %d has different output to %s.", state, FSMmodel::getInSequenceAsString(node->sequence).c_str());
						if (!nextStates.insert(fsm->getEndPathState(state, node->sequence)).second) {
							undistinguished++;
						}
					}
					checkSTnode(p.second, st, base + 3);
				}
				ARE_EQUAL(true, diffStates == succDiffStates, "States are not divided into successors properly.");
				ARE_EQUAL(node->undistinguishedStates, undistinguished, "The number of undistinguished states is not correct.");
			}
		}

		void testGetSplittingTree(string filename) {
			if (!filename.empty())  fsm->load(filename);
			auto st = getSplittingTree(fsm, true, fsm->getType() == TYPE_DFSM);
			size_t numberOfStates(fsm->getNumberOfStates());
			DEBUG_MSG("Splitting Tree of %s:\n", filename.c_str());
			ARE_EQUAL(numberOfStates, st->rootST->block.size(),
				"The root of ST does not contain all states.");
			ARE_EQUAL(numberOfStates, st->curNode.size(),
				"The curNode of ST does not contain all states.");
			ARE_EQUAL(((numberOfStates - 1) * numberOfStates) / 2, st->distinguished.size(),
				"The distinguished of ST has not the right size.");
			for (state_t i = 0; i < numberOfStates; i++) {
				ARE_EQUAL(i, st->rootST->block[i], "The root of ST has not the states in block in ascending order.");
				ARE_EQUAL(true, bool(st->curNode[i]), "The current node of %d has no pointer assigned.", i);
				ARE_EQUAL(size_t(1), st->curNode[i]->block.size(), "The current node of %d does not point to a leaf.", i);
				ARE_EQUAL(i, st->curNode[i]->block[0], "The current node of %d does not point to its leaf.", i);
				for (state_t j = i + 1; j < numberOfStates; j++) {
					auto idx = getStatePairIdx(i, j);
					ARE_EQUAL(true, bool(st->distinguished[idx]), "The distinguished node of (%u,%u) has no pointer assigned.", i, j);
					const auto& node = st->distinguished[idx];
					ARE_EQUAL(true, binary_search(node->block.begin(), node->block.end(), i),
						"The distinguished node of (%u,%u) has not state %u in its block.", i, j, i);
					ARE_EQUAL(true, binary_search(node->block.begin(), node->block.end(), j),
						"The distinguished node of (%u,%u) has not state %u in its block.", i, j, j);
					auto outI = fsm->getOutputAlongPath(i, node->sequence);
					auto outJ = fsm->getOutputAlongPath(j, node->sequence);
					ARE_EQUAL(true, outI != outJ,
						"The responses of (%u,%u) to %s are the same - %s.", i, j,
						FSMmodel::getInSequenceAsString(node->sequence).c_str(),
						FSMmodel::getOutSequenceAsString(outI).c_str());
				}
			}
			checkSTnode(st->rootST, st);
		}

		void testGetSepSeqFromST(string filename, state_t state) {
			if (!filename.empty())  fsm->load(filename);
			set<state_t> states;
			for (state_t i = 0; i < fsm->getNumberOfStates(); i++) {
				states.insert(i);
			}
			if ((states.size() > 2) && (rand() % 2)) {
				state_t s = rand() % fsm->getNumberOfStates();
				if (s != state) states.erase(s);
			}
			auto st = getSplittingTree(fsm, true);
			auto seq = getSeparatingSequenceFromSplittingTree(fsm, st, state, states);
			auto refOut = fsm->getOutputAlongPath(state, seq);
			bool isSeparating = false;
			for (auto& s : states) {
				if (refOut != fsm->getOutputAlongPath(s, seq)) {
					isSeparating = true;
				}
			}
			ARE_EQUAL(true, isSeparating, "Obtained sequence %s does not separate any state from the state %u",
				FSMmodel::getInSequenceAsString(seq).c_str(), state);
		}

		void testGetStatePairsSepSeqsFromST(string filename) {
			if (!filename.empty())  fsm->load(filename);
			auto seq = getStatePairsSeparatingSequencesFromSplittingTree(fsm, false);
			DEBUG_MSG("Separating sequences of %s:\n", filename.c_str());
			state_t k = 0, N = fsm->getNumberOfStates();
			ARE_EQUAL(((N - 1) * N) / 2, state_t(seq.size()), "The number of state pairs is not equal to the number of separating sequences");
			for (state_t j = 1; j < N; j++) {
				for (state_t i = 0; i < j; i++, k++) {
					ARE_EQUAL(true,
						fsm->getOutputAlongPath(i, seq[k]) != fsm->getOutputAlongPath(j, seq[k]),
						"States %d and %d are not separated by %s", i, j, FSMmodel::getInSequenceAsString(seq[k]).c_str());
					DEBUG_MSG("%d x %d: %s\n", i, j, FSMmodel::getInSequenceAsString(seq[k]).c_str());
				}
			}
		}

		void testGetHSIsFromST(string filename) {
			if (!filename.empty())  fsm->load(filename);
			auto HCSet = getHarmonizedStateIdentifiers(fsm, getSplittingTree(fsm, true));
			state_t N = fsm->getNumberOfStates();
			FSMlib::PrefixSet pset;
			seq_len_t len(0);
			sequence_in_t test;
			sequence_in_t::iterator sIt;
			DEBUG_MSG("HSI family of %s:\n", filename.c_str());
			for (state_t state = 0; state < N; state++) {
				DEBUG_MSG("%d:", state);
				pset.clear();
				ARE_EQUAL(false, HCSet[state].empty(), "HSI of state %u is empty.", state);
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
	};
}