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

#include "../BBport/BBport.h"

void testBBport() {
	auto id = initLearning(true, 5, 2);
	printf("id: %d\n", id);
	input_t input = getNextInput(id);
	printf("in: %d\n", input);
	while (input != LEARNING_COMPLETED) {
		//setResponse(id, output_t(input + 1));
		//input = getNextInput(id);
		input = updateAndGetNextInput(id, output_t(input + 1));
		printf("in: %d\n", input);
		if (input == RESPONSE_REQUESTED) {
			printf("shit\n");
			break;
		}
	}
	printf("good\n");

}