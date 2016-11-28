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
#include "../PrefixSet.h"

namespace FSMsequence {
	struct seq_info_t {
		sequence_set_t::iterator seqIt;
		vector<state_t> dist, lastDist;

		bool operator()(const seq_info_t& ls, const seq_info_t& rs) const {
			if (ls.lastDist.size() != rs.lastDist.size())
				return ls.lastDist.size() > rs.lastDist.size();
			if (ls.dist.size() != rs.dist.size())
				return ls.dist.size() > rs.dist.size();
			return *(ls.seqIt) > *(rs.seqIt);
		}
	};

	state_t getStatePairIdx(const state_t& s1, const state_t& s2, const state_t& N) {
		return (s1 < s2) ?
			(s1 * N + s2 - 1 - (s1 * (s1 + 3)) / 2) :
			(s2 * N + s1 - 1 - (s2 * (s2 + 3)) / 2);
	}
	
	state_t getStatePairIdx(const state_t& s1, const state_t& s2) {
		return (s1 < s2) ?
			((s2 * (s2 - 1)) / 2 + s1) :
			((s1 * (s1 - 1)) / 2 + s2);
	}

	state_t getIdx(const vector<state_t>& states, state_t stateId) {
		if ((stateId < states.size()) && (states[stateId] == stateId)) return stateId;
		auto lower = std::lower_bound(states.begin(), states.end(), stateId);
		if (lower != states.end() && *lower == stateId) {
			return state_t(std::distance(states.begin(), lower));
		}
		return NULL_STATE;
	}

