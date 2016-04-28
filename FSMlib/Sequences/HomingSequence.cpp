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

	struct hs_node_t {
		partition_t partition;
		sequence_in_t hs;

		hs_node_t(partition_t partition, sequence_in_t hs) {
			this->partition = partition;
			this->hs = hs;
		}
	};

	static const int getSetId(const block_t & block) {
		int sum = 0;
		for (block_t::iterator blockIt = block.begin(); blockIt != block.end(); blockIt++) {
			sum += *blockIt;
		}
		sum += int(block.size());
		return sum;
	}

	static bool isSubsetPartition(partition_t::iterator subsetFirstIt, partition_t::iterator subsetLastIt,
		partition_t::iterator partitionFirstIt, partition_t::iterator partitioLastIt) {
		while ((subsetFirstIt != subsetLastIt) && (partitionFirstIt != partitioLastIt)) {
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

	sequence_in_t getPresetHomingSequence(DFSM * fsm) {
		sequence_in_t outHS;
		partition_t partition;
		sequence_in_t s;
		block_t block;
		vector<block_t> sameOutput(fsm->getNumberOfOutputs() + 1);// +1 for DEFAULT_OUTPUT
		set<output_t> actOutputs;
		output_t output;
		partition.clear();
		if (fsm->isOutputState()) {
			// get output of all states
			for (auto state : fsm->getStates()) {
				output = fsm->getOutput(state, STOUT_INPUT);
				if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
				sameOutput[output].insert(state);
				actOutputs.insert(output);
			}
			if (actOutputs.size() > 1)
				s.push_back(STOUT_INPUT);
			// save block with more then one state and clear sameOutput
			for (set<output_t>::iterator out = actOutputs.begin(); out != actOutputs.end(); out++) {
				if (sameOutput[*out].size() > 1) {
					block_t b(sameOutput[*out]);
					partition.insert(b);
				}
				sameOutput[*out].clear();
			}
			actOutputs.clear();
			// all blocks are singletons
			if (partition.empty()) {
				outHS.push_back(STOUT_INPUT);
				return outHS;
			}
		}
		else {
			block.clear();
			for (auto state : fsm->getStates()) {
				block.insert(state);
			}
			partition.insert(block);
		}
		queue<hs_node_t*> fifo;
		multimap<int, hs_node_t*> used;// <id, node>, id = getSetId(node) = sum of state IDs in the first block of node's partition
		pair <multimap<int, hs_node_t*>::iterator, multimap<int, hs_node_t*>::iterator> usedIt;
		hs_node_t* act, *succ;
		bool stop, stoutUsed = false;
		act = new hs_node_t(partition, s);
		fifo.push(act);
		used.insert(make_pair(getSetId(*partition.begin()), act));
		while (!fifo.empty()) {
			act = fifo.front();
			fifo.pop();
			for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
				stop = false;
				partition.clear();
				// go through all blocks in current partition
				for (partition_t::iterator pIt = act->partition.begin(); pIt != act->partition.end(); pIt++) {
					// go through all states in current block
					for (block_t::iterator blockIt = (*pIt).begin(); blockIt != (*pIt).end(); blockIt++) {
						output = fsm->getOutput(*blockIt, input);
						if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
						if (output == WRONG_OUTPUT) {
							// there is no transition so next state is uncertain
							stop = true;
							break;
						}
						sameOutput[output].insert(fsm->getNextState(*blockIt, input));
						actOutputs.insert(output);
					}
					// save block with more then one state
					for (set<output_t>::iterator out = actOutputs.begin(); out != actOutputs.end(); out++) {
						if (!stop && (sameOutput[*out].size() > 1)) {
							block_t b(sameOutput[*out]);
							partition.insert(b);
						}
						sameOutput[*out].clear();
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
							output = fsm->getOutput(state, STOUT_INPUT);
							if (output == DEFAULT_OUTPUT) output = fsm->getNumberOfOutputs();
							sameOutput[output].insert(state);
							actOutputs.insert(output);
						}
						if (actOutputs.size() > 1) stoutUsed = true;
						// save block with more then one state and clear sameOutput
						for (set<output_t>::iterator out = actOutputs.begin(); out != actOutputs.end(); out++) {
							if (sameOutput[*out].size() > 1) {
								block_t b(sameOutput[*out]);
								tmp.insert(b);
							}
							sameOutput[*out].clear();
						}
						actOutputs.clear();
					}
					if (stoutUsed) {
						partition = tmp;
					}
				}
				// all blocks are singletons
				if (partition.empty()) {
					outHS = act->hs;
					outHS.push_back(input);
					if (stoutUsed) outHS.push_back(STOUT_INPUT);
					for (multimap<int, hs_node_t*>::iterator it = used.begin(); it != used.end(); it++) {
						delete it->second;
					}
					used.clear();
					return outHS;
				}
				// go through all blocks in new partition
				for (partition_t::iterator pIt = partition.begin(); pIt != partition.end(); pIt++) {
					usedIt = used.equal_range(getSetId(*pIt));
					for (multimap<int, hs_node_t*>::iterator it = usedIt.first; it != usedIt.second; it++) {
						if (isSubsetPartition(it->second->partition.begin(), it->second->partition.end(),
							pIt, partition.end())) {
							stop = true;
							break;
						}
					}
					if (stop) break;
				}
				// create new node
				if (!stop) {
					s = act->hs;
					s.push_back(input);
					if (stoutUsed) s.push_back(STOUT_INPUT);
					succ = new hs_node_t(partition, s);
					fifo.push(succ);
					used.insert(make_pair(getSetId(*partition.begin()), succ));
				}
			}
		}
		for (multimap<int, hs_node_t*>::iterator it = used.begin(); it != used.end(); it++) {
			delete it->second;
		}
		used.clear();
		return outHS;
	}
}