#include "stdafx.h"
#include "Resource.h"
#include "SMRRadar.hpp"

ULONG_PTR m_gdiplusToken;
CPoint mouseLocation(0, 0);
string TagBeingDragged;
set<string> TagsDetailed;
int LeaderLineDefaultlength = 50;

//
// Cursor Things
//

bool initCursor = true;
HCURSOR smrCursor = NULL;
bool standardCursor; // switches between mouse cursor and pointer cursor when moving tags
bool customCursor; // use SMR version or default windows mouse symbol
WNDPROC gSourceProc;
HWND pluginWindow;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

map<int, CInsetWindow *> appWindows;
string EmptyScratchpad = "..\0"; // Make Scratchpad tag clickable, if empty

inline double closest(vector<double> const& vec, double value) {
	auto const it = lower_bound(vec.begin(), vec.end(), value);
	if (it == vec.end()) { return -1; }
	
	return *it;
};
inline bool IsTagBeingDragged(string c)
{
	return TagBeingDragged == c;
}
bool mouseWithin(CRect rect) {
	if (mouseLocation.x >= rect.left + 1 && mouseLocation.x <= rect.right - 1 && mouseLocation.y >= rect.top + 1 && mouseLocation.y <= rect.bottom - 1)
		return true;
	return false;
}
bool is_digits(const string &str) {
	return str.find_first_not_of("0123456789") == string::npos;
}
// ReSharper disable CppMsExtAddressOfClassRValue

CSMRRadar::CSMRRadar()
{

	Logger::info("CSMRRadar::CSMRRadar()");

	// Initializing randomizer
	srand(static_cast<unsigned>(time(nullptr)));
		
	// Initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);


	menubar_font_left.CreateFont(14,   // height of the font   
		0,                        // width font  
		0,                        //  nEscapement 
		0,                        //  nOrientation   
		FW_NORMAL,                //   nWeight   
		FALSE,                    //   bItalic   
		FALSE,                    //   bUnderline   
		0,                        //   cStrikeOut   
		ANSI_CHARSET,             //   nCharSet   
		OUT_DEFAULT_PRECIS,       //   nOutPrecision   
		CLIP_DEFAULT_PRECIS,      //   nClipPrecision   
		DEFAULT_QUALITY,          //   nQuality   
		DEFAULT_PITCH | FF_SWISS, //   nPitchAndFamily     
		_T("Euroscope"));
	menubar_font_right.CreateFont(15,   // height of the font   
		0,                        // width font  
		0,                        //  nEscapement 
		0,                        //  nOrientation   
		FW_NORMAL,                //   nWeight   
		FALSE,                    //   bItalic   
		FALSE,                    //   bUnderline   
		0,                        //   cStrikeOut   
		ANSI_CHARSET,             //   nCharSet   
		OUT_DEFAULT_PRECIS,       //   nOutPrecision   
		CLIP_DEFAULT_PRECIS,      //   nClipPrecision   
		DEFAULT_QUALITY,          //   nQuality   
		DEFAULT_PITCH | FF_SWISS, //   nPitchAndFamily     
		_T("Euroscope"));
	// Getting the DLL file folder
	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	DllPath = DllPathFile;
	DllPath.resize(DllPath.size() - strlen("vSMR.dll"));
	
	ConfigPath = DllPath + "\\vSMR_Profiles.json";

	Logger::info("Loading callsigns");
	// Loading up the callsigns for the bottom line
	// Search for ICAO airlines file if it already exists (usually given by the VACC)
	string AirlinesPath = DllPath;
	for (int i = 0; i < 3; ++i) {
		AirlinesPath = AirlinesPath.substr(0, AirlinesPath.find_last_of("/\\"));
	}
	AirlinesPath += "\\ICAO\\ICAO_Airlines.txt";

	ifstream f(AirlinesPath.c_str());

	if (f.good()) {
		Callsigns = new CCallsignLookup(AirlinesPath);
	}
	else {
		Callsigns = new CCallsignLookup(DllPath + "\\ICAO_Airlines.txt");
	}
	f.close();

	Logger::info("Loading aircraft");
	// Loading up the aircraft for the bottom line
	// Search for ICAO aircraft file if it already exists (usually given by the VACC)
	string AircraftPath = DllPath;
	for (int i = 0; i < 3; ++i) {
		AircraftPath = AircraftPath.substr(0, AircraftPath.find_last_of("/\\"));
	}
	AircraftPath += "\\ICAO\\ICAO_Aircraft.txt";

	f.open(AircraftPath.c_str());

	if (f.good()) {
		Aircraft = new CAircraftLookup(AircraftPath);
	}
	else {
		Aircraft = new CAircraftLookup(DllPath + "\\ICAO_Aircraft.txt");
	}
	f.close();

	Logger::info("Loading RIMCAS & Config");
	// Creating the RIMCAS instance
	if (RimcasInstance == nullptr)
		RimcasInstance = new CRimcas();

	// Loading up the config file
	if (CurrentConfig == nullptr)
		CurrentConfig = new CConfig(ConfigPath);

	if (ColorManager == nullptr)
		ColorManager = new CColorManager();

	standardCursor = true;	
	ActiveAirport = "EGKK";

	// Setting up the data for the 2 approach windows
	appWindowDisplays[1] = false;
	appWindowDisplays[2] = false;
	appWindows[1] = new CInsetWindow(APPWINDOW_ONE);
	appWindows[2] = new CInsetWindow(APPWINDOW_TWO);

	Logger::info("Loading profile");

	this->CSMRRadar::LoadProfile("Default");

	this->CSMRRadar::LoadCustomFont();

	this->CSMRRadar::LoadCustomCursors();

	this->CSMRRadar::RefreshAirportActivity();

	GenerateClickable();
}

CSMRRadar::~CSMRRadar()
{
	Logger::info(string(__FUNCSIG__));
	try {
		this->OnAsrContentToBeSaved();
		//this->EuroScopePlugInExitCustom();
	}
	catch (exception &e) {
		stringstream s;
		s << e.what() << endl;
		AfxMessageBox(string("Error occurred " + s.str()).c_str());
	}
	// Shutting down GDI+
	GdiplusShutdown(m_gdiplusToken);
	delete CurrentConfig;
}

void CSMRRadar::CorrelateCursor() {
	if (NeedCorrelateCursor)
	{
		if (standardCursor)
		{
			if (customCursors["default"].length())
				smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
			else
				smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCORRELATE), IMAGE_CURSOR, 0, 0, LR_SHARED));

			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			ASSERT(smrCursor);
			SetCursor(smrCursor);
			standardCursor = false;
		}
	}
	else
	{
		if (!standardCursor)
		{
			if (customCursor) {
				if (customCursors["default"].length())
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
				else
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0, LR_SHARED));
			}
			else {
				smrCursor = (HCURSOR)::LoadCursor(NULL, IDC_ARROW);
			}

			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			ASSERT(smrCursor);
			SetCursor(smrCursor);
			standardCursor = true;
		}
	}
}

void CSMRRadar::LoadCustomFont() {
	Logger::info(string(__FUNCSIG__));
	// Loading the custom font if there is one in use
	customFonts.clear();

	const Value& FSizes = CurrentConfig->getActiveProfile()["font"]["sizes"];
	string font_name = CurrentConfig->getActiveProfile()["font"]["font_name"].GetString();
	wstring buffer = wstring(font_name.begin(), font_name.end());
	Gdiplus::FontStyle fontStyle = Gdiplus::FontStyleRegular;
	if (strcmp(CurrentConfig->getActiveProfile()["font"]["weight"].GetString(), "Bold") == 0)
		fontStyle = Gdiplus::FontStyleBold;
	if (strcmp(CurrentConfig->getActiveProfile()["font"]["weight"].GetString(), "Italic") == 0)
		fontStyle = Gdiplus::FontStyleItalic;

	customFonts[1] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["one"].GetInt()), fontStyle, Gdiplus::UnitPixel);
	customFonts[2] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["two"].GetInt()), fontStyle, Gdiplus::UnitPixel);
	customFonts[3] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["three"].GetInt()), fontStyle, Gdiplus::UnitPixel);
	customFonts[4] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["four"].GetInt()), fontStyle, Gdiplus::UnitPixel);
	customFonts[5] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["five"].GetInt()), fontStyle, Gdiplus::UnitPixel);
}

void CSMRRadar::LoadCustomCursors() {
	Logger::info(string(__FUNCSIG__));
	// Loading the custom cursors if there is one in use
	customCursors.clear();

	customCursors["default"] = "";
	customCursors["correlate"] = "";
	customCursors["inset_move"] = "";
	customCursors["inset_resize"] = "";
	customCursors["move_tag"] = "";

	if (CurrentConfig->getActiveProfile().HasMember("cursors")) {
		string temp = "";

		temp = CurrentConfig->getActiveProfile()["cursors"]["default"].GetString();
		if (temp.length() && !(FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(temp.c_str()) || (INVALID_FILE_ATTRIBUTES == GetFileAttributes(temp.c_str()) && (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND))))
			customCursors["default"] = temp;
		
		temp = CurrentConfig->getActiveProfile()["cursors"]["correlate"].GetString();
		if (temp.length() && !(FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(temp.c_str()) || (INVALID_FILE_ATTRIBUTES == GetFileAttributes(temp.c_str()) && (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND))))
			customCursors["correlate"] = temp;

		temp = CurrentConfig->getActiveProfile()["cursors"]["inset_move"].GetString();
		if (temp.length() && !(FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(temp.c_str()) || (INVALID_FILE_ATTRIBUTES == GetFileAttributes(temp.c_str()) && (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND))))
			customCursors["inset_move"] = temp;

		temp = CurrentConfig->getActiveProfile()["cursors"]["inset_resize"].GetString();
		if (temp.length() && !(FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(temp.c_str()) || (INVALID_FILE_ATTRIBUTES == GetFileAttributes(temp.c_str()) && (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND))))
			customCursors["inset_resize"] = temp;

		temp = CurrentConfig->getActiveProfile()["cursors"]["move_tag"].GetString();
		if (temp.length() && !(FILE_ATTRIBUTE_DIRECTORY == GetFileAttributes(temp.c_str()) || (INVALID_FILE_ATTRIBUTES == GetFileAttributes(temp.c_str()) && (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_PATH_NOT_FOUND))))
			customCursors["move_tag"] = temp;
	}

	initCursor = true;
}

void CSMRRadar::LoadProfile(string profileName) {
	Logger::info(string(__FUNCSIG__));
	// Loading the new profile
	CurrentConfig->setActiveProfile(profileName);
	

	Logger::info("Loading callsigns");
	// Loading up the callsigns for the bottom line
	// Search for ICAO airlines file if it already exists (usually given by the VACC)
	if (CurrentConfig->getActiveProfile()["icao_airlines"].IsString()) {
		string AirlinesPath = CurrentConfig->getActiveProfile()["icao_airlines"].GetString();
		if (AirlinesPath.length()) {
			ifstream f(AirlinesPath.c_str());

			if (f.good()) {
				Callsigns = new CCallsignLookup(AirlinesPath);
			}
			f.close();
		}
	}

	Logger::info("Loading aircraft");
	// Loading up the callsigns for the bottom line
	// Search for ICAO airlines file if it already exists (usually given by the VACC)
	if (CurrentConfig->getActiveProfile()["icao_aircraft"].IsString()) {
		string AircraftPath = CurrentConfig->getActiveProfile()["icao_aircraft"].GetString();
		if (AircraftPath.length()) {
			ifstream f(AircraftPath.c_str());

			if (f.good()) {
				Aircraft = new CAircraftLookup(AircraftPath);
			}
			f.close();
		}
	}

	if (CurrentConfig->getActiveProfile()["toolbar_offset"].IsInt()) {
		Toolbar_Offset = CurrentConfig->getActiveProfile()["toolbar_offset"].GetInt();
	}

	// Loading all the new data
	const Value &RimcasTimer = CurrentConfig->getActiveProfile()["rimcas"]["timer"];
	const Value &RimcasTimerLVP = CurrentConfig->getActiveProfile()["rimcas"]["timer_lvp"];

	vector<int> RimcasNorm;
	for (SizeType i = 0; i < RimcasTimer.Size(); i++) {
		RimcasNorm.push_back(RimcasTimer[i].GetInt());
	}

	vector<int> RimcasLVP;
	for (SizeType i = 0; i < RimcasTimerLVP.Size(); i++) {
		RimcasLVP.push_back(RimcasTimerLVP[i].GetInt());
	}
	RimcasInstance->setCountdownDefinition(RimcasNorm, RimcasLVP);
	LeaderLineDefaultlength = CurrentConfig->getActiveProfile()["labels"]["leader_line_length"].GetInt();

	customCursor = CurrentConfig->isCustomCursorUsed();

	// Reloading the fonts
	this->LoadCustomFont();

	// Reloading the cursors
	this->LoadCustomCursors();

	GenerateClickable();
}

void CSMRRadar::OnAsrContentLoaded(bool Loaded)
{
	Logger::info(string(__FUNCSIG__));
	const char * p_value;

	// ReSharper disable CppZeroConstantCanBeReplacedWithNullptr
	if ((p_value = GetDataFromAsr("Airport")) != NULL)
		setActiveAirport(p_value);

	if ((p_value = GetDataFromAsr("ActiveProfile")) != NULL)
		this->LoadProfile(string(p_value));

	if ((p_value = GetDataFromAsr("FontSize")) != NULL)
		currentFontSize = atoi(p_value);

	if ((p_value = GetDataFromAsr("Afterglow")) != NULL)
		Afterglow = atoi(p_value) == 1 ? true : false;

	if ((p_value = GetDataFromAsr("AppTrailsDots")) != NULL)
		Trail_App = atoi(p_value);

	if ((p_value = GetDataFromAsr("GndTrailsDots")) != NULL)
		Trail_Gnd = atoi(p_value);

	if ((p_value = GetDataFromAsr("PredictedLine")) != NULL)
		PredictedLength = atoi(p_value);

	for (int i = 1; i < 3; i++)
	{
		string prefix = "SRW" + to_string(i);

		if ((p_value = GetDataFromAsr(string(prefix + "TopLeftX").c_str())) != NULL)
			appWindows[i]->m_Area.left = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "TopLeftY").c_str())) != NULL)
			appWindows[i]->m_Area.top = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "BottomRightX").c_str())) != NULL)
			appWindows[i]->m_Area.right = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "BottomRightY").c_str())) != NULL)
			appWindows[i]->m_Area.bottom = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "OffsetX").c_str())) != NULL)
			appWindows[i]->m_Offset.x = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "OffsetY").c_str())) != NULL)
			appWindows[i]->m_Offset.y = atoi(p_value);


		if ((p_value = GetDataFromAsr(string(prefix + "Filter").c_str())) != NULL)
			appWindows[i]->m_Filter = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Scale").c_str())) != NULL)
			appWindows[i]->m_Scale = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Rotation").c_str())) != NULL)
			appWindows[i]->m_Rotation = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Display").c_str())) != NULL)
			appWindowDisplays[i] = atoi(p_value) == 1 ? true : false;
	}

	// Auto load the airport config on ASR opened.
	CSectorElement rwy;
	for (rwy = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
		rwy.IsValid();
		rwy = GetPlugIn()->SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (startsWith(getActiveAirport().c_str(), rwy.GetAirportName())) {
			string name = rwy.GetRunwayName(0) + string(" / ") + rwy.GetRunwayName(1);

			if (rwy.IsElementActive(true, 0) || rwy.IsElementActive(true, 1) || rwy.IsElementActive(false, 0) || rwy.IsElementActive(false, 1)) {
				RimcasInstance->toggleMonitoredRunwayDep(name);
				if (rwy.IsElementActive(false, 0) || rwy.IsElementActive(false, 1)) {
					RimcasInstance->toggleMonitoredRunwayArr(name);
				}
			}
		}
	}

	// ReSharper restore CppZeroConstantCanBeReplacedWithNullptr
}

