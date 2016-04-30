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

#include <algorithm>

#include "FSMsequence.h"
#include "../PrefixSet.h"

namespace FSMsequence {
	int MAX_CLOSED = 1000000;
#if SEQUENCES_PERFORMANCE_TEST
	int closedCount, openCount;
	string testOut = "";
#endif // SEQUENCES_PERFORMANCE_TEST

	static unsigned int WEIGHT = 4;

	void setWeight(unsigned int weight) {
		WEIGHT = weight;
	}

	typedef set<state_t> block_t;
	typedef struct block_node_t block_node_t;
	typedef pair<output_t, block_node_t*> out_bn_ref_t;
	typedef pair<input_t, vector<out_bn_ref_t> > in_out_bn_ref_t;
	typedef set<block_node_t*> bn_partition_t;

	struct block_node_t {
		block_t states;
		seq_len_t h; // min length of input sequence to distinguish states
		vector<in_out_bn_ref_t> succ;
	};

	struct bncomp {

		bool operator() (const block_node_t* lbn, const block_node_t* rbn) const {
			return lbn->states < rbn->states;
		}
	};

	struct node_pds_t {
		bn_partition_t partition;
		sequence_in_t ds;
		seq_len_t value; // length of ds + max of block's heuristic
	};

	struct pds_heur_comp {

		bool operator() (const unique_ptr<node_pds_t>& lpds, const unique_ptr<node_pds_t>& rpds) const {
			if (!lpds) return false; // root was moved
			return lpds->value > rpds->value;
		}
	};

	struct node_svs_t {
		block_t states;
		seq_len_t h; // min length of input sequence to distinguish states
		state_t actState;
		sequence_in_t svs;
	};
	
	struct svs_heur_comp {

		bool operator() (const unique_ptr<node_svs_t> & lsvs, const unique_ptr<node_svs_t> & rsvs) const {
			if (!lsvs) return false; // root was moved
			return lsvs->h + lsvs->svs.size() > rsvs->h + rsvs->svs.size();
		}
	};

	static bool succInComp(const in_out_bn_ref_t& lp, const input_t& rp) {
		return lp.first < rp;
	}

	static void cleanupBN(block_node_t* node) {
		delete node;
	}

	static void cleanupPDS(node_pds_t* node) {
		delete node;
	}

	static inline void initH(block_node_t* node, const vector<sequence_in_t>& seq, const state_t& N) {
		node->h = 0;
		state_t idx;
		for (block_t::iterator i = node->states.begin(); i != node->states.end(); i++) {
			block_t::iterator j = i;
			for (j++; j != node->states.end(); j++) {
				idx = *i * N + *j - 1 - (*i * (*i + 3)) / 2;
				if (node->h < seq[idx].size()) {
					node->h = seq_len_t(seq[idx].size());
				}
			}
		}
	}

	static inline void initSVS_H(const unique_ptr<node_svs_t>& node, const vector<sequence_in_t>& seq, const state_t& N) {
		node->h = 0;
		state_t idx, state = node->actState;
		for (block_t::iterator i = node->states.begin(); i != node->states.end(); i++) {
			if (*i < state) {
				idx = *i * N + state - 1 - (*i * (*i + 3)) / 2;
				if (node->h < seq[idx].size()) {
					node->h = seq_len_t(seq[idx].size());
				}
			}
			else if (*i > state) {
				idx = state * N + *i - 1 - (state * (state + 3)) / 2;
				if (node->h < seq[idx].size()) {
					node->h = seq_len_t(seq[idx].size());
				}
			}
		}
	}

	static inline void setHeuristic(const unique_ptr<node_pds_t>& node) {
		node->value *= WEIGHT;
		node->value += seq_len_t(node->ds.size());
	}

	static void printPDS(const unique_ptr<node_pds_t> node) {
		printf("%s - %d:", FSMmodel::getInSequenceAsString(node->ds).c_str(), node->value);
		for (auto bn : node->partition) {
			printf(" %d-(", bn->h);
			for (auto state : bn->states) {
				printf(" %u", state);
			}
			printf(")");
		}
		printf("\n");
	}

