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
	TEST_CLASS(ADS)
	{
	public:
		unique_ptr<DFSM> fsm;

		// TODO: incomplete machines

		TEST_METHOD(TestADS_DFSM)
		{
			fsm = make_unique<DFSM>();
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_ADS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R4_SCSet.fsm", false);
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFSM_R5_SVS.fsm", false);
		}

		TEST_METHOD(TestADS_Mealy)
		{
			fsm = make_unique<Mealy>();
			/*
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100.fsm", false);
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_1.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R100_PDS_l99.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R10_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_HS.fsm", false);
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R4_SS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Mealy_R6_ADS.fsm");
			//*/
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_ADS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_HS.fsm", false);
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SCSet.fsm", false);
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Mealy_R4_SVS.fsm", false);
			//*/
		}

		TEST_METHOD(TestADS_Moore)
		{
			fsm = make_unique<Moore>();
			/*
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100.fsm", false);
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS.fsm"); 
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R100_PDS_l99.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R10_PDS_E.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_HS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R4_SS.fsm");
			testGetAdaptiveDS(DATA_PATH + SEQUENCES_DIR + "Moore_R6_ADS.fsm");
			//*/
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_ADS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_HS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SCSet.fsm", false);
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R4_SS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "Moore_R5_SVS.fsm", false);
			//*/
		}

		TEST_METHOD(TestADS_DFA)
		{
			fsm = make_unique<DFA>();
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_ADS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_HS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_PDS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SCSet.fsm", false);
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R4_SS.fsm");
			testGetAdaptiveDS(DATA_PATH + EXAMPLES_DIR + "DFA_R5_SVS.fsm", false);
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

		void checkInputValidity(sequence_in_t& seq) {
			for (auto i : seq) {
				ARE_EQUAL(true, ((i < fsm->getNumberOfInputs()) || (i == STOUT_INPUT)),
					"Node with input %s contains invalid input %d.",
					FSMmodel::getInSequenceAsString(seq).c_str(), i); 
			}
		}

		void testGetAdaptiveDS(string filename, bool hasDS = true) {
			fsm->load(filename);
			auto ads = getAdaptiveDistinguishingSequence(fsm);
			if (ads) {
				DEBUG_MSG("Adaptive DS of %s:\n", filename.c_str());
				printADS(ads);
				ARE_EQUAL(true, hasDS, "FSM has not adaptive DS but it was found.");
				auto d = getAdaptiveDistinguishingSet(fsm, ads);
				ARE_EQUAL(fsm->getNumberOfStates(), state_t(d.size()), "ADSet has not exactly %d sequences.", fsm->getNumberOfStates());
				for (state_t state = 0; state < fsm->getNumberOfStates(); state++) {
					auto out = fsm->getOutputAlongPath(state, d[state]);
					DEBUG_MSG("%d: %s -> %s\n", state, FSMmodel::getInSequenceAsString(d[state]).c_str(), FSMmodel::getOutSequenceAsString(out).c_str());
					for (state_t i = 0; i < fsm->getNumberOfStates(); i++) {
						if (state != i) {
							ARE_EQUAL(true, out != fsm->getOutputAlongPath(i, d[state]), "States %d and %d are not distinguished by %s",
								state, i, FSMmodel::getInSequenceAsString(d[state]).c_str());
						}
					}
				}

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

				//checkInputValidity(ads->input);
				queue<AdaptiveDS*> fifo;
				fifo.push(ads.get());
				while (!fifo.empty()) {
					auto ads = fifo.front();
					fifo.pop();
					if (ads->currentStates.size() == 1) continue;
					states.clear();
					for (map<output_t, unique_ptr<AdaptiveDS>>::iterator it = ads->decision.begin(); it != ads->decision.end(); it++) {
						//checkInputValidity(it->second->input);
						it->second->input.insert(it->second->input.begin(),	ads->input.begin(), ads->input.end());
						ARE_EQUAL(it->second->currentStates.size(), it->second->initialStates.size(),
							"Node with entire input %s has current and initial sets of different size.",
							FSMmodel::getInSequenceAsString(it->second->input).c_str());
						for (state_t i = 0; i < it->second->initialStates.size(); i++) {
							ARE_EQUAL(it->second->currentStates[i],	fsm->getEndPathState(it->second->initialStates[i], ads->input),
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
			else {
				ARE_EQUAL(false, hasDS, "FSM has adaptive DS but it was not found.");
				if (ads) {
					printADS(ads);
					ARE_EQUAL(false, true, "FSM has not adaptive DS but it was found.");
				}
				DEBUG_MSG("ADS of %s: NO\n", filename.c_str());
			}
		}

	};
}
