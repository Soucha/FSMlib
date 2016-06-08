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
#pragma once

#include "Sequences\FSMsequence.h"

namespace FSMlib {
	struct prefix_set_node_t;

	class FSMLIB_API PrefixSet {
	public:
		/**
		* Inserts given sequence in the prefix set.
		* @param seq - sequence to insert
		* @return true if seq was inserted, false if prefix set had contained seq
		*/
		bool insert(sequence_in_t seq);

		/**
		* Creates a collection of all maximal sequences of the prefix set.
		* No maximal sequence is a prefix of another maximal one.
		* @return a collection of maximal sequences
		*/
		sequence_set_t getMaximalSequences();

		/**
		* Erases and returns the first maximal sequence of the prefix set.
		* @return a maximal sequence, or an empty sequence if the set is empty
		*/
		sequence_in_t popMaximalSequence();

		/**
		* Erases and returns a suffix of maximal sequence of the prefix set
		* with given prefix, if such sequence exists.
		* @param start of searched prefix
		* @param end of searched prefix
		* @return maximal sequence WITHOUT given prefix, or an empty sequence if there is no such maximal sequence
		*/
		sequence_in_t popMaximalSequenceWithGivenPrefix(sequence_in_t::iterator start, sequence_in_t::iterator end);
		
		/*
		* Clears prefix set.
		*/
		void clear();

		/*
		* Checks if prefix set contains any sequence.
		* @return true if prefix set is empty, false otherwise
		*/
		bool empty();

		/**
		* Given sequence is checked if it or its prefix is included in the set.<br>
		* The length of the longest prefix is returned.<br>
		* That is, 0 if given sequence is not in the Prefixset,<br>
		* and the size of seq is returned if the set contains the entire seq.<br>
		* Empty seq produces the result of seq_len_t(-1).
		* @param seq
		* @return the length of the longest prefix
		*/
		seq_len_t contains(sequence_in_t seq);

	private:
		shared_ptr<prefix_set_node_t> root;
	};
}