void CSMRRadar::OnAsrContentToBeSaved()
{
	Logger::info(string(__FUNCSIG__));

	SaveDataToAsr("Airport", "Active airport for RIMCAS", boost::algorithm::join(getActiveAirports(), " ").c_str());

	SaveDataToAsr("ActiveProfile", "vSMR active profile", CurrentConfig->getActiveProfileName().c_str());

	SaveDataToAsr("FontSize", "vSMR font size", to_string(currentFontSize).c_str());

	SaveDataToAsr("Afterglow", "vSMR Afterglow enabled", to_string(int(Afterglow)).c_str());

	SaveDataToAsr("AppTrailsDots", "vSMR APPR Trail Dots", to_string(Trail_App).c_str());

	SaveDataToAsr("GndTrailsDots", "vSMR GRND Trail Dots", to_string(Trail_Gnd).c_str());

	SaveDataToAsr("PredictedLine", "vSMR Predicted Track Lines", to_string(PredictedLength).c_str());

	for (int i = 1; i < 3; i++)
	{
		string prefix = "SRW" + to_string(i);

		SaveDataToAsr(string(prefix + "TopLeftX").c_str(), "SRW position", to_string(appWindows[i]->m_Area.left).c_str());
		SaveDataToAsr(string(prefix + "TopLeftY").c_str(), "SRW position", to_string(appWindows[i]->m_Area.top).c_str());
		SaveDataToAsr(string(prefix + "BottomRightX").c_str(), "SRW position", to_string(appWindows[i]->m_Area.right).c_str());
		SaveDataToAsr(string(prefix + "BottomRightY").c_str(), "SRW position", to_string(appWindows[i]->m_Area.bottom).c_str());

		SaveDataToAsr(string(prefix + "OffsetX").c_str(), "SRW offset", to_string(appWindows[i]->m_Offset.x).c_str());
		SaveDataToAsr(string(prefix + "OffsetY").c_str(), "SRW offset", to_string(appWindows[i]->m_Offset.y).c_str());

		SaveDataToAsr(string(prefix + "Filter").c_str(), "SRW filter", to_string(appWindows[i]->m_Filter).c_str());
		SaveDataToAsr(string(prefix + "Scale").c_str(), "SRW range", to_string(appWindows[i]->m_Scale).c_str());
		SaveDataToAsr(string(prefix + "Rotation").c_str(), "SRW rotation", to_string(appWindows[i]->m_Rotation).c_str());

		string to_save = "0";
		if (appWindowDisplays[i])
			to_save = "1";
		SaveDataToAsr(string(prefix + "Display").c_str(), "Display Secondary Radar Window", to_save.c_str());
	}	
}

void CSMRRadar::OnMoveScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, bool Released) {
	Logger::info(string(__FUNCSIG__));

	if (ObjectType == APPWINDOW_ONE || ObjectType == APPWINDOW_TWO) {
		int appWindowId = ObjectType - APPWINDOW_BASE;

		bool toggleCursor = appWindows[appWindowId]->OnMoveScreenObject(sObjectId, Pt, Area, Released);

		if (!toggleCursor)
		{
			if (standardCursor)
			{
				if (strcmp(sObjectId, "topbar") == 0) {
					if (customCursors["inset_move"].length())
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["inset_move"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
					else
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVEWINDOW), IMAGE_CURSOR, 0, 0, LR_SHARED));
				}
				else if (strcmp(sObjectId, "resize") == 0) {
					if (customCursors["inset_resize"].length())
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["inset_resize"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
					else
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRRESIZE), IMAGE_CURSOR, 0, 0, LR_SHARED));
				}
				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		} else
		{
			if (!standardCursor)
			{
				if (customCursor) {
					if (customCursors["default"].length())
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
					else
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0, LR_SHARED));
				}
				else {
					smrCursor = (HCURSOR)::LoadCursor(NULL, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}
	}

	if (ObjectType == DRAWING_TAG || ObjectType == TAG_CITEM_MANUALCORRELATE ||ObjectType == TAG_CITEM_CALLSIGN || ObjectType == TAG_CITEM_FPBOX || ObjectType == TAG_CITEM_RWY || ObjectType == TAG_CITEM_SID || ObjectType == TAG_CITEM_GATE || ObjectType == TAG_CITEM_NO || ObjectType == TAG_CITEM_GROUNDSTATUS) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);

		if (!Released)
		{
			if (standardCursor)
			{
				if (customCursors["move_tag"].length())
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["move_tag"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
				else
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVETAG), IMAGE_CURSOR, 0, 0, LR_SHARED));
				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		}
		else
		{
			if (!standardCursor)
			{
				if (customCursor) {
					if (customCursors["default"].length())
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
					else
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0, LR_SHARED));
				}
				else {
					smrCursor = (HCURSOR)::LoadCursor(NULL, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}

		if (rt.IsValid()) {
			CRect Temp = Area;
			POINT TagCenterPix = Temp.CenterPoint();
			POINT AcPosPix = ConvertCoordFromPositionToPixel(GetPlugIn()->RadarTargetSelect(sObjectId).GetPosition().GetPosition());
			POINT CustomTag = { TagCenterPix.x - AcPosPix.x, TagCenterPix.y - AcPosPix.y };

			if (CurrentConfig->getActiveProfile()["labels"]["auto_deconfliction"].GetBool())
			{
				double angle = RadToDeg(atan2(CustomTag.y, CustomTag.x));
				angle = fmod(angle + 360, 360);
				vector<double> angles;
				for (double k = 0.0; k <= 360.0; k += 22.5)
					angles.push_back(k);

				TagAngles[sObjectId] = closest(angles, angle);
				TagLeaderLineLength[sObjectId] = max(LeaderLineDefaultlength, min(int(DistancePts(AcPosPix, TagCenterPix)), LeaderLineDefaultlength * 2));

			}
			else
			{
				TagsOffsets[sObjectId] = CustomTag;
			}


			GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));

			if (Released)
			{
				TagBeingDragged = "";
			}
			else
			{
				TagBeingDragged = sObjectId;
			}

			RequestRefresh();
		}		
	}

	if (ObjectType == RIMCAS_IAW) {
		TimePopupAreas[sObjectId] = Area;

		if (!Released)
		{
			if (standardCursor)
			{
				if (customCursors["inset_move"].length())
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["inset_move"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
				else
					smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVEWINDOW), IMAGE_CURSOR, 0, 0, LR_SHARED));

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		}
		else
		{
			if (!standardCursor)
			{
				if (customCursor) {
					if (customCursors["default"].length())
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
					else
						smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0, LR_SHARED));
				}
				else {
					smrCursor = (HCURSOR)::LoadCursor(NULL, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}
	}

	mouseLocation = Pt;
	RequestRefresh();

}

void CSMRRadar::OnOverScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area)
{
	Logger::info(string(__FUNCSIG__));
	mouseLocation = Pt;
	RequestRefresh();
}

void CSMRRadar::OnDoubleClickScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, int Button)
{
	Logger::info(string(__FUNCSIG__));
	if (Button == BUTTON_RIGHT) {
		string callsign = GetPlugIn()->RadarTargetSelect(sObjectId).GetCallsign();
		if (TagsDetailed.find(callsign) != TagsDetailed.end()) {
			TagsDetailed.erase(callsign);
		}
		else {
			TagsDetailed.insert(callsign);
		}
	}
	RequestRefresh();
}

