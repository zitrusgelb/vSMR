#include "stdafx.h"
#include "Config.hpp"
#include <algorithm>
#include <regex>

CConfig::CConfig(string configPath)
{
	config_path = configPath;
	loadConfig();

	setActiveProfile("Default");
}

void CConfig::loadConfig() {

	stringstream ss;
	ifstream ifs;
	ifs.open(config_path.c_str(), std::ios::binary);
	ss << ifs.rdbuf();
	ifs.close();

	if (document.Parse<0>(ss.str().c_str()).HasParseError()) {
		CString msg;
		msg.Format("An error parsing vSMR configuration occurred. Error: %s (Offset: %i)\nOnce fixed, reload the config by typing '.smr reload'", document.GetParseError(), document.GetErrorOffset());
		AfxMessageBox(msg, MB_OK);

		document.Parse<0>("[{"
						"\"name\": \"Default\","
						"\"font\": {"
						"\"font_name\": \"EuroScope\","
						"\"weight\": \"Regular\","
						"\"sizes\": {\"one\": 0,\"two\": 0,\"three\": 0,\"four\": 0,\"five\": 0}},"
						"\"rimcas\": {\"timer\": [0],\"timer_lvp\": [0],\"rimcas_stage_two_speed_threshold\": 0},"
						"\"labels\": {\"leader_line_length\": 0,\"use_aspeed_for_gate\": false,\"airborne\": {\"use_departure_arrival_coloring\": false}},"
			"\"filters\": {\"hide_above_alt\": 0,\"hide_below_alt\": 0,\"hide_above_spd\": 0,\"show_on_rwy\": true,\"radar_range_nm\": 0,\"night_alpha_setting\": 0,\"pro_mode\": {\"enable\": false}},"
						"\"targets\": {\"show_primary_target\": false},"
			"\"approach_insets\": {\"extended_lines_length\": 0,\"extended_lines_ticks_spacing\": 1,\"background_color\": {\"r\": 127,\"g\": 122,\"b\": 122},\"extended_lines_color\": {\"r\": 0,\"g\": 0,\"b\": 0},\"runway_color\": {\"r\": 0,\"g\": 0,\"b\": 0}}"
						"}]");
	}
	
	profiles.clear();

	assert(document.IsArray());

	for (SizeType i = 0; i < document.Size(); i++) {
		const Value& profile = document[i];
		string profile_name = profile["name"].GetString();

		profiles.insert(pair<string, rapidjson::SizeType>(profile_name, i));
	}
}

const Value& CConfig::getActiveProfile() {
	return document[active_profile];
}

bool CConfig::isSidInitClbAvail(string sid, string airport) {
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				string sidzero;
				std::regex_replace(std::back_inserter(sidzero), sid.begin(), sid.end(), std::regex("[0-9]"), "0");
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);
						
						if (currentsid.find('0') != std::string::npos) {
							if (startsWith(sidzero.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("init_clb"))
								return true;
						}
						else if (startsWith(sid.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("init_clb"))
							return true;
					}
				}
			}
		}
	}
	return false;
}

int CConfig::getSidInitClb(string sid, string airport)
{
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				string sidzero;
				std::regex_replace(std::back_inserter(sidzero), sid.begin(), sid.end(), std::regex("[0-9]"), "0");
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);

						if (currentsid.find('0') != std::string::npos) {
							if (startsWith(sidzero.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("init_clb"))
								return SIDs[i]["init_clb"].GetInt();
						}
						else if (startsWith(sid.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("init_clb"))
							return SIDs[i]["init_clb"].GetInt();
					}
				}
			}
		}
	}
	return 0;
}

bool CConfig::isSidColorAvail(string sid, string airport) {
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				string sidzero;
				std::regex_replace(std::back_inserter(sidzero), sid.begin(), sid.end(), std::regex("[0-9]"), "0");

				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);

						if (currentsid.find('0') != std::string::npos) {
							if (startsWith(sidzero.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("color"))
								return true;
						}
						else if (startsWith(sid.c_str(), currentsid.c_str()) && getActiveProfile()["maps"][airport.c_str()]["sids"][i].HasMember("color"))
							return true;
						}
					}
				}
			}
		}
	return false;
}

Gdiplus::Color CConfig::getSidColor(string sid, string airport)
{
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				string sidzero;
				std::regex_replace(std::back_inserter(sidzero), sid.begin(), sid.end(), std::regex("[0-9]"), "0");
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);

						if (currentsid.find('0') != std::string::npos) {
							if (startsWith(sidzero.c_str(), currentsid.c_str()))
								return getConfigColor(SIDs[i]["color"]);
						}
						else if (startsWith(sid.c_str(), currentsid.c_str()))
						{
							return getConfigColor(SIDs[i]["color"]);
						}
					}
				}
			}
		}
	}
	return Gdiplus::Color(0, 0, 0);
}

Gdiplus::Color CConfig::getConfigColor(const Value& config_path) {
	int r = config_path["r"].GetInt();
	int g = config_path["g"].GetInt();
	int b = config_path["b"].GetInt();
	int a = 255;
	if (config_path.HasMember("a"))
		a = config_path["a"].GetInt();

	Gdiplus::Color Color(a, r, g, b);
	return Color;
}

COLORREF CConfig::getConfigColorRef(const Value& config_path) {
	int r = config_path["r"].GetInt();
	int g = config_path["g"].GetInt();
	int b = config_path["b"].GetInt();

	COLORREF Color(RGB(r, g, b));
	return Color;
}

const Value& CConfig::getAirportMapIfAny(string airport) {
	if (getActiveProfile().HasMember("maps")) {
		const Value& map_data = getActiveProfile()["maps"];
		if (map_data.HasMember(airport.c_str())) {
			const Value& airport_map = map_data[airport.c_str()];
			return airport_map;
		}
	}
	return getActiveProfile();
}

bool CConfig::isAirportMapAvail(string airport) {
	if (getActiveProfile().HasMember("maps")) {
		if (getActiveProfile()["maps"].HasMember(airport.c_str())) {
			return true;
		}
	}
	return false;
}

bool CConfig::isCustomCursorUsed() {
	if (getActiveProfile().HasMember("cursor")) {		
		if (strcmp(getActiveProfile()["cursor"].GetString(), "Default") == 0) {
			return false;
		}
	}
	return true; // by default use custom one so we don't break compatibility for old json settings that don't have the entry
}

bool CConfig::isCustomRunwayAvail(string airport, string name1, string name2) {
	if (getActiveProfile().HasMember("maps")) {
		if (getActiveProfile()["maps"].HasMember(airport.c_str())) {
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("runways") 
				&& getActiveProfile()["maps"][airport.c_str()]["runways"].IsArray()) {
				const Value& Runways = getActiveProfile()["maps"][airport.c_str()]["runways"];
				for (SizeType i = 0; i < Runways.Size(); i++) {
					if (startsWith(name1.c_str(), Runways[i]["runway_name"].GetString()) ||
						startsWith(name2.c_str(), Runways[i]["runway_name"].GetString())) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

vector<string> CConfig::getAllProfiles() {
	vector<string> toR;
	for (std::map<string, rapidjson::SizeType>::iterator it = profiles.begin(); it != profiles.end(); ++it)
	{
		toR.push_back(it->first);
	}
	return toR;
}

CConfig::~CConfig()
{
}