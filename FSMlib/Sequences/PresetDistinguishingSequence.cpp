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

namespace FSMsequence {
	typedef set<state_t> block_t;
	typedef set<block_t> partition_t;

	struct pds_node_t {
		partition_t partition;
		sequence_in_t ds;

		pds_node_t(partition_t partition, sequence_in_t ds) :
			partition(partition), ds(ds) {
		}
	};

	extern int MAX_CLOSED;
#if SEQUENCES_PERFORMANCE_TEST
	extern string testOut;
#endif // SEQUENCES_PERFORMANCE_TEST

	static const state_t getSetId(const block_t & block) {
		state_t sum = 0;
		for (auto state : block) {
			sum += state;
		}
		sum += state_t(block.size());
		return sum;
	}

	static bool isSubsetPartition(partition_t::iterator subsetFirstIt, partition_t::iterator subsetLastIt,
		partition_t::iterator partitionFirstIt, partition_t::iterator partitionLastIt) {
		while ((subsetFirstIt != subsetLastIt) && (partitionFirstIt != partitionLastIt)) {
			if (*subsetFirstIt == *partitionFirstIt) {
				subsetFirstIt++;
				partitionFirstIt++;
			}
			else if (*subsetFirstIt > *partitionFirstIt) {
				partitionFirstIt++;
			}
			else {
				return false;
			}
		}
		return (subsetFirstIt == subsetLastIt);
	}

	sequence_in_t getPresetDistinguishingSequence(DFSM * fsm) {
		sequence_in_t outPDS;
		vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// +1 for DEFAULT_OUTPUT
		set<output_t> actOutputs;
		partition_t partition;
		sequence_in_t s;
		auto states = fsm->getStates();
		if (fsm->isOutputState()) {
			// get output of all states
			for (auto state : states) {
				auto output = fsm->getOutput(state, STOUT_INPUT);
				if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
				sameOutput[output].emplace(state);
				actOutputs.emplace(output);
			}
			if (actOutputs.size() > 1)
				s.push_back(STOUT_INPUT);
			// save block with more then one state and clear sameOutput
			for (auto out : actOutputs) {
				if (sameOutput[out].size() > 1) {
					partition.emplace(sameOutput[out]);
				}
				sameOutput[out].clear();
			}
			actOutputs.clear();
			// are all blocks singletons?
			if (partition.empty()) {
				outPDS.push_back(STOUT_INPUT);
#if SEQUENCES_PERFORMANCE_TEST
				testOut += "1;0;0;";
				//printf("1;0;0;");
#endif // SEQUENCES_PERFORMANCE_TEST
				return outPDS;
			}
		}
		else {
			partition.emplace(block_t(states.begin(), states.end()));
		}
		queue<unique_ptr<pds_node_t>> fifo;
		// <id, node's partition>, id = getSetId(node) = sum of state IDs in the first block of node's partition
		multimap<state_t, partition_t> used;
		bool stop, stoutUsed = false;

		fifo.emplace(make_unique<pds_node_t>(partition, s));
		used.emplace(make_pair(getSetId(*partition.begin()), partition));
		while (!fifo.empty() && used.size() < MAX_CLOSED) {
			auto act = move(fifo.front());
			fifo.pop();
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				stop = false;
				partition.clear();
				// go through all blocks in current partition
				for (auto block : act->partition) {
					bool noTransition = false;
					// go through all states in current block
					for (auto state : block) {
						auto output = fsm->getOutput(state, input);
						if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
						if (output == WRONG_OUTPUT) {
							// one state can be distinguished by this input but no two
							if (noTransition) {
								// two states have no transition under this input
								stop = true;
								break;
							}
							noTransition = true;
						} else {
							if (!sameOutput[output].insert(fsm->getNextState(state, input)).second) {
								// two states are indistinguishable under this input
								stop = true;
								break;
							}
							actOutputs.emplace(output);
						}
					}
					// save block with more then one state or clear sameOutput if stop
					for (auto out : actOutputs) {
						if (!stop && (sameOutput[out].size() > 1)) {
							partition.emplace(sameOutput[out]);
						}
						sameOutput[out].clear();
					}
					actOutputs.clear();
					if (stop) break;
				}
				if (stop) {// try another input
					continue;
				}
				if (fsm->isOutputState()) {
					partition_t tmp;
					stoutUsed = false;
					for (auto block : partition) {
						for (auto state : block) {
							auto output = fsm->getOutput(state, STOUT_INPUT);
							if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
							sameOutput[output].emplace(state);
							actOutputs.emplace(output);
						}
						if (actOutputs.size() > 1) stoutUsed = true;
						// save block with more then one state and clear sameOutput
						for (auto out : actOutputs) {
							if (sameOutput[out].size() > 1) {
								tmp.emplace(sameOutput[out]);
							}
							sameOutput[out].clear();
						}
						actOutputs.clear();
					}
					if (stoutUsed) {
						partition = tmp;
					}
				}
				// all blocks are singletons
				if (partition.empty()) {
					outPDS = act->ds;
					outPDS.push_back(input);
					if (stoutUsed) outPDS.push_back(STOUT_INPUT);
#if SEQUENCES_PERFORMANCE_TEST
					ostringstream ss;
					ss << outPDS.size() << ';' << (used.size() - fifo.size()) << ';' << fifo.size() << ';';
					testOut += ss.str();
					//printf("%d;%d;%d;", outPDS.size(), used.size()-fifo.size(), fifo.size());
#endif // SEQUENCES_PERFORMANCE_TEST
					return outPDS;
				}
				// go through all blocks in new partition
				for (partition_t::iterator pIt = partition.begin(); pIt != partition.end(); pIt++) {
					auto usedIt = used.equal_range(getSetId(*pIt));
					for (auto it = usedIt.first; it != usedIt.second; it++) {
						if (isSubsetPartition(it->second.begin(), it->second.end(), pIt, partition.end())) {
							stop = true;
							break;
						}
					}
					if (stop) break;
				}
				// create new node
				if (!stop) {
					s = act->ds;
					s.push_back(input);
					if (stoutUsed) s.push_back(STOUT_INPUT);
					fifo.emplace(make_unique<pds_node_t>(partition, s));
					used.emplace(make_pair(getSetId(*partition.begin()), partition));
				}
			}
		}
#if SEQUENCES_PERFORMANCE_TEST
		ostringstream ss;
		ss << ((fifo.empty()) ? -1 : -2) << ';' << (used.size() - fifo.size()) << ';' << fifo.size() << ';';
		testOut += ss.str();
		//printf("%d;%d;%d;", ((fifo.empty()) ? -1 : -2), used.size()-fifo.size(), fifo.size());		
#endif // SEQUENCES_PERFORMANCE_TEST
		return outPDS;
	}
}
