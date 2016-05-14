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
	typedef pair<output_t, weak_ptr<block_node_t>> out_bn_ref_t;
	typedef pair<input_t, vector<out_bn_ref_t> > in_out_bn_ref_t;

	struct block_node_t {
		block_t states;
		seq_len_t h; // min length of input sequence to distinguish states
		vector<in_out_bn_ref_t> succ;
	};

	struct bncomp {

		bool operator() (const shared_ptr<block_node_t>& lbn, const shared_ptr<block_node_t>& rbn) const {
			return lbn->states < rbn->states;
		}
	};

	typedef set<shared_ptr<block_node_t>, bncomp> bn_partition_t;

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

	static inline void initH(const shared_ptr<block_node_t>& node, const vector<sequence_in_t>& seq, const state_t& N) {
		node->h = 0;
		for (block_t::iterator i = node->states.begin(); i != node->states.end(); i++) {
			block_t::iterator j = i;
			for (j++; j != node->states.end(); j++) {
				auto idx = getStatePairIdx(*i, *j, N);
				if (node->h < seq[idx].size()) {
					node->h = seq_len_t(seq[idx].size());
				}
			}
		}
	}

	static inline void initSVS_H(const unique_ptr<node_svs_t>& node, const vector<sequence_in_t>& seq, const state_t& N) {
		node->h = 0;
		for (const auto& i : node->states) {
			if (i != node->actState) {
				auto idx = getStatePairIdx(i, node->actState, N);
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
		for (const auto& bn : node->partition) {
			printf(" %d-(", bn->h);
			for (const auto& state : bn->states) {
				printf(" %u", state);
			}
			printf(")");
		}
		printf("\n");
	}

	static void printSVS(const unique_ptr<node_svs_t>& node) {
		printf("%s - %d, %d:", FSMmodel::getInSequenceAsString(node->svs).c_str(), node->actState, node->h);
		for (const auto& state : node->states) {
			printf(" %u", state);
		}
		printf("\n");
	}

	static const state_t getSetId(const block_t & block) {
		state_t sum = 0;
		for (const auto& state : block) {
			sum += state;
		}
		sum += state_t(block.size());
		return sum;
	}

	static bool isSubsetPartition(bn_partition_t::iterator subsetFirstIt, bn_partition_t::iterator subsetLastIt,
		bn_partition_t::iterator partitionFirstIt, bn_partition_t::iterator partitionLastIt) {
		while ((subsetFirstIt != subsetLastIt) && (partitionFirstIt != partitionLastIt)) {
			if ((*subsetFirstIt)->states == (*partitionFirstIt)->states) {
				subsetFirstIt++;
				partitionFirstIt++;
			}
			else if ((*subsetFirstIt)->states > (*partitionFirstIt)->states) {
				partitionFirstIt++;
			}
			else {
				return false;
			}
		}
		return (subsetFirstIt == subsetLastIt);
	}

	extern sequence_set_t getSCSet(const vector<sequence_in_t>& distSeqs, state_t state, state_t N, bool filterPrefixes = false, bool useStout = false);

	static void distinguishBN(const unique_ptr<DFSM>& fsm, const shared_ptr<block_node_t> bn, const sequence_vec_t& seq, bn_partition_t& allBN) {
		state_t N = fsm->getNumberOfStates();
		for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
			auto stop = false;
			vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// + 1 for DEFAULT_OUTPUT
			list<output_t> actOutputs;
			for (const auto& state : bn->states) {
				auto output = fsm->getOutput(state, input);
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
					state_t nextState = fsm->getNextState(state, input);
					if (!sameOutput[output].insert(nextState).second) {
						// two states are indistinguishable under this input
						actOutputs.clear();
						break;
					}
					actOutputs.emplace_back(output);
				}
			}
			if (!actOutputs.empty()) {
				vector<out_bn_ref_t> outOnIn;
				// save block with more then one state
				for (const auto& out : actOutputs) {
					if (!sameOutput[out].empty()) {
						auto succBN = make_shared<block_node_t>();
						succBN->states = move(sameOutput[out]);
						auto allBNit = allBN.find(succBN);
						if (allBNit != allBN.end()) {
							succBN = *allBNit;
						}
						else {
							initH(succBN, seq, N);
							allBN.emplace(succBN);
						}
						outOnIn.emplace_back(out, succBN);
						
						if (fsm->isOutputState() && succBN->succ.empty() && (succBN->states.size() > 1)) {
							// TODO some block could be checked several times
							vector<pair<output_t, block_t>> outStates;
							for (const auto& i : succBN->states) {
								stop = false;
								auto output = fsm->getOutput(i, STOUT_INPUT);
								for (output_t outIdx = 0; outIdx < outStates.size(); outIdx++) {
									if (output == outStates[outIdx].first) {
										outStates[outIdx].second.emplace(i);
										stop = true;
										break;
									}
								}
								if (!stop) {
									outStates.emplace_back(output, block_t({i}));
								}
							}
							if (outStates.size() > 1) {// distinguished by STOUT
								vector<out_bn_ref_t> outOnSTOUT;
								for (const auto& p : outStates) {
									auto succStoutBN = make_shared<block_node_t>();
									succStoutBN->states = move(p.second);
									auto allBNit = allBN.find(succStoutBN);
									if (allBNit != allBN.end()) {
										succStoutBN = *allBNit;
									}
									else {
										initH(succStoutBN, seq, N);
										allBN.insert(succStoutBN);
									}
									outOnSTOUT.emplace_back(p.first, move(succStoutBN));
								}
								succBN->succ.emplace_back(STOUT_INPUT, move(outOnSTOUT));
							}
						}
					}
				}
				bn->succ.emplace_back(input, move(outOnIn));
			}
		}
		if (bn->succ.empty()) {// undistinguishable set of states
			bn->succ.emplace_back(STOUT_INPUT, vector<out_bn_ref_t>());// could be arbitrary input
		}
	}

	static sequence_in_t findPDS(const unique_ptr<DFSM>& fsm, const sequence_vec_t& seq, int& retVal, sequence_vec_t& outVSet, bool useStout) {
		shared_ptr<block_node_t> hookBN;
		bn_partition_t allBN;
		multimap<state_t, bn_partition_t> closedPDS;
		priority_queue<unique_ptr<node_pds_t>, vector<unique_ptr<node_pds_t>>, pds_heur_comp> openPDS;
		auto N = fsm->getNumberOfStates();
		sequence_in_t outPDS;

		auto rootBN = make_shared<block_node_t>();
		for (state_t i = 0; i < N; i++) {
			rootBN->states.emplace(i);
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
				auto output = fsm->getOutput(i, STOUT_INPUT);
				if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
				sameOutput[output].emplace(i);
			}
			// save block with more then one state or clear sameOutput if stop
			for (output_t output = 0; output < fsm->getNumberOfOutputs() + 1; output++) {
				if (!sameOutput[output].empty()) {
					auto succBN = make_shared<block_node_t>();
					succBN->states.swap(sameOutput[output]);
					auto allBNit = allBN.find(succBN);
					if (allBNit != allBN.end()) {
						succBN = *allBNit;
					}
					else {
						initH(succBN, seq, N);
						allBN.emplace(succBN);
					}
					if (succBN->states.size() > 1) {
						succPDS->partition.emplace(succBN);
						if (succPDS->value < succBN->h) {
							succPDS->value = succBN->h;
						}
					}
					else {
						outVSet[*(succBN->states.begin())].push_back(STOUT_INPUT);
					}
					outOnIn.emplace_back(output, move(succBN));
				}
			}
			rootBN->succ.emplace_back(STOUT_INPUT, move(outOnIn));
			// all blocks are singletons
			if (succPDS->partition.empty()) {
				outPDS.push_back(STOUT_INPUT);
				retVal = PDS_FOUND;
			}
			else {
				closedPDS.emplace(getSetId(rootBN->states), bn_partition_t({ rootBN }));
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
			auto stop = false;
			// check closed
			for (auto pIt = actPDS->partition.begin(); pIt != actPDS->partition.end(); pIt++) {
				auto usedIt = closedPDS.equal_range(getSetId((*pIt)->states));
				for (auto it = usedIt.first; it != usedIt.second; it++) {
					if (isSubsetPartition(it->second.begin(), it->second.end(), pIt, actPDS->partition.end())) {
						stop = true;
						break;
					}
				}
				if (stop) break;
			}
			if (stop) continue;
			closedPDS.emplace(getSetId((*(actPDS->partition.begin()))->states), actPDS->partition);
		
			input_t minValidInput = fsm->getNumberOfInputs() + 1;
			// go through all blocks in current partition
			for (auto &block : actPDS->partition) {
				if (block->succ.empty()) {// block has not been expanded yet
					distinguishBN(fsm, block, seq, allBN);
				}
				if (block->succ[0].second.empty()) {// undistinguishable set of states
					minValidInput = 0;
					break;
				}
				else if (minValidInput > block->succ.size()) {
					minValidInput = input_t(block->succ.size());
					hookBN = block;
				}
			}
			
			for (input_t i = 0; i < minValidInput; i++) {
				stop = false;
				auto stoutNeeded = false;
				auto succPDS = make_unique<node_pds_t>();
				succPDS->value = 0;
				for (auto &block : actPDS->partition) {
					// find common input
					auto ioRefBNit = lower_bound(block->succ.begin(), block->succ.end(), hookBN->succ[i].first, succInComp);
					if ((ioRefBNit == block->succ.end()) || (ioRefBNit->first != hookBN->succ[i].first)) {
						stop = true;
						break;
					}
					else {// succBN on given input found
						for (output_t j = 0; j < ioRefBNit->second.size(); j++) {
							auto succBN = ioRefBNit->second[j].second.lock();
							if (succBN && (succBN->states.size() > 1)) {
								if ((succBN->succ.size() == 1) && (succBN->succ[0].first == STOUT_INPUT)) {
									if (succBN->succ[0].second.empty()) {// no way to distinguish such a block
										stop = true;
										break;
									}
									if (fsm->isOutputState()) {
										for (auto& out_bn : succBN->succ[0].second) {
											auto succPtr = out_bn.second.lock();
											if (succPtr && (succPtr->states.size() > 1)) {
												succPDS->partition.emplace(succPtr);
												if (succPDS->value < succPtr->h) {
													succPDS->value = succPtr->h;
												}
											}
										}
										stoutNeeded = true;
									}
									else {
										succPDS->partition.emplace(succBN);
										if (succPDS->value < succBN->h) {
											succPDS->value = succBN->h;
										}
									}
								}
								else {
									succPDS->partition.emplace(succBN);
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
					outPDS.swap(actPDS->ds);
					outPDS.push_back(hookBN->succ[i].first);
					if (stoutNeeded || useStout) outPDS.push_back(STOUT_INPUT);
					retVal = PDS_FOUND;
					break;
				}
				for (auto pIt = succPDS->partition.begin(); pIt != succPDS->partition.end(); pIt++) {
					auto usedIt = closedPDS.equal_range(getSetId((*pIt)->states));
					for (auto it = usedIt.first; it != usedIt.second; it++) {
						if (isSubsetPartition(it->second.begin(), it->second.end(), pIt, succPDS->partition.end())) {
							stop = true;
							break;
						}
					}
					if (stop) break;
				}
				if (stop) continue;
				
				succPDS->ds.insert(succPDS->ds.end(), actPDS->ds.begin(), actPDS->ds.end());
				succPDS->ds.push_back(hookBN->succ[i].first);
				if (stoutNeeded || useStout) succPDS->ds.push_back(STOUT_INPUT);
				setHeuristic(succPDS);
				//printPDS(succPDS);
				openPDS.emplace(move(succPDS));
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
		return outPDS;
	}

	static sequence_in_t findSVS(const unique_ptr<DFSM>& fsm, const state_t refState, const sequence_vec_t& seq, int& retVal, bool useStout) {
		multimap<state_t, block_t> closedSVS;
		priority_queue<unique_ptr<node_svs_t>, vector<unique_ptr<node_svs_t>>, svs_heur_comp> openSVS;
		sequence_in_t outSVS;
		bool stop, stoutNeeded;
		auto N = fsm->getNumberOfStates();
		auto actSVS = make_unique<node_svs_t>();
		actSVS->actState = refState;// refStata is a state index
		if (fsm->isOutputState()) {
			auto output = fsm->getOutput(actSVS->actState, STOUT_INPUT);
			for (state_t i = 0; i < N; i++) {
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
			for (state_t i = 0; i < N; i++) {
				actSVS->states.insert(i);
			}
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
			closedSVS.emplace(actSVS->actState, actSVS->states);
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				block_t sameOutput;
				auto output = fsm->getOutput(actSVS->actState, input);
				auto nextState = fsm->getNextState(actSVS->actState, input);
				//if (nextState == NULL_STATE) continue;
				for (const auto& state : actSVS->states) {
					if (output == fsm->getOutput(state, input)) {
						// is state undistinguishable from fixed state?
						if ((nextState == fsm->getNextState(state, input))
							&& (state != actSVS->actState)) {
							// other state goes to the same state under this input
							sameOutput.clear();
							break;
						}
						sameOutput.emplace(fsm->getNextState(state, input));// NULL_STATE is allowed once
					}
				}
				if (sameOutput.empty()) {
					continue;
				}
				stoutNeeded = false;
				if (fsm->isOutputState() && (sameOutput.size() > 1)) {
					block_t tmp;
					output_t outputNS = fsm->getOutput(nextState, STOUT_INPUT);
					for (const auto& i : sameOutput) {
						if (fsm->getOutput(i, STOUT_INPUT) == outputNS) {
							tmp.insert(i);
						}
					}
					// is reference state distinguished by appending STOUT_INPUT?
					if (tmp.size() != sameOutput.size()) {
						sameOutput.swap(tmp);
						stoutNeeded = true;
					}
				}
				if (sameOutput.size() == 1) {// SVS found
					outSVS.swap(actSVS->svs);
					outSVS.push_back(input);
					if (stoutNeeded || useStout) outSVS.push_back(STOUT_INPUT);
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
				succSVS->states.swap(sameOutput);
				initSVS_H(succSVS, seq, N);
				succSVS->svs.insert(succSVS->svs.begin(), actSVS->svs.begin(), actSVS->svs.end());
				succSVS->svs.push_back(input);
				if (stoutNeeded || useStout) succSVS->svs.push_back(STOUT_INPUT);
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

	int getDistinguishingSequences(const unique_ptr<DFSM>& fsm, sequence_in_t& outPDS, unique_ptr<AdaptiveDS>& outADS,
			sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet,
			function<sequence_vec_t(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs)> getSeparatingSequences, bool filterPrefixes,
			function<void(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t& outSCSet)> reduceSCSet,
			function<void(const unique_ptr<DFSM>& dfsm, sequence_set_t& outCSet)> reduceCSet, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getDistinguishingSequences", -1);
		state_t N = fsm->getNumberOfStates();
		int retVal = CSet_FOUND;
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();

		auto seq = getSeparatingSequences(fsm, omitUnnecessaryStoutInputs);

		outVSet.clear();
		outVSet.resize(N);

		// SCSets
		outSCSets.clear();
		outSCSets.resize(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes, !omitUnnecessaryStoutInputs && fsm->isOutputState());
			if (reduceSCSet)
				reduceSCSet(fsm, i, outSCSets[i]);
		}
		
		// CSet
		outCSet.clear();
		if (filterPrefixes) {
			FSMlib::PrefixSet pset;
			for (state_t i = 0; i < seq.size(); i++) {
				if (fsm->isOutputState() && (seq[i].front() != STOUT_INPUT)) seq[i].push_front(STOUT_INPUT);
				pset.insert(seq[i]);
			}
			outCSet = pset.getMaximalSequences();
		}
		else {
			for (state_t i = 0; i < seq.size(); i++) {
				outCSet.emplace(seq[i]);
			}
		}
		if (reduceCSet)
			reduceCSet(fsm, outCSet);

		// ADS
		outADS = move(getAdaptiveDistinguishingSequence(fsm, omitUnnecessaryStoutInputs));
		if (outADS) {// can has PDS
			retVal = ADS_FOUND;
			// PDS
			outPDS = findPDS(fsm, seq, retVal, outVSet, useStout);
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
				outVSet[refState] = findSVS(fsm, refState, seq, retVal, useStout);
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
