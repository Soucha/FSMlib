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

#include <iostream>
#include "../FSMlib/FSMlib.h"


DFSM * fsm, *fsm2;
wchar_t message[200];

int main(int argc, char** argv) {
	DFSM dfsm;
	fsm = &dfsm;
	fsm->generate(5, 3, 2);
	auto fn = fsm->writeDOTfile("../data/");
	cout << fn << endl;
	int rv = system((string("dot -Tjpg") + string(" -O ") + fn).c_str());
	cout << rv << endl;
	char c;
	cin >> c;
	return 0;
}