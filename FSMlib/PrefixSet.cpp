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

#include "PrefixSet.h"

namespace FSMlib {
	struct prefix_set_node_t {
		input_t input = STOUT_INPUT;
		shared_ptr<prefix_set_node_t> neighbor, child;
	};

	static bool insertInTree(shared_ptr<prefix_set_node_t>& ps, sequence_in_t& seq) {
		if (!ps) {
			ps = make_shared<prefix_set_node_t>();
			ps->input = seq.front();
			seq.pop_front();
			if (seq.empty()) return true;
			return insertInTree(ps->child, seq);
		}
		if (ps->input == seq.front()) {
			seq.pop_front();
			if (seq.empty()) return false;
			return insertInTree(ps->child, seq);
		}
		return insertInTree(ps->neighbor, seq);
	}

	bool PrefixSet::insert(sequence_in_t seq) {
		if (seq.empty()) return false;
		return insertInTree(root, seq);
	}

	sequence_set_t PrefixSet::getMaximalSequences() {
		if (!root) {// TODO return empty sequence or empty set???
			//sequence_in_t seq;
			//outSet.insert(seq);
			return sequence_set_t();
		}
		sequence_set_t outSet;
		stack<pair<shared_ptr<prefix_set_node_t>, sequence_in_t>> lifo;
		lifo.emplace(root, sequence_in_t());
		while (!lifo.empty()) {
			auto p = move(lifo.top());
			lifo.pop();
			if (p.first->neighbor) {
				lifo.emplace(p.first->neighbor, p.second);
			}
			p.second.push_back(p.first->input);
			if (p.first->child) {
				lifo.emplace(p.first->child, move(p.second));
			} else {
				outSet.emplace(move(p.second));
			}
		}
		return outSet;
	}

	sequence_in_t PrefixSet::popMaximalSequence() {
		if (!root) {
			return sequence_in_t();
		}
		sequence_in_t outSeq;
		auto node = root;
		auto removeNode = root;
		while (node) {
			outSeq.push_back(node->input);
			if (node->child && node->child->neighbor) {
				removeNode = node;
			}
			node = node->child;
		}
		if (removeNode == root) {
			node = root;
			root = root->neighbor;
		}
		else {
			node = removeNode->child;
			removeNode->child = node->neighbor;
		}
		node->neighbor.reset();
		node.reset();
		return outSeq;
	}

	sequence_in_t PrefixSet::popMaximalSequenceWithGivenPrefix(sequence_in_t::iterator start, sequence_in_t::iterator end) {
		auto node = root;
		shared_ptr<prefix_set_node_t> removeNode;
		bool removingChild = false;
		while (node && (start != end)) {
			if (node->input == *start) {
				start++;
				if (node->child && node->child->neighbor) {
					removeNode = node;
					removingChild = true;
				}
				node = node->child;
			}
			else {
				if (node->neighbor) {
					removeNode = node;
					removingChild = false;
				}
				node = node->neighbor;
			}
		}
		if (start != end) return sequence_in_t();
		sequence_in_t outSeq;
		while (node) {
			outSeq.push_back(node->input);
			if (node->child && node->child->neighbor) {
				removeNode = node;
			}
			node = node->child;
		}
		if (!removeNode) {
			node = root;
			root = root->neighbor;
		}
		else if (removingChild) {
			node = removeNode->child;
			removeNode->child = node->neighbor;
		}
		else {
			node = removeNode->neighbor;
			removeNode->neighbor = node->neighbor;
		}
		node->neighbor.reset();
		node.reset();
		return outSeq;
	}

	void PrefixSet::clear() {
		root.reset();
	}

	bool PrefixSet::empty() {
		return !root;
	}

	seq_len_t PrefixSet::contains(sequence_in_t seq) {
		if (seq.empty()) return -1;
		seq_len_t len = seq_len_t(seq.size());
		auto node = root;
		while (node && (!seq.empty())) {
			if (node->input == seq.front()) {
				seq.pop_front();
				node = node->child;
			}
			else {
				node = node->neighbor;
			}
		}
		return seq_len_t(len - seq.size());
	}
}
