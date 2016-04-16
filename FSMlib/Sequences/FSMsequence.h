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
	* input sequence in output vector will be empty.<br>
	* Sequence outVSet[i] belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).<br>
	* Applying SVS distinguishes related state from the others.
	* @param dfsm - Deterministic FSM
	* @param outVSet
	*/
	FSMLIB_API void getVerifyingSet(DFSM * dfsm, sequence_vec_t & outVSet);

	/**
	* Fills given seq table with the shortest possible sequences
	* that distinguish related pairs of states.\n
	*
	* State pair (i,j) has index in seq derived from:
	* &nbsp; (0,1) => 0, (0,2) => 1,..., (0,N-1) => N-2
	* &nbsp; (1,2) => N-1,...
	* &nbsp; i, j are indexes to the vector of states dfsm->getStates()
	* &nbsp; N is number of states and for each (i,j): i<j
	* &nbsp; (i,j) => i * N + j - 1 - (i * (i + 3)) / 2
	*
	* @param dfsm - Deterministic FSM
	* @param seq - a collection of shortest separating sequences for each state pair
	*/
	FSMLIB_API void getStatePairsShortestSeparatingSequences(DFSM * dfsm, vector<sequence_in_t> & seq);

#ifdef PARALLEL_COMPUTING
	/**
	* Fills given seq table with the shortest possible sequences
	* that distinguish related pairs of states.\n
	*
	* State pair (i,j) has index in seq derived from:
	* &nbsp; (0,1) => 0, (0,2) => 1,..., (0,N-1) => N-2
	* &nbsp; (1,2) => N-1,...
	* &nbsp; i, j are indexes to the vector of states dfsm->getStates()
	* &nbsp; N is number of states and for each (i,j): i<j
	* &nbsp; (i,j) => i * N + j - 1 - (i * (i + 3)) / 2
	*
	* A thread distinguishes one state pair by a different output on a common input in the first run
	* or in a consecutive run if its next states' pair is distinguished.
	*
	* @param dfsm - Deterministic FSM
	* @param seq - a collection of shortest separating sequences for each state pair, or empty collection if there is an error
	*/
	FSMLIB_API void getStatePairsShortestSeparatingSequences_ParallelSF(DFSM * dfsm, vector<sequence_in_t> & seq);

	/**
	* Fills given seq table with the shortest possible sequences
	* that distinguish related pairs of states.\n
	*
	* State pair (i,j) has index in seq derived from:
	* &nbsp; (0,1) => 0, (0,2) => 1,..., (0,N-1) => N-2
	* &nbsp; (1,2) => N-1,...
	* &nbsp; i, j are indexes to the vector of states dfsm->getStates()
	* &nbsp; N is number of states and for each (i,j): i<j
	* &nbsp; (i,j) => i * N + j - 1 - (i * (i + 3)) / 2
	*
	* At first, a thread distinguishes one state pair if it produces different outputs on an input.
	* Otherwise, a link from next states' pair is created.
	* Links are stored in memory in the third run after prescan counts required space.
	* Finally, a queue of distinguished pairs distinguishes previous state pairs, again a thread per state pair.
	*
	* @param dfsm - Deterministic FSM
	* @param seq - a collection of shortest separating sequences for each state pair, or empty collection if there is an error
	*/
	FSMLIB_API void getStatePairsShortestSeparatingSequences_ParallelQueue(DFSM * dfsm, vector<sequence_in_t> & seq);
