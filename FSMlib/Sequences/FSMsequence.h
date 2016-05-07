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

struct seqcomp {
	/**
	* Compares two input sequences firstly by their length,
	* shorter fisrt, then lexigraphically by their content
	*/
	bool operator() (const sequence_in_t& ls, const sequence_in_t& rs) const {
		if (ls.empty() || rs.empty()) return ls.size() < rs.size();
		if (ls.front() == STOUT_INPUT)
			if (rs.front() == STOUT_INPUT)
				if (ls.size() != rs.size()) return ls.size() < rs.size();
				else return ls < rs;
			else if (ls.size() - 1 != rs.size()) return (ls.size() - 1) < rs.size();
			else return true;// TODO: like ls.pop_front() and return ls < rs;
		else if (rs.front() == STOUT_INPUT)
			if (ls.size() != rs.size() - 1) return ls.size() < (rs.size() - 1);
			else return false;// TODO: like rs.pop_front() and return ls < rs;
		else if (ls.size() != rs.size()) return ls.size() < rs.size();
		else return ls < rs;
	}
	/* the first version
	bool operator() (const sequence_in_t& ls, const sequence_in_t& rs) const {
	if (ls.size() != rs.size()) return ls.size() < rs.size();
	if (ls.front() == STOUT_INPUT)
	if (rs.front() == STOUT_INPUT)
	return ls < rs;
	else return true;
	else if (rs.front() == STOUT_INPUT)
	return false;
	else return ls < rs;
	}
	*/
};

typedef vector<sequence_in_t> sequence_vec_t;
typedef set<sequence_in_t, seqcomp> sequence_set_t;

struct AdaptiveDS {
	sequence_in_t input;
	vector<state_t> initialStates, currentStates;
	map<output_t, unique_ptr<AdaptiveDS>> decision;
};

struct LinkCell {
	seq_len_t minLen = 0;
	vector<state_t> next;
};

#define PDS_FOUND       1
#define ADS_FOUND       2
#define SVS_FOUND       3
#define CSet_FOUND      4

#define RETURN_IF_NONCOMPACT(fsm, functionName, retVal) if (!fsm->isCompact()) {\
	ERROR_MESSAGE("%s - FSM needs to be compact.", functionName)\
	return retVal;}

namespace FSMsequence {// all design functions require a compact FSM
	/**
	* Fills given set with input sequences that cover states.
	* State 0 is start state and FSM goes to unique state using each sequence.
	* Sequences are sorted by lenght and then lexicographically from the shortest.
	* @param dfsm - Deterministic FSM
	* @param omitStoutInputs - STOUT_INPUT follows each transition input in all sequences if fsm->isOutputState() and false is set
	* @return state cover, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_set_t getStateCover(const unique_ptr<DFSM>& dfsm, bool omitStoutInputs = false);

	/**
	* Fills given set with input sequences that cover all transitions.
	* Transition cover is extended state cover so that each sequence
	* of state cover is extended by each input symbol.
	* State 0 is start state for all sequences.
	* @param dfsm - Deterministic FSM
	* @param omitStoutInputs - STOUT_INPUT follows each transition input in all sequences if fsm->isOutputState() and false is set
	* @return transition cover, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_set_t getTransitionCover(const unique_ptr<DFSM>& dfsm, bool omitStoutInputs = false);

	/**
	* Fills given set with all input sequences of the lenght up to depth.
	* @param dfsm - Deterministic FSM
	* @param depth - the length of the longest returned sequence
	* @param omitStoutInputs - STOUT_INPUT follows each transition input in all sequences if fsm->isOutputState() and false is set
	* @return traversal set, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_set_t getTraversalSet(const unique_ptr<DFSM>& dfsm, seq_len_t depth, bool omitStoutInputs = false);

	/**
	* Finds a shortest preset distinguishing sequence if FSM has it.<br><br>
	* Applying PDS distinguishes each state from the others.
	* 
	* Source:
	* InProceedings (deshmukh1994algorithm) 
	* Deshmukh, R. & Hawat, G. 
	* An algorithm to determine shortest length distinguishing, homing, and synchronizing sequences for sequential machines 
	* Southcon/94. Conference Record, 1994, 496-501 
	*
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in PDS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return a shortest preset distinguishing sequence, or empty sequence if there is no PDS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t getPresetDistinguishingSequence(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds an adaptive distinguishing sequence if FSM has it.<br><br>
	* Applying ADS distinguishes each state from the others.
	*
	* Found ADS does not have to be the shortest one but the search runs
	* in polynomial time of the number of states.
	*
	* Source:
	* Article (lee1994testing) 
	* Lee, D. & Yannakakis, M. 
	* Testing finite-state machines: State identification and verification 
	* Computers, IEEE Transactions on, IEEE, 1994, 43, 306-320
	*
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in ADS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return an Adaptive Distinguishing Sequence, or nullptr if there is no ADS or the FSM is not compact
	*/
	FSMLIB_API unique_ptr<AdaptiveDS> getAdaptiveDistinguishingSequence(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Creates a distinguishing sequence that is included in given
	* Adaptive Distinguishing Sequence for each state.
	* @param dfsm - Deterministic FSM
	* @param ads - Adaptive Distinguishing Sequence
	* @return a collection of distinguishing sequences for each state index, or empty collection if there is no ADS or the FSM is not compact
	*/
	FSMLIB_API sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, const unique_ptr<AdaptiveDS>& ads);

