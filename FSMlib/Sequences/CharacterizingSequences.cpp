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

	state_t getIdx(const vector<state_t>& states, state_t stateId) {
		if ((stateId < states.size()) && (states[stateId] == stateId)) return stateId;
		auto lower = std::lower_bound(states.begin(), states.end(), stateId);
		if (lower != states.end() && *lower == stateId) {
			return state_t(std::distance(states.begin(), lower));
		}
		return NULL_STATE;
	}

	sequence_vec_t getStatePairsShortestSeparatingSequences(DFSM * fsm) {
		sequence_vec_t seq;
		state_t M, N = fsm->getNumberOfStates();
		M = ((N - 1) * N) / 2;
		seq.resize(M);
		state_t nextStateI, nextStateJ, nextIdx, idx;
		vector< vector< pair<state_t, input_t> > > link(M);
		vector<state_t> states = fsm->getStates();
		queue<state_t> unchecked;
		for (state_t i = 0; i < N - 1; i++) {
			for (state_t j = i + 1; j < N; j++) {
				idx = i * N + j - 1 - (i * (i + 3)) / 2;
				if ((fsm->isOutputState()) && (fsm->getOutput(states[i], STOUT_INPUT) != fsm->getOutput(states[j], STOUT_INPUT))) {
					seq[idx].push_back(STOUT_INPUT);
					unchecked.push(idx);
					continue;
				}
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					if (fsm->getOutput(states[i], input) != fsm->getOutput(states[j], input)) {// TODO what about DEFAULT_OUTPUT
						seq[idx].push_back(input);
						unchecked.push(idx);
						break;
					}
				}
				if (seq[idx].empty()) {// not distinguished by one symbol?
					for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
						nextStateI = fsm->getNextState(states[i], input);
						nextStateJ = fsm->getNextState(states[j], input);
						// there are no transition -> same next state = NULL_STATE
						// only one next state cannot be NULL_STATE due to distinguishing be outputs (WRONG_OUTPUT)
						if (nextStateI != nextStateJ) {
							nextStateI = getIdx(states, nextStateI);
							nextStateJ = getIdx(states, nextStateJ);
							nextIdx = (nextStateI < nextStateJ) ?
								(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
								(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
							if (seq[nextIdx].empty()) {
								if (nextIdx != idx)
									link[nextIdx].emplace_back(make_pair(idx, input));
							}
							else {// distinguished by word of length 2
								link[nextIdx].emplace_back(make_pair(idx, input));
								unchecked.push(nextIdx);
								break;
							}
						}
					}
				}
			}
		}
		// fill all undistinguished pair gradually using links
		while (!unchecked.empty()) {
			idx = unchecked.front();
			unchecked.pop();
			for (state_t k = 0; k < link[idx].size(); k++) {
				nextIdx = link[idx][k].first;
				if (seq[nextIdx].empty()) {
					seq[nextIdx].push_back(link[idx][k].second);
					seq[nextIdx].insert(seq[nextIdx].end(), seq[idx].begin(), seq[idx].end());
					unchecked.push(nextIdx);
				}
			}
			link[idx].clear();
		}
		return seq;
	}

	vector<LinkCell> getSeparatingSequences(DFSM * fsm) {
		vector<LinkCell> seq;
		state_t M, N = fsm->getNumberOfStates();
		state_t nextStateI, nextStateJ, nextIdx, idx;
		queue<state_t> unchecked;
		vector<state_t> states = fsm->getStates();
		M = ((N - 1) * N) / 2;
		vector< vector< pair<state_t, input_t> > > link(M);

		// init seq
		seq.resize(M);
		for (state_t i = 0; i < M; i++) {
			seq[i].next.resize(fsm->getNumberOfInputs(), NULL_STATE);
		}
		for (state_t i = 0; i < N - 1; i++) {
			for (state_t j = i + 1; j < N; j++) {
				idx = i * N + j - 1 - (i * (i + 3)) / 2;
				if ((fsm->isOutputState()) && (fsm->getOutput(states[i], STOUT_INPUT) != fsm->getOutput(states[j], STOUT_INPUT))) {
					seq[idx].minLen = 1;// to correspond that STOUT_INPUT needs to be applied
					unchecked.push(idx);
				}
				for (input_t input = 0; input < fsm->getNumberOfInputs(); input++) {
					if (fsm->getOutput(states[i], input) != fsm->getOutput(states[j], input)) {// TODO what about DEFAULT_OUTPUT
						seq[idx].next[input] = idx;
						if (seq[idx].minLen == 0) {
							seq[idx].minLen = 1;
							unchecked.push(idx);
						}
					}
					else {
						nextStateI = fsm->getNextState(states[i], input);
						nextStateJ = fsm->getNextState(states[j], input);
						// there are no transitions -> same next state = NULL_STATE
						// only one next state cannot be NULL_STATE due to distinguishing be outputs (WRONG_OUTPUT)
						if (nextStateI != nextStateJ) {
							nextStateI = getIdx(states, nextStateI);
							nextStateJ = getIdx(states, nextStateJ);
							nextIdx = (nextStateI < nextStateJ) ?
								(nextStateI * N + nextStateJ - 1 - (nextStateI * (nextStateI + 3)) / 2) :
								(nextStateJ * N + nextStateI - 1 - (nextStateJ * (nextStateJ + 3)) / 2);
							if (nextIdx != idx) {
								seq[idx].next[input] = nextIdx;
								link[nextIdx].emplace_back(make_pair(idx, input));
							}// TODO what about swap 
						}
					}
				}
			}
		}
		// fill all undistinguished pair gradually using links
		while (!unchecked.empty()) {
			nextIdx = unchecked.front();
			unchecked.pop();
			for (state_t k = 0; k < link[nextIdx].size(); k++) {
				idx = link[nextIdx][k].first;
				if (seq[idx].minLen == 0) {
					seq[idx].minLen = seq[nextIdx].minLen + 1;
					unchecked.push(idx);
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

	static bool distinguishBySequence(DFSM * fsm, const sequence_in_t& seq,
		const vector<state_t>& states, vector<state_t>& dist, vector<bool>& distinguished) {
		
		state_t N = fsm->getNumberOfStates(), idx;
		sequence_out_t outI, outIWithoutLast, outJ;
		bool hasMinLen = false;
		dist.clear();
		// goes throught all pairs of states (i,j)
		for (state_t i = 0; i < N - 1; i++) {
			outIWithoutLast = outI = fsm->getOutputAlongPath(states[i], seq);
			outIWithoutLast.pop_back();
			for (state_t j = i + 1; j < N; j++) {
				outJ = fsm->getOutputAlongPath(states[j], seq);
				if (outI != outJ) {
					idx = i * N + j - 1 - (i * (i + 3)) / 2;
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

	static void truncateSeq(DFSM * fsm, sequence_in_t& shortSeq,
		const vector<state_t>& states, const vector<state_t>& dist, vector<bool>& distinguished,
		bool setDistinguished, state_t stateIdx = NULL_STATE) {
		
		state_t N = fsm->getNumberOfStates();
		sequence_out_t outI, outJ;
		int maxCount = 0, count; // count of needed symbol
		shortSeq.pop_back();
		if (stateIdx != NULL_STATE) {
			outI = fsm->getOutputAlongPath(states[stateIdx], shortSeq);
		}
		for (state_t k = 0; k < dist.size(); k++) {
			distinguished[dist[k]] = setDistinguished;
			if (stateIdx != NULL_STATE) {// SCSet reduction
				outJ = fsm->getOutputAlongPath(states[dist[k]], shortSeq);
			}
			else {// CSet reduction
				state_t i, j;
				for (i = 0; i < N - 1; i++) {// searching for indexes i, j
					j = dist[k] + 1 - i * N + (i * (i + 3)) / 2;
					if (i < j && j < N) {
						break;
					}
				}
				outI = fsm->getOutputAlongPath(states[i], shortSeq);
				outJ = fsm->getOutputAlongPath(states[j], shortSeq);
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

	void reduceCSet_LS_SL(DFSM * fsm, sequence_set_t & outCSet) {
		state_t N = fsm->getNumberOfStates();
		vector<bool> distinguished(((N - 1) * N) / 2, false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished pair of states by current sequence
		vector<state_t> states = fsm->getStates();
		for (sequence_set_t::reverse_iterator sIt = outCSet.rbegin(); sIt != outCSet.rend(); sIt++) {
			if (!distinguishBySequence(fsm, *sIt, states, dist, distinguished)) {
				if (dist.empty()) {
					outCSet.erase(--sIt.base());
					sIt--;
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outCSet.erase(--sIt.base());
					sIt--;
					truncateSeq(fsm, shortSeq, states, dist, distinguished, false);
					outCSet.emplace(shortSeq);
				}
			}
		}
		distinguished.assign(((N - 1) * N) / 2, false);
		for (sequence_set_t::iterator sIt = outCSet.begin(); sIt != outCSet.end(); sIt++) {
			if (!distinguishBySequence(fsm, *sIt, states, dist, distinguished)) {
				if (dist.empty()) {
					outCSet.erase(sIt--);
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outCSet.erase(sIt--);
					shortSeq.pop_back();
					truncateSeq(fsm, shortSeq, states, dist, distinguished, true);
					outCSet.emplace(shortSeq);
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
				for (auto v : it->lastDist) {
					if (!distinguished[v]) {
						seqInfo.lastDist.push_back(v);
					}
				}
				if (seqInfo.lastDist.empty()) {
					outSet.erase(it->seqIt);
				}
				else {
					for (auto v : it->dist) {
						if (!distinguished[v]) {
							seqInfo.dist.push_back(v);
						}
					}
					seqInfo.seqIt = it->seqIt;
					tmp.emplace(seqInfo);
				}
			}
			infos = tmp;
		}
	}

	void reduceCSet_EqualLength(DFSM* fsm, sequence_set_t & outCSet) {
		state_t N = fsm->getNumberOfStates(), idx;
		vector<bool> distinguished(((N - 1) * N) / 2, false); // is already a pair of states distinguished?
		seq_len_t len;
		set<seq_info_t, seq_info_t> infos;
		sequence_out_t outI, outIWithoutLast, outJ;
		sequence_set_t::iterator sIt;
		vector<state_t> states = fsm->getStates();
		size_t seqIdx = outCSet.size();
		if (fsm->isOutputState() && (outCSet.begin()->size() == 1) && (outCSet.begin()->front() == STOUT_INPUT)) {
			// STOUT_INPUT is the first element in outCSet
			for (state_t i = 0; i < N - 1; i++) {
				output_t output = fsm->getOutput(states[i], STOUT_INPUT);
				for (state_t j = i + 1; j < N; j++) {
					if (output != fsm->getOutput(states[j], STOUT_INPUT)) {
						idx = i * N + j - 1 - (i * (i + 3)) / 2;
						distinguished[idx] = true;
					}
				}
			}
			seqIdx--;// the first sequence won't be processed
		}
		sIt = outCSet.end();
		sIt--; // point to the last sequence, i.e. longest one
		len = seq_len_t(sIt->size());
		//printf("start %d\n",state);
		for (; seqIdx > 0; seqIdx--) {// passes CSet from the longest
			seq_info_t seqInfo;
			// goes throught all pairs of states (i,j)
			for (state_t i = 0; i < N - 1; i++) {
				outIWithoutLast = outI = fsm->getOutputAlongPath(states[i], *sIt);
				outIWithoutLast.pop_back();
				for (state_t j = i + 1; j < N; j++) {
					outJ = fsm->getOutputAlongPath(states[j], *sIt);
					if (outI != outJ) {
						idx = i * N + j - 1 - (i * (i + 3)) / 2;
						if (!distinguished[idx]) {
							seqInfo.dist.push_back(idx);
							outJ.pop_back();
							// could it be shorter?
							if (outIWithoutLast == outJ) {
								seqInfo.lastDist.push_back(idx);
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
				infos.emplace(seqInfo);
			}
			// is the first sequence of set reached? will be next sequence shorter?
			if ((sIt == outCSet.begin()) || ((--sIt)->size() != len) || (sIt->front() == STOUT_INPUT)) {
				len = seq_len_t(sIt->size());
				reduceSequencesOfSameLength(infos, distinguished, outCSet);
			}
		}
		// is STOUT_INPUT needed? -> put it in front of another sequence
		if (fsm->isOutputState() && (sIt->front() == STOUT_INPUT) && (outCSet.size() > 1)) {
			outCSet.erase(sIt++);
			bool stoutNeeded = false;
			for (state_t i = 0; i < N - 1; i++) {
				output_t output = fsm->getOutput(states[i], STOUT_INPUT);
				for (state_t j = i + 1; j < N; j++) {
					if (output == fsm->getOutput(states[j], STOUT_INPUT)) {
						stoutNeeded = true;
						for (auto seq : outCSet) {
							if (fsm->getOutputAlongPath(states[i], seq) != fsm->getOutputAlongPath(states[i], seq)) {
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
				outCSet.emplace(seq);
			}
		}
	}

	sequence_set_t getCharacterizingSet(DFSM * fsm,
			sequence_vec_t(*getSeparatingSequences)(DFSM * dfsm),
			bool filterPrefixes, void(*reduceFunc)(DFSM * fsm, sequence_set_t & outCSet)) {
		sequence_set_t outCSet;
		auto seq = (*getSeparatingSequences)(fsm);
		outCSet.clear();
		if (filterPrefixes) {
			FSMlib::PrefixSet pset;
			for (state_t i = 0; i < seq.size(); i++) {
				pset.insert(seq[i]);
			}
			pset.getMaximalSequences(outCSet);
		}
		else {
			for (state_t i = 0; i < seq.size(); i++) {
				outCSet.emplace(seq[i]);
			}
		}
		if (*reduceFunc != NULL)
			(*reduceFunc)(fsm, outCSet);
		return outCSet;
	}

	static bool distinguishBySequenceFromState(DFSM * fsm, const sequence_in_t& seq, state_t stateIdx,
			const vector<state_t>& states, vector<state_t>& dist, vector<bool>& distinguished) {	
		state_t N = fsm->getNumberOfStates();
		bool hasMinLen = false;
		sequence_out_t outS, outSWithoutLast, outJ;
		dist.clear();
		outSWithoutLast = outS = fsm->getOutputAlongPath(states[stateIdx], seq);
		outSWithoutLast.pop_back();
		for (state_t j = 0; j < N; j++) {
			if (j != stateIdx) {
				outJ = fsm->getOutputAlongPath(states[j], seq);
				if (outS != outJ) {
					if (!distinguished[j]) {
						dist.push_back(j);
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

	void reduceSCSet_LS_SL(DFSM* fsm, state_t stateIdx, sequence_set_t & outSCSet) {
		vector<bool> distinguished(fsm->getNumberOfStates(), false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished states by current sequence
		vector<state_t> states = fsm->getStates();
		for (sequence_set_t::reverse_iterator sIt = outSCSet.rbegin(); sIt != outSCSet.rend(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, stateIdx, states, dist, distinguished)) {
				if (dist.empty()) {
					outSCSet.erase(--sIt.base());
					sIt--;
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outSCSet.erase(--sIt.base());
					sIt--;
					truncateSeq(fsm, shortSeq, states, dist, distinguished, false, stateIdx);
					outSCSet.emplace(shortSeq);
				}
			}
		}
		distinguished.assign(fsm->getNumberOfStates(), false);
		for (sequence_set_t::iterator sIt = outSCSet.begin(); sIt != outSCSet.end(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, stateIdx, states, dist, distinguished)) {
				if (dist.empty()) {
					outSCSet.erase(sIt--);
				}
				else {
					sequence_in_t shortSeq(*sIt);
					outSCSet.erase(sIt--);
					truncateSeq(fsm, shortSeq, states, dist, distinguished, true, stateIdx);
					outSCSet.emplace(shortSeq);
				}
			}
		}
	}

	void reduceSCSet_LS(DFSM* fsm, state_t stateIdx, sequence_set_t & outSCSet) {
		vector<bool> distinguished(fsm->getNumberOfStates(), false); // is already a pair of states distinguished?
		vector<state_t> dist; // distinguished states by current sequence
		vector<state_t> states = fsm->getStates();
		for (sequence_set_t::reverse_iterator sIt = outSCSet.rbegin(); sIt != outSCSet.rend(); sIt++) {
			if (!distinguishBySequenceFromState(fsm, *sIt, stateIdx, states, dist, distinguished)) {
				outSCSet.erase(--sIt.base());
				sIt--;
			}
		}
	}

	void reduceSCSet_EqualLength(DFSM* fsm, state_t stateIdx, sequence_set_t & outSCSet) {
		state_t N = fsm->getNumberOfStates();
		vector<bool> distinguished(N, false); // is already a pair of states distinguished?
		seq_len_t len;
		set<seq_info_t, seq_info_t> infos;
		sequence_out_t outS, outSWithoutLast, outJ;
		sequence_set_t::iterator sIt;
		vector<state_t> states = fsm->getStates();
		size_t seqIdx = outSCSet.size();
		if (fsm->isOutputState() && (outSCSet.begin()->size() == 1) && (outSCSet.begin()->front() == STOUT_INPUT)) {
			// STOUT_INPUT is the first element in outSCSet
			output_t output = fsm->getOutput(states[stateIdx], STOUT_INPUT);
			for (state_t j = 0; j < N; j++) {
				if ((j != stateIdx) && (output != fsm->getOutput(states[j], STOUT_INPUT))) {
					distinguished[j] = true;
				}
			}
			seqIdx--;// the first sequence won't be processed
		}
		sIt = outSCSet.end();
		sIt--; // point to the last sequence, i.e. longest one
		len = seq_len_t(sIt->size());
		//printf("start %d\n",state);
		for (; seqIdx > 0; seqIdx--) {
			seq_info_t seqInfo;
			outSWithoutLast = outS = fsm->getOutputAlongPath(states[stateIdx], *sIt);
			outSWithoutLast.pop_back();
			for (state_t j = 0; j < N; j++) {
				if (j != stateIdx) {
					outJ = fsm->getOutputAlongPath(states[j], *sIt);
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
				infos.emplace(seqInfo);
			}
			// is the first sequence of set reached? will be next sequence shorter?
			if ((sIt == outSCSet.begin()) || ((--sIt)->size() != len) || (sIt->front() == STOUT_INPUT)) {
				len = seq_len_t(sIt->size());
				reduceSequencesOfSameLength(infos, distinguished, outSCSet);
			}
		}
		// is STOUT_INPUT needed? -> put it in front of another sequence
		if (fsm->isOutputState() && (sIt->front() == STOUT_INPUT) && (outSCSet.size() > 1)) {
			outSCSet.erase(sIt++);
			bool stoutNeeded = false;
			output_t output = fsm->getOutput(states[stateIdx], STOUT_INPUT);
			for (state_t j = 0; j < N; j++) {
				if ((j != stateIdx) && (output != fsm->getOutput(states[j], STOUT_INPUT))) {
					stoutNeeded = true;
					for (auto seq : outSCSet) {
						if (fsm->getOutputAlongPath(states[stateIdx], seq) != fsm->getOutputAlongPath(states[j], seq)) {
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
				outSCSet.emplace(seq);
			}
		}
	}

	sequence_set_t getSCSet(const sequence_vec_t& distSeqs, state_t stateIdx, state_t N, bool filterPrefixes = false) {
		state_t idx;
		sequence_set_t outSCSet;
		FSMlib::PrefixSet pset;
		// grab sequence from table seq incident with stateIdx
		for (state_t j = 0; j < stateIdx; j++) {
			idx = j * N + stateIdx - 1 - (j * (j + 3)) / 2;
			if (filterPrefixes)
				pset.insert(distSeqs[idx]);
			else
				outSCSet.emplace(distSeqs[idx]);
		}
		idx = stateIdx * N + stateIdx - (stateIdx * (stateIdx + 3)) / 2;
		for (state_t j = stateIdx + 1; j < N; j++, idx++) {
			if (filterPrefixes)
				pset.insert(distSeqs[idx]);
			else
				outSCSet.emplace(distSeqs[idx]);
		}
		if (filterPrefixes)
			pset.getMaximalSequences(outSCSet);
		return outSCSet;
	}

	sequence_set_t getStateCharacterizingSet(DFSM * fsm, state_t state,
			sequence_vec_t(*getSeparatingSequences)(DFSM * dfsm),
			bool filterPrefixes, void(*reduceFunc)(DFSM * fsm, state_t stateIdx, sequence_set_t & outSCSet)) {
		auto seq = (*getSeparatingSequences)(fsm);
		state_t stateIdx = getIdx(fsm->getStates(), state);
		auto outSCSet = getSCSet(seq, stateIdx, fsm->getNumberOfStates(), filterPrefixes);
		// try to reduce count of seqeunces
		if (*reduceFunc != NULL)
			(*reduceFunc)(fsm, stateIdx, outSCSet);
		return outSCSet;
	}

	vector<sequence_set_t> getStatesCharacterizingSets(DFSM * fsm,
			sequence_vec_t(*getSeparatingSequences)(DFSM * dfsm),
			bool filterPrefixes, void(*reduceFunc)(DFSM * fsm, state_t stateIdx, sequence_set_t & outSCSet)) {
		state_t N = fsm->getNumberOfStates();
		auto seq = (*getSeparatingSequences)(fsm);
		vector<sequence_set_t> outSCSets;
		outSCSets.resize(N);
		// grab sequence from table seq incident with stateIdx i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes);
			// try to reduce count of seqeunces
			if (*reduceFunc != NULL)
				(*reduceFunc)(fsm, i, outSCSets[i]);
		}
		return outSCSets;
	}

	vector<sequence_set_t> getHarmonizedStateIdentifiers(DFSM * fsm,
			sequence_vec_t(*getSeparatingSequences)(DFSM * dfsm),
			bool filterPrefixes, void(*reduceFunc)(DFSM * fsm, state_t stateIdx, sequence_set_t & outSCSet)) {
		state_t N = fsm->getNumberOfStates();
		auto seq = (*getSeparatingSequences)(fsm);
		vector<sequence_set_t> outSCSets;
		outSCSets.resize(N);
		// grab sequence from table seq incident with state i
		for (state_t i = 0; i < N; i++) {
			outSCSets[i] = getSCSet(seq, i, N, filterPrefixes);
			if (*reduceFunc != NULL)
				(*reduceFunc)(fsm, i, outSCSets[i]);
		}
		return outSCSets;
	}
}
