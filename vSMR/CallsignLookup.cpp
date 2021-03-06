#include "stdafx.h"
#include "CallsignLookup.hpp"

//
// CCallsignLookup Class by Even Rognlien, used with permission
//

CCallsignLookup::CCallsignLookup(string fileName) {
	ifstream myfile;

	myfile.open(fileName);
	if (myfile) {
		string line;
		stringstream sstream;

		while (getline(myfile, line)) {
			vector<string> strs;

			if (line[0] == ';') // ignore comments
				continue;

			sstream << line;
			strs = split(line, '\t');

			if (strs.size() >= 3) {
				callsigns[strs.front()] = strs.at(2);
			}
		}
	}
	myfile.close();
}

string CCallsignLookup::getCallsign(string airlineCode) {

	if (callsigns.find(airlineCode) == callsigns.end())
		return "";

	return callsigns.find(airlineCode)->second;
}

CCallsignLookup::~CCallsignLookup()
{
}