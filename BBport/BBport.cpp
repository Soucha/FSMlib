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

#include <condition_variable>
#include <thread>
#include "BBport.h"

struct learning_info_t {
	thread algThread;
	sequence_in_t seqToQuery;
	sequence_out_t response;
	bool waitingForResponse = false;
	std::condition_variable cv;
	std::mutex cv_m;
};

//vector<unique_ptr<learning_info_t>> runningAlgs;

learning_info_t li;

void learning() {
	std::unique_lock<std::mutex> lk(li.cv_m);
	lk.unlock();
	for (input_t i = 0; i < 5; i++) {
		lk.lock();
		li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });

		for (input_t j = 0; j < i + 1; j++)
		{
			li.seqToQuery.emplace_back(j);
			printf("learning: %u\n", j);
		}
		printf("learning: unlock\n");
		lk.unlock();
		li.cv.notify_one();
	}
	lk.lock();
	li.cv.wait(lk, []{return li.seqToQuery.empty() && !li.waitingForResponse; });
	li.seqToQuery.emplace_back(LEARNING_COMPLETED);
	lk.unlock();
	li.cv.notify_one();
}


int init() {
	//int id(0);
	li.algThread = thread(learning);
	return 0;// id;
}

input_t getNextInput(int id) {
	if (li.waitingForResponse) return RESPONSE_REQUESTED;
	if (li.seqToQuery.empty()) {
		std::unique_lock<std::mutex> lk(li.cv_m);
		printf("get: wait\n");
		li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
	}
	auto input = li.seqToQuery.front();
	if (input != LEARNING_COMPLETED)
		li.waitingForResponse = true;
	else {
		li.algThread.join();
	}
	li.seqToQuery.pop_front();
	return input;
}
void setResponse(int id, output_t output) {
	li.response.emplace_back(output);
	{
		printf("set: wait = false\n");
		std::lock_guard<std::mutex> lk(li.cv_m);
		li.waitingForResponse = false;
	}
	li.cv.notify_one();
}

input_t updateAndGetNextInput(int id, output_t output) {
	li.response.emplace_back(output);
	{
		printf("update: wait = false\n");
		std::lock_guard<std::mutex> lk(li.cv_m);
		li.waitingForResponse = false;
	}
	if (li.seqToQuery.empty()) {
		li.cv.notify_one();
		std::unique_lock<std::mutex> lk(li.cv_m);
		printf("get: wait\n");
		li.cv.wait(lk, []{return !li.seqToQuery.empty(); });
	}
	auto input = li.seqToQuery.front();
	if (input != LEARNING_COMPLETED)
		li.waitingForResponse = true;
	else {
		li.algThread.join();
	}
	li.seqToQuery.pop_front();
	return input;
}
