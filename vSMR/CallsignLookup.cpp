#include "stdafx.h"
#include "CallsignLookup.hpp"

//
// CCallsignLookup Class by Even Rognlien, used with permission
//

CCallsignLookup::CCallsignLookup(std::string fileName) {
	ifstream myfile;

	myfile.open(fileName);
	if (myfile) {
		string line;
		stringstream sstream;

		while (getline(myfile, line)) {
			vector<string> strs;

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