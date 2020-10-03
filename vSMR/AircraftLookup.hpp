#pragma once
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include "Constant.hpp"

using namespace std;

class CAircraftLookup
{
private:
	map<string, string> wingspans;


public:

	CAircraftLookup(string fileName);
	string getWingspan(string aircraftCode);

	~CAircraftLookup();
};