	static void printSVS(const unique_ptr<node_svs_t>& node) {
		printf("%s - %d, %d:", FSMmodel::getInSequenceAsString(node->svs).c_str(), node->actState, node->h);
		for (auto state : node->states) {
			printf(" %u", state);
		}
		printf("\n");
	}

	extern sequence_set_t getSCSet(const vector<sequence_in_t>& distSeqs, state_t stateIdx, state_t N, bool filterPrefixes = false);

	static void distinguishBN(DFSM * fsm, block_node_t* bn, const sequence_vec_t& seq, set<block_node_t*, bncomp>& allBN) {
		state_t N = fsm->getNumberOfStates();
		auto states = fsm->getStates();
		for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
			auto stop = false;
			vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// + 1 for DEFAULT_OUTPUT
			list<output_t> actOutputs;
			for (block_t::iterator blockIt = bn->states.begin(); blockIt != bn->states.end(); blockIt++) {
				auto output = fsm->getOutput(states[*blockIt], input);
				if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
				if (output == WRONG_OUTPUT) {
					// one state can be distinguished by this input but no two
					if (stop) {
						// two states have no transition under this input
						actOutputs.clear();
						break;
					}
					stop = true;
				}
				else {
					state_t nextStateId = getIdx(states, fsm->getNextState(states[*blockIt], input));
					if (!sameOutput[output].insert(nextStateId).second) {
						// two states are indistinguishable under this input
						actOutputs.clear();
						break;
					}
					actOutputs.push_back(output);
				}
			}
			if (!actOutputs.empty()) {
				vector<out_bn_ref_t> outOnIn;
				// save block with more then one state
				for (list<output_t>::iterator outIt = actOutputs.begin(); outIt != actOutputs.end(); outIt++) {
					if (!sameOutput[*outIt].empty()) {
						auto succBN = new block_node_t;
						succBN->states.insert(sameOutput[*outIt].begin(), sameOutput[*outIt].end());
						auto allBNit = allBN.find(succBN);
						if (allBNit != allBN.end()) {
							delete succBN;
							succBN = *allBNit;
						}
						else {
							initH(succBN, seq, N);
							allBN.insert(succBN);
						}
						outOnIn.push_back(make_pair(*outIt, succBN));
						/* *sameOutput[*outIt].begin() is not start state but end state!
						if ((sameOutput[*outIt].size() == 1) &&
						(outVSet[*sameOutput[*outIt].begin()].empty() ||
						(outVSet[*sameOutput[*outIt].begin()].size() > actPDS->ds.size() + 1))) {
						outVSet[*sameOutput[*outIt].begin()] = actPDS->ds;
						outVSet[*sameOutput[*outIt].begin()].push_back(input);
						printf("set SVS %d: %s\n", *sameOutput[*outIt].begin(),
						FSMutils::getInSequenceAsString(outVSet[*sameOutput[*outIt].begin()]).c_str());
						}*/
						sameOutput[*outIt].clear();

						if (fsm->isOutputState() && succBN->succ.empty() && (succBN->states.size() > 1)) {
							// TODO some block could be checked several times
							vector<out_bn_ref_t> outOnSTOUT;
							for (state_t i : succBN->states) {
								stop = false;
								auto output = fsm->getOutput(states[i], STOUT_INPUT);
								for (output_t outIdx = 0; outIdx < outOnSTOUT.size(); outIdx++) {
									if (output == outOnSTOUT[outIdx].first) {
										outOnSTOUT[outIdx].second->states.insert(i);
										stop = true;
										break;
									}
								}
								if (!stop) {
									block_node_t * tmpBN = new block_node_t;
									tmpBN->states.insert(i);
									outOnSTOUT.push_back(make_pair(output, tmpBN));
								}
							}
							if (outOnSTOUT.size() > 1) {// distinguished by STOUT
								for (output_t outIdx = 0; outIdx < outOnSTOUT.size(); outIdx++) {
									if ((allBNit = allBN.find(outOnSTOUT[outIdx].second)) != allBN.end()) {
										delete outOnSTOUT[outIdx].second;
										outOnSTOUT[outIdx].second = *allBNit;
									}
									else {
										initH(outOnSTOUT[outIdx].second, seq, N);
										allBN.insert(outOnSTOUT[outIdx].second);
									}
								}
								succBN->succ.push_back(make_pair(STOUT_INPUT, outOnSTOUT));
							}
							else {
								delete outOnSTOUT[0].second;
							}
						}
					}
				}
				bn->succ.emplace_back(make_pair(input, outOnIn));
			}
		}
		if (bn->succ.empty()) {// undistinguishable set of states
			bn->succ.emplace_back(make_pair(STOUT_INPUT, vector<out_bn_ref_t>()));// could be arbitrary input
		}
	}

	static sequence_in_t findPDS(DFSM * fsm, const sequence_vec_t& seq, int& retVal, sequence_vec_t& outVSet) {
		block_node_t *hookBN, *succBN, *rootBN;
		set<block_node_t*, bncomp> allBN;
		set<bn_partition_t> closedPDS;
		priority_queue<unique_ptr<node_pds_t>, vector<unique_ptr<node_pds_t>>, pds_heur_comp> openPDS;
		auto N = fsm->getNumberOfStates();
		auto states = fsm->getStates();
		sequence_in_t outPDS;

		rootBN = new block_node_t;
		for (state_t i = 0; i < N; i++) {
			rootBN->states.insert(i);
		}
		// like initH(rootBN, seq, N);
		rootBN->h = 0;
		for (state_t i = 0; i < seq.size(); i++) {
			if (rootBN->h < seq[i].size()) {// maximal length of separating sequences
				rootBN->h = seq_len_t(seq[i].size());
			}
		}
		allBN.emplace(rootBN);

		if (fsm->isOutputState()) {
			auto succPDS = make_unique<node_pds_t>();
			succPDS->ds.push_back(STOUT_INPUT);
			succPDS->value = 0;
			vector<out_bn_ref_t> outOnIn;
			vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// + 1 for DEFAULT_OUTPUT
			// get output of all states
			for (state_t i = 0; i < N; i++) {
				auto output = fsm->getOutput(states[i], STOUT_INPUT);
				if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
				sameOutput[output].insert(i);
			}
			// save block with more then one state or clear sameOutput if stop
			for (output_t output = 0; output < fsm->getNumberOfOutputs() + 1; output++) {
				if (!sameOutput[output].empty()) {
					succBN = new block_node_t;
					succBN->states.insert(sameOutput[output].begin(), sameOutput[output].end());
					auto allBNit = allBN.find(succBN);
					if (allBNit != allBN.end()) {
						delete succBN;
						succBN = *allBNit;
					}
					else {
						initH(succBN, seq, N);
						allBN.insert(succBN);
					}
					outOnIn.push_back(make_pair(output, succBN));
					if (sameOutput[output].size() > 1) {
						succPDS->partition.insert(succBN);
						if (succPDS->value < succBN->h) {
							succPDS->value = succBN->h;
						}
					}
					else {
						outVSet[*sameOutput[output].begin()].push_back(STOUT_INPUT);
					}
					sameOutput[output].clear();
				}
			}
			rootBN->succ.push_back(make_pair(STOUT_INPUT, outOnIn));
			// all blocks are singletons
			if (succPDS->partition.empty()) {
				outPDS.push_back(STOUT_INPUT);
				retVal = PDS_FOUND;
			}
			else {
				closedPDS.emplace(bn_partition_t({ rootBN }));
				setHeuristic(succPDS);
				//printPDS(succPDS);
				openPDS.emplace(move(succPDS));
			}
		}
		else {
			auto actPDS = make_unique<node_pds_t>();
			actPDS->partition.emplace(rootBN);
			actPDS->value = rootBN->h; // + length 0 of ds
			setHeuristic(actPDS);
			openPDS.emplace(move(actPDS));
		}
		while (!openPDS.empty() && closedPDS.size() < MAX_CLOSED) {
			auto actPDS = move(openPDS.top());
			openPDS.pop();

			// check closed

			input_t minValidInput = fsm->getNumberOfInputs() + 1;
			// go through all blocks in current partition
			for (bn_partition_t::iterator pIt = actPDS->partition.begin(); pIt != actPDS->partition.end(); pIt++) {

				if ((*pIt)->succ.empty()) {// block has not been expanded yet
					distinguishBN(fsm, *pIt, seq, allBN);
				}
				if ((*pIt)->succ[0].second.empty()) {// undistinguishable set of states
					minValidInput = 0;
					break;
				}
				else if (minValidInput > (*pIt)->succ.size()) {
					minValidInput = input_t((*pIt)->succ.size());
					hookBN = *pIt;
				}
			}
			// all blocks had been expanded before
			if (!closedPDS.insert(actPDS->partition).second) {
				continue;
			}
			for (input_t i = 0; i < minValidInput; i++) {
				auto stop = false;
				auto stoutUsed = false;
				auto succPDS = make_unique<node_pds_t>();
				succPDS->value = 0;
				for (bn_partition_t::iterator pIt = actPDS->partition.begin(); pIt != actPDS->partition.end(); pIt++) {
					// find common input
					auto ioRefBNit = lower_bound((*pIt)->succ.begin(), (*pIt)->succ.end(),
						hookBN->succ[i].first, succInComp);
					if ((ioRefBNit == (*pIt)->succ.end()) || (ioRefBNit->first != hookBN->succ[i].first)) {
						stop = true;
						break;
					}
					else {// succBN on given input found
						for (output_t j = 0; j < ioRefBNit->second.size(); j++) {
							if (ioRefBNit->second[j].second->states.size() > 1) {
								succBN = ioRefBNit->second[j].second;
								if ((succBN->succ.size() == 1) && (succBN->succ[0].first == STOUT_INPUT)) {
									if (succBN->succ[0].second.empty()) {// no way to distinguish such a block
										stop = true;
										break;
									}
									if (fsm->isOutputState()) {
										for (auto out_bn : succBN->succ[0].second) {
											if (out_bn.second->states.size() > 1) {
												succPDS->partition.insert(out_bn.second);
												if (succPDS->value < out_bn.second->h) {
													succPDS->value = out_bn.second->h;
												}
											}
										}
										stoutUsed = true;
									}
									else {
										succPDS->partition.insert(succBN);
										if (succPDS->value < succBN->h) {
											succPDS->value = succBN->h;
										}
									}
								}
								else {
									succPDS->partition.insert(succBN);
									if (succPDS->value < succBN->h) {
										succPDS->value = succBN->h;
									}
								}
							}
						}
						if (stop) break;
					}
				}
				if (stop) continue;
				if (succPDS->partition.empty()) {// PDS found
					outPDS.insert(outPDS.end(), actPDS->ds.begin(), actPDS->ds.end());
					outPDS.push_back(hookBN->succ[i].first);
					if (stoutUsed) outPDS.push_back(STOUT_INPUT);
					retVal = PDS_FOUND;
					break;
				}
				else if (closedPDS.find(succPDS->partition) != closedPDS.end()) {
					continue;
				}
				succPDS->ds.insert(succPDS->ds.end(), actPDS->ds.begin(), actPDS->ds.end());
				succPDS->ds.push_back(hookBN->succ[i].first);
				if (stoutUsed) succPDS->ds.push_back(STOUT_INPUT);
				setHeuristic(succPDS);
				//printPDS(succPDS);
				openPDS.push(move(succPDS));
			}
			if (retVal == PDS_FOUND) break;
		}
#if SEQUENCES_PERFORMANCE_TEST
		stringstream ss;
		if (retVal == PDS_FOUND) ss << outPDS.size();
		else if (openPDS.empty()) ss << -1;
		else ss << -2;
		ss << ';' << closedPDS.size() << ';' << openPDS.size() << ';';
		testOut += ss.str();
		//printf("%d;%d;%d;", (retVal == PDS_FOUND) ? outPDS.size() : ((openPDS.empty()) ? -1 : -2),
		//      closedPDS.size(), openPDS.size());
#endif // SEQUENCES_PERFORMANCE_TEST
		for_each(allBN.begin(), allBN.end(), cleanupBN);
		allBN.clear();
		return outPDS;
	}

	static sequence_in_t findSVS(DFSM * fsm, const state_t refState, const sequence_vec_t& seq, int& retVal) {
		multimap<state_t, block_t> closedSVS;
		priority_queue<unique_ptr<node_svs_t>, vector<unique_ptr<node_svs_t>>, svs_heur_comp> openSVS;
		sequence_in_t outSVS;
		bool stop, stoutUsed;
		auto N = fsm->getNumberOfStates();
		auto states = fsm->getStates();
		auto actSVS = make_unique<node_svs_t>();
		actSVS->actState = states[refState];// refStata is a state index
		if (fsm->isOutputState()) {
			auto output = fsm->getOutput(actSVS->actState, STOUT_INPUT);
			for (auto i : states) {
				if (fsm->getOutput(i, STOUT_INPUT) == output) {
					actSVS->states.insert(i);
				}
			}
			if (actSVS->states.size() == 1) {
				outSVS.push_back(STOUT_INPUT);
				return outSVS;
			}
			initSVS_H(actSVS, seq, N);
			//printf("%d states\n", succSVS->states.size());
			actSVS->svs.push_back(STOUT_INPUT);
		}
		else {
			actSVS->states.insert(states.begin(), states.end());
		}
		openSVS.emplace(move(actSVS));
		stop = false;

		while (!openSVS.empty()) {
			auto actSVS = move(openSVS.top());
			openSVS.pop();
			
			auto usedIt = closedSVS.equal_range(actSVS->actState);
			for (auto it = usedIt.first; it != usedIt.second; it++) {
				if (includes(actSVS->states.begin(), actSVS->states.end(), it->second.begin(), it->second.end())) {
					stop = true;
					break;
				}
			}
			if (stop) {
				stop = false;
				continue;
			}
			closedSVS.emplace(make_pair(actSVS->actState, actSVS->states));
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				block_t sameOutput;
				auto output = fsm->getOutput(states[actSVS->actState], input);
				auto nextState = fsm->getNextState(states[actSVS->actState], input);
				//if (nextState == NULL_STATE) continue;
				for (auto state : actSVS->states) {
					if (output == fsm->getOutput(state, input)) {
						// is state undistinguishable from fixed state?
						if ((nextState == fsm->getNextState(state, input))
							&& (state != actSVS->actState)) {
							// other state goes to the same state under this input
							sameOutput.clear();
							break;
						}
						sameOutput.emplace(fsm->getNextState(state, input));
					}
				}
				if (sameOutput.empty()) {
					continue;
				}
				stoutUsed = false;
				if (fsm->isOutputState() && (sameOutput.size() > 1)) {
					block_t tmp;
					output_t outputNS = fsm->getOutput(nextState, STOUT_INPUT);
					for (state_t i : sameOutput) {
						if (fsm->getOutput(states[i], STOUT_INPUT) == outputNS) {
							tmp.insert(i);
						}
					}
					// is reference state distinguished by appending STOUT_INPUT?
					if (tmp.size() != sameOutput.size()) {
						sameOutput = tmp;
						stoutUsed = true;
					}
				}
				if (sameOutput.size() == 1) {// SVS found
					outSVS.insert(outSVS.end(), actSVS->svs.begin(), actSVS->svs.end());
					outSVS.push_back(input);
					if (stoutUsed) outSVS.push_back(STOUT_INPUT);
					stop = true;
					break;
				}

				auto usedIt = closedSVS.equal_range(nextState);
				for (auto it = usedIt.first; it != usedIt.second; it++) {
					if (includes(sameOutput.begin(), sameOutput.end(), it->second.begin(), it->second.end())) {
						stop = true;
						break;
					}
				}
				if (stop) {
					stop = false;
					continue;
				}

				auto succSVS = make_unique<node_svs_t>();
				succSVS->actState = nextState;
				succSVS->states.insert(sameOutput.begin(), sameOutput.end());
				initSVS_H(succSVS, seq, N);
				succSVS->svs.insert(succSVS->svs.begin(), actSVS->svs.begin(), actSVS->svs.end());
				succSVS->svs.push_back(input);
				if (stoutUsed) succSVS->svs.push_back(STOUT_INPUT);
				//printSVS(succSVS);
				openSVS.emplace(move(succSVS));
			}
			if (stop) break;
		}
		if ((retVal == SVS_FOUND) && (outSVS.empty())) {
			retVal = CSet_FOUND;
		}
