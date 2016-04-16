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
	struct prefix_set_node_t {
		input_t input = STOUT_INPUT;
		prefix_set_node_t *neighbor = NULL, *child = NULL;
	};

	class FSMLIB_API PrefixSet {
	public:
		PrefixSet();
		virtual ~PrefixSet();

		/**
		* Inserts given sequence in prefix set.
		* @param seq - sequence to insert
		* @return true if seq was inserted, false if prefix set had contained seq
		*/
		bool insert(sequence_in_t seq);

		/**
		* Fills given set with maximal sequences of prefix set.
		* No maximal sequence is a prefix of another maximal one.
		* @param outSet
		*/
		void getMaximalSequences(sequence_set_t & outSet);

		/**
		* Erases and returns the first maximal sequence of prefix set.
		* @param outSeq - returned maximal sequence
		*/
		void popMaximalSequence(sequence_in_t & outSeq);

		/**
		* Erases and returns a suffix of maximal sequence of prefix set
		* with given prefix, if such sequence exists.
		* @param start of searched prefix
		* @param end of searched prefix
		* @param outSeq - returned maximal sequence WITHOUT given prefix
		* @return true if maximal sequence is returned, 
		*	false if there is no maximal sequence with given prefix
		*/
		bool popMaximalSequenceWithGivenPrefix(sequence_in_t::iterator start,
			sequence_in_t::iterator end, sequence_in_t & outSeq);
		
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
		* Empty seq produces the result -1.
		* @param seq
		* @return the length of the longest prefix
		*/
		seq_len_t contains(sequence_in_t seq);

	private:
		prefix_set_node_t* root;
	};
}
