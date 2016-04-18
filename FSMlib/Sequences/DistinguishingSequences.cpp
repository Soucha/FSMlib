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
	typedef set<block_node_t*> partition_t;

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
		partition_t partition;
		sequence_in_t ds;
		seq_len_t value; // length of ds + max of block's heuristic
	};

	struct pdscomp {

		bool operator() (const node_pds_t* lpds, const node_pds_t* rpds) const {
			return lpds->partition < rpds->partition;
		}
	};

	struct pds_heur_comp {

		bool operator() (const node_pds_t* lpds, const node_pds_t* rpds) const {
			return lpds->value > rpds->value;
		}
	};

	struct node_svs_t {
		block_t states;
		seq_len_t h; // min length of input sequence to distinguish states
		state_t actState;
		sequence_in_t svs;
	};

	struct svscomp {

		bool operator() (const node_svs_t* lsvs, const node_svs_t* rsvs) const {
			if (lsvs->actState == rsvs->actState) {
				return (lsvs->states < rsvs->states);
			}
			return (lsvs->actState < rsvs->actState);
		}
	};

	struct svs_heur_comp {

		bool operator() (const node_svs_t* lsvs, const node_svs_t* rsvs) const {
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

	static void cleanupSVS(node_svs_t* node) {
		delete node;
	}

	static inline void initH(block_node_t* node, vector<sequence_in_t>& seq, state_t& N) {
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

	static inline void initSVS_H(node_svs_t* node, vector<sequence_in_t>& seq, state_t& N) {
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

	static inline void setHeuristic(node_pds_t* node) {
		node->value *= WEIGHT;
		node->value += seq_len_t(node->ds.size());
	}

	static void printState(state_t state) {
		printf(" %u", state);
	}

	static void printBN(block_node_t* node) {
		printf(" %d-(", node->h);
		for_each(node->states.begin(), node->states.end(), printState);
		printf(")");
	}

	static void printPDS(node_pds_t* node) {
		printf("%s - %d:", FSMmodel::getInSequenceAsString(node->ds).c_str(), node->value);
		for_each(node->partition.begin(), node->partition.end(), printBN);
		printf("\n");
	}

	static void printSVS(node_svs_t* node) {
		printf("%s - %d, %d:", FSMmodel::getInSequenceAsString(node->svs).c_str(), node->actState, node->h);
		for_each(node->states.begin(), node->states.end(), printState);
		printf("\n");
	}

	extern void getSCSet(vector<sequence_in_t>& distSeqs, state_t stateIdx, state_t N,
		sequence_set_t & outSCSet, bool filterPrefixes = false);

	int getDistinguishingSequences(DFSM * fsm, sequence_in_t& outPDS, AdaptiveDS*& outADS,
			sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet,
			void(*getSeparatingSequences)(DFSM * dfsm, vector<sequence_in_t> & seq), bool filterPrefixes,
			void(*reduceSCSetFunc)(DFSM * dfsm, state_t stateIdx, sequence_set_t & outSCSet),
			void(*reduceCSetFunc)(DFSM * dfsm, sequence_set_t & outCSet)) {
		state_t M, N = fsm->getNumberOfStates();
		int retVal = CSet_FOUND;
		M = ((N - 1) * N) / 2;
		vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// + 1 for DEFAULT_OUTPUT
		output_t output;
		list<output_t> actOutputs;
		bool stop, stoutUsed;
		vector<state_t> states = fsm->getStates();
		vector<sequence_in_t> seq(M);

		(*getSeparatingSequences)(fsm, seq);

		outVSet.clear();
		outVSet.resize(N);

		// SCSets
		outSCSets.clear();
		outSCSets.resize(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			getSCSet(seq, i, N, outSCSets[i], filterPrefixes);
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
				outCSet.insert(seq[i]);
			}
		}
		if (*reduceCSetFunc != NULL)
			(*reduceCSetFunc)(fsm, outCSet);

		// ADS
		bool hasADS = getAdaptiveDistinguishingSequence(fsm, outADS);

		if (hasADS) {// can has PDS
			retVal = ADS_FOUND;

			input_t minValidInput;
			block_node_t *hookBN, *succBN, *rootBN;
			set<block_node_t*, bncomp> allBN;
			set<block_node_t*, bncomp>::iterator allBNit;
			vector<out_bn_ref_t> outOnIn;
			vector<in_out_bn_ref_t>::iterator ioRefBNit;
			node_pds_t* actPDS, *succPDS;
			set<node_pds_t*, pdscomp> closedPDS;
			priority_queue<node_pds_t*, vector<node_pds_t*>, pds_heur_comp> openPDS;

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
			allBN.insert(rootBN);

			actPDS = new node_pds_t;
			actPDS->partition.insert(rootBN);
			actPDS->value = rootBN->h; // + length 0 of ds
			setHeuristic(actPDS);
			if (fsm->isOutputState()) {
				succPDS = new node_pds_t;
				succPDS->ds.push_back(STOUT_INPUT);
				succPDS->value = 0;
				outOnIn.clear();
				// get output of all states
				for (state_t i = 0; i < N; i++) {
					output = fsm->getOutput(states[i], STOUT_INPUT);
					if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
					sameOutput[output].insert(i);
				}
				// save block with more then one state or clear sameOutput if stop
				for (output = 0; output < fsm->getNumberOfOutputs() + 1; output++) {
					if (!sameOutput[output].empty()) {
						succBN = new block_node_t;
						succBN->states.insert(sameOutput[output].begin(), sameOutput[output].end());
						if ((allBNit = allBN.find(succBN)) != allBN.end()) {
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
					outPDS.clear();
					outPDS.push_back(STOUT_INPUT);
					delete succPDS;
					delete actPDS;
					retVal = PDS_FOUND;
				}
				else {
					closedPDS.insert(actPDS);
					setHeuristic(succPDS);
					//printPDS(succPDS);
					openPDS.push(succPDS);
				}
			}
			else {
				openPDS.push(actPDS);
			}
			while (!openPDS.empty() && closedPDS.size() < MAX_CLOSED) {
				actPDS = openPDS.top();
				openPDS.pop();
				minValidInput = fsm->getNumberOfInputs() + 1;
				// go through all blocks in current partition
				for (partition_t::iterator pIt = actPDS->partition.begin(); pIt != actPDS->partition.end(); pIt++) {

					if ((*pIt)->succ.empty()) {// block has not been expanded yet
						for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
							stop = false;
							for (block_t::iterator blockIt = (*pIt)->states.begin(); blockIt != (*pIt)->states.end(); blockIt++) {
								output = fsm->getOutput(states[*blockIt], input);
								if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
								if (output == WRONG_OUTPUT) {
									// one state can be distinguished by this input but no two
									if (stop) {
										// two states have no transition under this input
										for (list<output_t>::iterator outIt = actOutputs.begin();
											outIt != actOutputs.end(); outIt++) {
											if (!sameOutput[*outIt].empty()) {
												sameOutput[*outIt].clear();
											}
										}
										actOutputs.clear();
										break;
									}
									stop = true;
								} else {
									state_t nextStateId = getIdx(states, fsm->getNextState(states[*blockIt], input));
									if (!sameOutput[output].insert(nextStateId).second) {
										// two states are indistinguishable under this input
										for (list<output_t>::iterator outIt = actOutputs.begin();
											outIt != actOutputs.end(); outIt++) {
											if (!sameOutput[*outIt].empty()) {
												sameOutput[*outIt].clear();
											}
										}
										actOutputs.clear();
										break;
									}
									actOutputs.push_back(output);
								}
							}
							if (!actOutputs.empty()) {
								outOnIn.clear();
								// save block with more then one state
								for (list<output_t>::iterator outIt = actOutputs.begin(); outIt != actOutputs.end(); outIt++) {
									if (!sameOutput[*outIt].empty()) {
										succBN = new block_node_t;
										succBN->states.insert(sameOutput[*outIt].begin(), sameOutput[*outIt].end());
										if ((allBNit = allBN.find(succBN)) != allBN.end()) {
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
												output = fsm->getOutput(states[i], STOUT_INPUT);
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
								(*pIt)->succ.push_back(make_pair(input, outOnIn));
								actOutputs.clear();
							}
						}
						if ((*pIt)->succ.empty()) {// undistinguishable set of states
							outOnIn.clear();
							(*pIt)->succ.push_back(make_pair(STOUT_INPUT, outOnIn));// could be arbitrary input
						}
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
				if (!closedPDS.insert(actPDS).second) {
					delete actPDS;
					continue;
				}
				for (input_t i = 0; i < minValidInput; i++) {
					stop = false;
					stoutUsed = false;
					succPDS = new node_pds_t;
					succPDS->value = 0;
					for (partition_t::iterator pIt = actPDS->partition.begin(); pIt != actPDS->partition.end(); pIt++) {
						// find common input
						ioRefBNit = lower_bound((*pIt)->succ.begin(), (*pIt)->succ.end(),
							hookBN->succ[i].first, succInComp);
						if ((ioRefBNit == (*pIt)->succ.end()) || (ioRefBNit->first != hookBN->succ[i].first)) {
							stop = true;
							delete succPDS;
							break;
						}
						else {// succBN on given input found
							for (output_t j = 0; j < ioRefBNit->second.size(); j++) {
								if (ioRefBNit->second[j].second->states.size() > 1) {
									succBN = ioRefBNit->second[j].second;
									if ((succBN->succ.size() == 1) && (succBN->succ[0].first == STOUT_INPUT)) {
										if (succBN->succ[0].second.empty()) {// no way to distinguish such a block
											stop = true;
											delete succPDS;
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
						outPDS.clear();
						outPDS.insert(outPDS.end(), actPDS->ds.begin(), actPDS->ds.end());
						outPDS.push_back(hookBN->succ[i].first);
						if (stoutUsed) outPDS.push_back(STOUT_INPUT);
						retVal = PDS_FOUND;
						delete succPDS;
						break;
					}
					else if (closedPDS.find(succPDS) != closedPDS.end()) {
						delete succPDS;
						continue;
					}
					succPDS->ds.insert(succPDS->ds.end(), actPDS->ds.begin(), actPDS->ds.end());
					succPDS->ds.push_back(hookBN->succ[i].first);
					if (stoutUsed) succPDS->ds.push_back(STOUT_INPUT);
					setHeuristic(succPDS);
					//printPDS(succPDS);
					openPDS.push(succPDS);
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
			while (!openPDS.empty()) {
				delete openPDS.top();
				openPDS.pop();
			}
			for_each(closedPDS.begin(), closedPDS.end(), cleanupPDS);
			closedPDS.clear();
			for_each(allBN.begin(), allBN.end(), cleanupBN);
			allBN.clear();
		}
#if SEQUENCES_PERFORMANCE_TEST
		else {
			testOut += "-1;0;0;";
			//printf("-1;0;0;");
		}
#endif // SEQUENCES_PERFORMANCE_TEST
		

		// SVS
		node_svs_t* actSVS, *succSVS;
		set<node_svs_t*, svscomp> closedSVS;
		priority_queue<node_svs_t*, vector<node_svs_t*>, svs_heur_comp> openSVS;
		state_t nextState;
#if SEQUENCES_PERFORMANCE_TEST
		openCount = closedCount = 0;
#endif // SEQUENCES_PERFORMANCE_TEST
		
		if (retVal > SVS_FOUND) {
			retVal = SVS_FOUND;
		}
		for (state_t refState = 0; refState < N; refState++) {
			//printf("refState %d\n", refState);

			actSVS = new node_svs_t;
			actSVS->actState = refState;
			if (fsm->isOutputState()) {
				if (!outVSet[refState].empty()) {
					delete actSVS;
					continue;
				}
				output = fsm->getOutput(states[refState], STOUT_INPUT);
				for (state_t i = 0; i < N; i++) {
					if (fsm->getOutput(states[i], STOUT_INPUT) == output) {
						actSVS->states.insert(i);
					}
				}
				if (actSVS->states.size() == 1) {
					outVSet[refState].push_back(STOUT_INPUT);
					delete actSVS;
					continue;
				}
				initSVS_H(actSVS, seq, N);
				//printf("%d states\n", succSVS->states.size());
				actSVS->svs.push_back(STOUT_INPUT);
			}
			else {
				for (state_t i = 0; i < N; i++) {
					actSVS->states.insert(i);
				}
			}
			openSVS.push(actSVS);
			stop = false;

			while (!openSVS.empty()) {
				actSVS = openSVS.top();
				openSVS.pop();
				/*
				if (!outVSet[refState].empty() && (outVSet[refState].size() <= actSVS->svs.size() + 1)) {
				printf("%s\n", FSMutils::getInSequenceAsString(outVSet[refState]).c_str());
				delete actSVS;
				continue;
				}*/
				if (!closedSVS.insert(actSVS).second) {
					delete actSVS;
					continue;
				}
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					output = fsm->getOutput(states[actSVS->actState], input);
					nextState = fsm->getNextState(states[actSVS->actState], input);
					for (block_t::iterator blockIt = actSVS->states.begin();
						blockIt != actSVS->states.end(); blockIt++) {
						if (output == fsm->getOutput(states[*blockIt], input)) {
							// is state undistinguishable from fixed state?
							if ((nextState == fsm->getNextState(states[*blockIt], input))
								&& (*blockIt != actSVS->actState)) {
								// other state goes to the same state under this input
								sameOutput[output].clear();
								break;
							}
							state_t nextStateId = getIdx(states, fsm->getNextState(states[*blockIt], input));
							sameOutput[output].insert(nextStateId);
						}
					}
					if (sameOutput[output].empty()) {
						continue;
					}
					stoutUsed = false;
					if (fsm->isOutputState() && (sameOutput[output].size() > 1)) {
						block_t tmp;
						output_t outputNS = fsm->getOutput(nextState, STOUT_INPUT);
						for (state_t i : sameOutput[output]) {
							if (fsm->getOutput(states[i], STOUT_INPUT) == outputNS) {
								tmp.insert(i);
							}
						}
						// is reference state distinguished by appending STOUT_INPUT?
						if (tmp.size() != sameOutput[output].size()) {
							sameOutput[output] = tmp;
							stoutUsed = true;
						}
					}
					if (sameOutput[output].size() == 1) {// SVS found
						outVSet[refState].clear();
						outVSet[refState].insert(outVSet[refState].end(), actSVS->svs.begin(), actSVS->svs.end());
						outVSet[refState].push_back(input);
						if (stoutUsed) outVSet[refState].push_back(STOUT_INPUT);
						stop = true;
						sameOutput[output].clear();
						break;
					}

					succSVS = new node_svs_t;
					succSVS->actState = nextState;
					succSVS->states.insert(sameOutput[output].begin(), sameOutput[output].end());
					sameOutput[output].clear();
					if (closedSVS.find(succSVS) == closedSVS.end()) {
						initSVS_H(succSVS, seq, N);
						succSVS->svs.insert(succSVS->svs.begin(), actSVS->svs.begin(), actSVS->svs.end());
						succSVS->svs.push_back(input);
						if (stoutUsed) succSVS->svs.push_back(STOUT_INPUT);
						openSVS.push(succSVS);
						//printSVS(succSVS);
					}
					else {
						delete succSVS;
					}
				}
				if (stop) break;
			}
			if ((retVal == SVS_FOUND) && (outVSet[refState].empty())) {
				retVal = CSet_FOUND;
			}
#if SEQUENCES_PERFORMANCE_TEST
			openCount += openSVS.size();
			closedCount += closedSVS.size();
#endif // SEQUENCES_PERFORMANCE_TEST
			while (!openSVS.empty()) {
				delete openSVS.top();
				openSVS.pop();
			}
			for_each(closedSVS.begin(), closedSVS.end(), cleanupSVS);
			closedSVS.clear();
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