void CSMRRadar::OnClickScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, int Button)
{
	Logger::info(string(__FUNCSIG__));
	mouseLocation = Pt;

	if (ObjectType == APPWINDOW_ONE || ObjectType == APPWINDOW_TWO) {
		int appWindowId = ObjectType - APPWINDOW_BASE;

		if (strcmp(sObjectId, "close") == 0)
			appWindowDisplays[appWindowId] = false;
		else if (strcmp(sObjectId, "range") == 0) {
			GetPlugIn()->OpenPopupList(Area, "SRW Zoom", 1);
			GetPlugIn()->AddPopupListElement("55", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 55));
			GetPlugIn()->AddPopupListElement("50", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 50));
			GetPlugIn()->AddPopupListElement("45", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 45));
			GetPlugIn()->AddPopupListElement("40", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 40));
			GetPlugIn()->AddPopupListElement("35", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 35));
			GetPlugIn()->AddPopupListElement("30", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 30));
			GetPlugIn()->AddPopupListElement("25", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 25));
			GetPlugIn()->AddPopupListElement("20", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 20));
			GetPlugIn()->AddPopupListElement("15", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 15));
			GetPlugIn()->AddPopupListElement("10", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 10));
			GetPlugIn()->AddPopupListElement("5", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 5));
			GetPlugIn()->AddPopupListElement("1", "", RIMCAS_UPDATERANGE + appWindowId, false, int(appWindows[appWindowId]->m_Scale == 1));
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
		else if (strcmp(sObjectId, "filter") == 0) {
			GetPlugIn()->OpenPopupList(Area, "SRW Filter (ft)", 1);
			GetPlugIn()->AddPopupListElement("UNL", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 66000));
			GetPlugIn()->AddPopupListElement("9500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 9500));
			GetPlugIn()->AddPopupListElement("8500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 8500));
			GetPlugIn()->AddPopupListElement("7500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 7500));
			GetPlugIn()->AddPopupListElement("6500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 6500));
			GetPlugIn()->AddPopupListElement("5500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 5500));
			GetPlugIn()->AddPopupListElement("4500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 4500));
			GetPlugIn()->AddPopupListElement("3500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 3500));
			GetPlugIn()->AddPopupListElement("2500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 2500));
			GetPlugIn()->AddPopupListElement("1500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 1500));
			GetPlugIn()->AddPopupListElement("500", "", RIMCAS_UPDATEFILTER + appWindowId, false, int(appWindows[appWindowId]->m_Filter == 500));
			string tmp = to_string(GetPlugIn()->GetTransitionAltitude());
			GetPlugIn()->AddPopupListElement(tmp.c_str(), "", RIMCAS_UPDATEFILTER + appWindowId, false, 2, false, true);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
		else if (strcmp(sObjectId, "rotate") == 0) {
			GetPlugIn()->OpenPopupList(Area, "SRW Rotate (deg)", 1);
			for (int k = 0; k <= 360; k++)
			{
				string tmp = to_string(k);
				GetPlugIn()->AddPopupListElement(tmp.c_str(), "", RIMCAS_UPDATEROTATE + appWindowId, false, int(appWindows[appWindowId]->m_Rotation == k));
			}
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
	}

	else if (ObjectType == RIMCAS_ACTIVE_AIRPORT) {
		GetPlugIn()->OpenPopupEdit(Area, RIMCAS_ACTIVE_AIRPORT_FUNC, boost::algorithm::join(getActiveAirports(), " ").c_str());		
	}

	else if (ObjectType == DRAWING_BACKGROUND_CLICK)
	{
		if (QDMSelectEnabled)
		{
			if (Button == BUTTON_LEFT)
			{
				QDMSelectPt = Pt;
				RequestRefresh();
			}

			else if (Button == BUTTON_RIGHT)
			{
				QDMSelectEnabled = false;
				RequestRefresh();
			}
		}

		if (QDMenabled)
		{
			if (Button == BUTTON_RIGHT)
			{
				QDMenabled = false;
				RequestRefresh();
			}
		}
	}

	else if (ObjectType == RIMCAS_MENU) {

		if (strcmp(sObjectId, "DisplayMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Display Menu", 1);
			GetPlugIn()->AddPopupListElement("QDR Fixed Reference", "", RIMCAS_QDM_TOGGLE);
			GetPlugIn()->AddPopupListElement("QDR Select Reference", "", RIMCAS_QDM_SELECT_TOGGLE);
			GetPlugIn()->AddPopupListElement("SRW 1", "", APPWINDOW_ONE, false, int(appWindowDisplays[1]));
			GetPlugIn()->AddPopupListElement("SRW 2", "", APPWINDOW_TWO, false, int(appWindowDisplays[2]));
			GetPlugIn()->AddPopupListElement("Profiles", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		else if (strcmp(sObjectId, "TargetMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Target", 1);
			GetPlugIn()->AddPopupListElement("Label Font Size", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Afterglow", "", RIMCAS_UPDATE_AFTERGLOW, false, int(Afterglow));
			GetPlugIn()->AddPopupListElement("GRND Trail Dots", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("APPR Trail Dots", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Predicted Track Line", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Acquire", "", RIMCAS_UPDATE_ACQUIRE);
			GetPlugIn()->AddPopupListElement("Release", "", RIMCAS_UPDATE_RELEASE);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		else if (strcmp(sObjectId, "MapMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Maps", 1);
			GetPlugIn()->AddPopupListElement("Airport Maps", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Custom Maps", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		else if (strcmp(sObjectId, "ColourMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Colours", 1);
			GetPlugIn()->AddPopupListElement("Colour Settings", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Brightness", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		else if (strcmp(sObjectId, "RIMCASMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Alerts", 1);
			GetPlugIn()->AddPopupListElement("Conflict Alert ARR", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Conflict Alert DEP", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Runway closed", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Visibility", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
		else if (strcmp(sObjectId, "QuestionmarkMenu") == 0) {
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;
			
			GetPlugIn()->OpenPopupList(Area, "Options", 1);
			GetPlugIn()->AddPopupListElement("Reload", "", RIMCAS_RELOAD_PROFILE);
			GetPlugIn()->AddPopupListElement("CPDLC Settings", "", RIMCAS_CPDLC_SETTINGS);
			GetPlugIn()->AddPopupListElement("CPDLC Connect", "", RIMCAS_CPDLC_CONNECT);
			GetPlugIn()->AddPopupListElement("CPDLC Poll Messages", "", RIMCAS_CPDLC_POLL);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		else if (strcmp(sObjectId, "/") == 0)
		{
			if (Button == BUTTON_LEFT)
			{
				DistanceToolActive = !DistanceToolActive;
				if (!DistanceToolActive)
					ActiveDistance = pair<string, string>("", "");

				if (DistanceToolActive)
				{
					QDMenabled = false;
					QDMSelectEnabled = false;
				}
			}
			else if (Button == BUTTON_RIGHT)
			{
				DistanceToolActive = false;
				ActiveDistance = pair<string, string>("", "");
				DistanceTools.clear();
			}
		}
	}

	else if (ObjectType == DRAWING_TAG || ObjectType == DRAWING_AC_SYMBOL) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		//GetPlugIn()->SetASELAircraft(rt); // NOTE: This does NOT work eventhough the api says it should?
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));  // make sure the correct aircraft is selected before calling 'StartTagFunction'
		
		if (rt.GetCorrelatedFlightPlan().IsValid()) {
			StartTagFunction(rt.GetCallsign(), NULL, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(), NULL, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO, Pt, Area);
		}

		// Release & correlate actions
		if (ReleaseInProgress || AcquireInProgress)
		{
			if (ReleaseInProgress)
			{
				ReleaseInProgress = NeedCorrelateCursor = false;

				ReleasedTracks.push_back(rt.GetSystemID());

				if (find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()) != ManuallyCorrelated.end())
					ManuallyCorrelated.erase(find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()));
			}

			if (AcquireInProgress)
			{
				AcquireInProgress = NeedCorrelateCursor = false;

				ManuallyCorrelated.push_back(rt.GetSystemID());

				if (find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
					ReleasedTracks.erase(find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()));
			}


			CorrelateCursor();

			return;
		}

		if (ObjectType == DRAWING_AC_SYMBOL)
		{
			if (QDMSelectEnabled)
			{
				if (Button == BUTTON_LEFT)
				{
					QDMSelectPt = Pt;
					RequestRefresh();
				}
			}
			else if (DistanceToolActive) {
				if (ActiveDistance.first == "")
				{
					ActiveDistance.first = sObjectId;
				}
				else if (ActiveDistance.second == "")
				{
					ActiveDistance.second = sObjectId;
					DistanceTools.insert(ActiveDistance);
					ActiveDistance = pair<string, string>("", "");
					DistanceToolActive = false;
				}
				RequestRefresh();
			}
			else
			{
				if (TagsOffsets.find(sObjectId) != TagsOffsets.end())
					TagsOffsets.erase(sObjectId);

				if (Button == BUTTON_LEFT)
				{
					if (TagAngles.find(sObjectId) == TagAngles.end())
					{
						TagAngles[sObjectId] = 0;
					} else
					{
						TagAngles[sObjectId] = fmod(TagAngles[sObjectId] - 22.5, 360);
					}
				}

				if (Button == BUTTON_RIGHT)
				{
					if (TagAngles.find(sObjectId) == TagAngles.end())
					{
						TagAngles[sObjectId] = 0;
					}
					else
					{
						TagAngles[sObjectId] = fmod(TagAngles[sObjectId] + 22.5, 360);
					}
				}

				RequestRefresh();
			}
		}
	}

	else if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW1 || ObjectType == DRAWING_AC_SYMBOL_APPWINDOW2)
	{
		if (DistanceToolActive) {
			if (ActiveDistance.first == "")
			{
				ActiveDistance.first = sObjectId;
			}
			else if (ActiveDistance.second == "")
			{
				ActiveDistance.second = sObjectId;
				DistanceTools.insert(ActiveDistance);
				ActiveDistance = pair<string, string>("", "");
				DistanceToolActive = false;
			}
			RequestRefresh();
		} else
		{
			if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW1)
				appWindows[1]->OnClickScreenObject(sObjectId, Pt, Button);

			else if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW2)
				appWindows[2]->OnClickScreenObject(sObjectId, Pt, Button);
		}
	}

	else if (ObjectType == RIMCAS_DISTANCE_TOOL)
	{
		vector<string> s = split(sObjectId, ',');
		pair<string, string> toRemove = pair<string, string>(s.front(), s.back());

		typedef multimap<string, string>::iterator iterator;
		pair<iterator, iterator> iterpair = DistanceTools.equal_range(toRemove.first);

		iterator it = iterpair.first;
		for (; it != iterpair.second; ++it) {
			if (it->second == toRemove.second) {
				it = DistanceTools.erase(it);
				break;
			}
		}

	}
	else if (ObjectType == TAG_CITEM_CONTROLLER && Button == BUTTON_MIDDLE) {
		if (sectorIndicator >= 1)
			sectorIndicator = 0;
		else
			sectorIndicator++;
	}
	if (Button == BUTTON_LEFT && TagObjectLeftTypes.find(ObjectType) != TagObjectLeftTypes.end()) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		StartTagFunction(rt.GetCallsign(), NULL, ObjectType, rt.GetCallsign(), TagObjectLeftTypes[ObjectType].second.c_str(), TagObjectLeftTypes[ObjectType].first, Pt, Area);
	}
	else if (Button == BUTTON_LEFT) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);

		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		if (rt.GetCorrelatedFlightPlan().IsValid()) {
			StartTagFunction(rt.GetCallsign(), NULL, TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(), NULL, TAG_ITEM_FUNCTION_NO, Pt, Area);
		}
	}
	else if (Button == BUTTON_MIDDLE && TagObjectMiddleTypes.find(ObjectType) != TagObjectMiddleTypes.end()) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		StartTagFunction(rt.GetCallsign(), NULL, ObjectType, rt.GetCallsign(), TagObjectMiddleTypes[ObjectType].second.c_str(), TagObjectMiddleTypes[ObjectType].first, Pt, Area);
	}
	else if (Button == BUTTON_RIGHT && TagObjectRightTypes.find(ObjectType) != TagObjectRightTypes.end()) {
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		StartTagFunction(rt.GetCallsign(), NULL, ObjectType, rt.GetCallsign(), TagObjectRightTypes[ObjectType].second.c_str(), TagObjectRightTypes[ObjectType].first, Pt, Area);
	}
	RequestRefresh();
};

void CSMRRadar::OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area) {
	Logger::info(string(__FUNCSIG__));
	mouseLocation = Pt;
	if (FunctionId == APPWINDOW_ONE || FunctionId == APPWINDOW_TWO) {
		int id = FunctionId - APPWINDOW_BASE;
		appWindowDisplays[id] = !appWindowDisplays[id];
	}

	else if (FunctionId == RIMCAS_ACTIVE_AIRPORT_FUNC) {
		setActiveAirport(sItemString);
		SaveDataToAsr("Airport", "Active airport", boost::algorithm::join(getActiveAirports(), " ").c_str());
	}

	else if (FunctionId == RIMCAS_UPDATE_FONTS) {
		if (strcmp(sItemString, "Size 1") == 0)
			currentFontSize = 1;
		else if (strcmp(sItemString, "Size 2") == 0)
			currentFontSize = 2;
		else if (strcmp(sItemString, "Size 3") == 0)
			currentFontSize = 3;
		else if (strcmp(sItemString, "Size 4") == 0)
			currentFontSize = 4;
		else if (strcmp(sItemString, "Size 5") == 0)
			currentFontSize = 5;
		
		SaveDataToAsr("FontSize", "vSMR font size", to_string(currentFontSize).c_str());
		ShowLists["Label Font Size"] = true;
	}

	else if (FunctionId == RIMCAS_QDM_TOGGLE) {
		QDMenabled = !QDMenabled;
		QDMSelectEnabled = false;
	}

	else if (FunctionId == RIMCAS_QDM_SELECT_TOGGLE)
	{
		if (!QDMSelectEnabled)
		{
			QDMSelectPt = ConvertCoordFromPositionToPixel(AirportPositions[getActiveAirport()]);
		}
		QDMSelectEnabled = !QDMSelectEnabled;
		QDMenabled = false;
	}

	else if (FunctionId == RIMCAS_UPDATE_PROFILE) {
		this->CSMRRadar::LoadProfile(sItemString);
		LoadCustomFont();
		LoadCustomCursors();
		GenerateClickable();
		SaveDataToAsr("ActiveProfile", "vSMR active profile", sItemString);

		ShowLists["Profiles"] = true;
	}

	else if (FunctionId == RIMCAS_RELOAD_PROFILE) {
		this->CSMRRadar::CurrentConfig = new CConfig(ConfigPath);
		this->CSMRRadar::LoadProfile(CurrentConfig->getActiveProfileName());
	}

	else if (FunctionId == RIMCAS_UPDATEFILTER1 || FunctionId == RIMCAS_UPDATEFILTER2) {
		int id = FunctionId - RIMCAS_UPDATEFILTER;
		if (startsWith("UNL", sItemString))
			sItemString = "66000";
		appWindows[id]->m_Filter = atoi(sItemString);
	}

	else if (FunctionId == RIMCAS_UPDATERANGE1 || FunctionId == RIMCAS_UPDATERANGE2) {
		int id = FunctionId - RIMCAS_UPDATERANGE;
		appWindows[id]->m_Scale = atoi(sItemString);
	}

	else if (FunctionId == RIMCAS_UPDATEROTATE1 || FunctionId == RIMCAS_UPDATEROTATE2) {
		int id = FunctionId - RIMCAS_UPDATEROTATE;
		appWindows[id]->m_Rotation = atoi(sItemString);
	}

	else if (FunctionId == RIMCAS_UPDATE_BRIGHNESS) {
		if (strcmp(sItemString, "Day") == 0)
			ColorSettingsDay = true;
		else
			ColorSettingsDay = false;

		ShowLists["Colour Settings"] = true;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_CA_ARRIVAL_FUNC) {
		RimcasInstance->toggleMonitoredRunwayArr(string(sItemString));

		ShowLists["Conflict Alert ARR"] = true;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_CA_MONITOR_FUNC) {
		RimcasInstance->toggleMonitoredRunwayDep(string(sItemString));

		ShowLists["Conflict Alert DEP"] = true;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_CLOSED_RUNWAYS_FUNC) {
		RimcasInstance->toggleClosedRunway(string(sItemString));

		ShowLists["Runway closed"] = true;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_OPEN_LIST) {

		ShowLists[string(sItemString)] = true;
		ListAreas[string(sItemString)] = Area;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_UPDATE_LVP) {
		if (strcmp(sItemString, "Normal") == 0)
			isLVP = false;
		if (strcmp(sItemString, "Low") == 0)
			isLVP = true;
		
		ShowLists["Visibility"] = true;

		RequestRefresh();
	}

	else if (FunctionId == RIMCAS_UPDATE_AFTERGLOW)
	{
		Afterglow = !Afterglow;
		SaveDataToAsr("Afterglow", "vSMR Afterglow enabled", to_string(int(Afterglow)).c_str());
	}

	else if (FunctionId == RIMCAS_UPDATE_GND_TRAIL)
	{
		Trail_Gnd = atoi(sItemString);
		SaveDataToAsr("GndTrailsDots", "vSMR GRND Trail Dots", to_string(Trail_Gnd).c_str());

		ShowLists["GRND Trail Dots"] = true;
	}

	else if (FunctionId == RIMCAS_UPDATE_APP_TRAIL)
	{
		Trail_App = atoi(sItemString);
		SaveDataToAsr("AppTrailsDots", "vSMR APPR Trail Dots", to_string(Trail_App).c_str());

		ShowLists["APPR Trail Dots"] = true;
	}

	else if (FunctionId == RIMCAS_UPDATE_PTL)
	{
		PredictedLength = atoi(sItemString);
		SaveDataToAsr("PredictedLine", "vSMR Predicted Track Lines", to_string(PredictedLength).c_str());

		ShowLists["Predicted Track Line"] = true;
	}

	else if (FunctionId == RIMCAS_BRIGHTNESS_LABEL)
	{
		ColorManager->update_brightness("label", atoi(sItemString));
		ShowLists["Label"] = true;
	}

	else if (FunctionId == RIMCAS_BRIGHTNESS_AFTERGLOW)
	{
		ColorManager->update_brightness("afterglow", atoi(sItemString));
		ShowLists["Afterglow"] = true;
	}

	else if (FunctionId == RIMCAS_BRIGHTNESS_SYMBOL)
	{
		ColorManager->update_brightness("symbol", atoi(sItemString));
		ShowLists["Symbol"] = true;
	}

	else if (FunctionId == RIMCAS_UPDATE_RELEASE)
	{
		ReleaseInProgress = !ReleaseInProgress;
		if (ReleaseInProgress)
			AcquireInProgress = false;
		NeedCorrelateCursor = ReleaseInProgress;

		CorrelateCursor();
	}

	else if (FunctionId == RIMCAS_UPDATE_ACQUIRE)
	{
		AcquireInProgress = !AcquireInProgress;
		if (AcquireInProgress)
			ReleaseInProgress = false;
		NeedCorrelateCursor = AcquireInProgress;

		CorrelateCursor();
	}

	else if (FunctionId == RIMCAS_CPDLC_SETTINGS)
	{
		GetPlugIn()->OnCompileCommand(".smr");
	}

	else if (FunctionId == RIMCAS_CPDLC_CONNECT)
	{
		GetPlugIn()->OnCompileCommand(".smr connect");
	}

	else if (FunctionId == RIMCAS_CPDLC_POLL)
	{
		GetPlugIn()->OnCompileCommand(".smr poll");
	}
	else if (FunctionId == TAG_FUNC_DETAILED) {
		string callsign = GetPlugIn()->RadarTargetSelectASEL().GetCallsign();
		if (TagsDetailed.find(callsign) != TagsDetailed.end()) {
			TagsDetailed.erase(callsign);
		}
		else {
			TagsDetailed.insert(callsign);
		}
	}
}

void CSMRRadar::RefreshAirportActivity(void) {
	Logger::info(string(__FUNCSIG__));
	//
	// Getting the depatures and arrivals airports
	//

	Active_Arrivals.clear();
	CSectorElement airport;
	for (airport = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT);
		airport.IsValid();
		airport = GetPlugIn()->SectorFileElementSelectNext(airport, SECTOR_ELEMENT_AIRPORT))
	{
		if (airport.IsElementActive(false)) {
			string s = airport.GetName();
			s = s.substr(0, 4);
			transform(s.begin(), s.end(), s.begin(), ::toupper);
			Active_Arrivals.push_back(s);
		}
	}
}

void CSMRRadar::OnRadarTargetPositionUpdate(CRadarTarget RadarTarget)
{
	Logger::info(string(__FUNCSIG__));
	if (!RadarTarget.IsValid() || !RadarTarget.GetPosition().IsValid())
		return;

	CRadarTargetPositionData RtPos = RadarTarget.GetPosition();

	Patatoides[RadarTarget.GetCallsign()].History_three_points = Patatoides[RadarTarget.GetCallsign()].History_two_points;
	Patatoides[RadarTarget.GetCallsign()].History_two_points = Patatoides[RadarTarget.GetCallsign()].History_one_points;
	Patatoides[RadarTarget.GetCallsign()].History_one_points = Patatoides[RadarTarget.GetCallsign()].points;

	Patatoides[RadarTarget.GetCallsign()].points.clear();

	CFlightPlan fp = GetPlugIn()->FlightPlanSelect(RadarTarget.GetCallsign());

	// All units in M
	float width = 34.0f;
	float cabin_width = 4.0f;
	float length = 38.0f;

	if (fp.IsValid()) {
		char wtc = fp.GetFlightPlanData().GetAircraftWtc();

		if (wtc == 'L') {
			width = 13.0f;
			cabin_width = 2.0f;
			length = 12.0f;
		}

		else if (wtc == 'H') {
			width = 61.0f;
			cabin_width = 7.0f;
			length = 64.0f;
		}

		else if (wtc == 'J') {
			width = 80.0f;
			cabin_width = 7.0f;
			length = 73.0f;
		}
	}


	width = width + float((rand() % 5) - 2);
	cabin_width = cabin_width + float((rand() % 3) - 1);
	length = length + float((rand() % 5) - 2);


	float trackHead = float(RadarTarget.GetPosition().GetReportedHeadingTrueNorth());
	float inverseTrackHead = float(fmod(trackHead + 180.0f, 360));
	float leftTrackHead = float(fmod(trackHead - 90.0f, 360));
	float rightTrackHead = float(fmod(trackHead + 90.0f, 360));

	float HalfLength = length / 2.0f;
	float HalfCabWidth = cabin_width / 2.0f;
	float HalfSpanWidth = width / 2.0f;

	// Base shape is like a deformed cross


	CPosition topMiddle = Haversine(RtPos.GetPosition(), trackHead, HalfLength);
	CPosition topLeft = Haversine(topMiddle, leftTrackHead, HalfCabWidth);
	CPosition topRight = Haversine(topMiddle, rightTrackHead, HalfCabWidth);

	CPosition bottomMiddle = Haversine(RtPos.GetPosition(), inverseTrackHead, HalfLength);
	CPosition bottomLeft = Haversine(bottomMiddle, leftTrackHead, HalfCabWidth);
	CPosition bottomRight = Haversine(bottomMiddle, rightTrackHead, HalfCabWidth);

	CPosition middleTopLeft = Haversine(topLeft, float(fmod(inverseTrackHead + 25.0f, 360)), 0.8f*HalfLength);
	CPosition middleTopRight = Haversine(topRight, float(fmod(inverseTrackHead - 25.0f, 360)), 0.8f*HalfLength);
	CPosition middleBottomLeft = Haversine(bottomLeft, float(fmod(trackHead - 15.0f, 360)), 0.8f*HalfLength);
	CPosition middleBottomRight = Haversine(bottomRight, float(fmod(trackHead + 15.0f, 360)), 0.8f*HalfLength);

	CPosition rightTop = Haversine(middleBottomRight, rightTrackHead, 0.7f*HalfSpanWidth);
	CPosition rightBottom = Haversine(rightTop, inverseTrackHead, cabin_width);

	CPosition leftTop = Haversine(middleBottomLeft, leftTrackHead, 0.7f*HalfSpanWidth);
	CPosition leftBottom = Haversine(leftTop, inverseTrackHead, cabin_width);

	CPosition basePoints[12];
	basePoints[0] = topLeft;
	basePoints[1] = middleTopLeft;
	basePoints[2] = leftTop;
	basePoints[3] = leftBottom;
	basePoints[4] = middleBottomLeft;
	basePoints[5] = bottomLeft;
	basePoints[6] = bottomRight;
	basePoints[7] = middleBottomRight;
	basePoints[8] = rightBottom;
	basePoints[9] = rightTop;
	basePoints[10] = middleTopRight;
	basePoints[11] = topRight;

	// 12 points total, so 11 from 0
	// ------

	// Random points between points of base shape

	for (int i = 0; i < 12; i++){

		CPosition newPoint, lastPoint, endPoint, startPoint;

		startPoint = basePoints[i];
		if (i == 11) endPoint = basePoints[0];
		else endPoint = basePoints[i + 1];

		double dist, rndHeading;
		dist = startPoint.DistanceTo(endPoint);

		Patatoides[RadarTarget.GetCallsign()].points[i * 7] = { startPoint.m_Latitude, startPoint.m_Longitude };
		lastPoint = startPoint;

		for (int k = 1; k < 7; k++){

			rndHeading = float(fmod(lastPoint.DirectionTo(endPoint) + (-25.0 + (rand() % 50 + 1)), 360));
			newPoint = Haversine(lastPoint, rndHeading, dist * 200);
			Patatoides[RadarTarget.GetCallsign()].points[(i * 7) + k] = { newPoint.m_Latitude, newPoint.m_Longitude };
			lastPoint = newPoint;
		}
	}
}

string CSMRRadar::GetBottomLine(const char * Callsign) {
	Logger::info(string(__FUNCSIG__));

	CFlightPlan fp = GetPlugIn()->FlightPlanSelect(Callsign);
	string to_render = "";
	if (fp.IsValid()) {
		to_render += fp.GetCallsign();

		string callsign_code = fp.GetCallsign();
		callsign_code = callsign_code.substr(0, 3);
		to_render += " (" + Callsigns->getCallsign(callsign_code) + ")";

		to_render += " (";
		to_render += fp.GetPilotName();
		to_render += "): ";
		to_render += fp.GetFlightPlanData().GetAircraftFPType();

		string span = Aircraft->getWingspan(fp.GetFlightPlanData().GetAircraftFPType());
		if (span.length() > 0) {
			to_render += " (";
			to_render += span;
			to_render += "m): ";
		}

		to_render += " ";
		
		if (fp.GetFlightPlanData().IsReceived()) {
			const char * assr = fp.GetControllerAssignedData().GetSquawk();
			const char * ssr = GetPlugIn()->RadarTargetSelect(fp.GetCallsign()).GetPosition().GetSquawk();
			if (strlen(assr) != 0 && !startsWith(ssr, assr)) {
				to_render += assr;
				to_render += ":";
				to_render += ssr;
			}
			else {
				to_render += "I:";
				to_render += ssr;
			}

			to_render += " ";
			to_render += fp.GetFlightPlanData().GetOrigin();
			to_render += "==>";
			to_render += fp.GetFlightPlanData().GetDestination();
			to_render += " (";
			to_render += fp.GetFlightPlanData().GetAlternate();
			to_render += ")";

			to_render += " at ";
			int rfl = fp.GetControllerAssignedData().GetFinalAltitude();
			string rfl_s;
			if (rfl == 0)
				rfl = fp.GetFlightPlanData().GetFinalAltitude();
			else if (rfl > GetPlugIn()->GetTransitionAltitude())
				rfl_s = "FL" + to_string(rfl / 100);
			else
				rfl_s = to_string(rfl) + "ft";

			to_render += rfl_s;
			to_render += " Route: ";
			to_render += fp.GetFlightPlanData().GetRoute();
		}
		else {
			const char* ssr = GetPlugIn()->RadarTargetSelect(fp.GetCallsign()).GetPosition().GetSquawk();
			to_render += "I:";
			to_render += ssr;
			to_render += " () ==>NOFP at 0 ft Route: ";
		}
	}

	return to_render;
}

bool CSMRRadar::OnCompileCommand(const char * sCommandLine)
{
	Logger::info(string(__FUNCSIG__));
	if (strcmp(sCommandLine, ".smr reload") == 0) {
		CurrentConfig = new CConfig(ConfigPath);
		LoadProfile(CurrentConfig->getActiveProfileName());
		return true;
	}

	return false;
}

map<string, string> CSMRRadar::GenerateTagData(CPlugIn* Plugin, CRadarTarget rt, CFlightPlan fp, bool isAcCorrelated, bool isProMode, int TransitionAltitude, bool useSpeedForGates, int sectorIndicator, string ActiveAirport)
{
	Logger::info(string(__FUNCSIG__));
	// ----
	// Tag items available
	// callsign: Callsign with freq state and comm *
	// actype: Aircraft type *
	// sctype: Aircraft type that changes for squawk error *
	// sqerror: Squawk error if there is one, or empty *
	// deprwy: Departure runway *
	// seprwy: Departure runway that changes to speed if speed > 25kts *
	// arvrwy: Arrival runway *
	// srvrwy: Speed that changes to arrival runway if speed < 25kts *
	// gate: Gate, from speed or scratchpad *
	// sate: Gate, from speed or scratchpad that changes to speed if speed > 25kts *
	// flightlevel: Flightlevel/Pressure altitude of the ac *
	// gs: Ground speed of the ac *
	// tendency: Climbing or descending symbol *
	// wake: Wake turbulance cat *
	// groundstatus: Current status *
	// ssr: the current squawk of the ac
	// asid: the assigned SID
	// ssid: a short version of the SID
	// origin: origin aerodrome
	// dest: destination aerodrome
	// scratch: Scratchpad
	// controller: Current Controller
	// asshdg: Assigned heading
	// ----

	bool IsPrimary = !rt.GetPosition().GetTransponderC();
	bool isAirborne = rt.GetPosition().GetReportedGS() > 50;

	// ----- Callsign -------
	string isVFR = IsVFR(fp, rt);
	string callsign = rt.GetCallsign();;
	string vfrCallsign = "";
	if (isVFR.length()) {
		vfrCallsign = callsign;
		callsign = isVFR;
	}
	if (fp.IsValid()) {
		if (fp.GetControllerAssignedData().GetCommunicationType() == 't' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'T' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'r' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'R' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'v' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'V')
		{
			if (fp.GetControllerAssignedData().GetCommunicationType() != 'v' &&
				fp.GetControllerAssignedData().GetCommunicationType() != 'V') {
				callsign.append("/");
				callsign += fp.GetControllerAssignedData().GetCommunicationType();
			}
		}
		else if (fp.GetFlightPlanData().GetCommunicationType() == 't' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'r' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'T' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'R')
		{
			callsign.append("/");
			callsign += fp.GetFlightPlanData().GetCommunicationType();
		}

		switch (fp.GetState()) {

		case FLIGHT_PLAN_STATE_TRANSFER_TO_ME_INITIATED:
			callsign = ">>" + callsign;
			break;

		case FLIGHT_PLAN_STATE_TRANSFER_FROM_ME_INITIATED:
			callsign = callsign + ">>";
			break;

		}
	}

	// ----- Squawk error -------
	string sqerror = "";
	const char * assr = fp.GetControllerAssignedData().GetSquawk();
	const char * ssr = rt.GetPosition().GetSquawk();
	bool has_squawk_error = false;
	if (strlen(assr) != 0 && !startsWith(ssr, assr)) {
		has_squawk_error = true;
		sqerror = "A";
		sqerror.append(assr);
	}

	// ----- Aircraft type -------
	string actype = "NoFPL";
	if (fp.IsValid() && fp.GetFlightPlanData().IsReceived())
		actype = fp.GetFlightPlanData().GetAircraftFPType();
	if (actype.size() > 4 && actype != "NoFPL")
		actype = actype.substr(0, 4);
	if (vfrCallsign.length())
		actype = vfrCallsign;
	
	// ----- Aircraft type that changes to squawk error -------
	string sctype = actype;
	if (has_squawk_error)
		sctype = sqerror;

	// ----- Groundspeed -------
	string speed = to_string(int(rt.GetPosition().GetReportedGS() / 10));

	int ass_spd = fp.GetControllerAssignedData().GetAssignedSpeed();
	if (ass_spd)
		speed += "/" + to_string(int(ass_spd / 10));

	// ----- Departure runway -------
	string deprwy = fp.GetFlightPlanData().GetDepartureRwy();
	if (deprwy.length() == 0)
		deprwy = "RWY";

	// ----- Departure runway that changes for overspeed -------
	string seprwy = deprwy;
	if (rt.GetPosition().GetReportedGS() > 25)
		seprwy = speed;

	// ----- Arrival runway -------
	string arvrwy = fp.GetFlightPlanData().GetArrivalRwy();
	if (arvrwy.length() == 0)
		arvrwy = "RWY";

	// ----- Speed that changes to arrival runway -----
	string srvrwy = speed;
	if (rt.GetPosition().GetReportedGS() < 25)
		srvrwy = arvrwy;

	// ----- Gate -------
	string gate;
	if (useSpeedForGates)
		gate = to_string(fp.GetControllerAssignedData().GetAssignedSpeed());
	else
		gate = fp.GetControllerAssignedData().GetScratchPadString();

	if (gate.size() > 4)
		gate = "NoG";
	else
		gate = gate.substr(0, 4);

	if (gate.size() == 0 || gate == "0" || !isAcCorrelated)
		gate = "NoG";

	// ----- Gate that changes to speed -------
	string sate = gate;
	if (rt.GetPosition().GetReportedGS() > 25)
		sate = speed;

	// ----- Scratchpad -------
	string scratch;
	scratch = fp.GetControllerAssignedData().GetScratchPadString();
	if (strlen(fp.GetControllerAssignedData().GetScratchPadString()) == 0)
		scratch = EmptyScratchpad;

	// ----- Flightlevel -------
	int fl = rt.GetPosition().GetFlightLevel();
	int padding = 5;
	string pfls = "";
	if (fl <= TransitionAltitude) {
		fl = rt.GetPosition().GetPressureAltitude();
		pfls = "A";
		padding = 4;
	}
	string flightlevel = (pfls + padWithZeros(padding, fl)).substr(0, 3);

	int clrd = fp.GetClearedAltitude();
	if (clrd && clrd != fp.GetFinalAltitude())
	{
		if (clrd < 10000) {
			if (clrd % 1000)
				flightlevel += "/" + padWithZeros(4, clrd / 10).substr(0, 3);
			else
				flightlevel += "/" + padWithZeros(4, clrd / 10).substr(0, 2);
		}
		else
			flightlevel += "/" + padWithZeros(4, clrd).substr(0, 2);
	}
	// ----- Tendency -------
	string tendency = "";
	int delta_fl = rt.GetVerticalSpeed() / 200;

	if (delta_fl < -2)
		tendency = "|";
	else if (delta_fl > 2)
		tendency = "^";

	if (delta_fl < 0)
		delta_fl *= -1;

	if (delta_fl <= 2)
		NULL;
	else if (delta_fl < 10)
	{
		tendency += "0" + to_string(delta_fl);
	}
	else {
		tendency += to_string(delta_fl);
	}


	// ----- Wake cat -------
	string wake = "?";
	if (fp.IsValid() && isAcCorrelated) {
		wake = "";
		wake += fp.GetFlightPlanData().GetAircraftWtc();
	}

	// ----- SSR -------
	string tssr = "";
	if (rt.IsValid())
	{
		tssr = rt.GetPosition().GetSquawk();
	}

	// ----- SID -------
	string dep = "SID";
	if (fp.IsValid() && isAcCorrelated)
	{
		dep = fp.GetFlightPlanData().GetSidName();
	}

	// ----- Short SID -------
	string ssid = dep;
	if (fp.IsValid() && ssid.size() > 5 && isAcCorrelated)
	{
		ssid = dep.substr(0, 3);
		ssid += dep.substr(dep.size() - 2, dep.size());
	}

	// ------- Origin aerodrome -------
	string origin = "????";
	if (isAcCorrelated)
	{
		origin = fp.GetFlightPlanData().GetOrigin();
	}

	// ------- Destination aerodrome -------
	string dest = "????";
	if (isAcCorrelated)
	{
		dest = fp.GetFlightPlanData().GetDestination();
	}

	// ----- GSTAT -------
	string gstat = "STS";
	if (fp.IsValid() && isAcCorrelated) {
		if (strlen(fp.GetGroundState()) != 0)
			gstat = fp.GetGroundState();
	}

	// ----- Current Controller -------
	string controller = fp.GetTrackingControllerId();
	string controller_next = fp.GetCoordinatedNextController();

	if (controller.length() == 0 && sectorIndicator == 0) {
		controller = "*";
	}
	else if (sectorIndicator == 1) {
		if (controller_next.length() == 0 || controller_next == "UNICOM")
			controller = "-";
		else {
			string next = Plugin->ControllerSelect(fp.GetCoordinatedNextController()).GetPositionId();
			controller = "+" + next;
		}
	}
	else if (sectorIndicator == 2){
		if (controller_next.length() == 0 || controller_next == "UNICOM")
			controller = "-122.80";
		else {
			char tmp[10] = {};
			double tt1 = Plugin->ControllerSelect(fp.GetCoordinatedNextController()).GetPrimaryFrequency();
			_gcvt_s(tmp, 10, Plugin->ControllerSelect(fp.GetCoordinatedNextController()).GetPrimaryFrequency(), 6);
			string next = tmp;
			controller = "+" + next;
		}
	}

	// ----- ASSHDG -------
	string asshdg = "";
	int ass = fp.GetControllerAssignedData().GetAssignedHeading();
	if (ass > 0)
		asshdg = "H" + to_string(int(ass / 10));

	// ----- UK Controller Plugin / Assigned Stand -------
	string uk_stand;
	uk_stand = fp.GetControllerAssignedData().GetFlightStripAnnotation(3);

	// ----- Ground Radar Plugin / Assigned Stand -------
	string grp_stand;
	grp_stand = fp.GetControllerAssignedData().GetFlightStripAnnotation(6);
	grp_stand = grp_stand.erase(0, 2);
	grp_stand = grp_stand.substr(0, grp_stand.size() - 2);

	// ----- Generating the replacing map -----
	map<string, string> TagReplacingMap;

	// System ID for uncorrelated
	TagReplacingMap["systemid"] = "T:";
	string tpss = rt.GetSystemID();
	TagReplacingMap["systemid"].append(tpss.substr(1, 6));

	// Pro mode data here
	if (isProMode)
	{

		if (isAirborne && !isAcCorrelated)
		{
			callsign = tssr;
		}

		if (!isAcCorrelated)
		{
			actype = "NoFPL";
		}

		// Is a primary target
		if (isAirborne && !isAcCorrelated && IsPrimary)
		{
			flightlevel = "NoALT";
			tendency = "?";
			speed = to_string(rt.GetGS());
		}

		if (isAirborne && !isAcCorrelated && IsPrimary)
		{
			callsign = TagReplacingMap["systemid"];
		}
	}

	TagReplacingMap["callsign"] = callsign;
	TagReplacingMap["actype"] = actype;
	TagReplacingMap["sctype"] = sctype;
	TagReplacingMap["sqerror"] = sqerror;
	TagReplacingMap["deprwy"] = deprwy;
	TagReplacingMap["seprwy"] = seprwy;
	TagReplacingMap["arvrwy"] = arvrwy;
	TagReplacingMap["srvrwy"] = srvrwy;
	TagReplacingMap["gate"] = gate;
	TagReplacingMap["sate"] = sate;
	TagReplacingMap["scratch"] = scratch;
	TagReplacingMap["flightlevel"] = flightlevel;
	TagReplacingMap["gs"] = speed;
	TagReplacingMap["tendency"] = tendency;
	TagReplacingMap["wake"] = wake;
	TagReplacingMap["ssr"] = tssr;
	TagReplacingMap["asid"] = dep;
	TagReplacingMap["ssid"] = ssid;
	TagReplacingMap["origin"] = origin;
	TagReplacingMap["dest"] = dest;
	TagReplacingMap["groundstatus"] = gstat;
	TagReplacingMap["controller"] = controller;
	TagReplacingMap["asshdg"] = asshdg;
	TagReplacingMap["uk_stand"] = uk_stand;

	TagReplacingMap["grp_stand"] = grp_stand;

	return TagReplacingMap;
}

void CSMRRadar::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	Logger::info(string(__FUNCSIG__));
	string callsign = string(FlightPlan.GetCallsign());

	for (multimap<string, string>::iterator itr = DistanceTools.begin(); itr != DistanceTools.end(); ++itr) {
		if (itr->first == callsign || itr->second == callsign)
			itr = DistanceTools.erase(itr);
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_SETCURSOR:
		SetCursor(smrCursor);
		return true;
	default:
		return CallWindowProc(gSourceProc, hwnd, uMsg, wParam, lParam);
	}
}

void CSMRRadar::OnRefresh(HDC hDC, int Phase)
{
	Logger::info(string(__FUNCSIG__));
	// Changing the mouse cursor
	if (initCursor)
	{
		if (customCursor) {
			if (customCursors["default"].length())
				smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), customCursors["default"].c_str(), IMAGE_CURSOR, 0, 0, LR_SHARED | LR_LOADFROMFILE));
			else
				smrCursor = CopyCursor((HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0, LR_SHARED));
			// This got broken because of threading as far as I can tell
			// The cursor does change for some milliseconds but gets reset almost instantly by external MFC code

		}
		else {
			smrCursor = (HCURSOR)::LoadCursor(NULL, IDC_ARROW);
		}

		if (smrCursor != nullptr && gSourceProc == nullptr)
		{		
			pluginWindow = GetActiveWindow();
			gSourceProc = (WNDPROC)SetWindowLong(pluginWindow, GWL_WNDPROC, (LONG)WindowProc);
		}
		initCursor = false;
	}

	if (Phase == REFRESH_PHASE_AFTER_LISTS) {
		Logger::info("Phase == REFRESH_PHASE_AFTER_LISTS");
		if (!ColorSettingsDay) {
			// Creating the gdi+ graphics
			Graphics graphics(hDC);
			graphics.SetPageUnit(Gdiplus::UnitPixel);

			graphics.SetSmoothingMode(SmoothingModeAntiAlias);

			SolidBrush AlphaBrush(Color(CurrentConfig->getActiveProfile()["filters"]["night_alpha_setting"].GetInt(), 0, 0, 0));

			CRect RadarArea(GetRadarArea());
			RadarArea.top = RadarArea.top - 1;
			RadarArea.bottom = GetChatArea().bottom;

			graphics.FillRectangle(&AlphaBrush, CopyRect(CRect(RadarArea)));

			graphics.ReleaseHDC(hDC);
		}

		Logger::info("break Phase == REFRESH_PHASE_AFTER_LISTS");
		return;
	}

	if (Phase != REFRESH_PHASE_BEFORE_TAGS)
		return;

	Logger::info("Phase != REFRESH_PHASE_BEFORE_TAGS");

	struct Utils {
		static RECT GetAreaFromText(CDC * dc, string text, POINT Pos) {
			RECT Area = { Pos.x, Pos.y, Pos.x + dc->GetTextExtent(text.c_str()).cx, Pos.y + dc->GetTextExtent(text.c_str()).cy };
			return Area;
		}
		static string getEnumString(TagTypes type) {
			if (type == TagTypes::Departure)
				return "departure";
			else if (type == TagTypes::Arrival)
				return "arrival";
			else if (type == TagTypes::Uncorrelated)
				return "uncorrelated";
			else if (type == TagTypes::VFR)
				return "vfr";
			return "airborne";
		}
	};

	// Timer each seconds
	clock_final = clock() - clock_init;
	double delta_t = (double)clock_final / ((double)CLOCKS_PER_SEC);
	if (delta_t >= 1) {
		clock_init = clock();
		BLINK = !BLINK;
		RefreshAirportActivity();
	}

	if (!QDMenabled && !QDMSelectEnabled)
	{
		POINT p;
		if (GetCursorPos(&p)) {
			if (ScreenToClient(GetActiveWindow(), &p)) {
				mouseLocation = p;
			}
		}
	}

	Logger::info("Graphics set up");
	CDC dc;
	dc.Attach(hDC);

	// Creating the gdi+ graphics
	Graphics graphics(hDC);
	graphics.SetPageUnit(Gdiplus::UnitPixel);

	graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	RECT RadarArea = GetRadarArea();
	RECT ChatArea = GetChatArea();
	RadarArea.bottom = ChatArea.top;

	AirportPositions.clear();


	CSectorElement apt;
	for (apt = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT);
		apt.IsValid();
		apt = GetPlugIn()->SectorFileElementSelectNext(apt, SECTOR_ELEMENT_AIRPORT))
	{
		CPosition Pos;
		apt.GetPosition(&Pos, 0);
		AirportPositions[string(apt.GetName())] = Pos;
	}

	RimcasInstance->RunwayAreas.clear();

	if (QDMSelectEnabled || QDMenabled)
	{
		CRect R(GetRadarArea());
		R.top += 20;
		R.bottom = GetChatArea().top;

		R.NormalizeRect();
		AddScreenObject(DRAWING_BACKGROUND_CLICK, "", R, false, "");
	}

	Logger::info("Runway loop");
	CSectorElement rwy;
	for (rwy = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
		rwy.IsValid();
		rwy = GetPlugIn()->SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (startsWith(getActiveAirport().c_str(), rwy.GetAirportName())) {

			CPosition Left;
			rwy.GetPosition(&Left, 1);
			CPosition Right;
			rwy.GetPosition(&Right, 0);

			string runway_name = rwy.GetRunwayName(0);
			string runway_name2 = rwy.GetRunwayName(1);

			double bearing1 = TrueBearing(Left, Right);
			double bearing2 = TrueBearing(Right, Left);

			const Value& CustomMap = CurrentConfig->getAirportMapIfAny(getActiveAirport());

			vector<CPosition> def = RimcasInstance->GetRunwayArea(Left, Right);
			RimcasInstance->AddRunwayArea(this, runway_name, runway_name2, def);

			string RwName = runway_name + " / " + runway_name2;

			if (RimcasInstance->ClosedRunway.find(RwName) != RimcasInstance->ClosedRunway.end()) {
				if (RimcasInstance->ClosedRunway[RwName]) {

					CPen RedPen(PS_SOLID, 2, RGB(150, 0, 0));
					CPen * oldPen = dc.SelectObject(&RedPen);

					if (CurrentConfig->isCustomRunwayAvail(getActiveAirport(), runway_name, runway_name2)) {
						const Value& Runways = CustomMap["runways"];

						if (Runways.IsArray()) {
							for (SizeType i = 0; i < Runways.Size(); i++) {
								if (startsWith(runway_name.c_str(), Runways[i]["runway_name"].GetString()) ||
									startsWith(runway_name2.c_str(), Runways[i]["runway_name"].GetString())) {

									string path_name = "path";

									if (isLVP)
										path_name = "path_lvp";

									const Value& Path = Runways[i][path_name.c_str()];

									PointF lpPoints[5000];

									int k = 1;
									int l = 0;
									for (SizeType w = 0; w < Path.Size(); w++) {
										CPosition position;
										position.LoadFromStrings(Path[w][static_cast<SizeType>(1)].GetString(), Path[w][static_cast<SizeType>(0)].GetString());

										POINT cv = ConvertCoordFromPositionToPixel(position);
										lpPoints[l] = { REAL(cv.x), REAL(cv.y) };

										k++;
										l++;
									}

									graphics.FillPolygon(&SolidBrush(Color(150, 0, 0)), lpPoints, k - 1);

									break;
								}
							}
						}

					}
					else {
						vector<CPosition> Area = RimcasInstance->GetRunwayArea(Left, Right);

						PointF lpPoints[5000];
						int w = 0;
						for(auto &Point : Area)
						{
							POINT toDraw = ConvertCoordFromPositionToPixel(Point);

							lpPoints[w] = { REAL(toDraw.x), REAL(toDraw.y) };
							w++;
						}

						graphics.FillPolygon(&SolidBrush(Color(150, 0, 0)), lpPoints, w);
					}

					dc.SelectObject(oldPen);
				}
			}
		}
	}

	RimcasInstance->OnRefreshBegin(isLVP);