#endif

	/**
	* Fills given Characterizing table with all possible distinguishing
	* inputs for each state pair. A distinguishing input points to successive
	* state pair.
	* @param dfsm - Deterministic FSM
	* @param seq
	*/
	FSMLIB_API void getSeparatingSequences(DFSM * dfsm, vector<LinkCell*> & seq);

	/**
	* Finds state characterizing set for given state.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes state from the others.
	* @param dfsm - Deterministic FSM
	* @param state
	* @param outSCSet
	* @param filterPrefixes - no sequence of resulted SCSet is a prefix of another one if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted SCSet additionally,
	*			- reduceSCSet_LS_SL or reduceSCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	*/
	FSMLIB_API void getStateCharacterizingSet(DFSM * dfsm, state_t state, sequence_set_t & outSCSet, 
		void(*getSeparatingSequences)(DFSM * dfsm, vector<sequence_in_t> & seq) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true, void(*reduceFunc)(DFSM * dfsm, state_t stateIdx, sequence_set_t & outSCSet) = NULL);

	/**
	* Finds state characterizing sets for all states of FSM.<br>
	* Output vector will have length of the number of states.<br>
	* Sequence set outSCSets[i] belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).
	* @param dfsm - Deterministic FSM
	* @param outSCSets
	* @param filterPrefixes - no sequence of any SCSet is a prefix of another one of the same SCSet if true
	* @param reduceFunc - a pointer to function that can reduce the size of each resulted SCSet additionally,
	*			- reduceSCSet_LS_SL or reduceSCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	*/
	FSMLIB_API void getStatesCharacterizingSets(DFSM * dfsm, vector<sequence_set_t> & outSCSets, 
		void(*getSeparatingSequences)(DFSM * dfsm, vector<sequence_in_t> & seq) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true,	void(*reduceFunc)(DFSM * dfsm, state_t stateIdx, sequence_set_t & outSCSet) = NULL);

	/**
	* Finds state characterizing sets for all states of FSM<br>
	* such that each pair of sets contains <br>
	* same separating sequence of related states.<br>
	* Result is thus a family of harmonized state identifiers.
	* Output vector will have length of the number of states.<br>
	* Sequence set outSCSets[i] belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).
	* @param dfsm - Deterministic FSM
	* @param outHarmSCSets
	* @param filterPrefixes - no sequence of any SCSet is a prefix of another one of the same SCSet if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted CSet additionally
	*/
	FSMLIB_API void getHarmonizedStateIdentifiers(DFSM * dfsm, vector<sequence_set_t> & outSCSets, 
		void(*getSeparatingSequences)(DFSM * dfsm, vector<sequence_in_t> & seq) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true, void(*reduceFunc)(DFSM * dfsm, state_t stateIdx, sequence_set_t & outSCSet) = NULL);

	/**
	* Finds characterizing set for FSM.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes each state from the others.
	* @param dfsm - Deterministic FSM
	* @param outCSet
	* @param filterPrefixes - no sequence of resulted CSet is a prefix of another one if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted CSet additionally,
	*			- reduceCSet_LS_SL or reduceCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	*/
	FSMLIB_API void getCharacterizingSet(DFSM * dfsm, sequence_set_t & outCSet, 
		void(*getSeparatingSequences)(DFSM * dfsm, vector<sequence_in_t> & seq) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true,	void(*reduceFunc)(DFSM * dfsm, sequence_set_t & outCSet) = NULL);

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
	* Finds all distinguishing types of sequences which FSM has.<br><br>
	* PDS and ADS don't be the shortest.<br><br>
	*
	* @param dfsm - Deterministic FSM
	* @param outPDS - it is filled only if PDS_FOUND is returned
	* @param outADS - it is filled when PDS_FOUND or ADS_FOUND is returned
	* @param outVSet - it is filled with sequences for all states when
	*   PDS_FOUND, ADS_FOUND or SVS_FOUND is returned. However, if a state has
	*   SVS, then it is set in the outVSet although only CSet_FOUND is returned.
	*   Sequence outVSet[i] belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).
	* @param outSCSets - it is always filled
	*   Sequence set outSCSets[i] belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).
	* @param outCSet - it is always filled
	* @return type of the strongest found sequence, i.e. PDS_FOUND,
	* ADS_FOUND, SVS_FOUND or CSet_FOUND
	*/
	FSMLIB_API int getDistinguishingSequences(DFSM * dfsm, sequence_in_t& outPDS, AdaptiveDS* & outADS,
		sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet);

	/**
	* Finds index of given state in given sorted collection of state Ids in logarithmic time (in the number of states).
	* @param states = fsm->getStates()
	* @param stateId that is in states
	* @return index of stateId in states, or NULL_STATE if missing
	*/
	FSMLIB_API state_t getIdx(vector<state_t>& states, state_t stateId);

	/**
	* Reduces the number of sequences in given Characterizing set.
	* At first, it goes from longer sequences to shorter ones
	* and removes unused ones (that don't distinguish any yet undistinguished pair of states).
	* Next it goes from shorter to longer sequences in the set
	* and again removes unused ones, or some distinguishable sequences could be shortened
	* so it truncates these sequences as much as possible.
	* @param dfsm
	* @param outCSet
	*/
	FSMLIB_API void reduceCSet_LS_SL(DFSM * dfsm, sequence_set_t & outCSet);
	
	/**
	* Reduces the number of sequence in given Characterizing set.
	* It chooses gradually a sequence of equally long sequences that
	* distinguishes most state pairs with the last symbol.
	* Sequences that do not distinguish any yet undistinguished pair of states are removed.
	*
	* CSet is expected to be sorted by length of sequences, and
	* no sequence should begin with STOUT_INPUT besides the first one
	* that contains only STOUT_INPUT and fsm->isOutputState() holds.
	*
	* For each state pair, CSet needs to contain a sequence that particular state pair in the last symbol
	* because this function does not truncate given characterizing sequences.
	* @param dfsm
	* @param outCSet - CSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceCSet_EqualLength(DFSM* dfsm, sequence_set_t & outCSet);
	
	/**
	* Reduces the number of sequences in given State Characterizing set.
	* It goes from longer sequences to shorter ones
	* and removes unused ones (that don't distinguish any yet undistinguished pair of states).
	* @param dfsm
	* @param stateIdx - index of state in fsm->getStates()
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_LS(DFSM* dfsm, state_t stateIdx, sequence_set_t & outSCSet);
	
	/**
	* Reduces the number of sequences in given State Characterizing set.
	* At first, it goes from longer sequences to shorter one
	* and removes unused ones
	* (that don't distinguish any yet undistinguished pair of states).
	* Next it goes from shorter to longer sequences in set
	* and again removes unused or some distinguishable sequence could be shortened
	* so it truncated these sequences as much as possible.
	* @param dfsm
	* @param stateIdx - index of state in fsm->getStates()
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_LS_SL(DFSM* dfsm, state_t stateIdx, sequence_set_t & outSCSet);
	
	/**
	* Reduces the number of sequence in given State Characterizing set.
	* It chooses gradually a sequence of equally long sequences that
	* distinguishes most state pairs with the last symbol.
	* Sequences that do not distinguish any yet undistinguished pair of states are removed.
	*
	* SCSet is expected to be sorted by length of sequences, and
	* no sequence should begin with STOUT_INPUT besides the first one
	* that contains only STOUT_INPUT and fsm->isOutputState() holds.
	* 
	* For each state pair, SCSet needs to contain a sequence that particular state pair in the last symbol
	* because this function does not truncate given characterizing sequences.
	* @param dfsm
	* @param stateIdx - index of state in fsm->getStates()
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_EqualLength(DFSM* dfsm, state_t stateIdx, sequence_set_t & outSCSet);
	
	
}