#if SEQUENCES_PERFORMANCE_TEST
		openCount += openSVS.size();
		closedCount += closedSVS.size();
#endif // SEQUENCES_PERFORMANCE_TEST
		return outSVS;
	}

	int getDistinguishingSequences(DFSM * fsm, sequence_in_t& outPDS, unique_ptr<AdaptiveDS>& outADS,
			sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet,
			sequence_vec_t(*getSeparatingSequences)(DFSM * dfsm), bool filterPrefixes,
			void(*reduceSCSetFunc)(DFSM * dfsm, state_t stateIdx, sequence_set_t & outSCSet),
			void(*reduceCSetFunc)(DFSM * dfsm, sequence_set_t & outCSet)) {
		state_t N = fsm->getNumberOfStates();
		int retVal = CSet_FOUND;
		
		auto seq = (*getSeparatingSequences)(fsm);

		outVSet.clear();
		outVSet.resize(N);

		// SCSets
		outSCSets.clear();
		outSCSets.resize(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes);
			if (*reduceSCSetFunc != NULL)
				(*reduceSCSetFunc)(fsm, i, outSCSets[i]);
		}
		
		// CSet
		outCSet.clear();
		if (filterPrefixes) {
			FSMlib::PrefixSet pset;
			for (state_t i = 0; i < seq.size(); i++) {
				pset.insert(seq[i]);
			}
			pset.getMaximalSequences(outCSet);
		}
		else {
			for (state_t i = 0; i < seq.size(); i++) {
				outCSet.emplace(seq[i]);
			}
		}
		if (*reduceCSetFunc != NULL)
			(*reduceCSetFunc)(fsm, outCSet);

		// ADS
		outADS = move(getAdaptiveDistinguishingSequence(fsm));
		if (outADS) {// can has PDS
			retVal = ADS_FOUND;
			// PDS
			outPDS = findPDS(fsm, seq, retVal, outVSet);
		}
#if SEQUENCES_PERFORMANCE_TEST
		else {
			testOut += "-1;0;0;";
			//printf("-1;0;0;");
		}
#endif // SEQUENCES_PERFORMANCE_TEST
		
		// SVS
#if SEQUENCES_PERFORMANCE_TEST
		openCount = closedCount = 0;
#endif // SEQUENCES_PERFORMANCE_TEST
		
		if (retVal > SVS_FOUND) {
			retVal = SVS_FOUND;
		}
		for (state_t refState = 0; refState < N; refState++) {
			//printf("refState %d\n", refState);
			if (outVSet[refState].empty()) {
				outVSet[refState] = findSVS(fsm, refState, seq, retVal);
			}
		}
#if SEQUENCES_PERFORMANCE_TEST
		stringstream ss;
		ss << closedCount << ';' << openCount << ';';
		testOut += ss.str();
		//printf("%d;%d;", closedCount, openCount);
#endif // SEQUENCES_PERFORMANCE_TEST
		return retVal;
	}
}