#pragma region symbols
	// Drawing the symbols
	Logger::info("Symbols loop");
	EuroScopePlugIn::CRadarTarget rt;
	for (rt = GetPlugIn()->RadarTargetSelectFirst();
		rt.IsValid();
		rt = GetPlugIn()->RadarTargetSelectNext(rt))
	{
		if (!rt.IsValid() || !rt.GetPosition().IsValid())
			continue;
		
		int reportedGs = rt.GetPosition().GetReportedGS();
		int radarRange = CurrentConfig->getActiveProfile()["filters"]["radar_range_nm"].GetInt();
		int altitudeFilter_above = CurrentConfig->getActiveProfile()["filters"]["hide_above_alt"].GetInt();
		int altitudeFilter_below = CurrentConfig->getActiveProfile()["filters"]["hide_below_alt"].GetInt();
		int speedFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_spd"].GetInt();
		bool isAcDisplayed = isVisible(rt);

		if (!isAcDisplayed)
			continue;

		RimcasInstance->OnRefresh(rt, this, IsCorrelated(GetPlugIn()->FlightPlanSelect(rt.GetCallsign()), rt));

		CRadarTargetPositionData RtPos = rt.GetPosition();

		POINT acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());

		if (rt.GetGS() > 5) {
			POINT oldacPosPix;
			CRadarTargetPositionData pAcPos = rt.GetPosition();

			for (int i = 1; i <= 2; i++) {
				oldacPosPix = ConvertCoordFromPositionToPixel(pAcPos.GetPosition());
				pAcPos = rt.GetPreviousPosition(pAcPos);
				acPosPix = ConvertCoordFromPositionToPixel(pAcPos.GetPosition());

				if (i == 1 && !Patatoides[rt.GetCallsign()].History_one_points.empty() && Afterglow && CurrentConfig->getActiveProfile()["targets"]["show_primary_target"].GetBool()) {
					SolidBrush H_Brush(ColorManager->get_corrected_color("afterglow",
						CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["targets"]["history_one_color"])));

					PointF lpPoints[100];
					for (unsigned int i1 = 0; i1 < Patatoides[rt.GetCallsign()].History_one_points.size(); i1++)
					{
						CPosition pos;
						pos.m_Latitude = Patatoides[rt.GetCallsign()].History_one_points[i1].x;
						pos.m_Longitude = Patatoides[rt.GetCallsign()].History_one_points[i1].y;

						lpPoints[i1] = { REAL(ConvertCoordFromPositionToPixel(pos).x), REAL(ConvertCoordFromPositionToPixel(pos).y) };
					}
					graphics.FillPolygon(&H_Brush, lpPoints, Patatoides[rt.GetCallsign()].History_one_points.size());
				}

				if (i != 2) {
					if (!Patatoides[rt.GetCallsign()].History_two_points.empty() && Afterglow && CurrentConfig->getActiveProfile()["targets"]["show_primary_target"].GetBool()) {
						SolidBrush H_Brush(ColorManager->get_corrected_color("afterglow",
							CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["targets"]["history_two_color"])));

						PointF lpPoints[100];
						for (unsigned int i1 = 0; i1 < Patatoides[rt.GetCallsign()].History_two_points.size(); i1++)
						{
							CPosition pos;
							pos.m_Latitude = Patatoides[rt.GetCallsign()].History_two_points[i1].x;
							pos.m_Longitude = Patatoides[rt.GetCallsign()].History_two_points[i1].y;

							lpPoints[i1] = { REAL(ConvertCoordFromPositionToPixel(pos).x), REAL(ConvertCoordFromPositionToPixel(pos).y) };
						}
						graphics.FillPolygon(&H_Brush, lpPoints, Patatoides[rt.GetCallsign()].History_two_points.size());
					}
				}

				if (i == 2 && !Patatoides[rt.GetCallsign()].History_three_points.empty() && Afterglow && CurrentConfig->getActiveProfile()["targets"]["show_primary_target"].GetBool()) {
					SolidBrush H_Brush(ColorManager->get_corrected_color("afterglow",
						CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["targets"]["history_three_color"])));

					PointF lpPoints[100];
					for (unsigned int i1 = 0; i1 < Patatoides[rt.GetCallsign()].History_three_points.size(); i1++)
					{
						CPosition pos;
						pos.m_Latitude = Patatoides[rt.GetCallsign()].History_three_points[i1].x;
						pos.m_Longitude = Patatoides[rt.GetCallsign()].History_three_points[i1].y;

						lpPoints[i1] = { REAL(ConvertCoordFromPositionToPixel(pos).x), REAL(ConvertCoordFromPositionToPixel(pos).y) };
					}
					graphics.FillPolygon(&H_Brush, lpPoints, Patatoides[rt.GetCallsign()].History_three_points.size());
				}
			}

			int TrailNumber = Trail_Gnd;
			if (reportedGs > 50)
				TrailNumber = Trail_App;

			CRadarTargetPositionData previousPos = rt.GetPreviousPosition(rt.GetPosition());
			for (int j = 1; j <= TrailNumber; j++) {
				POINT pCoord = ConvertCoordFromPositionToPixel(previousPos.GetPosition());

				graphics.FillRectangle(&SolidBrush(ColorManager->get_corrected_color("symbol", Gdiplus::Color::White)),
					pCoord.x - 1, pCoord.y - 1, 2, 2);

				previousPos = rt.GetPreviousPosition(previousPos);
			}
		}


		if (CurrentConfig->getActiveProfile()["targets"]["show_primary_target"].GetBool()) {

			SolidBrush H_Brush(ColorManager->get_corrected_color("afterglow",
				CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["targets"]["target_color"])));

			PointF lpPoints[100];
			for (unsigned int i = 0; i < Patatoides[rt.GetCallsign()].points.size(); i++)
			{
				CPosition pos;
				pos.m_Latitude = Patatoides[rt.GetCallsign()].points[i].x;
				pos.m_Longitude = Patatoides[rt.GetCallsign()].points[i].y;

				lpPoints[i] = { REAL(ConvertCoordFromPositionToPixel(pos).x), REAL(ConvertCoordFromPositionToPixel(pos).y) };
			}

			graphics.FillPolygon(&H_Brush, lpPoints, Patatoides[rt.GetCallsign()].points.size());
		}
		acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());

		bool AcisCorrelated = IsCorrelated(GetPlugIn()->FlightPlanSelect(rt.GetCallsign()), rt);

		if (!AcisCorrelated && reportedGs < 1 && !ReleaseInProgress && !AcquireInProgress)
			continue;

		
		CFlightPlan fp = GetPlugIn()->FlightPlanSelect(rt.GetCallsign());
		CPen qTrailPen(PS_SOLID, 1, ColorManager->get_corrected_color("symbol", Gdiplus::Color::White).ToCOLORREF());

		CPen* pqOrigPen = dc.SelectObject(&qTrailPen);

		if (RtPos.GetTransponderC()) {
			dc.MoveTo({ acPosPix.x, acPosPix.y - 6 });
			dc.LineTo({ acPosPix.x - 6, acPosPix.y });
			dc.LineTo({ acPosPix.x, acPosPix.y + 6 });
			dc.LineTo({ acPosPix.x + 6, acPosPix.y });
			dc.LineTo({ acPosPix.x, acPosPix.y - 6 });
		}
		else {
			dc.MoveTo(acPosPix.x, acPosPix.y);
			dc.LineTo(acPosPix.x - 4, acPosPix.y - 4);
			dc.MoveTo(acPosPix.x, acPosPix.y);
			dc.LineTo(acPosPix.x + 4, acPosPix.y - 4);
			dc.MoveTo(acPosPix.x, acPosPix.y);
			dc.LineTo(acPosPix.x - 4, acPosPix.y + 4);
			dc.MoveTo(acPosPix.x, acPosPix.y);
			dc.LineTo(acPosPix.x + 4, acPosPix.y + 4);
		}

		// Predicted Track Line
		// It starts 20 seconds away from the ac
		if (reportedGs > 50)
		{
			double d = double(rt.GetPosition().GetReportedGS()*0.514444) * 10;
			CPosition AwayBase = BetterHarversine(rt.GetPosition().GetPosition(), rt.GetTrackHeading(), d);

			d = double(rt.GetPosition().GetReportedGS()*0.514444) * (PredictedLength * 60) - 10;
			CPosition PredictedEnd = BetterHarversine(AwayBase, rt.GetTrackHeading(), d);

			dc.MoveTo(ConvertCoordFromPositionToPixel(AwayBase));
			dc.LineTo(ConvertCoordFromPositionToPixel(PredictedEnd));
		}

		if (mouseWithin({ acPosPix.x - 5, acPosPix.y - 5, acPosPix.x + 5, acPosPix.y + 5 })) {
			dc.MoveTo(acPosPix.x, acPosPix.y - 8);
			dc.LineTo(acPosPix.x - 6, acPosPix.y - 12);
			dc.MoveTo(acPosPix.x, acPosPix.y - 8);
			dc.LineTo(acPosPix.x + 6, acPosPix.y - 12);

			dc.MoveTo(acPosPix.x, acPosPix.y + 8);
			dc.LineTo(acPosPix.x - 6, acPosPix.y + 12);
			dc.MoveTo(acPosPix.x, acPosPix.y + 8);
			dc.LineTo(acPosPix.x + 6, acPosPix.y + 12);

			dc.MoveTo(acPosPix.x - 8, acPosPix.y );
			dc.LineTo(acPosPix.x - 12, acPosPix.y -6);
			dc.MoveTo(acPosPix.x - 8, acPosPix.y);
			dc.LineTo(acPosPix.x - 12 , acPosPix.y + 6);

			dc.MoveTo(acPosPix.x + 8, acPosPix.y);
			dc.LineTo(acPosPix.x + 12, acPosPix.y - 6);
			dc.MoveTo(acPosPix.x + 8, acPosPix.y);
			dc.LineTo(acPosPix.x + 12, acPosPix.y + 6);
		}

		AddScreenObject(DRAWING_AC_SYMBOL, rt.GetCallsign(), { acPosPix.x - 5, acPosPix.y - 5, acPosPix.x + 5, acPosPix.y + 5 }, false, GetBottomLine(rt.GetCallsign()).c_str());

		dc.SelectObject(pqOrigPen);
	}