	/**
	* Finds an adaptive distinguishing sequence for each state (using getAdaptiveDistinguishingSequence).
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in ADS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return a collection of distinguishing sequences for each state index, or empty collection if there is no ADS or the FSM is not compact 
	*/
	FSMLIB_API sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds a shortest state verifying sequence for given state.<br><br>
	* Applying SVS distinguishes given state from the others.
	* @param dfsm - Deterministic FSM
	* @param state
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in SVS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return a shortest state verifying sequence, or an empty sequence if there is no SVS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t getStateVerifyingSequence(const unique_ptr<DFSM>& dfsm, state_t state, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds state verifying sequences for all states of FSM.<br>
	* If some state has not SVS,
	* input sequence in output vector will be empty.<br>
	* Sequence VSet[i] of resulting collection belongs to state dfsm->getStates()[i] (for all i < dfsm->getNumberOfStates()).<br>
	* Applying SVS distinguishes related state from the others.
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in each SVS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return Verifying Set of SVSs, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_vec_t getVerifyingSet(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds the shortest possible sequences that distinguish related pairs of states.
	* See getStatePairIdx() to obtain state pair index from indexes of states.
	*
	* @param dfsm - Deterministic FSM
	* @return a collection of shortest separating sequences for each state pair, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_vec_t getStatePairsShortestSeparatingSequences(const unique_ptr<DFSM>& dfsm);

#ifdef PARALLEL_COMPUTING
	/**
	* Finds the shortest possible sequences that distinguish related pairs of states.
	* See getStatePairIdx() to obtain state pair index from indexes of states.
	*
	* A thread distinguishes one state pair by a different output on a common input in the first run
	* or in a consecutive run if its next states' pair is distinguished.
	*
	* @param dfsm - Deterministic FSM
	* @return a collection of shortest separating sequences for each state pair, or empty collection if there is an error
	*/
	FSMLIB_API sequence_vec_t getStatePairsShortestSeparatingSequences_ParallelSF(const unique_ptr<DFSM>& dfsm);

	/**
	* Finds the shortest possible sequences that distinguish related pairs of states.
	* See getStatePairIdx() to obtain state pair index from indexes of states.
	*
	* At first, a thread distinguishes one state pair if it produces different outputs on an input.
	* Otherwise, a link from next states' pair is created.
	* Links are stored in memory in the third run after prescan counts required space.
	* Finally, a queue of distinguished pairs distinguishes previous state pairs, again a thread per state pair.
	*
	* @param dfsm - Deterministic FSM
	* @return a collection of shortest separating sequences for each state pair, or empty collection if there is an error
	*/
	FSMLIB_API sequence_vec_t getStatePairsShortestSeparatingSequences_ParallelQueue(const unique_ptr<DFSM>& dfsm);
#endif

	/**
	* Finds all possible distinguishing inputs for each state pair.
	* A distinguishing input points to successive state pair.
	* See getStatePairIdx() to obtain state pair index from indexes of states.
	* @param dfsm - Deterministic FSM
	* @return characterizing table, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API vector<LinkCell> getSeparatingSequences(const unique_ptr<DFSM>& dfsm);

	/**
	* Finds state characterizing set for given state.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes state from the others.
	* @param dfsm - Deterministic FSM
	* @param state
	* @param getSeparatingSequences - a pointer to function that fills provided vector with a separating
	*			sequence for each state pair, see default function getStatePairsShortestSeparatingSequences
	* @param filterPrefixes - no sequence of resulted SCSet is a prefix of another one if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted SCSet additionally,
	*			- reduceSCSet_LS_SL or reduceSCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	* @return State Characterizing Set of given state, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_set_t getStateCharacterizingSet(const unique_ptr<DFSM>& dfsm, state_t state,
		sequence_vec_t(*getSeparatingSequences)(const unique_ptr<DFSM>& dfsm) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true, void(*reduceFunc)(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet) = nullptr);

	/**
	* Finds state characterizing sets for all states of FSM.<br>
	* Output vector will have length of the number of states.<br>
	* Sequence set outSCSets[i] belongs to state i (for all i < dfsm->getNumberOfStates()).
	* @param dfsm - Deterministic FSM
	* @param getSeparatingSequences - a pointer to function that fills provided vector with a separating
	*			sequence for each state pair, see default function getStatePairsShortestSeparatingSequences
	* @param filterPrefixes - no sequence of any SCSet is a prefix of another one of the same SCSet if true
	* @param reduceFunc - a pointer to function that can reduce the size of each resulted SCSet additionally,
	*			- reduceSCSet_LS_SL or reduceSCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	* @return a collection of State Charactirizing Sets of all states, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API vector<sequence_set_t> getStatesCharacterizingSets(const unique_ptr<DFSM>& dfsm,
		sequence_vec_t(*getSeparatingSequences)(const unique_ptr<DFSM>& dfsm) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true,	void(*reduceFunc)(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet) = nullptr);

	/**
	* Finds state characterizing sets for all states of FSM<br>
	* such that each pair of sets contains <br>
	* same separating sequence of related states.<br>
	* Result is thus a family of harmonized state identifiers.
	* Output vector will have length of the number of states.<br>
	* Sequence set outSCSets[i] belongs to state i (for all i < dfsm->getNumberOfStates()).
	* @param dfsm - Deterministic FSM
	* @param getSeparatingSequences - a pointer to function that fills provided vector with a separating
	*			sequence for each state pair, see default function getStatePairsShortestSeparatingSequences
	* @param filterPrefixes - no sequence of any SCSet is a prefix of another one of the same SCSet if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted CSet additionally
	* @return a family of Harmonized State Identifiers, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API vector<sequence_set_t> getHarmonizedStateIdentifiers(const unique_ptr<DFSM>& dfsm,
		sequence_vec_t(*getSeparatingSequences)(const unique_ptr<DFSM>& dfsm) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true, void(*reduceFunc)(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet) = nullptr);

	/**
	* Finds characterizing set for FSM.<br>
	* Such a set always exists!<br><br>
	* Applying all of sequences distinguishes each state from the others.
	* @param dfsm - Deterministic FSM
	* @param getSeparatingSequences - a pointer to function that fills provided vector with a separating
	*			sequence for each state pair, see default function getStatePairsShortestSeparatingSequences
	* @param filterPrefixes - no sequence of resulted CSet is a prefix of another one if true
	* @param reduceFunc - a pointer to function that can reduce the size of resulted CSet additionally,
	*			- reduceCSet_LS_SL or reduceCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	* @return Characterizing Set, or an empty collection if the FSM is not compact
	*/
	FSMLIB_API sequence_set_t getCharacterizingSet(const unique_ptr<DFSM>& dfsm,
		sequence_vec_t(*getSeparatingSequences)(const unique_ptr<DFSM>& dfsm) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true,	void(*reduceFunc)(const unique_ptr<DFSM>& dfsm, sequence_set_t & outCSet) = nullptr);

	/**
	* Finds synchronizing sequence of FSM if exists.<br><br>
	* SS brings FSM to one unique state regardless on output.
	* 
	* Source:
	* InProceedings (deshmukh1994algorithm) 
	* Deshmukh, R. & Hawat, G. 
	* An algorithm to determine shortest length distinguishing, homing, and synchronizing sequences for sequential machines 
	* Southcon/94. Conference Record, 1994, 496-501 
	*
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in SS
	*		if fsm->isOutputState() and false is set, otherwise no STOUT_INPUT occurs in SS
	* @return a synchronizing sequence, or an empty sequence if there is no SS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t getSynchronizingSequence(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds homing sequence of FSM if exists.<br><br>
	* HS brings FSM to some specific states
	* which can be uniquely determined by output.
	* 
	* Source:
	* InProceedings (deshmukh1994algorithm) 
	* Deshmukh, R. & Hawat, G. 
	* An algorithm to determine shortest length distinguishing, homing, and synchronizing sequences for sequential machines 
	* Southcon/94. Conference Record, 1994, 496-501 
	*
	* @param dfsm - Deterministic FSM
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in PHS
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return a homing sequence, or an empty sequence if there is no HS or the FSM is not compact
	*/
	FSMLIB_API sequence_in_t getPresetHomingSequence(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds all distinguishing types of sequences which FSM has.<br><br>
	* PDS and ADS don't be the shortest.<br><br>
	*
	* Source:
	* Bachelor Thesis (soucha2014finite) 
	* Soucha, M. 
	* Finite-State Machine State Identification Sequences
	* Czech Technical Univerzity in Prague, 2014
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
	* @param getSeparatingSequences - a pointer to function that fills provided vector with a separating
	*			sequence for each state pair, see default function getStatePairsShortestSeparatingSequences
	* @param filterPrefixes - no sequence of resulted SCSet is a prefix of another one if true, same for CSet
	* @param reduceSCSetFunc - a pointer to function that can reduce the size of each resulted SCSet additionally,
	*			- reduceSCSet_LS_SL or reduceSCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	* @param reduceCSetFunc - a pointer to function that can reduce the size of resulted CSet additionally,
	*			- reduceCSet_LS_SL or reduceCSet_EqualLength are examples
	*			- NOTE that some require not to filter prefixes
	* @param omitUnnecessaryStoutInputs - STOUT_INPUT follows each transition input (except the last one) in all sequences
	*		if fsm->isOutputState() and false is set, otherwise only necessary STOUT_INPUTs are preserved
	* @return type of the strongest found sequence, i.e. PDS_FOUND, ADS_FOUND, SVS_FOUND or CSet_FOUND,
	*			or -1 if the FSM is not compact
	*/
	FSMLIB_API int getDistinguishingSequences(const unique_ptr<DFSM>& dfsm, sequence_in_t& outPDS, unique_ptr<AdaptiveDS>& outADS,
		sequence_vec_t& outVSet, vector<sequence_set_t>& outSCSets, sequence_set_t& outCSet,
		sequence_vec_t(*getSeparatingSequences)(const unique_ptr<DFSM>& dfsm) = getStatePairsShortestSeparatingSequences,
		bool filterPrefixes = true, void(*reduceSCSetFunc)(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet) = nullptr,
		void(*reduceCSetFunc)(const unique_ptr<DFSM>& dfsm, sequence_set_t & outCSet) = nullptr, bool omitUnnecessaryStoutInputs = false);

	/**
	* Finds index of given state in given sorted collection of state Ids in logarithmic time (in the number of states).
	* @param states = fsm->getStates()
	* @param stateId that is in states
	* @return index of stateId in states, or NULL_STATE if missing
	*/
	FSMLIB_API state_t getIdx(const vector<state_t>& states, state_t stateId);

	/**
	* Calculates index of given pair of states as follows:
	* Index of state pair (i,j) is derived from:
	*   (0,1) => 0, (0,2) => 1,..., (0,N-1) => N-2
	*   (1,2) => N-1,...
	*   i, j are indexes to the vector of states dfsm->getStates()
	*   N is number of states and for each (i,j): i<j
	*   (i,j) => i * N + j - 1 - (i * (i + 3)) / 2
	* @param s1 - the first state
	* @param s2 - the second state
	* @param N - the number of states
	* @return index of state pair
	*/
	FSMLIB_API state_t getStatePairIdx(const state_t& s1, const state_t& s2, const state_t& N);

	/**
	* Reduces the number of sequences in given Characterizing set.
	* At first, it goes from longer sequences to shorter ones
	* and removes unused ones (that don't distinguish any yet undistinguished pair of states).
	* Next it goes from shorter to longer sequences in the set
	* and again removes unused ones, or some distinguishable sequences could be shortened
	* so it truncates these sequences as much as possible.
	*
	* An error message is produced if the FSM is not compact
	* @param dfsm
	* @param outCSet
	*/
	FSMLIB_API void reduceCSet_LS_SL(const unique_ptr<DFSM>& dfsm, sequence_set_t & outCSet);
	
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
	*
	* An error message is produced if the FSM is not compact
	* @param dfsm
	* @param outCSet - CSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceCSet_EqualLength(const unique_ptr<DFSM>& dfsm, sequence_set_t & outCSet);
	
	/**
	* Reduces the number of sequences in given State Characterizing set.
	* It goes from longer sequences to shorter ones
	* and removes unused ones (that don't distinguish any yet undistinguished pair of states).
	*
	* An error message is produced if the FSM is not compact
	* @param dfsm
	* @param state
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_LS(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet);
	
	/**
	* Reduces the number of sequences in given State Characterizing set.
	* At first, it goes from longer sequences to shorter one
	* and removes unused ones
	* (that don't distinguish any yet undistinguished pair of states).
	* Next it goes from shorter to longer sequences in set
	* and again removes unused or some distinguishable sequence could be shortened
	* so it truncated these sequences as much as possible.
	*
	* An error message is produced if the FSM is not compact
	* @param dfsm
	* @param state
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_LS_SL(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet);
	
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
	*
	* An error message is produced if the FSM is not compact
	* @param dfsm
	* @param state
	* @param outSCSet - SCSet of given state to reduce (output set as well)
	*/
	FSMLIB_API void reduceSCSet_EqualLength(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t & outSCSet);	
}
