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
	static void deleteTree(prefix_set_node_t* ps) {
		if (ps == NULL) return;
		deleteTree(ps->neighbor);
		deleteTree(ps->child);
		delete ps;
	}

	static bool insertInTree(prefix_set_node_t* & ps, sequence_in_t seq) {
		if (ps == NULL) {
			ps = new prefix_set_node_t;
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

	PrefixSet::PrefixSet() {
		root = NULL;
	}

	PrefixSet::~PrefixSet() {
		deleteTree(root);
	}

	bool PrefixSet::insert(sequence_in_t seq) {
		if (seq.empty()) return false;
		return insertInTree(root, seq);
	}

	void PrefixSet::getMaximalSequences(sequence_set_t & outSet) {
		outSet.clear();
		if (root == NULL) {// TODO return empty sequence or empty set???
			//sequence_in_t seq;
			//outSet.insert(seq);
			return;
		}
		stack< pair<prefix_set_node_t*, sequence_in_t> > lifo;
		sequence_in_t seq;
		lifo.push(make_pair(root, seq));
		while (!lifo.empty()) {
			auto p = lifo.top();
			lifo.pop();
			if (p.first->neighbor != NULL) {
				lifo.push(make_pair(p.first->neighbor, p.second));
			}
			p.second.push_back(p.first->input);
			if (p.first->child == NULL) {
				outSet.insert(p.second);
			}
			else {
				lifo.push(make_pair(p.first->child, p.second));
			}
		}
	}

	void PrefixSet::popMaximalSequence(sequence_in_t& outSeq) {
		outSeq.clear();
		if (root == NULL) {
			return;
		}
		auto node = root;
		auto removeNode = root;
		while (node != NULL) {
			outSeq.push_back(node->input);
			if ((node->child != NULL) && (node->child->neighbor != NULL)) {
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
		node->neighbor = NULL;
		deleteTree(node);
	}

	bool PrefixSet::popMaximalSequenceWithGivenPrefix(sequence_in_t::iterator start,
		sequence_in_t::iterator end, sequence_in_t& outSeq) {
		outSeq.clear();
		auto node = root;
		prefix_set_node_t* removeNode = NULL;
		bool removingChild = false;
		while ((node != NULL) && (start != end)) {
			if (node->input == *start) {
				start++;
				if ((node->child != NULL) && (node->child->neighbor != NULL)) {
					removeNode = node;
					removingChild = true;
				}
				node = node->child;
			}
			else {
				if (node->neighbor != NULL) {
					removeNode = node;
					removingChild = false;
				}
				node = node->neighbor;
			}
		}
		if (start != end) return false;
		while (node != NULL) {
			outSeq.push_back(node->input);
			if ((node->child != NULL) && (node->child->neighbor != NULL)) {
				removeNode = node;
			}
			node = node->child;
		}
		if (removeNode == NULL) {
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
		node->neighbor = NULL;
		deleteTree(node);
		return true;
	}

	void PrefixSet::clear() {
		deleteTree(root);
		root = NULL;
	}

	bool PrefixSet::empty() {
		return (root == NULL);
	}

	seq_len_t PrefixSet::contains(sequence_in_t seq) {
		if (seq.empty()) return -1;
		seq_len_t len = seq_len_t(seq.size());
		prefix_set_node_t* node = root;
		while ((node != NULL) && (!seq.empty())) {
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