	sequence_vec_t getStatePairsShortestSeparatingSequences(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStatePairsShortestSeparatingSequences", sequence_vec_t());
		state_t M, N = fsm->getNumberOfStates();
		M = ((N - 1) * N) / 2;
		sequence_vec_t seq(M);
		vector< vector< pair<state_t, input_t> > > link(M);
		queue<state_t> unchecked;
		for (state_t j = 1; j < N; j++) {
			for (state_t i = 0; i < j; i++) {
				auto idx = getStatePairIdx(i, j);
				if ((fsm->isOutputState()) && (fsm->getOutput(i, STOUT_INPUT) != fsm->getOutput(j, STOUT_INPUT))) {
					seq[idx].push_back(STOUT_INPUT);
					unchecked.emplace(idx);
					continue;
				}
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					output_t outputI = (fsm->getNextState(i, input) == NULL_STATE) ? WRONG_OUTPUT : fsm->getOutput(i, input);
					output_t outputJ = (fsm->getNextState(j, input) == NULL_STATE) ? WRONG_OUTPUT : fsm->getOutput(j, input);
					if (outputI != outputJ) {// TODO what about DEFAULT_OUTPUT
						seq[idx].push_back(input);
						unchecked.emplace(idx);
						break;
					}
				}
				if (seq[idx].empty()) {// not distinguished by one symbol?
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						auto nextStateI = fsm->getNextState(i, input);
						auto nextStateJ = fsm->getNextState(j, input);
						// there are no transition -> same next state = NULL_STATE
						// only one next state cannot be NULL_STATE due to distinguishing be outputs (WRONG_OUTPUT)
						if (nextStateI != nextStateJ) {
							auto nextIdx = getStatePairIdx(nextStateI, nextStateJ);
							if (seq[nextIdx].empty()) {
								if (nextIdx != idx)
									link[nextIdx].emplace_back(idx, input);
							}
							else {// distinguished by word of length 2
								link[nextIdx].emplace_back(idx, input);
								unchecked.emplace(nextIdx);
								break;
							}
						}
					}
				}
			}
		}
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		// fill all undistinguished pair gradually using links
		while (!unchecked.empty()) {
			auto idx = unchecked.front();
			unchecked.pop();
			for (state_t k = 0; k < link[idx].size(); k++) {
				auto nextIdx = link[idx][k].first;
				if (seq[nextIdx].empty()) {
					seq[nextIdx].push_back(link[idx][k].second);
					if (useStout && seq[idx].front() != STOUT_INPUT) seq[nextIdx].push_back(STOUT_INPUT);
					seq[nextIdx].insert(seq[nextIdx].end(), seq[idx].begin(), seq[idx].end());
					unchecked.emplace(nextIdx);
				}
			}
			link[idx].clear();
		}
		return seq;
	}

	vector<LinkCell> getSeparatingSequences(const unique_ptr<DFSM>& fsm) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getSeparatingSequences", vector<LinkCell>());
		state_t M, N = fsm->getNumberOfStates();
		queue<state_t> unchecked;
		M = ((N - 1) * N) / 2;
		vector<vector<pair<state_t, input_t>>> link(M);

		// init seq
		vector<LinkCell> seq(M);
		for (state_t i = 0; i < M; i++) {
			seq[i].next.resize(fsm->getNumberOfInputs(), NULL_STATE);
		}
		for (state_t j = 1; j < N; j++) {
			for (state_t i = 0; i < j; i++) {
				auto idx = getStatePairIdx(i, j);
				if ((fsm->isOutputState()) && (fsm->getOutput(i, STOUT_INPUT) != fsm->getOutput(j, STOUT_INPUT))) {
					seq[idx].minLen = 1;// to correspond that STOUT_INPUT needs to be applied
					unchecked.emplace(idx);
				}
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					output_t outputI = (fsm->getNextState(i, input) == NULL_STATE) ? WRONG_OUTPUT : fsm->getOutput(i, input);
					output_t outputJ = (fsm->getNextState(j, input) == NULL_STATE) ? WRONG_OUTPUT : fsm->getOutput(j, input);
					if (outputI != outputJ) {// TODO what about DEFAULT_OUTPUT
						seq[idx].next[input] = idx;
						if (seq[idx].minLen == 0) {
							seq[idx].minLen = 1;
							unchecked.emplace(idx);
						}
					}
					else {
						auto nextStateI = fsm->getNextState(i, input);
						auto nextStateJ = fsm->getNextState(j, input);
						// there are no transitions -> same next state = NULL_STATE
						// only one next state cannot be NULL_STATE due to distinguishing be outputs (WRONG_OUTPUT)
						if (nextStateI != nextStateJ) {
							auto nextIdx = getStatePairIdx(nextStateI, nextStateJ);
							if (nextIdx != idx) {
								seq[idx].next[input] = nextIdx;
								link[nextIdx].emplace_back(idx, input);
							}// TODO what about swap 
						}
					}
				}
			}
		}
		// fill all undistinguished pair gradually using links
		while (!unchecked.empty()) {
			auto nextIdx = unchecked.front();
			unchecked.pop();
			for (state_t k = 0; k < link[nextIdx].size(); k++) {
				auto idx = link[nextIdx][k].first;
				if (seq[idx].minLen == 0) {
					seq[idx].minLen = seq[nextIdx].minLen + 1;
					unchecked.emplace(idx);
				}
			}
			link[nextIdx].clear();
		}
		return seq;
	}

	/**
	* Compares two output sequences and returns the lenght of greatest prefix.
	* @param a
	* @param b
	* @return length of equal prefixes
	*/
	static seq_len_t compSeq(sequence_out_t& a, sequence_out_t& b) {
		sequence_out_t::iterator aIt = a.begin(), bIt = b.begin();
		seq_len_t c = 0, size = seq_len_t((a.size() < b.size()) ? a.size() : b.size());
		while (c < size && *aIt == *bIt) {
			aIt++;
			bIt++;
			c++;
		}
		return c;
	}

	static bool distinguishBySequence(const unique_ptr<DFSM>& fsm, const sequence_in_t& seq,
			vector<state_t>& dist, vector<bool>& distinguished) {
		state_t N = fsm->getNumberOfStates();
		sequence_out_t outI, outIWithoutLast, outJ;
		bool hasMinLen = false;
		dist.clear();
		// goes throught all pairs of states (i,j)
		for (state_t i = 0; i < N - 1; i++) {
			outIWithoutLast = outI = fsm->getOutputAlongPath(i, seq);
			outIWithoutLast.pop_back();
			for (state_t j = i + 1; j < N; j++) {
				outJ = fsm->getOutputAlongPath(j, seq);
				if (outI != outJ) {
					auto idx = getStatePairIdx(i, j);
					if (!distinguished[idx]) {
						dist.push_back(idx);
						outJ.pop_back();
						// could it be shorter?
						if (outIWithoutLast == outJ) {
							hasMinLen = true;
						}
					}
				}
			}
		}
		if (hasMinLen) {
			for (state_t i = 0; i < dist.size(); i++) {
				distinguished[dist[i]] = true;
			}
		}
		return hasMinLen;
	}

	static void truncateSeq(const unique_ptr<DFSM>& fsm, sequence_in_t& shortSeq, const vector<state_t>& dist, 
			vector<bool>& distinguished, bool setDistinguished, state_t state = NULL_STATE) {
		state_t N = fsm->getNumberOfStates();
		sequence_out_t outI, outJ;
		seq_len_t maxCount = 0, count; // count of needed symbol
		shortSeq.pop_back();
		if (state != NULL_STATE) {
			outI = fsm->getOutputAlongPath(state, shortSeq);
		}
		for (state_t k = 0; k < dist.size(); k++) {
			distinguished[dist[k]] = setDistinguished;
			if (state != NULL_STATE) {// SCSet reduction
				outJ = fsm->getOutputAlongPath(dist[k], shortSeq);
			}
			else {// CSet reduction
				state_t i, j;
				for (i = 0; i < N - 1; i++) {// searching for indexes i, j
					j = dist[k] + 1 - i * N + (i * (i + 3)) / 2;
					if (i < j && j < N) {
						break;
					}
				}
				outI = fsm->getOutputAlongPath(i, shortSeq);
				outJ = fsm->getOutputAlongPath(j, shortSeq);
			}
			count = compSeq(outI, outJ);
			if (count > maxCount) {
				maxCount = count;
			}
		}
		// truncate the sequence
		for (++maxCount; maxCount < outI.size(); maxCount++) {
			shortSeq.pop_back();
		}
	}

	void reduceCSet_LS_SL(const unique_ptr<DFSM>& fsm, sequence_set_t & outCSet) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::reduceCSet_LS_SL",);
		state_t N = fsm->getNumberOfStates();
		vector<bool> distinguished(((N - 1) * N) / 2, false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished pair of states by current sequence
		for (sequence_set_t::reverse_iterator sIt = outCSet.rbegin(); sIt != outCSet.rend(); sIt++) {
			if (!distinguishBySequence(fsm, *sIt, dist, distinguished)) {
				if (dist.empty()) {
					outCSet.erase(--sIt.base());
					sIt--;
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outCSet.erase(--sIt.base());
					sIt--;
					truncateSeq(fsm, shortSeq, dist, distinguished, false);
					outCSet.emplace(move(shortSeq));
				}
			}
		}
		distinguished.assign(((N - 1) * N) / 2, false);
		for (sequence_set_t::iterator sIt = outCSet.begin(); sIt != outCSet.end(); sIt++) {
			if (!distinguishBySequence(fsm, *sIt, dist, distinguished)) {
				if (dist.empty()) {
					outCSet.erase(sIt--);
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outCSet.erase(sIt--);
					shortSeq.pop_back();
					truncateSeq(fsm, shortSeq, dist, distinguished, true);
					outCSet.emplace(move(shortSeq));
				}
			}
		}
	}

	static void reduceSequencesOfSameLength(set<seq_info_t, seq_info_t>& infos, vector<bool>& distinguished, sequence_set_t & outSet) {
		while (!infos.empty()) {
			auto it = infos.begin();// sequence that distinguishes the most pairs, see seq_info_t comparison operator()
			for (state_t i = 0; i < it->dist.size(); i++) {
				distinguished[it->dist[i]] = true;
			}
			set<seq_info_t, seq_info_t> tmp;
			for (++it; it != infos.end(); it++) {
				seq_info_t seqInfo;
				for (const auto& v : it->lastDist) {
					if (!distinguished[v]) {
						seqInfo.lastDist.push_back(v);
					}
				}
				if (seqInfo.lastDist.empty()) {
					outSet.erase(it->seqIt);
				}
				else {
					for (const auto& v : it->dist) {
						if (!distinguished[v]) {
							seqInfo.dist.push_back(v);
						}
					}
					seqInfo.seqIt = it->seqIt;
					tmp.emplace(move(seqInfo));
				}
			}
			infos.swap(tmp);
		}
	}

	void reduceCSet_EqualLength(const unique_ptr<DFSM>& fsm, sequence_set_t & outCSet) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::reduceCSet_EqualLength", );
		state_t N = fsm->getNumberOfStates();
		vector<bool> distinguished(((N - 1) * N) / 2, false); // is already a pair of states distinguished?
		set<seq_info_t, seq_info_t> infos;
		sequence_out_t outI, outIWithoutLast, outJ;
		auto seqIdx = outCSet.size();
		if (fsm->isOutputState() && (outCSet.begin()->size() == 1) && (outCSet.begin()->front() == STOUT_INPUT)) {
			// STOUT_INPUT is the first element in outCSet
			for (state_t i = 0; i < N - 1; i++) {
				output_t output = fsm->getOutput(i, STOUT_INPUT);
				for (state_t j = i + 1; j < N; j++) {
					if (output != fsm->getOutput(j, STOUT_INPUT)) {
						auto idx = getStatePairIdx(i, j);
						distinguished[idx] = true;
					}
				}
			}
			seqIdx--;// the first sequence won't be processed
		}
		auto sIt = outCSet.end();
		sIt--; // point to the last sequence, i.e. longest one
		auto len = sIt->size();
		//printf("start %d\n",state);
		for (; seqIdx > 0; seqIdx--) {// passes CSet from the longest
			seq_info_t seqInfo;
			// goes throught all pairs of states (i,j)
			for (state_t i = 0; i < N - 1; i++) {
				outIWithoutLast = outI = fsm->getOutputAlongPath(i, *sIt);
				outIWithoutLast.pop_back();
				for (state_t j = i + 1; j < N; j++) {
					outJ = fsm->getOutputAlongPath(j, *sIt);
					if (outI != outJ) {
						auto idx = getStatePairIdx(i, j);
						if (!distinguished[idx]) {
							seqInfo.dist.emplace_back(idx);
							outJ.pop_back();
							// could it be shorter?
							if (outIWithoutLast == outJ) {
								seqInfo.lastDist.emplace_back(idx);
							}
						}
					}
				}
			}
			if (seqInfo.lastDist.empty()) {
				outCSet.erase(sIt++);
			}
			else {
				seqInfo.seqIt = sIt;
				infos.emplace(move(seqInfo));
			}
			// is the first sequence of set reached? will be next sequence shorter?
			if ((sIt == outCSet.begin()) || ((--sIt)->size() < len)) {// || (sIt->front() == STOUT_INPUT)) {
				len = sIt->size();
				reduceSequencesOfSameLength(infos, distinguished, outCSet);
			}
		}
		// is STOUT_INPUT needed? -> put it in front of another sequence
		if (fsm->isOutputState() && (sIt->front() == STOUT_INPUT) && (outCSet.size() > 1)) {
			outCSet.erase(sIt++);
			bool stoutNeeded = false;
			for (state_t i = 0; i < N - 1; i++) {
				output_t output = fsm->getOutput(i, STOUT_INPUT);
				for (state_t j = i + 1; j < N; j++) {
					if (output == fsm->getOutput(j, STOUT_INPUT)) {
						stoutNeeded = true;
						for (auto seq : outCSet) {
							if (fsm->getOutputAlongPath(i, seq) != fsm->getOutputAlongPath(i, seq)) {
								stoutNeeded = false;
								break;
							}
						}
						if (stoutNeeded) break;
					}
				}
				if (stoutNeeded) break;
			}
			if (stoutNeeded) {
				auto seq = *(outCSet.begin());
				outCSet.erase(outCSet.begin());
				seq.push_front(STOUT_INPUT);
				outCSet.emplace(move(seq));
			}
		}
	}

	sequence_set_t getCharacterizingSet(const unique_ptr<DFSM>& fsm,
		function<sequence_vec_t(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs)> getSeparatingSequences,
		bool filterPrefixes, function<void(const unique_ptr<DFSM>& dfsm, sequence_set_t& outCSet)> reduceCSet,
		bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getCharacterizingSet", sequence_set_t());
		sequence_set_t outCSet;
		auto seq = getSeparatingSequences(fsm, omitUnnecessaryStoutInputs);
		if (filterPrefixes) {
			FSMlib::PrefixSet pset;
			for (state_t i = 0; i < seq.size(); i++) {
				if (fsm->isOutputState() && (seq[i].front() != STOUT_INPUT)) seq[i].push_front(STOUT_INPUT);
				pset.insert(seq[i]);
			}
			outCSet = pset.getMaximalSequences();
		}
		else {
			for (state_t i = 0; i < seq.size(); i++) {
				outCSet.emplace(seq[i]);
			}
		}
		if (reduceCSet)
			reduceCSet(fsm, outCSet);
		return outCSet;
	}

	static bool distinguishBySequenceFromState(const unique_ptr<DFSM>& fsm, const sequence_in_t& seq, state_t state,
			vector<state_t>& dist, vector<bool>& distinguished) {	
		state_t N = fsm->getNumberOfStates();
		bool hasMinLen = false;
		sequence_out_t outS, outSWithoutLast, outJ;
		dist.clear();
		outSWithoutLast = outS = fsm->getOutputAlongPath(state, seq);
		outSWithoutLast.pop_back();
		for (state_t j = 0; j < N; j++) {
			if (j != state) {
				outJ = fsm->getOutputAlongPath(j, seq);
				if (outS != outJ) {
					if (!distinguished[j]) {
						dist.emplace_back(j);
						outJ.pop_back();
						// could it be shorter?
						if (outSWithoutLast == outJ) {
							hasMinLen = true;
						}
					}
				}
			}
		}
		if (hasMinLen) {
			for (state_t i = 0; i < dist.size(); i++) {
				distinguished[dist[i]] = true;
			}
		}
		return hasMinLen;
	}

	void reduceSCSet_LS_SL(const unique_ptr<DFSM>& fsm, state_t state, sequence_set_t & outSCSet) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::reduceSCSet_LS_SL", );
		vector<bool> distinguished(fsm->getNumberOfStates(), false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished states by current sequence
		for (sequence_set_t::reverse_iterator sIt = outSCSet.rbegin(); sIt != outSCSet.rend(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, state, dist, distinguished)) {
				if (dist.empty()) {
					outSCSet.erase(--sIt.base());
					sIt--;
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outSCSet.erase(--sIt.base());
					sIt--;
					truncateSeq(fsm, shortSeq, dist, distinguished, false, state);
					outSCSet.emplace(move(shortSeq));
				}
			}
		}
		distinguished.assign(fsm->getNumberOfStates(), false);
		for (sequence_set_t::iterator sIt = outSCSet.begin(); sIt != outSCSet.end(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, state, dist, distinguished)) {
				if (dist.empty()) {
					outSCSet.erase(sIt--);
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outSCSet.erase(sIt--);
					truncateSeq(fsm, shortSeq, dist, distinguished, true, state);
					outSCSet.emplace(move(shortSeq));
				}
			}
		}
	}

	void reduceSCSet_LS(const unique_ptr<DFSM>& fsm, state_t state, sequence_set_t & outSCSet) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::reduceSCSet_LS", );
		vector<bool> distinguished(fsm->getNumberOfStates(), false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished states by current sequence
		for (sequence_set_t::reverse_iterator sIt = outSCSet.rbegin(); sIt != outSCSet.rend(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, state, dist, distinguished)) {
				outSCSet.erase(--sIt.base());
				sIt--;
			}
		}
	}

	void reduceSCSet_EqualLength(const unique_ptr<DFSM>& fsm, state_t state, sequence_set_t & outSCSet) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::reduceSCSet_EqualLength", );
		state_t N = fsm->getNumberOfStates();
		vector<bool> distinguished(N, false); // is already a pair of states distinguished?
		set<seq_info_t, seq_info_t> infos;
		sequence_out_t outS, outSWithoutLast, outJ;
		auto seqIdx = outSCSet.size();
		if (fsm->isOutputState() && (outSCSet.begin()->size() == 1) && (outSCSet.begin()->front() == STOUT_INPUT)) {
			// STOUT_INPUT is the first element in outSCSet
			output_t output = fsm->getOutput(state, STOUT_INPUT);
			for (state_t j = 0; j < N; j++) {
				if ((j != state) && (output != fsm->getOutput(j, STOUT_INPUT))) {
					distinguished[j] = true;
				}
			}
			seqIdx--;// the first sequence won't be processed
		}
		auto sIt = outSCSet.end();
		sIt--; // point to the last sequence, i.e. longest one
		auto len = sIt->size();
		//printf("start %d\n",state);
		for (; seqIdx > 0; seqIdx--) {
			seq_info_t seqInfo;
			outSWithoutLast = outS = fsm->getOutputAlongPath(state, *sIt);
			outSWithoutLast.pop_back();
			for (state_t j = 0; j < N; j++) {
				if (j != state) {
					outJ = fsm->getOutputAlongPath(j, *sIt);
					if (outS != outJ) {
						if (!distinguished[j]) {
							seqInfo.dist.push_back(j);
							outJ.pop_back();
							// could it be shorter?
							if (outSWithoutLast == outJ) {
								seqInfo.lastDist.push_back(j);
							}
						}
					}
				}
			}
			if (seqInfo.lastDist.empty()) {
				outSCSet.erase(sIt++);
			}
			else {
				seqInfo.seqIt = sIt;
				infos.emplace(move(seqInfo));
			}
			// is the first sequence of set reached? will be next sequence shorter?
			if ((sIt == outSCSet.begin()) || ((--sIt)->size() < len)) {// || (sIt->front() == STOUT_INPUT)) {
				len = sIt->size();
				reduceSequencesOfSameLength(infos, distinguished, outSCSet);
			}
		}
		// is STOUT_INPUT needed? -> put it in front of another sequence
		if (fsm->isOutputState() && (sIt->front() == STOUT_INPUT) && (outSCSet.size() > 1)) {
			outSCSet.erase(sIt++);
			bool stoutNeeded = false;
			output_t output = fsm->getOutput(state, STOUT_INPUT);
			for (state_t j = 0; j < N; j++) {
				if ((j != state) && (output != fsm->getOutput(j, STOUT_INPUT))) {
					stoutNeeded = true;
					for (const auto& seq : outSCSet) {
						if (fsm->getOutputAlongPath(state, seq) != fsm->getOutputAlongPath(j, seq)) {
							stoutNeeded = false;
							break;
						}
					}
					if (stoutNeeded) break;
				}
			}
			if (stoutNeeded) {
				auto seq = *(outSCSet.begin());
				outSCSet.erase(outSCSet.begin());
				seq.push_front(STOUT_INPUT);
				outSCSet.emplace(move(seq));
			}
		}
	}

	sequence_set_t getSCSet(const sequence_vec_t& distSeqs, state_t state, state_t N, bool filterPrefixes = false, bool useStout = false) {
		sequence_set_t outSCSet;
		FSMlib::PrefixSet pset;
		// grab sequence from table seq incident with state
		for (state_t j = 0; j < N; j++) {
			if (j != state) {
				auto idx = getStatePairIdx(j, state);
				if (filterPrefixes) {
					if (useStout && distSeqs[idx].front() != STOUT_INPUT) {
						sequence_in_t s(distSeqs[idx]);
						s.push_front(STOUT_INPUT);
						pset.insert(move(s));
					} else 
						pset.insert(distSeqs[idx]);
				}
				else
					outSCSet.emplace(distSeqs[idx]);
			}
		}
		if (filterPrefixes)
			return pset.getMaximalSequences();
		return outSCSet;
	}

	sequence_set_t getStateCharacterizingSet(const unique_ptr<DFSM>& fsm, state_t state,
		function<sequence_vec_t(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs)> getSeparatingSequences,
		bool filterPrefixes, function<void(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t& outSCSet)> reduceSCSet,
		bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStateCharacterizingSet", sequence_set_t());
		auto seq = getSeparatingSequences(fsm, omitUnnecessaryStoutInputs);
		auto outSCSet = getSCSet(seq, state, fsm->getNumberOfStates(), filterPrefixes, !omitUnnecessaryStoutInputs && fsm->isOutputState());
		// try to reduce count of seqeunces
		if (reduceSCSet)
			reduceSCSet(fsm, state, outSCSet);
		return outSCSet;
	}

	vector<sequence_set_t> getStatesCharacterizingSets(const unique_ptr<DFSM>& fsm,
		function<sequence_vec_t(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs)> getSeparatingSequences,
		bool filterPrefixes, function<void(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t& outSCSet)> reduceSCSet,
		bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getStatesCharacterizingSets", vector<sequence_set_t>());
		state_t N = fsm->getNumberOfStates();
		auto seq = getSeparatingSequences(fsm, omitUnnecessaryStoutInputs);
		vector<sequence_set_t> outSCSets(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes, !omitUnnecessaryStoutInputs && fsm->isOutputState());
			// try to reduce count of seqeunces
			if (reduceSCSet)
				reduceSCSet(fsm, i, outSCSets[i]);
		}
		return outSCSets;
	}

	vector<sequence_set_t> getHarmonizedStateIdentifiers(const unique_ptr<DFSM>& fsm,
		function<sequence_vec_t(const unique_ptr<DFSM>& dfsm, bool omitUnnecessaryStoutInputs)> getSeparatingSequences,
		bool filterPrefixes, function<void(const unique_ptr<DFSM>& dfsm, state_t state, sequence_set_t& outSCSet)> reduceSCSet,
		bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getHarmonizedStateIdentifiers", vector<sequence_set_t>());
		state_t N = fsm->getNumberOfStates();
		auto seq = getSeparatingSequences(fsm, omitUnnecessaryStoutInputs);
		vector<sequence_set_t> outSCSets(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes, !omitUnnecessaryStoutInputs && fsm->isOutputState());
			if (reduceSCSet)
				reduceSCSet(fsm, i, outSCSets[i]);
		}
		return outSCSets;
	}
}
