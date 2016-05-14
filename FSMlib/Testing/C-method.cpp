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

#include "FSMtesting.h"

using namespace FSMsequence;

namespace FSMtesting {
	struct ver_seq_t {
		input_t input = STOUT_INPUT;
		shared_ptr<ver_seq_t> neighbor, child;
	};

	struct TestNodeC {
		bool confirmed = false;
		state_t state;
		output_t stateOutput;
		output_t output;
		input_t input;

		TestNodeC(input_t input, output_t output, state_t state, output_t stateOutput) :
			input(input), output(output), state(state), stateOutput(stateOutput) {
		}
	};

	static bool equalSeqPart(seq_len_t idx, sequence_in_t::iterator & itE, sequence_in_t::iterator endE,
			const vector<unique_ptr<TestNodeC>>& cs) {
		while ((idx != cs.size()) && (itE != endE)) {
			if (cs[idx]->input != *itE)
				return false;
			++idx;
			++itE;
		}
		return true;
	}

	static shared_ptr<ver_seq_t> findNext(const shared_ptr<ver_seq_t>& vs, input_t input) {
		auto nextVs = vs->child;
		while (nextVs && (nextVs->input != input)) {
			nextVs = nextVs->neighbor;
		}
		return nextVs;
	}

	static void shortenTo(const shared_ptr<ver_seq_t>& vs, sequence_in_t seq) {
		if (seq.empty()) {
			vs->child.reset();
			return;
		}
		auto nextVs = findNext(vs, seq.front());
		if (!nextVs) {
			if (!vs->child) {
				vs->child = make_shared<ver_seq_t>();
				nextVs = vs->child;
			}
			else {
				nextVs = vs->child;
				while (nextVs->neighbor) {
					nextVs = nextVs->neighbor;
				}
				nextVs->neighbor = make_shared<ver_seq_t>();
				nextVs = nextVs->neighbor;
			}
			nextVs->input = seq.front();
		}
		seq.pop_front();
		shortenTo(nextVs, move(seq));
	}

	static void checkSucc(seq_len_t idx, const sequence_in_t & seq,
			const vector<unique_ptr<TestNodeC>>& cs, queue<seq_len_t>& newlyConfirmed) {
		seq_len_t aIdx = idx;
		for (auto input : seq) {
			aIdx++;
			if ((aIdx == cs.size()) || (cs[aIdx]->input != input)) return;
		}
		if (!cs[aIdx]->confirmed) {
			cs[aIdx]->confirmed = true;
			newlyConfirmed.emplace(aIdx);
		}
	}

	static void update(seq_len_t idx, const vector<unique_ptr<TestNodeC>>& cs, queue<seq_len_t>& newlyConfirmed,
		vector<vector<bool>>& verifiedTransition, vector<input_t>& verifiedState, vector<shared_ptr<ver_seq_t>>& verSeq,
		vector<vector<seq_len_t>>& confirmedNodes, seq_len_t& counter);

	static void processNewlyConfirmed(const vector<unique_ptr<TestNodeC>>& cs, queue<seq_len_t>& newlyConfirmed,
		vector<vector<bool>>& verifiedTransition, vector<input_t>& verifiedState, vector<shared_ptr<ver_seq_t>>& verSeq,
		vector<vector<seq_len_t>>& confirmedNodes, seq_len_t& counter, seq_len_t& currIdx, seq_len_t& lastConfIdx) {
		while (!newlyConfirmed.empty()) {
			auto idx = newlyConfirmed.front();
			if ((idx > lastConfIdx) && (idx <= currIdx)) lastConfIdx = idx;
			confirmedNodes[cs[idx]->state].push_back(idx);
			auto vs = verSeq[cs[idx]->state];
			for (++idx; idx < cs.size(); idx++) {
				auto nextVs = findNext(vs, cs[idx]->input);
				if (!nextVs) {
					for (; idx < cs.size(); idx++) {
						if (cs[idx]->confirmed) {
							update(newlyConfirmed.front(), cs, newlyConfirmed, verifiedTransition, verifiedState,
								verSeq, confirmedNodes, counter);
							break;
						}
					}
					break;
				}
				if (!nextVs->child) {// leaf
					if (!cs[idx]->confirmed) {
						cs[idx]->confirmed = true;
						newlyConfirmed.emplace(idx);
					}
					break;
				}
				if (cs[idx]->confirmed) {
					update(newlyConfirmed.front(), cs, newlyConfirmed, verifiedTransition, verifiedState, verSeq, confirmedNodes, counter);
					break;
				}
				vs = nextVs;
			}
			newlyConfirmed.pop();
		}
	}

