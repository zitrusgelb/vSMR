#pragma once
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include "Constant.hpp"

using namespace std;

class CCallsignLookup
{
private:
	map<string, string> callsigns;


public:

	CCallsignLookup(string fileName);
	string getCallsign(string airlineCode);

	~CCallsignLookup();
};