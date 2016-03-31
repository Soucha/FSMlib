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

#include "../Model/FSMmodel.h"

namespace FSMsequence {
	/**
	* Fills given set with input sequences that cover states.
	* State 0 is start state and FSM goes to unique state using each sequence.
	* Sequences are sorted by lenght and then lexicographically from the shortest.
	* @param dfsm - Deterministic FSM
	* @param stateCover
	*/
	FSMLIB_API void getStateCover(DFSM * dfsm, sequence_set_t & stateCover);

	/**
	* Fills given set with input sequences that cover all transitions.
	* Transition cover is extended state cover so that each sequence
	* of state cover is extended by each input symbol.
	* State 0 is start state for all sequences.
	* @param dfsm - Deterministic FSM
	* @param transitionCover
	*/
	FSMLIB_API void getTransitionCover(DFSM * dfsm, sequence_set_t & transitionCover);

	/**
	* Fills given set with all input sequences of the lenght up to depth.
	* @param dfsm - Deterministic FSM
	* @param traversalSet
	* @param depth
	*/
	FSMLIB_API void getTraversalSet(DFSM * dfsm, sequence_set_t & traversalSet, int depth);

	/**
	* Finds preset distinguishing sequence if FSM has it.<br><br>
	* Applying PDS distinguishes each state from the others.
	* @param dfsm - Deterministic FSM
	* @param outPDS
	* @return true if FSM has PDS, false otherwise
	* @throw Exception if FSM is not properly defined
	*/
	FSMLIB_API bool getPresetDistinguishingSequence(DFSM * dfsm, sequence_in_t & outPDS);

	/**
	* Finds adaptive distinguishing sequence if FSM has it.<br><br>
	* Applying ADS distinguishes each state from the others.
	* @param dfsm - Deterministic FSM
	* @param outADS
	* @return true if FSM has ADS, false otherwise
	*/
	FSMLIB_API bool getAdaptiveDistinguishingSequence(DFSM * dfsm, AdaptiveDS* & outADS);

	/**
	* Finds state verifying sequence for given state.<br><br>
	* Applying SVS distinguishes given state from the others.
	* @param dfsm - Deterministic FSM
	* @param state
	* @param outSVS
	* @return true if given state of FSM has SVS, false otherwise
	*/
	FSMLIB_API bool getStateVerifyingSequence(DFSM * dfsm, state_t state, sequence_in_t & outSVS);

	/**
	* Finds state verifying sequences for all states of FSM.<br>
	* If some state has not SVS,
	* input sequence in output vector will be empty.<br><br>
	* Applying SVS distinguishes related state from the others.
	* @param dfsm - Deterministic FSM
	* @param outVSet
	*/
	FSMLIB_API void getVerifyingSet(DFSM * dfsm, sequence_vec_t & outVSet);

	/**
	* Finds state characterizing set for given state.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes state from the others.
	* @param dfsm - Deterministic FSM
	* @param state
	* @param outSCSet
	*/
	FSMLIB_API void getStateCharacterizingSet(DFSM * dfsm, state_t state, sequence_set_t & outSCSet);

	/**
	* Finds state characterizing sets for all states of FSM.<br>
	* Output vector will have length of number of states.
	* @param dfsm - Deterministic FSM
	* @param outSCSets
	*/
	FSMLIB_API void getStatesCharacterizingSets(DFSM * dfsm, vector<sequence_set_t> & outSCSets);

	/**
	* Finds state characterizing sets for all states of FSM<br>
	* such that each pair of sets contains <br>
	* same separating sequence of related states.<br>
	* Result is thus a family of harmonized state identifiers.
	* Output vector will have length of the number of states.
	* @param dfsm - Deterministic FSM
	* @param outHarmSCSets
	*/
	FSMLIB_API void getHarmonizedStateIdentifiers(DFSM * dfsm, vector<sequence_set_t> & outSCSets);

	/**
	* Finds characterizing set for FSM.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes each state from the others.
	* @param dfsm - Deterministic FSM
	* @param outCSet
	*/
	FSMLIB_API void getCharacterizingSet(DFSM * dfsm, sequence_set_t & outCSet);

	/**
	* Finds synchronizing sequence of FSM if exists.<br><br>
	* SS brings FSM to one unique state regardless on output.
	* @param dfsm - Deterministic FSM
	* @param outSS
	* @return true if FSM has SS, false otherwise
	*/
	FSMLIB_API bool getSynchronizingSequence(DFSM * dfsm, sequence_in_t & outSS);

	/**
	* Finds homing sequence of FSM if exists.<br><br>
	* HS brings FSM to some specific states
	* which can be uniquely determined by output.
	* @param dfsm - Deterministic FSM
	* @param outHS
	* @return true if FSM has HS, false otherwise
	*/
	FSMLIB_API bool getPresetHomingSequence(DFSM * dfsm, sequence_in_t & outHS);

	/**
	* Fills given seq table with the shortest possible sequences
	* that distinguish related pairs of states.\n
	*
	* Pair (i,j) has index in seq derived from:\n
	* &nbsp; (0,1) => 0, (0,2) => 1,..., (0,N-1) => N-2\n
	* &nbsp; (1,2) => N-1,...\n
	* &nbsp; N is number of states and for each (i,j): i<j\n
	* &nbsp; (i,j) => i * N + j - 1 - (i * (i + 3)) / 2
	* @param dfsm - Deterministic FSM
	* @param seq
	*/
	FSMLIB_API void getStatePairsDistinguishingSequence(DFSM * dfsm, vector<sequence_in_t> & seq);

	/**
	* Fills given Characterizing table with all possible distinguishing
	* inputs for each state pair. A distinguishing input points to successive
	* state pair.
	* @param dfsm - Deterministic FSM
	* @param seq
	*/
	FSMLIB_API void getSeparatingSequences(DFSM * dfsm, vector<LinkCell*> & seq);

	/**
	* Finds all distinguishing types of sequences which FSM has.<br><br>
	* PDS and ADS don't be the shortest.<br><br>
	*
	* @param dfsm - Deterministic FSM
	* @param outPDS - it is filled only if PDS_FOUND is returned
	* @param outADS - it is filled when PDS_FOUND or ADS_FOUND is returned
	* @param outVSet - it is filled with sequences for all states when
	* PDS_FOUND, ADS_FOUND or SVS_FOUND is returned. However, if state has
	* SVS, then it is set in the outVSet although only CSet_FOUND is returned
	* @param outSCSets - it is always filled
	* @param outCSet - it is always filled
	* @return type of the strongest found sequence, i.e. PDS_FOUND,
	* ADS_FOUND, SVS_FOUND or CSet_FOUND
	*/
	FSMLIB_API int getDistinguishingSequences(DFSM * dfsm, sequence_in_t& outPDS, AdaptiveDS* & outADS,
		sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet);

}
