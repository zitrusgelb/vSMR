#include "stdafx.h"
#include "AircraftLookup.hpp"

//
// CAircraftLookup Class by Even Rognlien, used with permission
//

CAircraftLookup::CAircraftLookup(string fileName) {
	ifstream myfile;

	myfile.open(fileName);
	if (myfile) {
		string line;
		stringstream sstream;

		while (getline(myfile, line)) {
			vector<string> strs;

			sstream << line;
			strs = split(line, '\t');

			if (strs.size() >= 5) {
				string tt1 = strs.front();
				string tt2 = strs.at(4);
				wingspans[strs.front()] = strs.at(4);

			}
		}
	}
	myfile.close();
}

string CAircraftLookup::getWingspan(string aircraftCode) {
	if (wingspans.find(aircraftCode) == wingspans.end())
		return "";

	return wingspans.find(aircraftCode)->second;
}

CAircraftLookup::~CAircraftLookup()
{
}