	static void update(seq_len_t idx, const vector<unique_ptr<TestNodeC>>& cs, queue<seq_len_t>& newlyConfirmed,
		vector<vector<bool>>& verifiedTransition, vector<input_t>& verifiedState, vector<shared_ptr<ver_seq_t>>& verSeq,
		vector<vector<seq_len_t>>& confirmedNodes, seq_len_t& counter) {
		sequence_in_t seq;
		seq_len_t actIdx = idx;
		do {
			actIdx++;
			seq.push_back(cs[actIdx]->input);
		} while (!cs[actIdx]->confirmed);
		if ((seq.size() == 1) && (!verifiedTransition[cs[idx]->state][seq.front()])) {// a transition verified
			verifiedTransition[cs[idx]->state][seq.front()] = true;
			verifiedState[cs[idx]->state]--;
			counter--;
		}
		shortenTo(verSeq[cs[idx]->state], seq);
		for (const auto& nIdx : confirmedNodes[cs[idx]->state]) {
			checkSucc(nIdx, seq, cs, newlyConfirmed);
		}
	}

	static sequence_in_t getCS(bool stoutUsed, const vector<unique_ptr<TestNodeC>>& cs) {
		sequence_in_t CS;
		for (const auto& n : cs) {
			CS.push_back(n->input);
			if (stoutUsed) CS.push_back(STOUT_INPUT);
		}
		CS.pop_front();
		return CS;
	}

	static seq_len_t equalLength(sequence_out_t::iterator f, sequence_out_t::iterator s, seq_len_t maxLen) {
		seq_len_t len = 0;
		while ((len < maxLen) && (*f == *s)) {
			f++;
			s++;
			len++;
		}
		return len;
	}

	sequence_in_t C_method(const unique_ptr<DFSM>& fsm, int extraStates) {
		RETURN_IF_UNREDUCED(fsm, "FSMtesting::C_method", sequence_in_t());
		auto E = getAdaptiveDistinguishingSet(fsm);
		if (E.empty()) {
			return sequence_in_t();
		}
		auto N = fsm->getNumberOfStates();
		auto P = fsm->getNumberOfInputs();
		
		/* // Example from simao2009checking
		E.clear();
		E[0].push_back(0);
		E[0].push_back(1);
		E[0].push_back(0);
		E[1].push_back(0);
		E[1].push_back(1);
		E[1].push_back(0);
		E[2].push_back(0);
		E[2].push_back(1);
		E[3].push_back(0);
		E[3].push_back(1);
		E[4].push_back(0);
		E[4].push_back(1);
		//*/
		vector<vector<bool>> verifiedTransition(N);
		vector<input_t> verifiedState(N, P);
		vector<shared_ptr<ver_seq_t>> verSeq(N);
		for (state_t i = 0; i < N; i++) {
			verifiedTransition[i].resize(P, false);
			verSeq[i] = make_shared<ver_seq_t>();
		}
		if (fsm->isOutputState()) {
			for (state_t i = 0; i < N; i++) {
				sequence_in_t seq;
				for (const auto& input : E[i]) {
					if (input == STOUT_INPUT) continue;
					seq.push_back(input);
				}
				E[i].swap(seq);
			}
		}
		output_t outputState = (fsm->isOutputState()) ? fsm->getOutput(0, STOUT_INPUT) : DEFAULT_OUTPUT;
		output_t outputTransition;
		vector<unique_ptr<TestNodeC>> cs;
		cs.emplace_back(make_unique<TestNodeC>(STOUT_INPUT, DEFAULT_OUTPUT, 0, outputState));
		state_t currState = 0;
		for (const auto& input : E[0]) {
			auto nextState = fsm->getNextState(currState, input);
			outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
			outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, input) : DEFAULT_OUTPUT;
			cs.emplace_back(make_unique<TestNodeC>(input, outputTransition, nextState, outputState));
			currState = nextState;
		}
		vector<vector<seq_len_t>> confirmedNodes(N);
		confirmedNodes[0].push_back(0);
		queue<seq_len_t> newlyConfirmed;
		seq_len_t counter = N * fsm->getNumberOfInputs();
		seq_len_t currIdx = 1;
		seq_len_t lastConfIdx = 0;
		currState = 0;

