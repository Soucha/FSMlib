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
	struct ce_t {
		state_t startState;
		sequence_in_t prefix;
		sequence_in_t suffix;
		vector<sequence_out_t> outputs;

		ce_t(state_t state, sequence_in_t ce) : startState(state), suffix(move(ce)) {
			init();
		}

		void init() {
			outputs.clear();
			outputs.resize(prefix.size() + suffix.size());
		}
	};
	
	static void checkNumberOfOutputs(const unique_ptr<Teacher>& teacher, const unique_ptr<DFSM>& conjecture) {
		if (conjecture->getNumberOfOutputs() != teacher->getNumberOfOutputs()) {
			conjecture->incNumberOfOutputs(teacher->getNumberOfOutputs() - conjecture->getNumberOfOutputs());
		}
	}

	static shared_ptr<dt_node_t> createNode(const shared_ptr<dt_node_t>& parent, sequence_in_t s, sequence_out_t output) {
		auto leaf = make_shared<dt_node_t>();
		leaf->level = parent->level + 1;
		leaf->parent = parent;
		leaf->sequence = move(s);
		leaf->incomingOutput = output;
		leaf->state = NULL_STATE;
		parent->succ.emplace(move(output), leaf);
		return leaf;
	}

	static void addState(shared_ptr<dt_node_t>& node, vector<shared_ptr<dt_node_t>>& stateNodes,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		node->state = conjecture->addState();
		stateNodes.emplace_back(node);
		if (conjecture->isOutputState()) {
			auto output = teacher->resetAndOutputQueryOnSuffix(node->sequence, STOUT_INPUT);
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(node->state, output);
		}
	}

	static shared_ptr<dt_node_t> sift(const shared_ptr<dt_node_t>& dt, const sequence_in_t& s, const unique_ptr<Teacher>& teacher) {
		auto currentNode = dt;
		while ((currentNode->state == NULL_STATE) || (currentNode->state == WRONG_STATE)) {
			auto output = teacher->resetAndOutputQueryOnSuffix(s, currentNode->sequence);
			auto it = currentNode->succ.find(output);
			if (it == currentNode->succ.end()) {
				return createNode(currentNode, s, move(output));
			}
			currentNode = it->second;
		}
		return currentNode;
	}

	static void addNewTransitions(state_t startState, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		for (state_t state = startState; state < stateNodes.size(); state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				sequence_in_t prefix(stateNodes[state]->sequence);
				prefix.emplace_back(i);
				auto dtNode = sift(dt, prefix, teacher);
				if (dtNode->state == NULL_STATE) {// new state
					addState(dtNode, stateNodes, conjecture, teacher);
				}
				conjecture->setTransition(state, i, dtNode->state);
				if (conjecture->isOutputTransition()) {
					auto output = teacher->resetAndOutputQuery(prefix);
					checkNumberOfOutputs(teacher, conjecture);
					conjecture->setOutput(state, output.back(), i);
				}
			}
		}
	}

	static void extendInputs(input_t byNumInputs, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto numStates = state_t(stateNodes.size());
		for (state_t state = 0; state < numStates; state++) {
			for (input_t i = conjecture->getNumberOfInputs(); i < conjecture->getNumberOfInputs() + byNumInputs; i++) {
				sequence_in_t prefix(stateNodes[state]->sequence);
				prefix.emplace_back(i);
				auto dtNode = sift(dt, prefix, teacher);
				if (dtNode->state == NULL_STATE) {// new state
					addState(dtNode, stateNodes, conjecture, teacher);
				}
				conjecture->setTransition(state, i, dtNode->state);
				if (conjecture->isOutputTransition()) {
					auto output = teacher->resetAndOutputQuery(prefix);
					conjecture->setOutput(state, output.back(), i);
				}
			}
		}
		if (numStates != state_t(stateNodes.size())) {
			addNewTransitions(numStates, dt, stateNodes, conjecture, teacher);
		}
	}

	static void updateConjecture(state_t splittedState, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto newState = stateNodes.back()->state;
		auto distNode = stateNodes[splittedState]->parent.lock();
		// fix all transitions that lead to the new state instead of to the splitted state
		for (state_t state = 0; state < newState; state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				if (conjecture->getNextState(state, i) == splittedState) {
					sequence_in_t prefix(stateNodes[state]->sequence);
					prefix.emplace_back(i);
					if (prefix == stateNodes[splittedState]->sequence) continue;
					if (prefix == stateNodes[newState]->sequence) {
						conjecture->setTransition(state, i, newState,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
						continue;
					}
					//auto dtNode = sift(dt, prefix, teacher);// similar to what follows but with more effort
					auto output = teacher->resetAndOutputQueryOnSuffix(prefix, distNode->sequence);
					auto it = distNode->succ.find(output);
					if (it == distNode->succ.end()) {// new state
						auto leaf = createNode(distNode, move(prefix), move(output));
						addState(leaf, stateNodes, conjecture, teacher);
						conjecture->setTransition(state, i, leaf->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
					else if (it->second->state != splittedState) {// == newState
						conjecture->setTransition(state, i, it->second->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
				}
			}
		}
		addNewTransitions(newState, dt, stateNodes, conjecture, teacher);
	}

	static bool findOutputInconsistency(ce_t& ce, const vector<shared_ptr<dt_node_t>>& stateNodes,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		ce.suffix.clear();
		ce.prefix.clear();
		sequence_out_t expOut;
		for (auto node : stateNodes) {
			auto state = node->state;
			while (node->level > 0) {
				const auto& expectedOutput = node->incomingOutput;
				node = node->parent.lock();
				if (teacher->isProvidedOnlyMQ()) {
					if (ce.suffix.empty() || (node->sequence.size() < ce.suffix.size())) {
						auto output = conjecture->getOutputAlongPath(state, node->sequence);
						if (output.back() != expectedOutput.back()) {
							ce.startState = state;
							ce.suffix = node->sequence;
							expOut = expectedOutput;
						}
					}
				}
				else {
					auto output = conjecture->getOutputAlongPath(state, node->sequence);
					sequence_in_t suffix;
					auto currState = state;
					auto outIt = expectedOutput.begin();
					for (auto inIt = node->sequence.begin(); inIt != node->sequence.end(); inIt++, outIt++) {
						suffix.push_back(*inIt); 
						if (*outIt != conjecture->getOutput(currState, *inIt)) {
							if (ce.suffix.empty() || (suffix.size() < ce.suffix.size())) {
								ce.startState = state;
								ce.suffix = suffix;
								expOut = expectedOutput;
							}
							break;
						}
						currState = conjecture->getNextState(currState, *inIt);
					}	
				}
			}
		}
		ce.init();
		if (ce.suffix.empty()) return false;
		while (expOut.size() > ce.suffix.size()) expOut.pop_back();
		ce.outputs[0] = move(expOut);
		return true;
	}

	static void shortenCE(sequence_in_t& ce, sequence_out_t& bbOutput,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		if (bbOutput.empty()) bbOutput = teacher->resetAndOutputQuery(ce);
		auto output = conjecture->getOutputAlongPath(0, ce);
		while (output.back() == bbOutput.back()) {
			ce.pop_back();
			output.pop_back();
			if (teacher->isProvidedOnlyMQ()) {
				bbOutput = teacher->resetAndOutputQuery(ce);
			}
			else {
				bbOutput.pop_back();
			}
		}
	}

	static void divideCE(ce_t& ce, const vector<shared_ptr<dt_node_t>>& stateNodes,
			const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		auto& prefix = ce.prefix;
		auto& suffix = ce.suffix;
		auto refOutput = conjecture->getOutputAlongPath(ce.startState, suffix).back();
		size_t len = suffix.size();
		size_t mod = 0;
		bool sameOutput = (ce.outputs[0].back() == refOutput);
		while (len + mod > 1) {
			if (sameOutput) {
				if (len == 1) break;
				mod = len % 2;
				len /= 2;
				for (size_t i = 0; i < len + mod; i++) {
					suffix.push_front(prefix.back());
					prefix.pop_back();
				}
			}
			else {
				len += mod;
				mod = len % 2;
				len /= 2;
				for (size_t i = 0; i < len; i++) {
					prefix.push_back(suffix.front());
					suffix.pop_front();
				}
			}
			auto output = teacher->resetAndOutputQueryOnSuffix(stateNodes[conjecture->getEndPathState(ce.startState, prefix)]->sequence, suffix);
			ce.outputs[prefix.size()] = output;
			sameOutput = (output.back() == refOutput);
		}
		if (!sameOutput) {
			prefix.push_back(suffix.front());
			suffix.pop_front();
		}
	}

	static shared_ptr<dt_node_t> splitState(ce_t& ce, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {

		auto oldOut = ce.outputs[ce.prefix.size()];
		if (oldOut.empty()) // should not happen
			oldOut = teacher->resetAndOutputQueryOnSuffix(stateNodes[conjecture->getEndPathState(ce.startState, ce.prefix)]->sequence, ce.suffix);
		auto lastInput = ce.prefix.back();
		ce.prefix.pop_back();		
		auto newOut = ce.outputs[ce.prefix.size()];
		if (newOut.empty()) {// should not happen
			ce.suffix.push_front(lastInput);
			newOut = teacher->resetAndOutputQueryOnSuffix(stateNodes[conjecture->getEndPathState(ce.startState, ce.prefix)]->sequence, ce.suffix);
			ce.suffix.pop_front();
		}
		if (!teacher->isProvidedOnlyMQ()) newOut.pop_front();
		if (lastInput == STOUT_INPUT) {// can this happen?
			lastInput = ce.prefix.back();
			ce.prefix.pop_back();
		}

		auto predState = conjecture->getEndPathState(ce.startState, ce.prefix);
		auto currState = conjecture->getNextState(predState, lastInput);

		auto refNode = stateNodes[currState];
		auto leaf = createNode(refNode, move(refNode->sequence), oldOut);
		leaf->state = currState;
		stateNodes[currState] = leaf;

		// access sequences are to be prefix closed
		ce.prefix = stateNodes[predState]->sequence;
		ce.prefix.push_back(lastInput);
		leaf = createNode(refNode, move(ce.prefix), newOut);
		addState(leaf, stateNodes, conjecture, teacher);

		refNode->sequence = move(ce.suffix);
		refNode->state = WRONG_STATE;

		updateConjecture(currState, dt, stateNodes, conjecture, teacher);
		return refNode;
	}

	static shared_ptr<dt_node_t> getLeastCommonAncestor(shared_ptr<dt_node_t> n1, shared_ptr<dt_node_t> n2) {
		while (n1->level > n2->level) n1 = n1->parent.lock();
		while (n1->level < n2->level) n2 = n2->parent.lock();
		while (n1 != n2) {
			n1 = n1->parent.lock();
			n2 = n2->parent.lock();
		}
		return n1;
	}

	static pair<input_t, shared_ptr<dt_node_t>> findSplitter(shared_ptr<dt_node_t> node, const vector<shared_ptr<dt_node_t>>& stateNodes,
			const unique_ptr<DFSM>& conjecture) {
		vector<state_t> states;
		stack<shared_ptr<dt_node_t>> dtLifo;
		dtLifo.push(node);
		while (!dtLifo.empty()) {// find all states in the subtree of the given node
			node = move(dtLifo.top());
			dtLifo.pop();
			for (auto& p : node->succ) {
				if (p.second->state == WRONG_STATE) {// inner node with temporal discriminator
					dtLifo.push(p.second);
				} else {// leaf -> state
					states.push_back(p.second->state);
				}
			}
		}

		input_t bestInput = STOUT_INPUT;
		seq_len_t minLen = 1;
		// distinguishing by an input
		for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
			set<output_t> outputs;
			for (auto& state : states) {
				outputs.insert(conjecture->getOutput(state, i));
			}
			if (outputs.size() > minLen) {
				minLen = outputs.size();
				bestInput = i;
			}
		}
		if (bestInput != STOUT_INPUT) return make_pair(bestInput, nullptr);
		
		// find the best successor
		shared_ptr<dt_node_t> bestSucc;
		size_t maxDist;
		for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
			set<state_t> nextStates;
			for (auto& state : states) {
				nextStates.insert(conjecture->getNextState(state, i));
			}
			shared_ptr<dt_node_t> lca = stateNodes[*nextStates.begin()];
			for (auto& state : nextStates) {
				lca = getLeastCommonAncestor(lca, stateNodes[state]);
			}
			if (lca->state == NULL_STATE) {// inner node with a permanent discriminator
				set<sequence_out_t> outputs;
				for (auto& state : nextStates) {
					auto node = stateNodes[state];
					while (node->level > lca->level + 1) node = node->parent.lock();
					outputs.insert(node->incomingOutput);
				}
				if ((bestInput == STOUT_INPUT) || (maxDist < outputs.size()) ||
					((maxDist == outputs.size()) && (minLen > lca->sequence.size()))) {
					bestInput = i;
					bestSucc = lca;
					maxDist = outputs.size();
					minLen = lca->sequence.size();
				}
			}
		}
		return make_pair(bestInput, bestSucc);
	}

	static bool findDiscriminator(shared_ptr<dt_node_t>& br, pair<input_t, shared_ptr<dt_node_t>>& bestSplitter,
		list<shared_ptr<dt_node_t>>& blockRoots, const vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture) {

		auto bestRoot = blockRoots.end();
		for (auto it = blockRoots.begin(); it != blockRoots.end(); it++) {
			auto splitter = findSplitter(*it, stateNodes, conjecture);
			if (splitter.first != STOUT_INPUT) {// splitter found
				if ((bestRoot == blockRoots.end()) || (!splitter.second) ||
					(bestSplitter.second->sequence.size() > splitter.second->sequence.size())) {
					bestRoot = it;
					bestSplitter = move(splitter);
					if (!bestSplitter.second) break;
				}
			}
		}
		if (bestRoot == blockRoots.end()) return false;
		br = *bestRoot;
		blockRoots.erase(bestRoot);
		return true;
	}

	static shared_ptr<dt_node_t> getSubtree(const shared_ptr<dt_node_t>& original,
		map<sequence_out_t, vector<shared_ptr<dt_node_t>>>& statesDirection, function<sequence_out_t(state_t)> getOutput) {

		auto newNode = make_shared<dt_node_t>();
		vector<sequence_out_t> outputs;
		seq_len_t i = 0;
		for (auto& p : original->succ) {
			outputs.push_back(p.first);
			if (p.second->state == WRONG_STATE) {
				auto node = getSubtree(p.second, statesDirection, getOutput);
				// merge
				for (auto& otherP : node->succ) {
					auto it = newNode->succ.find(otherP.first);
					if (it == newNode->succ.end()) {// first of this output
						newNode->succ.emplace(otherP.first, otherP.second);
						otherP.second->parent = newNode;
						otherP.second->level = i;
					}
					else {// another child has the same output
						if ((it->second->state != WRONG_STATE) || (it->second->sequence != original->sequence)) {// lower the child
							auto leaf = it->second;
							it->second = make_shared<dt_node_t>();
							it->second->incomingOutput = leaf->incomingOutput;
							it->second->parent = newNode;
							leaf->incomingOutput = outputs[leaf->level];
							leaf->parent = it->second;
							it->second->succ.emplace(leaf->incomingOutput, leaf);
							it->second->sequence = original->sequence;
							it->second->state = WRONG_STATE;
						}
						it->second->succ.emplace(p.first, otherP.second);
						otherP.second->parent = it->second;
						otherP.second->incomingOutput = p.first;
					}
				}
			}
			else {// leaf -> state
				sequence_out_t output = getOutput(p.second->state);
				auto it = newNode->succ.find(output);
				if (it == newNode->succ.end()) {// first of this output
					auto leaf = createNode(newNode, p.second->sequence, output);
					leaf->state = p.second->state;
					leaf->level = i;
					statesDirection[output].emplace_back(move(leaf));
				}
				else {// another child has the same output
					if ((it->second->state != WRONG_STATE) || (it->second->sequence != original->sequence)) {// lower the child
						auto leaf = it->second;
						it->second = make_shared<dt_node_t>();
						it->second->incomingOutput = leaf->incomingOutput;
						it->second->parent = newNode;
						leaf->incomingOutput = outputs[leaf->level];
						leaf->parent = it->second;
						it->second->succ.emplace(leaf->incomingOutput, leaf);
						it->second->sequence = original->sequence;
						it->second->state = WRONG_STATE;
					}
					auto leaf = createNode(it->second, p.second->sequence, p.first);
					leaf->state = p.second->state;
					statesDirection[output].emplace_back(move(leaf));
				}
			}
			i++;
		}
		return newNode;
	}

	static void updateLevels(const shared_ptr<dt_node_t>& node) {
		for (auto& p : node->succ) {
			p.second->level = node->level + 1;
			if (p.second->state == WRONG_STATE)
				updateLevels(p.second);
		}
	}

	static sequence_out_t findOutput(shared_ptr<dt_node_t> node, const sequence_in_t& distSeq, 
			const sequence_in_t& prefix, const unique_ptr<Teacher>& teacher) {
		while (node->parent.lock()) {
			auto parent = node->parent.lock();
			if (teacher->isProvidedOnlyMQ()) {
				if (parent->sequence == distSeq) return node->incomingOutput;
			} else {
				if (distSeq.size() <= parent->sequence.size()) {
					sequence_out_t out;
					auto it1 = distSeq.begin();
					auto it2 = parent->sequence.begin();
					auto outIt = node->incomingOutput.begin();
					for (; (it1 != distSeq.end()) && (*it1 == *it2); it1++, it2++, outIt++) {
						out.emplace_back(*outIt);
					}
					if (it1 == distSeq.end()) return out;
				}
			}
			node = parent;
		}
		return teacher->resetAndOutputQueryOnSuffix(prefix, distSeq);
	}

	static void updateTransitionsTo(state_t targetState, const shared_ptr<dt_node_t>& br, const sequence_out_t& expOutput,
		const state_t& numStates, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		for (state_t state = 0; state < numStates; state++) {
			for (input_t i = 0; i < conjecture->getNumberOfInputs(); i++) {
				if (conjecture->getNextState(state, i) == targetState) {
					sequence_in_t prefix(stateNodes[state]->sequence);
					prefix.emplace_back(i);
					if (prefix == stateNodes[targetState]->sequence) continue;// access sequence
					
					auto output = findOutput(stateNodes[targetState], br->sequence, prefix, teacher);
					if (output == expOutput) continue;// the transition does not change
					auto it = br->succ.find(output);
					if (it == br->succ.end()) {// new state
						auto leaf = createNode(br, move(prefix), move(output));
						addState(leaf, stateNodes, conjecture, teacher);
						conjecture->setTransition(state, i, leaf->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
					else {
						// sift in the subtree
						auto currentNode = it->second;
						while (currentNode->state == WRONG_STATE) {
							auto output = findOutput(stateNodes[targetState], currentNode->sequence, prefix, teacher);
							auto it = currentNode->succ.find(output);
							if (it == currentNode->succ.end()) {
								currentNode = createNode(currentNode, move(prefix), move(output));
								addState(currentNode, stateNodes, conjecture, teacher);
								break;
							}
							currentNode = it->second;
						}
						conjecture->setTransition(state, i, currentNode->state,
							conjecture->isOutputTransition() ? conjecture->getOutput(state, i) : DEFAULT_OUTPUT);
					}
				}
			}
		}
	}

	static void refineHypothesis(ce_t& ce, shared_ptr<dt_node_t>& dt, vector<shared_ptr<dt_node_t>>& stateNodes,
		const unique_ptr<DFSM>& conjecture, const unique_ptr<Teacher>& teacher) {
		
		list<shared_ptr<dt_node_t>> blockRoots;
		do {
			divideCE(ce, stateNodes, conjecture, teacher);
			auto blockRoot = splitState(ce, dt, stateNodes, conjecture, teacher);
			if (!blockRoot->parent.lock() || (blockRoot->parent.lock()->state == NULL_STATE)) {
				blockRoots.emplace_back(blockRoot);
			}
			pair<input_t, shared_ptr<dt_node_t>> splitter;
			while (findDiscriminator(blockRoot, splitter, blockRoots, stateNodes, conjecture)) {
				auto discriminator = splitter.second ? splitter.second->sequence : sequence_in_t();
				discriminator.push_front(splitter.first);
				if ((discriminator.back() == STOUT_INPUT) && conjecture->isOutputState() && !conjecture->isOutputTransition()) {// Moore and DFA
					discriminator.pop_back();
				}
				if ((blockRoot->sequence != discriminator) && (blockRoot->sequence.size() > discriminator.size())) {
					map<sequence_out_t, vector<shared_ptr<dt_node_t>>> statesDirection;
					auto node = getSubtree(blockRoot, statesDirection, [&](state_t state){
						return findOutput(stateNodes[state], discriminator, stateNodes[state]->sequence, teacher);
						/*// predict output
						auto node = stateNodes[conjecture->getNextState(state, splitter.first)];
						while (node->level > splitter.second->level + 1) node = node->parent.lock();
						auto output = node->incomingOutput;
						if (!teacher->isProvidedOnlyMQ()) output.push_front(conjecture->getOutput(state, splitter.first));
						return output;*/
					});
					node->level = blockRoot->level;
					updateLevels(node);
					node->state = WRONG_STATE;
					node->incomingOutput = move(blockRoot->incomingOutput);
					node->sequence = move(discriminator);
					if (blockRoot != dt) {
						node->parent = blockRoot->parent;
						blockRoot->parent.reset();// blockRoot begins to be a root of the old subtree
						node->parent.lock()->succ[node->incomingOutput] = node;
					}
					// update transitions and stateNodes
					state_t numStates = conjecture->getNumberOfStates();
					for (auto& sd : statesDirection) {
						for (auto& stateNode : sd.second) {
							updateTransitionsTo(stateNode->state, node, sd.first, numStates, stateNodes, conjecture, teacher);
							stateNodes[stateNode->state] = stateNode;
						}
					}
					if (blockRoot == dt) {
						dt = node;
					}
					blockRoot = node;
					if (numStates != conjecture->getNumberOfStates()) // new states found
						addNewTransitions(numStates, dt, stateNodes, conjecture, teacher);
				}
				blockRoot->state = NULL_STATE;
				for (auto& p : blockRoot->succ) {
					if (p.second->state == WRONG_STATE) {
						blockRoots.emplace_back(p.second);
					}
				}
			}
		} while (findOutputInconsistency(ce, stateNodes, conjecture, teacher));
	}

	unique_ptr<DFSM> TTT(const unique_ptr<Teacher>& teacher, function<bool(const unique_ptr<DFSM>& conjecture)> provideTentativeModel) {
		if (!teacher->isBlackBoxResettable()) {
			ERROR_MESSAGE("FSMlearning::TTT - the Black Box needs to be resettable");
			return nullptr;
		}
		/// counterexample
		sequence_in_t ce;
		/// Discrimination Tree root
		auto dt = make_shared<dt_node_t>();
		dt->level = 0;
		vector<shared_ptr<dt_node_t>> stateNodes;
		// numberOfOutputs can produce error message
		auto conjecture = FSMmodel::createFSM(teacher->getBlackBoxModelType(), 1, teacher->getNumberOfInputs(), teacher->getNumberOfOutputs());
		if (conjecture->isOutputState()) {
			dt->sequence.emplace_back(STOUT_INPUT);
			auto output = teacher->resetAndOutputQuery(STOUT_INPUT);
			checkNumberOfOutputs(teacher, conjecture);
			conjecture->setOutput(0, output);
			auto leaf = createNode(dt, sequence_in_t(), sequence_out_t({ output }));// the initial state -> empty access sequence
			leaf->state = 0;
			stateNodes.emplace_back(leaf);
			dt->state = NULL_STATE;
		}
		else {
			stateNodes.emplace_back(dt);
			dt->state = 0;
		}
		addNewTransitions(0, dt, stateNodes, conjecture, teacher);
		bool unlearned = true;
		if (provideTentativeModel) {
			unlearned = provideTentativeModel(conjecture);
		}
		while (unlearned) {
			ce = teacher->equivalenceQuery(conjecture);
			if (ce.empty()) unlearned = false;
			else {
				if (conjecture->getNumberOfInputs() != teacher->getNumberOfInputs()) {
					auto deltaInputs = teacher->getNumberOfInputs() - conjecture->getNumberOfInputs();
					extendInputs(deltaInputs, dt, stateNodes, conjecture, teacher);
					conjecture->incNumberOfInputs(deltaInputs);
				}
				if (conjecture->isOutputState() && !conjecture->isOutputTransition()) {// Moore and DFA
					sequence_in_t newCE;
					for (auto& input : ce) {
						if (input == STOUT_INPUT) continue;
						newCE.emplace_back(input);
					}
					ce.swap(newCE);
				}

				auto bbOutput = teacher->resetAndOutputQuery(ce);		
				bool isCE = true;
				do {
					shortenCE(ce, bbOutput, conjecture, teacher);
					ce_t divCE(0, ce);
					divCE.outputs[0] = bbOutput;
					refineHypothesis(divCE, dt, stateNodes, conjecture, teacher);
						
					if (provideTentativeModel) {
						unlearned = provideTentativeModel(conjecture);
					}
					auto output = conjecture->getOutputAlongPath(0, ce);
					isCE = teacher->isProvidedOnlyMQ() ? (output.back() != bbOutput.back()) : (output != bbOutput);
				} while (unlearned && isCE);
			}
		}
		return conjecture;
	}
}
