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

namespace FSMsequence {
	sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, const unique_ptr<AdaptiveDS>& ads) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSet", sequence_vec_t());
		sequence_vec_t ADSet;
		if (!ads) return ADSet;
		stack<pair<AdaptiveDS*, sequence_in_t>> lifo;
		lifo.emplace(ads.get(), sequence_in_t());
		ADSet.resize(fsm->getNumberOfStates());
		while (!lifo.empty()) {
			auto p = move(lifo.top());
			lifo.pop();
			if (p.first->decision.empty()) {
				ADSet[p.first->initialStates.front()] = p.second;
			}
			else {
				p.second.insert(p.second.end(), p.first->input.begin(), p.first->input.end());
				for (auto it = p.first->decision.begin(); it != p.first->decision.end(); it++) {
					lifo.emplace(it->second.get(), p.second);
				}
			}
		}
		return ADSet;
	}

	sequence_vec_t getAdaptiveDistinguishingSet(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSet", sequence_vec_t());
		auto ads = getAdaptiveDistinguishingSequence(fsm, omitUnnecessaryStoutInputs);
		return getAdaptiveDistinguishingSet(fsm, ads);
	}

	unique_ptr<AdaptiveDS> buildADS(const vector<state_t>& block,
		const unique_ptr<SplittingTree>& st, bool useStout) {
		if (!st) return nullptr;
		auto outADS = make_unique<AdaptiveDS>();
		outADS->initialStates = outADS->currentStates = block;

		queue<AdaptiveDS*> fifo;
		fifo.push(outADS.get());
		while (!fifo.empty()) {
			auto adsNode = fifo.front();
			fifo.pop();
			if (adsNode->currentStates.size() == 1) continue;
			auto pivot = adsNode->currentStates[0];
			auto next = st->distinguished[getStatePairIdx(pivot, adsNode->currentStates[1])];
			// find lowest node of ST with block of all currentStates
			for (state_t j = 2; j < adsNode->currentStates.size(); j++) {
				auto idx = getStatePairIdx(pivot, adsNode->currentStates[j]);
				if (next->block.size() < st->distinguished[idx]->block.size()) {
					next = st->distinguished[idx];
				}
			}
			// set distinguishing input sequence
			adsNode->input.insert(adsNode->input.end(), next->sequence.begin(), next->sequence.end());
			for (state_t sI = 0; sI < adsNode->initialStates.size(); sI++) {
				for (const auto &p : next->succ) {
					if (binary_search(p.second->block.begin(), p.second->block.end(), adsNode->currentStates[sI])) {
						state_t idx;
						for (idx = 0; next->block[idx] != adsNode->currentStates[sI]; idx++);
						auto outIt = adsNode->decision.find(p.first);
						if (outIt == adsNode->decision.end()) {
							auto adsNext = make_unique<AdaptiveDS>();
							if (useStout && (adsNode->input.back() != STOUT_INPUT)) adsNext->input.push_back(STOUT_INPUT);
							adsNext->initialStates.push_back(adsNode->initialStates[sI]);
							adsNext->currentStates.push_back(next->nextStates[idx]);
							fifo.push(adsNext.get());
							adsNode->decision[p.first] = move(adsNext);
						}
						else {
							outIt->second->initialStates.push_back(adsNode->initialStates[sI]);
							outIt->second->currentStates.push_back(next->nextStates[idx]);
						}
						break;
					}
				}
			}
		}
		return outADS;
	}

	unique_ptr<AdaptiveDS> getAdaptiveDistinguishingSequence(const unique_ptr<DFSM>& fsm, bool omitUnnecessaryStoutInputs) {
		RETURN_IF_UNREDUCED(fsm, "FSMsequence::getAdaptiveDistinguishingSequence", nullptr);
		bool useStout = !omitUnnecessaryStoutInputs && fsm->isOutputState();
		auto st = getSplittingTree(fsm, false, useStout);
		if (!st) return nullptr;
		// build ADS from ST
		return buildADS(st->rootST->block, st, useStout);
	}
}