		while (counter > 0) {
			//getCS(CS, fsm->isOutputState());
			//printf("%u/%u %u %s\n", currIdx, cs.size(), lastConfIdx, FSMmodel::getInSequenceAsString(CS).c_str());
			if (cs.back()->confirmed) {
				currIdx = seq_len_t(cs.size());
				lastConfIdx = currIdx - 1;
			}
			if (currIdx < cs.size()) {
				if (!cs[currIdx]->confirmed) {
					auto nextState = cs[currIdx]->state;
					auto nextInput = E[nextState].begin();
					if (equalSeqPart(currIdx + 1, nextInput, E[nextState].end(), cs)) {
						currState = cs.back()->state;
						for (; nextInput != E[cs[currIdx]->state].end(); nextInput++) {
							nextState = fsm->getNextState(currState, *nextInput);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, *nextInput) : DEFAULT_OUTPUT;
							cs.emplace_back(make_unique<TestNodeC>(*nextInput, outputTransition, nextState, outputState));
							currState = nextState;
						}
						cs[currIdx]->confirmed = true;
						newlyConfirmed.emplace(currIdx);
						update(lastConfIdx, cs, newlyConfirmed, verifiedTransition, verifiedState, verSeq, confirmedNodes, counter);
						processNewlyConfirmed(cs, newlyConfirmed, verifiedTransition, verifiedState, verSeq, confirmedNodes, counter,
							currIdx, lastConfIdx);
					}
				}
				else {
					lastConfIdx = currIdx;
				}
				currIdx++;
			}
			else if (verifiedState[cs.back()->state] > 0) {
				currState = cs.back()->state;
				for (input_t input = 0; input < P; input++) {
					if (!verifiedTransition[currState][input]) {
						auto nextState = fsm->getNextState(currState, input);
						outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
						outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, input) : DEFAULT_OUTPUT;
						cs.emplace_back(make_unique<TestNodeC>(input, outputTransition, nextState, outputState));
						cs.back()->confirmed = true;
						newlyConfirmed.emplace(currIdx); //cs.size()-1

						sequence_in_t seqE(E[nextState]);
						// output-confirmed
						if (!E[currState].empty() && input == E[currState].front()) {
							sequence_in_t suf(E[currState]);
							suf.pop_front();
							auto outSuf = fsm->getOutputAlongPath(nextState, suf);
							auto outE = fsm->getOutputAlongPath(nextState, E[nextState]);
							//printf("(%d,%d,%d) %s/%s %s\n", currState, input, nextState,
							//      FSMmodel::getInSequenceAsString(suf).c_str(),
							//    FSMmodel::getOutSequenceAsString(outSuf).c_str(),
							//  FSMmodel::getOutSequenceAsString(outE).c_str());
							seq_len_t lenE = 0; //outE.size();
							for (state_t i = 0; i < N; i++) {
								if (i != nextState) {
									auto outSufI = fsm->getOutputAlongPath(i, suf);
									auto osl = equalLength(outSuf.begin(), outSufI.begin(), seq_len_t(outSuf.size()));
									if (osl != outSuf.size()) {
										bool outConfirmed = false;
										auto sufIt = suf.begin();
										osl++;
										while (osl-- > 0) sufIt++;
										for (auto& cnIdx : confirmedNodes[i]) {
											auto sufBeginIt = suf.begin();
											if (equalSeqPart(cnIdx + 1, sufBeginIt, sufIt, cs)) {
												outConfirmed = true;
												break;
											}
										}

										if (outConfirmed) {
											continue;
											/*
											outConfirmed = false;
											for (auto cnIdx : confirmedNodes[nextState]) {
											auto sufBeginIt = suf.begin();
											if (equalSeqPart(cnIdx, sufBeginIt, sufIt)) {
											outConfirmed = true;
											break;
											}
											}
											if (outConfirmed) {
											continue;
											}
											*/
										}
									}
									auto outI = fsm->getOutputAlongPath(i, E[nextState]);
									auto oel = 1 + equalLength(outE.begin(), outI.begin(), seq_len_t(outI.size()));
									//printf("%s/%s x %s %d %d-%d\n", 
									//      FSMmodel::getInSequenceAsString(E[nextState]).c_str(),
									//    FSMmodel::getOutSequenceAsString(outE).c_str(),
									//  FSMmodel::getOutSequenceAsString(outI).c_str(),
									//oel, lenE, outE.size());
									if (oel > lenE) {
										lenE = oel;
										if (lenE == outE.size()) {// entire E is needed
											break;
										}
									}
								}
							}
							// adjust E
							for (; lenE < outE.size(); lenE++) {
								seqE.pop_back();
							}
						}
						currState = nextState;
						for (const auto& input : seqE) {
							nextState = fsm->getNextState(currState, input);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, input) : DEFAULT_OUTPUT;
							cs.emplace_back(make_unique<TestNodeC>(input, outputTransition, nextState, outputState));
							currState = nextState;
						}
						update(currIdx - 1, cs, newlyConfirmed, verifiedTransition, verifiedState, verSeq, confirmedNodes, counter);
						processNewlyConfirmed(cs, newlyConfirmed, verifiedTransition, verifiedState, verSeq, confirmedNodes, counter,
							currIdx, lastConfIdx);
						break;
					}
				}
			}
			else {// find unverified transition
				vector<bool> covered(N, false);
				list<pair<state_t, sequence_in_t>> fifo;
				currState = cs.back()->state;
				covered[currState] = true;
				fifo.emplace_back(currState, sequence_in_t());
				while (!fifo.empty()) {
					auto current = move(fifo.front());
					fifo.pop_front();
					for (input_t input = 0; input < P; input++) {
						auto nextState = fsm->getNextState(current.first, input);
						if (nextState == WRONG_STATE) continue;
						if (verifiedState[nextState] > 0) {
							for (auto nextInput = current.second.begin(); nextInput != current.second.end(); nextInput++) {
								nextState = fsm->getNextState(currState, *nextInput);
								outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
								outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, *nextInput) : DEFAULT_OUTPUT;
								cs.emplace_back(make_unique<TestNodeC>(*nextInput, outputTransition, nextState, outputState));
								cs.back()->confirmed = true;
								currState = nextState;
							}
							nextState = fsm->getNextState(currState, input);
							outputState = (fsm->isOutputState()) ? fsm->getOutput(nextState, STOUT_INPUT) : DEFAULT_OUTPUT;
							outputTransition = (fsm->isOutputTransition()) ? fsm->getOutput(currState, input) : DEFAULT_OUTPUT;
							lastConfIdx = seq_len_t(cs.size());
							cs.emplace_back(make_unique<TestNodeC>(input, outputTransition, nextState, outputState));
							cs.back()->confirmed = true;
							currIdx = seq_len_t(cs.size());
							fifo.clear();
							break;
						}
						if (!covered[nextState]) {
							covered[nextState] = true;
							sequence_in_t newPath(current.second);
							newPath.push_back(input);
							fifo.emplace_back(nextState, move(newPath));
						}
					}
				}

			}
		}
		return getCS(fsm->isOutputState(), cs);
	}

}
