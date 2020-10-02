#pragma once
#include <EuroScopePlugIn.h>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <time.h>
#include <GdiPlus.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "Constant.hpp"
#include "CallsignLookup.hpp"
#include "AircraftLookup.hpp"
#include "Config.hpp"
#include "Rimcas.hpp"
#include "InsetWindow.h"
#include <memory>
#include <thread>
#include "ColorManager.h"
#include "Logger.h"
#include <boost/algorithm/string/join.hpp>

using namespace std;
using namespace Gdiplus;
using namespace EuroScopePlugIn;


namespace SMRSharedData
{
	static vector<string> ReleasedTracks;
	static vector<string> ManuallyCorrelated;
};


using namespace SMRSharedData;

class CSMRRadar :
	public EuroScopePlugIn::CRadarScreen
{
public:
	CSMRRadar();
	virtual ~CSMRRadar();

	bool BLINK = false;

	map<string, POINT> TagsOffsets;

	vector<string> Active_Arrivals;

	clock_t clock_init, clock_final;

	COLORREF SMR_TARGET_COLOR = RGB(255, 242, 73);
	COLORREF SMR_H1_COLOR = RGB(0, 255, 255);
	COLORREF SMR_H2_COLOR = RGB(0, 219, 219);
	COLORREF SMR_H3_COLOR = RGB(0, 183, 183);

	typedef struct tagPOINT2 {
		double x;
		double y;
	} POINT2;

	struct Patatoide_Points {
		map<int, POINT2> points;
		map<int, POINT2> History_one_points;
		map<int, POINT2> History_two_points;
		map<int, POINT2> History_three_points;
	};

	unordered_map<const char *, Patatoide_Points> Patatoides;

	map<string, bool> ClosedRunway;

	char DllPathFile[_MAX_PATH];
	string DllPath;
	string ConfigPath;
	CCallsignLookup * Callsigns;
	CAircraftLookup * Aircraft;
	CColorManager * ColorManager;

	map<string, bool> ShowLists;
	map<string, RECT> ListAreas;

	map<int, bool> appWindowDisplays;

	map<string, CRect> tagAreas;
	map<string, double> TagAngles;
	map<string, int> TagLeaderLineLength;

	bool QDMenabled = false;
	bool QDMSelectEnabled = false;
	POINT QDMSelectPt;
	POINT QDMmousePt;

	bool ColorSettingsDay = true;

	bool isLVP = false;
	int sectorIndicator = 0;

	map<string, RECT> TimePopupAreas;

	map<int, string> TimePopupData;
	multimap<string, string> AcOnRunway;
	map<string, bool> ColorAC;

	map<string, CRimcas::RunwayAreaType> RunwayAreas;

	map<string, RECT> MenuPositions;
	map<string, bool> DisplayMenu;

	map<string, clock_t> RecentlyAutoMovedTags;

	CRimcas * RimcasInstance = nullptr;
	CConfig * CurrentConfig = nullptr;

	map<int, Gdiplus::Font *> customFonts;
	int currentFontSize = 1;
	CFont menubar_font_left, menubar_font_right, menubar_font_timer;

	map<string, string> customCursors;

	map<string, CPosition> AirportPositions;

	bool Afterglow = true;

	int Trail_Gnd = 4;
	int Trail_App = 4;
	int PredictedLength = 0;

	bool NeedCorrelateCursor = false;
	bool ReleaseInProgress = false;
	bool AcquireInProgress = false;

	multimap<string, string> DistanceTools;
	bool DistanceToolActive = false;
	pair<string, string> ActiveDistance;


	int Toolbar_Offset = 0;

	//----
	// Tag types
	//---

	enum TagTypes { Departure, Arrival, Airborne, Uncorrelated, VFR };

	string ActiveAirport = "EGKK";
	list <string> ActiveAirports = { "EGKK" };

	inline string getActiveAirport() {
		return ActiveAirport;
	}
	inline list<string> getActiveAirports() {
		return ActiveAirports;
	}
	inline bool isActiveAirport(string value) {
		bool is = false;
		for (string airport : getActiveAirports()) {
			if (airport == value)
				return true;
		}
		return false;
	}

	inline string setActiveAirport(string value) {
		list<string> airports;
		StrTrimA(const_cast<char *>(value.c_str()), " ,;\t\r\n");
		transform(value.begin(), value.end(), value.begin(), ::toupper);


		int i = 0, start = 0;
		while (i <= value.size())
		{
			if (i == value.size() || value[i] < 'A' || value[i] > 'Z') {
				if (i - start == 4)
					airports.insert(airports.end(), value.substr(start, 4));
				start = i + 1;
			}
			i++;
		}
		ActiveAirports = airports;
		return ActiveAirport = ActiveAirports.front();
	}

	//---GenerateTagData--------------------------------------------

	static map<string, string> GenerateTagData(CPlugIn* Plugin, CRadarTarget Rt, CFlightPlan fp, bool isAcCorrelated, bool isProMode, int TransitionAltitude, bool useSpeedForGates, int sectorIndicator, string ActiveAirport);

	//---IsCorrelatedFuncs---------------------------------------------

	inline virtual bool IsCorrelated(CFlightPlan fp, CRadarTarget rt)
	{

		if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["enable"].GetBool())
		{
			if (fp.IsValid())
			{
				bool isCorr = false;
				if (strcmp(fp.GetControllerAssignedData().GetSquawk(), rt.GetPosition().GetSquawk()) == 0)
				{
					isCorr = true;
				}

				if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["accept_pilot_squawk"].GetBool())
				{
					isCorr = true;
				}

				if (isCorr)
				{
					const Value& sqs = CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["do_not_autocorrelate_squawks"];
					for (SizeType i = 0; i < sqs.Size(); i++) {
						if (strcmp(rt.GetPosition().GetSquawk(), sqs[i].GetString()) == 0)
						{
							isCorr = false;
							break;
						}
					}
				}

				if (find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()) != ManuallyCorrelated.end())
				{
					isCorr = true;
				}

				if (find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
				{
					isCorr = false;
				}

				return isCorr;
			}

			return false;
		} else
		{
			// If the pro mode is not used, then the AC is always correlated
			return true;
		}
	};

	//---CorrelateCursor--------------------------------------------

	virtual void CorrelateCursor();

	//---LoadCustomFont--------------------------------------------

	virtual void LoadCustomFont();

	//---LoadCustomCursors--------------------------------------------

	virtual void LoadCustomCursors();

	//---LoadProfile--------------------------------------------

	virtual void LoadProfile(string profileName);

	//---OnAsrContentLoaded--------------------------------------------

	virtual void OnAsrContentLoaded(bool Loaded);

	//---OnAsrContentToBeSaved------------------------------------------

	virtual void OnAsrContentToBeSaved();

	//---OnRefresh------------------------------------------------------

	virtual void OnRefresh(HDC hDC, int Phase);

	//---OnClickScreenObject-----------------------------------------

	virtual void OnClickScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button);

	//---OnDoubleClickScreenObject-----------------------------------------

	virtual void OnDoubleClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button);

	//---OnMoveScreenObject---------------------------------------------

	virtual void OnMoveScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, bool Released);

	//---OnOverScreenObject---------------------------------------------

	virtual void OnOverScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area);

	//---OnCompileCommand-----------------------------------------

	virtual bool OnCompileCommand(const char * sCommandLine);

	//---RefreshAirportActivity---------------------------------------------

	virtual void RefreshAirportActivity(void);

	//---OnRadarTargetPositionUpdate---------------------------------------------

	virtual void OnRadarTargetPositionUpdate(CRadarTarget RadarTarget);

	//---OnFlightPlanDisconnect---------------------------------------------

	virtual void OnFlightPlanDisconnect(CFlightPlan FlightPlan);

	//---isVisible---------------------------------------------

	virtual bool isVisible(CRadarTarget rt)
	{
		CRadarTargetPositionData RtPos = rt.GetPosition();
		int radarRange = CurrentConfig->getActiveProfile()["filters"]["radar_range_nm"].GetInt();
		int altitudeFilter_above = CurrentConfig->getActiveProfile()["filters"]["hide_above_alt"].GetInt();
		int altitudeFilter_below = CurrentConfig->getActiveProfile()["filters"]["hide_below_alt"].GetInt();
		int speedFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_spd"].GetInt();
		bool show_on_rwy = CurrentConfig->getActiveProfile()["filters"]["show_on_rwy"].GetBool();
		bool isAcDisplayed = false;

		if (show_on_rwy && RimcasInstance->isAcOnRunway(rt.GetCallsign())) {
			return true;
		}
		else if (rt.GetCorrelatedFlightPlan().GetTrackingControllerIsMe())
			return true;

		for (string airport : getActiveAirports()) {
			if (AirportPositions[airport].DistanceTo(RtPos.GetPosition()) <= radarRange) {
				isAcDisplayed = true;
				break;
			}
		}

		if (altitudeFilter_above != 0) {
			if (RtPos.GetPressureAltitude() > altitudeFilter_above)
			isAcDisplayed = false;
		}
		if (altitudeFilter_below != 0) {
			if (RtPos.GetPressureAltitude() < altitudeFilter_below)
				isAcDisplayed = false;
		}

		if (speedFilter != 0) {
			if (RtPos.GetReportedGS() > speedFilter)
				isAcDisplayed = false;
		}

		return isAcDisplayed;
	}

	//---Haversine---------------------------------------------
	// Heading in deg, distance in m
	const double PI = (double)M_PI;

	inline virtual CPosition Haversine(CPosition origin, double heading, double distance) {

		CPosition newPos;

		double d = (distance*0.00053996) / 60 * PI / 180;
		double trk = DegToRad(heading);
		double lat0 = DegToRad(origin.m_Latitude);
		double lon0 = DegToRad(origin.m_Longitude);

		double lat = asin(sin(lat0) * cos(d) + cos(lat0) * sin(d) * cos(trk));
		double lon = cos(lat) == 0 ? lon0 : fmod(lon0 + asin(sin(trk) * sin(d) / cos(lat)) + PI, 2 * PI) - PI;

		newPos.m_Latitude = RadToDeg(lat);
		newPos.m_Longitude = RadToDeg(lon);

		return newPos;
	}

	inline virtual float randomizeHeading(float originHead) {
		return float(fmod(originHead + float((rand() % 5) - 2), 360));
	}

	//---GetBottomLine---------------------------------------------

	virtual string GetBottomLine(const char * Callsign);

	//---IsVFR---------------------------------------------

	static string IsVFR(CFlightPlan fp, CRadarTarget rt)
	{
		if (!rt.GetPosition().IsValid())
			return "";

		if (fp.IsValid() && fp.GetFlightPlanData().GetPlanType()[0] == 'I')
			return "";

		switch (atoi(rt.GetPosition().GetSquawk()))
		{
		case 20:
			return "RESCU";
		case 23:
			return "BPO";
		case 24:
			return "TFFN";
		case 25:
			return "PJE";
		case 27:
			return "ACRO";
		case 30:
			return "CAL";
		case 31:
			return "OPSKY";
		case 33:
			return "VM";
		case 34:
			return "SAR";
		case 35:
			return "AIRCL";
		case 36:
			return "POL";
		case 37:
			return "BIV";
		case 76:
			return "VFRCD";
		case 3701:
			return "VFS";
		case 3702:
			return "VFW";
		case 3703:
			return "VFM";
		case 3704:
			return "VFN";
		case 3707:
			return "CR";
		case 4406:
			return "SW";
		case 4472:
			return "PJV";
		case 4473:
			return "CHX4";
		case 4474:
			return "BALL";
		case 4476:
			return "TAXI";
		case 4660:
			return "TWR";
		case 4642:
			return "VDH";
		case 4643:
			return "SSF";
		case 4644:
			return "SV";
		case 4645:
			return "EDHI";
		case 4647:
			return "HL";
		case 6103:
			return "RAFIS";
		case 6311:
			return "FR1";
		case 6312:
			return "FR2";
		case 6313:
			return "FR3";
		case 6314:
			return "FR4";
		case 6315:
			return "FR5";
		case 6316:
			return "FIS";
		case 6317:
			return "VMR";
		case 6550:
			return "DT";
		case 6570:
			return "DB";
		case 7000:
			return "V";
		case 7001:
			return "VOUT";
		case 7010:
			return "VIN";
		case 7012:
			return "HELI";
		case 7040:
			return "NL";
		case 7050:
			return "NU";
		default:
			if (fp.IsValid() && fp.GetFlightPlanData().GetPlanType()[0] == 'V')
				return "V";
			return "";
		}
	}


	//---LineIntersect---------------------------------------------

	/*inline virtual POINT getIntersectionPoint(POINT lineA, POINT lineB, POINT lineC, POINT lineD) {

		double x1 = lineA.x;
		double y1 = lineA.y;
		double x2 = lineB.x;
		double y2 = lineB.y;

		double x3 = lineC.x;
		double y3 = lineC.y;
		double x4 = lineD.x;
		double y4 = lineD.y;

		POINT p = { 0, 0 };

		double d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (d != 0) {
			double xi = ((x3 - x4) * (x1 * y2 - y1 * x2) - (x1 - x2) * (x3 * y4 - y3 * x4)) / d;
			double yi = ((y3 - y4) * (x1 * y2 - y1 * x2) - (y1 - y2) * (x3 * y4 - y3 * x4)) / d;

			p = { (int)xi, (int)yi };

		}
		return p;
	}*/

	//---OnFunctionCall-------------------------------------------------

	virtual void OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area);

	//---OnAsrContentToBeClosed-----------------------------------------

	void CSMRRadar::EuroScopePlugInExitCustom();

	//  This gets called before OnAsrContentToBeSaved()
	// -> we can't delete CurrentConfig just yet otherwise we can't save the active profile
	inline virtual void OnAsrContentToBeClosed(void)
	{
		delete RimcasInstance;
		//delete CurrentConfig;
		delete this;
	};
};