#pragma endregion Drawing of the symbols

	TimePopupData.clear();
	AcOnRunway.clear();
	ColorAC.clear();
	tagAreas.clear();

	RimcasInstance->OnRefreshEnd(this, CurrentConfig->getActiveProfile()["rimcas"]["rimcas_stage_two_speed_threshold"].GetInt());

	graphics.SetSmoothingMode(SmoothingModeDefault);

#pragma region tags
	// Drawing the Tags
	Logger::info("Tags loop");
	for (rt = GetPlugIn()->RadarTargetSelectFirst();
		rt.IsValid();
		rt = GetPlugIn()->RadarTargetSelectNext(rt))
	{
		if (!rt.IsValid())
			continue;

		CRadarTargetPositionData RtPos = rt.GetPosition();
		POINT acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());
		CFlightPlan fp = GetPlugIn()->FlightPlanSelect(rt.GetCallsign());
		int reportedGs = RtPos.GetReportedGS();

		// Filtering the targets
		int radarRange = CurrentConfig->getActiveProfile()["filters"]["radar_range_nm"].GetInt();
		int altitudeFilter_above = CurrentConfig->getActiveProfile()["filters"]["hide_above_alt"].GetInt();
		int altitudeFilter_below = CurrentConfig->getActiveProfile()["filters"]["hide_below_alt"].GetInt();
		int speedFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_spd"].GetInt();
		bool show_on_rwy = CurrentConfig->getActiveProfile()["filters"]["show_on_rwy"].GetBool();
		bool isAcDisplayed = isVisible(rt);

		bool AcisCorrelated = IsCorrelated(fp, rt);

		if (!AcisCorrelated && reportedGs < 3)
			isAcDisplayed = false;

		if (find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
			isAcDisplayed = false;

		if (show_on_rwy && RimcasInstance->isAcOnRunway(rt.GetCallsign()))
			isAcDisplayed = true;

		if (!isAcDisplayed)
			continue;

		// Getting the tag center/offset
		POINT TagCenter;
		map<string, POINT>::iterator it = TagsOffsets.find(rt.GetCallsign());
		if (it != TagsOffsets.end()) {
			TagCenter = { acPosPix.x + it->second.x, acPosPix.y + it->second.y };
		}
		else {
			// Use angle:
			if (TagAngles.find(rt.GetCallsign()) == TagAngles.end())
				TagAngles[rt.GetCallsign()] = 270.0f;

			int length = LeaderLineDefaultlength;
			if (TagLeaderLineLength.find(rt.GetCallsign()) != TagLeaderLineLength.end())
				length = TagLeaderLineLength[rt.GetCallsign()];

			TagCenter.x = long(acPosPix.x + float(length * cos(DegToRad(TagAngles[rt.GetCallsign()]))));
			TagCenter.y = long(acPosPix.y + float(length * sin(DegToRad(TagAngles[rt.GetCallsign()]))));
		}

		TagTypes TagType = TagTypes::Uncorrelated;
		TagTypes ColorTagType = TagTypes::Uncorrelated;

		if (fp.IsValid() && isActiveAirport(fp.GetFlightPlanData().GetOrigin())) {
			TagType = TagTypes::Departure;
			ColorTagType = TagTypes::Departure;
		}
		else if (fp.IsValid() && isActiveAirport(fp.GetFlightPlanData().GetDestination())) {
			TagType = TagTypes::Arrival;
			ColorTagType = TagTypes::Arrival;
		}


		if (reportedGs > 50) {
			TagType = TagTypes::Airborne;

			// Is "use_departure_arrival_coloring" enabled? if not, then use the airborne colors
			bool useDepArrColors = CurrentConfig->getActiveProfile()["labels"]["airborne"]["use_departure_arrival_coloring"].GetBool();
			if (!useDepArrColors) {
				ColorTagType = TagTypes::Airborne;
			}
			
			if (IsVFR(fp, rt).length()) {
				TagType = TagTypes::VFR;
				ColorTagType = TagTypes::VFR;
			}
		}

		if (!AcisCorrelated && reportedGs >= 3)
		{
			TagType = TagTypes::Uncorrelated;
			ColorTagType = TagTypes::Uncorrelated;
		}

		map<string, string> TagReplacingMap = GenerateTagData(GetPlugIn(), rt, fp, IsCorrelated(fp, rt), CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["enable"].GetBool(), GetPlugIn()->GetTransitionAltitude(), CurrentConfig->getActiveProfile()["labels"]["use_aspeed_for_gate"].GetBool(), sectorIndicator, getActiveAirport());

		// Special for Circuit Traffic
		if (reportedGs <= 50 && TagReplacingMap["origin"] == TagReplacingMap["dest"] &&
			isActiveAirport(TagReplacingMap["origin"]) && is_digits(TagReplacingMap["gate"])) {
			TagType = TagTypes::Arrival;
			ColorTagType = TagTypes::Arrival;
		}
		
		if (reportedGs <= 50 && IsVFR(fp, rt).length()) {
			TagType = TagTypes::VFR;
			ColorTagType = TagTypes::VFR;
		}

		// ----- Generating the clickable map -----
		map<string, int> TagClickableMap;
		TagClickableMap[TagReplacingMap["callsign"]] = TAG_CITEM_CALLSIGN;
		TagClickableMap[TagReplacingMap["actype"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["sctype"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["sqerror"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["deprwy"]] = TAG_CITEM_RWY;
		TagClickableMap[TagReplacingMap["seprwy"]] = TAG_CITEM_RWY;
		TagClickableMap[TagReplacingMap["arvrwy"]] = TAG_CITEM_RWY;
		TagClickableMap[TagReplacingMap["srvrwy"]] = TAG_CITEM_RWY;
		TagClickableMap[TagReplacingMap["gate"]] = TAG_CITEM_GATE;
		TagClickableMap[TagReplacingMap["sate"]] = TAG_CITEM_GATE;
		TagClickableMap[TagReplacingMap["scratch"]] = TAG_CITEM_SCRATCH;
		TagClickableMap[TagReplacingMap["flightlevel"]] = TAG_CITEM_FL;
		TagClickableMap[TagReplacingMap["gs"]] = TAG_CITEM_GS;
		TagClickableMap[TagReplacingMap["tendency"]] = TAG_CITEM_NO;
		TagClickableMap[TagReplacingMap["wake"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["ssr"]] = TAG_CITEM_SSR;
		TagClickableMap[TagReplacingMap["asid"]] = TagClickableMap[TagReplacingMap["ssid"]] = TAG_CITEM_SID;
		TagClickableMap[TagReplacingMap["origin"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["dest"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["systemid"]] = TAG_CITEM_NO;
		TagClickableMap[TagReplacingMap["groundstatus"]] = TAG_CITEM_GROUNDSTATUS;
		TagClickableMap[TagReplacingMap["controller"]] = TAG_CITEM_CONTROLLER;
		TagClickableMap[TagReplacingMap["asshdg"]] = TAG_CITEM_ASSHDG;
		TagClickableMap[TagReplacingMap["uk_stand"]] = TAG_CITEM_UKSTAND;

		TagClickableMap[TagReplacingMap["grp_stand"]] = TAG_CITEM_GRPSTAND;

		//
		// ----- Now the hard part, drawing (using gdi+) -------
		//

		// First we need to figure out the tag size
		int TagWidth = 0, TagHeight = 0;
		RectF mesureRect;
		graphics.MeasureString(L" ", wcslen(L" "), customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);
		int blankWidth = (int)mesureRect.GetRight();

		mesureRect = RectF(0, 0, 0, 0);
		graphics.MeasureString(L"AZERTYUIOPQSDFGHJKLMWXCVBN", wcslen(L"AZERTYUIOPQSDFGHJKLMWXCVBN"),
			customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);
		int oneLineHeight = (int)mesureRect.GetBottom();

		const Value& LabelsSettings = CurrentConfig->getActiveProfile()["labels"];
		const Value& LabelLines_Normal = LabelsSettings[Utils::getEnumString(TagType).c_str()]["definition"];
		const Value& LabelLines_Detailed = LabelsSettings[Utils::getEnumString(TagType).c_str()]["definition_detailed"];
		vector<vector<string>> ReplacedLabelLines;

		if (!LabelLines_Normal.IsArray())
			return;
		
		bool isDetailed = false;
		if (LabelLines_Detailed.IsArray()) {
			if (TagsDetailed.find(rt.GetCallsign()) != TagsDetailed.end()) {
				isDetailed = true;
			}
		}

		const Value& LabelLines = isDetailed ? LabelLines_Detailed : LabelLines_Normal;
		for (unsigned int i = 0; i < LabelLines.Size(); i++)
		{
			const Value& line = LabelLines[i];
			vector<string> lineStringArray;

			// Empty Scratchpad special
			if (line.Size() == unsigned(1) && strcmp(line[unsigned(0)].GetString(), "scratch") == 0) {
				string element = line[unsigned(0)].GetString();

				for (auto& kv : TagReplacingMap)
					replaceAll(element, kv.first, kv.second);

				if (strcmp(element.c_str(), EmptyScratchpad.c_str()) == 0)
					continue;
				if (TagType == CSMRRadar::TagTypes::Arrival && strcmp(element.c_str(), TagReplacingMap["gate"].c_str()) == 0)
					continue;
			}

			// Adds one line height
			TagHeight += oneLineHeight;

			int TempTagWidth = 0;

			for(unsigned int j = 0; j < line.Size(); j++)
			{
				mesureRect = RectF(0, 0, 0, 0);
				string element = line[j].GetString();

				for (auto& kv : TagReplacingMap)
					replaceAll(element, kv.first, kv.second);

				lineStringArray.push_back(element);

				wstring wstr = wstring(element.begin(), element.end());
				graphics.MeasureString(wstr.c_str(), wcslen(wstr.c_str()),
					customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);

				TempTagWidth += (int) mesureRect.GetRight();

				if (j != line.Size() - 1)
					TempTagWidth += (int) blankWidth;
			}

			TagWidth = max(TagWidth, TempTagWidth);

			ReplacedLabelLines.push_back(lineStringArray);
		}
		TagHeight = TagHeight - 2;

		Color definedBackgroundColor = CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["background_color"]);
		
		if (ColorTagType == TagTypes::Departure) {
			if (!TagReplacingMap["asid"].empty() && 
				isActiveAirport(TagReplacingMap["origin"].c_str()) &&
				CurrentConfig->isSidColorAvail(TagReplacingMap["asid"], TagReplacingMap["origin"].c_str())) {
				definedBackgroundColor = CurrentConfig->getSidColor(TagReplacingMap["asid"], TagReplacingMap["origin"].c_str());
			}
			if (fp.GetFlightPlanData().GetPlanType()[0] == 'I' && TagReplacingMap["asid"].empty() && LabelsSettings[Utils::getEnumString(ColorTagType).c_str()].HasMember("nosid_color")) {
				definedBackgroundColor = CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["nosid_color"]);
			}
		}
			if (TagReplacingMap["actype"] == "NoFPL" && LabelsSettings[Utils::getEnumString(ColorTagType).c_str()].HasMember("nofpl_color")) {
				definedBackgroundColor = CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["nofpl_color"]);
		}
		if (TagReplacingMap["actype"] == "NoFPL" && LabelsSettings[Utils::getEnumString(ColorTagType).c_str()].HasMember("nofpl_color")) {
			definedBackgroundColor = CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["nofpl_color"]);
		}

		Color TagBackgroundColor = RimcasInstance->GetAircraftColor(rt.GetCallsign(),
			definedBackgroundColor,
			CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["background_color_on_runway"]),
			CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_one"]),
			CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_two"]));

		// We need to figure out if the tag color changes according to RIMCAS alerts, or not
		bool rimcasLabelOnly = CurrentConfig->getActiveProfile()["rimcas"]["rimcas_label_only"].GetBool();

		if (rimcasLabelOnly)
			TagBackgroundColor = RimcasInstance->GetAircraftColor(rt.GetCallsign(),
			definedBackgroundColor,
			CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["background_color_on_runway"]));

		TagBackgroundColor = ColorManager->get_corrected_color("label", TagBackgroundColor);

		// Drawing the tag background
		CRect TagBackgroundRect(TagCenter.x - (TagWidth / 2), TagCenter.y - (TagHeight / 2), TagCenter.x + (TagWidth / 2), TagCenter.y + (TagHeight / 2));
		SolidBrush TagBackgroundBrush(TagBackgroundColor);
		graphics.FillRectangle(&TagBackgroundBrush, CopyRect(TagBackgroundRect));
		if (mouseWithin(TagBackgroundRect) || IsTagBeingDragged(rt.GetCallsign()))
		{
			Pen pw(ColorManager->get_corrected_color("label", Color::White));
			graphics.DrawRectangle(&pw, CopyRect(TagBackgroundRect));
		}
		// Border, when selected
		else if (strcmp(GetPlugIn()->RadarTargetSelectASEL().GetCallsign(), rt.GetCallsign()) == 0) {
			Pen pw(ColorManager->get_corrected_color("label", Color::DarkGray));
			graphics.DrawRectangle(&pw, CopyRect(TagBackgroundRect));
		}

		// Drawing the tag text
		SolidBrush FontColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["text_color"])));
		SolidBrush FontColorUnrelated(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings[Utils::getEnumString(ColorTagType).c_str()]["text_color_unrelated"])));
		SolidBrush SquawkErrorColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["squawk_error_color"])));
		SolidBrush RimcasTextColor(CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["alert_text_color"]));
		SolidBrush GroundPushColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["groundstatus_colors"]["push"])));
		SolidBrush GroundTaxiColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["groundstatus_colors"]["taxi"])));
		SolidBrush GroundDepaColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["groundstatus_colors"]["depa"])));
		SolidBrush ArrivalColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["arrival_color"])));
		SolidBrush ContrAssumedColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["controller_colors"]["assumed"])));
		SolidBrush ContrTrToColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["controller_colors"]["transfer_to_me"])));
		SolidBrush ContrTrFromColor(ColorManager->get_corrected_color("label",
			CurrentConfig->getConfigColor(LabelsSettings["controller_colors"]["transfer_from_me"])));

		// Drawing the leader line
		RECT TagBackRectData = TagBackgroundRect;
		POINT toDraw1, toDraw2;
		if (LiangBarsky(TagBackRectData, acPosPix, TagBackgroundRect.CenterPoint(), toDraw1, toDraw2))
			graphics.DrawLine(&Pen(ColorManager->get_corrected_color("symbol", Color::White)), PointF(Gdiplus::REAL(acPosPix.x), Gdiplus::REAL(acPosPix.y)), PointF(Gdiplus::REAL(toDraw1.x), Gdiplus::REAL(toDraw1.y)));

		// If we use a RIMCAS label only, we display it, and adapt the rectangle
		CRect oldCrectSave = TagBackgroundRect;

		if (rimcasLabelOnly) {
			Color RimcasLabelColor = RimcasInstance->GetAircraftColor(rt.GetCallsign(), Color::AliceBlue, Color::AliceBlue,
				CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_one"]),
				CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_two"]));

			if (RimcasLabelColor.ToCOLORREF() != Color(Color::AliceBlue).ToCOLORREF()) {
				RimcasLabelColor = ColorManager->get_corrected_color("label", RimcasLabelColor);
				int rimcas_height = 0;

				wstring wrimcas_height = wstring(L"ALERT");

				RectF RectRimcas_height;

				graphics.MeasureString(wrimcas_height.c_str(), wcslen(wrimcas_height.c_str()), customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(), &RectRimcas_height);
				rimcas_height = int(RectRimcas_height.GetBottom());

				// Drawing the rectangle
				CRect RimcasLabelRect(TagBackgroundRect.left, TagBackgroundRect.top - rimcas_height, TagBackgroundRect.right, TagBackgroundRect.top);
				graphics.FillRectangle(&SolidBrush(RimcasLabelColor), CopyRect(RimcasLabelRect));
				TagBackgroundRect.top -= rimcas_height;

				// Drawing the text
				wstring rimcasw = wstring(L"ALERT");
				StringFormat stformat = new StringFormat();
				stformat.SetAlignment(StringAlignment::StringAlignmentCenter);
				graphics.DrawString(rimcasw.c_str(), wcslen(rimcasw.c_str()), customFonts[currentFontSize], PointF(Gdiplus::REAL((TagBackgroundRect.left + TagBackgroundRect.right) / 2), Gdiplus::REAL(TagBackgroundRect.top)), &stformat, &RimcasTextColor);
			}
		}

		// Adding the tag screen object
		tagAreas[rt.GetCallsign()] = TagBackgroundRect;
		AddScreenObject(DRAWING_TAG, rt.GetCallsign(), TagBackgroundRect, true, GetBottomLine(rt.GetCallsign()).c_str());

		TagBackgroundRect = oldCrectSave;

		// Clickable zones
		int heightOffset = 0;
		//for (auto&& line : ReplacedLabelLines)
		for (unsigned int i = 0; i < ReplacedLabelLines.size(); i++)
		{
			auto&& line = ReplacedLabelLines[i];
			int widthOffset = 0;
			for (unsigned int j = 0; j < line.size(); j++)
			{
				auto&& element = line[j];
				SolidBrush* color = &FontColor;
				if (TagReplacingMap["sqerror"].size() > 0 && (strcmp(LabelLines[i][j].GetString(), "sqerror") == 0 || strcmp(LabelLines[i][j].GetString(), "sctype") == 0))
					color = &SquawkErrorColor;

				if (RimcasInstance->getAlert(rt.GetCallsign()) != CRimcas::NoAlert)
					color = &RimcasTextColor;
				
				// Tag colors (colors the callsign)
				if (element.length() > 0 && strcmp(LabelLines[i][j].GetString(), "callsign") == 0) {
					// Departure
					if (isActiveAirport(TagReplacingMap["origin"].c_str())) {
						//Ground (Dep)
						if (strcmp(TagReplacingMap["groundstatus"].c_str(), "PUSH") == 0)
							color = &GroundPushColor;
						else if (strcmp(TagReplacingMap["groundstatus"].c_str(), "TAXI") == 0)
							color = &GroundTaxiColor;
						else if (strcmp(TagReplacingMap["groundstatus"].c_str(), "DEPA") == 0)
							color = &GroundDepaColor;

						// Circuits
						if (isActiveAirport(TagReplacingMap["dest"].c_str())) {
							if (reportedGs > 50)
								color = &ArrivalColor;
							else if (is_digits(TagReplacingMap["gate"]))
								color = &FontColor;
						}
					}
					// Arrival
					else if (element.length() > 0 && isActiveAirport(TagReplacingMap["dest"].c_str())) {
						if (reportedGs > 50)
							color = &ArrivalColor;
					}
				}

				// Hide empty clickable Scratchpad content
				else if (strcmp(LabelLines[i][j].GetString(), "scratch") == 0 && strcmp(element.c_str(), EmptyScratchpad.c_str()) == 0)
					color = &TagBackgroundBrush;

				// Tag colors (colours the Controller)
				else if (element.length() > 0 && strcmp(LabelLines[i][j].GetString(), "controller") == 0) {
					switch (fp.GetState()) {
					case FLIGHT_PLAN_STATE_ASSUMED:
						color = &ContrAssumedColor;
						break;
					case FLIGHT_PLAN_STATE_TRANSFER_TO_ME_INITIATED:
						color = &ContrTrToColor;
						break;
					case FLIGHT_PLAN_STATE_TRANSFER_FROM_ME_INITIATED:
						color = &ContrTrFromColor;
						break;
					}
				}

				RectF mRect(0, 0, 0, 0);
				wstring welement = wstring(element.begin(), element.end());

				graphics.DrawString(welement.c_str(), wcslen(welement.c_str()), customFonts[currentFontSize],
					PointF(Gdiplus::REAL(TagBackgroundRect.left + widthOffset), Gdiplus::REAL(TagBackgroundRect.top + heightOffset)),
					&Gdiplus::StringFormat(), color);


				graphics.MeasureString(welement.c_str(), wcslen(welement.c_str()), customFonts[currentFontSize],
					PointF(0, 0), &Gdiplus::StringFormat(), &mRect);

				CRect ItemRect(TagBackgroundRect.left + widthOffset, TagBackgroundRect.top + heightOffset,
					TagBackgroundRect.left + widthOffset + (int)mRect.GetRight(), TagBackgroundRect.top + heightOffset + (int)mRect.GetBottom());

				AddScreenObject(TagClickableMap[element], rt.GetCallsign(), ItemRect, true, GetBottomLine(rt.GetCallsign()).c_str());

				widthOffset += (int)mRect.GetRight();
				widthOffset += blankWidth;
			}
			heightOffset += oneLineHeight;
		}
	}

