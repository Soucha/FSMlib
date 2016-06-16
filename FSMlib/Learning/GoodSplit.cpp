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

#include "FSMlearning.h"

namespace FSMlearning {
	struct gs_node_t {
		sequence_in_t accessSequence;
		state_t state;
		vector<shared_ptr<gs_node_t>> succ;
		output_t incomingOutput, stateOutput;
		size_t counter;

		state_t lastComparedState;
		vector<shared_ptr<gs_node_t>> consistentStates;

		gs_node_t(sequence_in_t seq, const input_t& numInputs) : 
			accessSequence(move(seq)), state(NULL_STATE), succ(numInputs),
			incomingOutput(DEFAULT_OUTPUT), stateOutput(DEFAULT_OUTPUT),
			counter(0), lastComparedState(0) {
		}
	};

	struct out_node_t {
		size_t counter;
		map<output_t, shared_ptr<out_node_t>> succ;
	};

	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static bool areNodesDifferent(const shared_ptr<gs_node_t>& first, const shared_ptr<gs_node_t>& second) {
		if ((first->stateOutput != second->stateOutput) && 
			(first->stateOutput != DEFAULT_OUTPUT) && (second->stateOutput != DEFAULT_OUTPUT)) return true;
		for (input_t i = 0; i < first->succ.size(); i++) {
			if ((first->succ[i]) && (second->succ[i]) &&
				(((first->succ[i]->incomingOutput != second->succ[i]->incomingOutput) &&
				(first->succ[i]->incomingOutput != DEFAULT_OUTPUT) && (second->succ[i]->incomingOutput != DEFAULT_OUTPUT))
				|| areNodesDifferent(first->succ[i], second->succ[i])))
				return true;
		}
		return false;
	}

