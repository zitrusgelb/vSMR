#pragma once
#include "stdafx.h"
#include <string>
#include <sstream>
#include <sstream>
#include <iomanip>
#include <fstream>

using namespace std;

class Logger {
public:
	static bool ENABLED;
	static string DLL_PATH;

	static void info(string message) {
		if (Logger::ENABLED && Logger::DLL_PATH.length() > 0) {
			ofstream file;
			file.open(Logger::DLL_PATH + "\\vsmr.log", ofstream::out | ofstream::app);
			file << "INFO: " << message << endl;
			file.close();
		}
	}
};