#pragma endregion Drawing of the tags

	// Releasing the hDC after the drawing
	graphics.ReleaseHDC(hDC);

	CBrush BrushGrey(RGB(150, 150, 150));
	COLORREF oldColor = dc.SetTextColor(RGB(33, 33, 33));

	int TextHeight = dc.GetTextExtent("60").cy;
	Logger::info("RIMCAS Loop");
	for (map<string, bool>::iterator it = RimcasInstance->MonitoredRunwayArr.begin(); it != RimcasInstance->MonitoredRunwayArr.end(); ++it)
	{
		if (!it->second || RimcasInstance->TimeTable[it->first].empty())
			continue;

		vector<int> TimeDefinition = RimcasInstance->CountdownDefinition;
		if (isLVP)
			TimeDefinition = RimcasInstance->CountdownDefinitionLVP;

		if (TimePopupAreas.find(it->first) == TimePopupAreas.end())
			TimePopupAreas[it->first] = { 300, 300, 430, 300+LONG(TextHeight*(TimeDefinition.size()+1)) };

		CRect CRectTime = TimePopupAreas[it->first];
		CRectTime.NormalizeRect();

		dc.FillRect(CRectTime, &BrushGrey);

		// Drawing the runway name
		string tempS = it->first;
		dc.TextOutA(CRectTime.left + CRectTime.Width() / 2 - dc.GetTextExtent(tempS.c_str()).cx / 2, CRectTime.top, tempS.c_str());

		int TopOffset = TextHeight;
		// Drawing the times
		for (auto &Time : TimeDefinition)
		{
			dc.SetTextColor(RGB(33, 33, 33));

			tempS = to_string(Time) + ": " + RimcasInstance->TimeTable[it->first][Time];
			if (RimcasInstance->AcColor.find(RimcasInstance->TimeTable[it->first][Time]) != RimcasInstance->AcColor.end())
			{
				CBrush RimcasBrush(RimcasInstance->GetAircraftColor(RimcasInstance->TimeTable[it->first][Time],
					Color::Black,
					Color::Black,
					CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_one"]),
					CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["background_color_stage_two"])).ToCOLORREF()
					);

				CRect TempRect = { CRectTime.left, CRectTime.top + TopOffset, CRectTime.right, CRectTime.top + TopOffset + TextHeight };
				TempRect.NormalizeRect();

				dc.FillRect(TempRect, &RimcasBrush);
				dc.SetTextColor(RGB(238, 238, 208));
			}

			dc.TextOutA(CRectTime.left, CRectTime.top + TopOffset, tempS.c_str());

			TopOffset += TextHeight;
		}
		AddScreenObject(RIMCAS_IAW, it->first.c_str(), CRectTime, true, "");
	}

	Logger::info("Menu bar lists");

	if (ShowLists["Conflict Alert ARR"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Conflict Alert ARR"], "CA Arrival", 1);
		for (map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it != RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CA_ARRIVAL_FUNC, false, RimcasInstance->MonitoredRunwayArr[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Conflict Alert ARR"] = false;
	}

	if (ShowLists["Conflict Alert DEP"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Conflict Alert DEP"], "CA Departure", 1);
		for (map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it != RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CA_MONITOR_FUNC, false, RimcasInstance->MonitoredRunwayDep[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Conflict Alert DEP"] = false;
	}

	if (ShowLists["Runway closed"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Runway closed"], "Runway Closed", 1);
		for (map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it != RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CLOSED_RUNWAYS_FUNC, false, RimcasInstance->ClosedRunway[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Runway closed"] = false;
	}

	if (ShowLists["Visibility"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Visibility"], "Visibility", 1);
		GetPlugIn()->AddPopupListElement("Normal", "", RIMCAS_UPDATE_LVP, false, int(!isLVP));
		GetPlugIn()->AddPopupListElement("Low", "", RIMCAS_UPDATE_LVP, false, int(isLVP));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Visibility"] = false;
	}

	if (ShowLists["Profiles"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Profiles"], "Profiles", 1);
		vector<string> allProfiles = CurrentConfig->getAllProfiles();
		for (vector<string>::iterator it = allProfiles.begin(); it != allProfiles.end(); ++it) {
			GetPlugIn()->AddPopupListElement(it->c_str(), "", RIMCAS_UPDATE_PROFILE, false, int(CurrentConfig->isItActiveProfile(it->c_str())));
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Profiles"] = false;
	}

	if (ShowLists["Colour Settings"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Colour Settings"], "Colour Settings", 1);
		GetPlugIn()->AddPopupListElement("Day", "", RIMCAS_UPDATE_BRIGHNESS, false, int(ColorSettingsDay));
		GetPlugIn()->AddPopupListElement("Night", "", RIMCAS_UPDATE_BRIGHNESS, false, int(!ColorSettingsDay));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Colour Settings"] = false;
	}

	if (ShowLists["Label Font Size"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Label Font Size"], "Label Font Size", 1);
		GetPlugIn()->AddPopupListElement("Size 1", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 1)));
		GetPlugIn()->AddPopupListElement("Size 2", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 2)));
		GetPlugIn()->AddPopupListElement("Size 3", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 3)));
		GetPlugIn()->AddPopupListElement("Size 4", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 4)));
		GetPlugIn()->AddPopupListElement("Size 5", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 5)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Label Font Size"] = false;
	}

	if (ShowLists["GRND Trail Dots"]) {
		GetPlugIn()->OpenPopupList(ListAreas["GRND Trail Dots"], "GRND Trail Dots", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 0)));
		GetPlugIn()->AddPopupListElement("2", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 2)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 4)));
		GetPlugIn()->AddPopupListElement("8", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 8)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["GRND Trail Dots"] = false;
	}

	if (ShowLists["APPR Trail Dots"]) {
		GetPlugIn()->OpenPopupList(ListAreas["APPR Trail Dots"], "APPR Trail Dots", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 0)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 4)));
		GetPlugIn()->AddPopupListElement("8", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 8)));
		GetPlugIn()->AddPopupListElement("12", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 12)));
		GetPlugIn()->AddPopupListElement("16", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 16)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["APPR Trail Dots"] = false;
	}

	if (ShowLists["Predicted Track Line"]) {
		GetPlugIn()->OpenPopupList(ListAreas["Predicted Track Line"], "Predicted Track Line", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 0)));
		GetPlugIn()->AddPopupListElement("1", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 1)));
		GetPlugIn()->AddPopupListElement("2", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 2)));
		GetPlugIn()->AddPopupListElement("3", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 3)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 4)));
		GetPlugIn()->AddPopupListElement("5", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 5)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Predicted Track Line"] = false;
	}

	if (ShowLists["Brightness"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Brightness"], "Brightness", 1);
		GetPlugIn()->AddPopupListElement("Label", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Symbol", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Afterglow", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Brightness"] = false;
	}

	if (ShowLists["Label"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Label"], "Label Brightness", 1);
		for(int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i +=10)
			GetPlugIn()->AddPopupListElement(to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_LABEL, false, int(bool(i == ColorManager->get_brightness("label"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Label"] = false;
	}

	if (ShowLists["Symbol"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Symbol"], "Symbol Brightness", 1);
		for (int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i += 10)
			GetPlugIn()->AddPopupListElement(to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_SYMBOL, false, int(bool(i == ColorManager->get_brightness("symbol"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Symbol"] = false;
	}

	if (ShowLists["Afterglow"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Afterglow"], "Afterglow Brightness", 1);
		for (int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i += 10)
			GetPlugIn()->AddPopupListElement(to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_AFTERGLOW, false, int(bool(i == ColorManager->get_brightness("afterglow"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Afterglow"] = false;
	}

	Logger::info("QRD");

	//---------------------------------
	// QRD
	//---------------------------------

	if (QDMenabled || QDMSelectEnabled || (DistanceToolActive && ActiveDistance.first != "")) {
		CPen Pen(PS_SOLID, 1, RGB(255, 255, 255));
		CPen *oldPen = dc.SelectObject(&Pen);

		POINT AirportPos = ConvertCoordFromPositionToPixel(AirportPositions[getActiveAirport()]);
		CPosition AirportCPos = AirportPositions[getActiveAirport()];
		if (QDMSelectEnabled)
		{
			AirportPos = QDMSelectPt;
			AirportCPos = ConvertCoordFromPixelToPosition(QDMSelectPt);
		}
		if (DistanceToolActive)
		{
			CPosition r = GetPlugIn()->RadarTargetSelect(ActiveDistance.first.c_str()).GetPosition().GetPosition();
			AirportPos = ConvertCoordFromPositionToPixel(r);
			AirportCPos = r;
		}
		dc.MoveTo(AirportPos);
		POINT point = mouseLocation;
		dc.LineTo(point);

		CPosition CursorPos = ConvertCoordFromPixelToPosition(point);
		double Distance = AirportCPos.DistanceTo(CursorPos);
		double Bearing = AirportCPos.DirectionTo(CursorPos);
	
		Gdiplus::Pen WhitePen(Color::White);
		graphics.DrawEllipse(&WhitePen, point.x - 5, point.y - 5, 10, 10);

		Distance = Distance / 0.00053996f;

		Distance = round(Distance * 10) / 10;

		Bearing = round(Bearing * 10) / 10;

		POINT TextPos = { point.x + 20, point.y };

		if (!DistanceToolActive)
		{
			string distances = to_string(Distance);
			size_t decimal_pos = distances.find(".");
			distances = distances.substr(0, decimal_pos + 2);

			string bearings = to_string(Bearing);
			decimal_pos = bearings.find(".");
			bearings = bearings.substr(0, decimal_pos + 2);

			string text = bearings;
			text += "° / ";
			text += distances;
			text += "m";
			COLORREF old_color = dc.SetTextColor(RGB(255, 255, 255));
			dc.TextOutA(TextPos.x, TextPos.y, text.c_str());
			dc.SetTextColor(old_color);
		}

		dc.SelectObject(oldPen);
		RequestRefresh();
	}

	// Distance tools here
	for (auto&& kv : DistanceTools)
	{
		CRadarTarget one = GetPlugIn()->RadarTargetSelect(kv.first.c_str());
		CRadarTarget two = GetPlugIn()->RadarTargetSelect(kv.second.c_str());

		if (!isVisible(one) || !isVisible(two))
			continue;

		CPen Pen(PS_SOLID, 1, RGB(255, 255, 255));
		CPen *oldPen = dc.SelectObject(&Pen);

		POINT onePoint = ConvertCoordFromPositionToPixel(one.GetPosition().GetPosition());
		POINT twoPoint = ConvertCoordFromPositionToPixel(two.GetPosition().GetPosition());

		dc.MoveTo(onePoint);
		dc.LineTo(twoPoint);

		POINT TextPos = { twoPoint.x + 20, twoPoint.y };

		double Distance = one.GetPosition().GetPosition().DistanceTo(two.GetPosition().GetPosition());
		double Bearing = one.GetPosition().GetPosition().DirectionTo(two.GetPosition().GetPosition());

		string distances = to_string(Distance);
		size_t decimal_pos = distances.find(".");
		distances = distances.substr(0, decimal_pos + 2);

		string bearings = to_string(Bearing);
		decimal_pos = bearings.find(".");
		bearings = bearings.substr(0, decimal_pos + 2);

		string text = bearings;
		text += "° / ";
		text += distances;
		text += "nm";
		COLORREF old_color = dc.SetTextColor(RGB(0, 0, 0));

		CRect ClickableRect = { TextPos.x - 2, TextPos.y, TextPos.x + dc.GetTextExtent(text.c_str()).cx + 2, TextPos.y + dc.GetTextExtent(text.c_str()).cy };
		graphics.FillRectangle(&SolidBrush(Color(127, 122, 122)), CopyRect(ClickableRect));
		dc.Draw3dRect(ClickableRect, RGB(75, 75, 75), RGB(45, 45, 45));
		dc.TextOutA(TextPos.x, TextPos.y, text.c_str());

		AddScreenObject(RIMCAS_DISTANCE_TOOL, string(kv.first+","+kv.second).c_str(), ClickableRect, false, "");

		dc.SetTextColor(old_color);

		dc.SelectObject(oldPen);
	}

	//---------------------------------
	// Drawing the toolbar
	//---------------------------------

	Logger::info("Menu Bar");

	COLORREF qToolBarColor = RGB(127, 122, 122);

	// Drawing the toolbar on the top
	CRect ToolBarAreaTop(RadarArea.left, RadarArea.top + Toolbar_Offset, RadarArea.right, RadarArea.top + 20 + Toolbar_Offset);
	dc.FillSolidRect(ToolBarAreaTop, qToolBarColor);

	COLORREF oldTextColor = dc.SetTextColor(RGB(0, 0, 0));
	CFont* old_font = dc.SelectObject(&menubar_font_left);

	int offset = 2;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, boost::algorithm::join(getActiveAirports(), " ").c_str());
	AddScreenObject(RIMCAS_ACTIVE_AIRPORT, "ActiveAirport", { ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent(boost::algorithm::join(getActiveAirports(), " ").c_str()).cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent(boost::algorithm::join(getActiveAirports(), " ").c_str()).cy }, false, "Active Airport");

	offset += dc.GetTextExtent(boost::algorithm::join(getActiveAirports(), " ").c_str()).cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Display");
	AddScreenObject(RIMCAS_MENU, "DisplayMenu", { ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent("Display").cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent("Display").cy }, false, "Display menu");

	offset += dc.GetTextExtent("Display").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Target");
	AddScreenObject(RIMCAS_MENU, "TargetMenu", { ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent("Target").cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent("Target").cy }, false, "Target menu");

	offset += dc.GetTextExtent("Target").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Colours");
	AddScreenObject(RIMCAS_MENU, "ColourMenu", { ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent("Colour").cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent("Colour").cy }, false, "Colour menu");

	offset += dc.GetTextExtent("Colours").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Alerts");
	AddScreenObject(RIMCAS_MENU, "RIMCASMenu", { ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent("Alerts").cx, ToolBarAreaTop.top + 4 + +dc.GetTextExtent("Alerts").cy }, false, "RIMCAS menu");

	offset += dc.GetTextExtent("Alerts").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "/");
	CRect barDistanceRect = { ToolBarAreaTop.left + offset - 2, ToolBarAreaTop.top + 4, ToolBarAreaTop.left + offset + dc.GetTextExtent("/").cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent("/").cy };
	if (DistanceToolActive)
	{
		graphics.DrawRectangle(&Pen(Color::White), CopyRect(barDistanceRect));
	}
	AddScreenObject(RIMCAS_MENU, "/", barDistanceRect, false, "Distance tool");


	dc.SelectObject(&menubar_font_right);

	offset = 10 + dc.GetTextExtent("?").cx;
	dc.TextOutA(ToolBarAreaTop.right - offset, ToolBarAreaTop.top + 4, "?");
	AddScreenObject(RIMCAS_MENU, "QuestionmarkMenu", { ToolBarAreaTop.right - offset, ToolBarAreaTop.top + 4, ToolBarAreaTop.right - offset + dc.GetTextExtent("?").cx, ToolBarAreaTop.top + 4 + dc.GetTextExtent("?").cy }, false, "Questionmark menu");

	dc.SelectObject(old_font);
	

	//
	// Tag deconflicting
	//

	Logger::info("Tag deconfliction loop");

	for (const auto areas : tagAreas)
	{
		if (!CurrentConfig->getActiveProfile()["labels"]["auto_deconfliction"].GetBool())
			break;

		if (TagsOffsets.find(areas.first) != TagsOffsets.end())
			continue;

		if (IsTagBeingDragged(areas.first))
			continue;

		if (RecentlyAutoMovedTags.find(areas.first) != RecentlyAutoMovedTags.end())
		{
			double t = (double)clock() - RecentlyAutoMovedTags[areas.first] / ((double)CLOCKS_PER_SEC);
			if (t >= 0.8) {
				RecentlyAutoMovedTags.erase(areas.first);
			} else
			{
				continue;
			}
		}

		// We need to see wether the rotation will be clockwise or anti-clockwise
		bool isAntiClockwise = false;

		for (const auto area2 : tagAreas)
		{
			if (areas.first == area2.first)
				continue;

			if (IsTagBeingDragged(area2.first))
				continue;

			CRect h;

			if (h.IntersectRect(tagAreas[areas.first], area2.second))
			{
				if (areas.second.left <= area2.second.left)
				{
					isAntiClockwise = true;
				}

				break;
			}
		}

		// We then rotate the tags until we did a 360 or there is no more conflicts
		POINT acPosPix = ConvertCoordFromPositionToPixel(GetPlugIn()->RadarTargetSelect(areas.first.c_str()).GetPosition().GetPosition());
		int length = LeaderLineDefaultlength;
		if (TagLeaderLineLength.find(areas.first) != TagLeaderLineLength.end())
			length = TagLeaderLineLength[areas.first];

		int width = areas.second.Width();
		int height = areas.second.Height();

		for (double rotated = 0.0; abs(rotated) <= 360.0;)
		{
			// We first rotate the tag
			double newangle = fmod(TagAngles[areas.first] + rotated, 360.0f);

			POINT TagCenter;
			TagCenter.x = long(acPosPix.x + float(length * cos(DegToRad(newangle))));
			TagCenter.y = long(acPosPix.y + float(length * sin(DegToRad(newangle))));

			CRect NewRectangle(TagCenter.x - (width / 2), TagCenter.y - (height / 2), TagCenter.x + (width / 2), TagCenter.y + (height / 2));
			NewRectangle.NormalizeRect();

			// Assume there is no conflict, then try again
			bool isTagConflicing = false;

			for (const auto area2 : tagAreas)
			{
				if (areas.first == area2.first)
					continue;

				if (IsTagBeingDragged(area2.first))
					continue;

				CRect h;

				if (h.IntersectRect(NewRectangle, area2.second))
				{
					isTagConflicing = true;
					break;
				}
			}

			if (!isTagConflicing)
			{
				TagAngles[areas.first] = fmod(TagAngles[areas.first] + rotated, 360);
				tagAreas[areas.first] = NewRectangle;
				RecentlyAutoMovedTags[areas.first] = clock();
				break;
			}

			if (isAntiClockwise)
				rotated -= 22.5f;
			else
				rotated += 22.5f;
		}
	}

	//
	// App windows
	//

	Logger::info("App window rendering");

	for (map<int, bool>::iterator it = appWindowDisplays.begin(); it != appWindowDisplays.end(); ++it)
	{
		if (!it->second)
			continue;

		int appWindowId = it->first;
		appWindows[appWindowId]->render(hDC, this, &graphics, mouseLocation, DistanceTools, TagsDetailed);
	}

	dc.Detach();

	Logger::info("END "+ string(__FUNCSIG__));

}

// ReSharper restore CppMsExtAddressOfClassRValue

//---EuroScopePlugInExitCustom-----------------------------------------------

void CSMRRadar::EuroScopePlugInExitCustom()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

		if (smrCursor != nullptr && smrCursor != NULL)
		{
			SetWindowLong(pluginWindow, GWL_WNDPROC, (LONG)gSourceProc);
		}
}