	static bool makeClosed(state_t& endState, vector<shared_ptr<gs_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto end = endState;
		endState = 0;
		for (state_t state = 0; state < end; state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				if (!stateNodes[state]->succ[i]) {
					stateNodes[state]->succ[i] = make_shared<gs_node_t>(stateNodes[state]->accessSequence, conjecture->getNumberOfInputs());
					stateNodes[state]->succ[i]->accessSequence.emplace_back(i);
				}
				auto & node = stateNodes[state]->succ[i];
				if (node->state != NULL_STATE) continue;
				if (conjecture->isOutputTransition() && (node->incomingOutput == DEFAULT_OUTPUT)) {
					if (teacher->isProvidedOnlyMQ() || (!conjecture->isOutputState()) || (node->stateOutput != DEFAULT_OUTPUT)) {
						node->incomingOutput = teacher->resetAndOutputQueryOnSuffix(stateNodes[state]->accessSequence, i);
					} else {// DFSM
						sequence_in_t suffix({ i, STOUT_INPUT });
						auto outputSeq = teacher->resetAndOutputQueryOnSuffix(stateNodes[state]->accessSequence, suffix);
						node->incomingOutput = outputSeq.front();
						node->stateOutput = outputSeq.back();
					}
				}
				if (conjecture->isOutputState() && (node->stateOutput == DEFAULT_OUTPUT)) {
					sequence_in_t prefix(stateNodes[state]->accessSequence);
					prefix.emplace_back(i);
					node->stateOutput = teacher->resetAndOutputQueryOnSuffix(prefix, STOUT_INPUT);
				}
				// compare with previous states
				for (int sn = 0; sn < node->consistentStates.size(); sn++) {
					if (areNodesDifferent(node->consistentStates[sn], node)) {
						node->consistentStates[sn] = node->consistentStates.back();
						node->consistentStates.pop_back();
						sn--;
					}
				}
				for (auto sn = node->lastComparedState; sn < stateNodes.size(); sn++) {
					if (!areNodesDifferent(stateNodes[sn], node)) {
						node->consistentStates.emplace_back(stateNodes[sn]);
					}
				}
				node->lastComparedState = state_t(stateNodes.size());
				if (node->consistentStates.empty()) {// new state
					checkNumberOfOutputs(teacher, conjecture);
					node->state = conjecture->addState(node->stateOutput);
					stateNodes.emplace_back(node);
					conjecture->setTransition(state, i, node->state, node->incomingOutput);
					end = state_t(stateNodes.size());
					endState = state + 1;
				} else {//if (node->consistentStates.size() == 1) {
					checkNumberOfOutputs(teacher, conjecture);
					conjecture->setTransition(state, i, node->consistentStates[rand() % node->consistentStates.size()]->state, node->incomingOutput);
				}
			}
		}
		return (endState != 0);
	}

	static output_t getLastOutput(const shared_ptr<gs_node_t >& node, const sequence_in_t& sequence, bool getStateOutput) {
		auto curr = node;
		for (auto& input : sequence) {
			if (input == STOUT_INPUT) continue;
			if (!curr->succ[input]) return DEFAULT_OUTPUT;
			curr = curr->succ[input];
		}
		return ((sequence.back() == STOUT_INPUT) || getStateOutput) ? curr->stateOutput : curr->incomingOutput;
	}

	static sequence_out_t getOutput(const shared_ptr<gs_node_t >& node, const sequence_in_t& sequence, bool getStateOutput) {
		auto curr = node;
		sequence_out_t output;
		for (auto& input : sequence) {
			if (input == STOUT_INPUT) {
				output.emplace_back(curr->stateOutput);
			} else {
				if (!curr->succ[input]) {
					output.emplace_back(DEFAULT_OUTPUT);
					return output;
				}
				curr = curr->succ[input];
				output.emplace_back(getStateOutput ? curr->stateOutput : curr->incomingOutput);
			}
		}
		return output;
	}

	static void query(const shared_ptr<gs_node_t >& node, const sequence_in_t& sequence, const unique_ptr<Teacher>& teacher, bool setStateOutput) {
		auto output = teacher->resetAndOutputQueryOnSuffix(node->accessSequence, sequence);
		node->counter++;
		auto curr = node;
		for (auto& input : sequence) {
			if (input == STOUT_INPUT) {
				if (!teacher->isProvidedOnlyMQ()) {
					curr->stateOutput = output.front();
					output.pop_front();
				}
				continue;
			}
			if (!curr->succ[input]) {
				curr->succ[input] = make_shared<gs_node_t>(curr->accessSequence, teacher->getNumberOfInputs());
				curr->succ[input]->accessSequence.emplace_back(input);
			}
			curr = curr->succ[input];
			if (!teacher->isProvidedOnlyMQ()) {
				if (setStateOutput) curr->stateOutput = output.front();
				else curr->incomingOutput = output.front();
				output.pop_front();
			}
			else {
				curr->counter++;
			}
		}
		if (teacher->isProvidedOnlyMQ()) {
			if ((sequence.back() == STOUT_INPUT) || setStateOutput) curr->stateOutput = output.back();
			else curr->incomingOutput = output.back();
			curr->counter--;
		}
	}

	static void addOutputToOutTree(const shared_ptr<out_node_t>& root, const sequence_out_t& output) {
		auto node = root;
		for (auto& out : output) {
			if (out == DEFAULT_OUTPUT) break;
			auto it = node->succ.find(out);
			if (it == node->succ.end()) {
				auto next = make_shared<out_node_t>();
				node->succ.emplace(out, next);
				next->counter = 0;
				node = next;
			} else {
				node = it->second;
			}
		}
		node->counter++;
	}

	static size_t getNumOfEqualOutputs(const shared_ptr<out_node_t>& node) {
		size_t maxNum = 0;
		for (auto& p : node->succ) {
			auto num = getNumOfEqualOutputs(p.second);
			if (maxNum < num) maxNum = num;
		}
		return maxNum + node->counter;
	}

	static size_t closeNextStates(const vector<shared_ptr<gs_node_t>>& stateNodes, const sequence_vec_t& distSequences,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		size_t totalSeqApplied = 0;
		for (state_t state = 0; state < stateNodes.size(); state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				auto & node = stateNodes[state]->succ[i];
				if (node->state != NULL_STATE) continue;
				while (node->consistentStates.size() > 1) {
					auto bestSeqIt = distSequences.end();
					size_t maxDist = 0, notIdentified = node->consistentStates.size();
					for (auto it = distSequences.begin(); it != distSequences.end(); ++it) {
						if (getLastOutput(node, *it, !conjecture->isOutputTransition()) != DEFAULT_OUTPUT) continue;
						size_t withoutSeq = 0, distinguishedNum;
						if (teacher->isProvidedOnlyMQ()) {
							map<output_t, state_t> outCounter;
							for (auto& cn : node->consistentStates) {
								auto output = getLastOutput(cn, *it, !conjecture->isOutputTransition());
								if (output == DEFAULT_OUTPUT) {
									withoutSeq++;
									continue;
								}
								auto statIt = outCounter.find(output);
								if (statIt == outCounter.end()) {
									outCounter.emplace(move(output), 1);
								} else {
									statIt->second++;
								}
							}
							if (outCounter.empty()) continue;
							// find the largest group of states with the same output
							distinguishedNum = outCounter.begin()->second;
							for (auto& p : outCounter) {
								if (distinguishedNum < p.second) distinguishedNum = p.second;
							}
						}
						else {
							auto outputTree = make_shared<out_node_t>();
							outputTree->counter = 0;
							for (auto& cn : node->consistentStates) {
								auto output = getOutput(cn, *it, !conjecture->isOutputTransition());
								addOutputToOutTree(outputTree, output);
							}
							withoutSeq = outputTree->counter;
							if (withoutSeq == node->consistentStates.size()) continue;
							distinguishedNum = getNumOfEqualOutputs(outputTree) - withoutSeq;
						}
						// minimum of distinguished states after applying this distSeq
						distinguishedNum = node->consistentStates.size() - withoutSeq - distinguishedNum;
						if ((distinguishedNum > maxDist) || ((distinguishedNum == maxDist) && (withoutSeq < notIdentified))) {
							maxDist = distinguishedNum;
							notIdentified = withoutSeq;
							bestSeqIt = it;
						}
					}
					query(node, *bestSeqIt, teacher, !conjecture->isOutputTransition());
					
					// update node->consistentStates;
					auto refOutput = getOutput(node, *bestSeqIt, !conjecture->isOutputTransition());
					for (int sn = 0; sn < node->consistentStates.size(); sn++) {
						bool distinguished;
						if(teacher->isProvidedOnlyMQ()) {
							auto output = getLastOutput(node->consistentStates[sn], *bestSeqIt, !conjecture->isOutputTransition());
							distinguished = (output != DEFAULT_OUTPUT) && (output != refOutput.back());
						} else {
							auto output = getOutput(node->consistentStates[sn], *bestSeqIt, !conjecture->isOutputTransition());
							distinguished = false;
							for (auto itRef = refOutput.begin(), it = output.begin(); it != output.end(); ++itRef, ++it) {
								if ((*it != DEFAULT_OUTPUT) && (*it != *itRef)) {
									distinguished = true;
									break;
								}
							}
						}
						if (distinguished) {
							node->consistentStates[sn] = node->consistentStates.back();
							node->consistentStates.pop_back();
							sn--;
						}
					}
					if (node->consistentStates.empty()) {
						totalSeqApplied += distSequences.size() - node->counter;
					}
				}
				totalSeqApplied += node->counter;
			}
		}
		return totalSeqApplied;
	}
	
	static void extendDistinguishingSequences(sequence_vec_t& distSequences, list<sequence_in_t>& longestDistSequences,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		list<sequence_in_t> tmpLongest;
		for (input_t input = 0; input < conjecture->getNumberOfInputs(); input++) {
			for (auto seq : longestDistSequences) {
				seq.emplace_back(input);
				if (teacher->isProvidedOnlyMQ() || (conjecture->getType() != TYPE_DFSM))
					distSequences.emplace_back(seq);
				if (conjecture->getType() == TYPE_DFSM) {
					seq.emplace_back(STOUT_INPUT);
					distSequences.emplace_back(seq);
				}
				tmpLongest.emplace_back(move(seq));
			}
		}
		longestDistSequences.swap(tmpLongest);
	}

	static void makeRandomQueries(const vector<shared_ptr<gs_node_t>>& stateNodes, const sequence_vec_t& distSequences,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher, size_t remainingSeq) {
		auto numStates = conjecture->getNumberOfStates();
		auto numInputs = conjecture->getNumberOfInputs();
		auto numQueries = numStates / 2 + numStates % 2;
		shared_ptr<gs_node_t> node;
		size_t seqIdx;
		while (numQueries > 0) {
			do {
				node = stateNodes[rand() % numStates]->succ[rand() % numInputs];
			} while ((node->state != NULL_STATE) || (node->consistentStates.empty()) || (node->counter == distSequences.size()));
			do {
				seqIdx = rand() % distSequences.size();
			} while (getLastOutput(node, distSequences[seqIdx], !conjecture->isOutputTransition()) != DEFAULT_OUTPUT);// queried?
			query(node, distSequences[seqIdx], teacher, !conjecture->isOutputTransition());
			remainingSeq--;
			if (getLastOutput(node->consistentStates.front(), distSequences[seqIdx], !conjecture->isOutputTransition()) == DEFAULT_OUTPUT) {
				query(node->consistentStates.front(), distSequences[seqIdx], teacher, !conjecture->isOutputTransition());
				if (teacher->isProvidedOnlyMQ()) remainingSeq--;// some state's next node was extended
			}
			bool distinguished = (teacher->isProvidedOnlyMQ() ?
				(getLastOutput(node, distSequences[seqIdx], !conjecture->isOutputTransition()) !=
					getLastOutput(node->consistentStates.front(), distSequences[seqIdx], !conjecture->isOutputTransition())) :
				(getOutput(node, distSequences[seqIdx], !conjecture->isOutputTransition()) !=
					getOutput(node->consistentStates.front(), distSequences[seqIdx], !conjecture->isOutputTransition())));
			if (distinguished) {
				node->consistentStates.pop_back();// -> new state (or consistent with a new state)
				if (node->consistentStates.empty()) {
					remainingSeq -= (distSequences.size() - node->counter);
				}
			}
			if (remainingSeq == 0) break;
			numQueries--;
		}
	}

	static void extendInputs(const shared_ptr<gs_node_t>& node, const input_t& numInputs) {
		for (auto& next : node->succ) {
			if (next) extendInputs(next, numInputs);
		}
		node->succ.resize(numInputs);
	}

	unique_ptr<DFSM> GoodSplit(const unique_ptr<Teacher>& teacher, seq_len_t maxDistinguishingLength,
		function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel) {

		auto conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());

		vector<shared_ptr<gs_node_t>> stateNodes;
		stateNodes.emplace_back(make_shared<gs_node_t>(sequence_in_t(), conjecture->getNumberOfInputs()));
		stateNodes[0]->state = 0;
		if (conjecture->isOutputState()) {
			auto output = teacher->resetAndOutputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(0, output);
			stateNodes[0]->stateOutput = output;
		}
		if (provideTentativeModel) {
			if (!provideTentativeModel(conjecture)) return conjecture;
		}
		sequence_vec_t distSequences;
		list<sequence_in_t> longestDistSequences;
		longestDistSequences.emplace_back(sequence_in_t());
		extendDistinguishingSequences(distSequences, longestDistSequences, conjecture, teacher);
		seq_len_t len = 1;
		while (len <= maxDistinguishingLength) {
			// 1. step - make the tree closed and update conjecture accordingly
			auto endState = state_t(stateNodes.size());
			while (makeClosed(endState, stateNodes, conjecture, teacher));
			if (provideTentativeModel) {
				if (!provideTentativeModel(conjecture)) return conjecture;
			}
			if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
				auto numInputs = teacher->getNumberOfInputs();
				extendInputs(stateNodes[0], numInputs);
				conjecture->incNumberOfInputs(numInputs - conjecture->getNumberOfInputs());
				continue;
			}
			// 2. step - a greedy choice of distinguishing sequence
			auto totalApplied = closeNextStates(stateNodes, distSequences, conjecture, teacher);
			// |X^(<=len)| * |P - D| ... |P-D| = number of successor nodes of state nodes that are not states
			auto totalDistSeq = distSequences.size() * (conjecture->getNumberOfStates() * (conjecture->getNumberOfInputs() - 1) + 1);
			// 3. step - the increase of len
			if (10 * totalApplied >= 9 * totalDistSeq) {// totalApplied needs to be at least 90 % of totalDistSeq
				len++;
				if (len > maxDistinguishingLength) break;
				// extend distSequences
				if (!teacher->isProvidedOnlyMQ()) {
					distSequences.clear();
					// clear all counters
					for (auto& sn : stateNodes) {
						for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
							if (sn->succ[i]->state == NULL_STATE) sn->succ[i]->counter = 0;
						}
					}
					totalApplied = 0;
				}
				extendDistinguishingSequences(distSequences, longestDistSequences, conjecture, teacher);
				totalDistSeq = distSequences.size() * (conjecture->getNumberOfStates() * (conjecture->getNumberOfInputs() - 1) + 1);
			}

			// 4. step - additional queries at random
			makeRandomQueries(stateNodes, distSequences, conjecture, teacher, totalDistSeq - totalApplied);
		}
		auto endState = state_t(stateNodes.size());
		while (makeClosed(endState, stateNodes, conjecture, teacher));// 1. step
		if (provideTentativeModel) {
			provideTentativeModel(conjecture);
		}
		return conjecture;
	}
}
