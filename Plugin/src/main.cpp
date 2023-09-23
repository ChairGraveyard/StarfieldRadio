#include "DKUtil/Config.hpp"
#include "DKUtil/Hook.hpp"
#include "DKUtil/Logger.hpp"



// For MCI
#include <Mmsystem.h>
#include <mciapi.h>
#pragma comment(lib, "Winmm.lib")

// Formatting, string and console
#include <codecvt>
#include <fstream>
#include <locale>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>
#include <ctime>

// For type aliases
using namespace DKUtil::Alias;  
std::wstring to_wstring(const std::string& stringToConvert)
{
	std::wstring wideString =
		std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(stringToConvert);
	return wideString;
}

static REL::Relocation<__int64 (*)(double, char*, ...)> ExecuteCommand{ REL::Offset(0x287DF04) };

void Notification(std::string Message)
{
	if (!RE::TESForm::LookupByID(0x14))
		return;
	
	std::string Command = fmt::format("cgf \"Debug.Notification\" \"{}\"", Message);
	ExecuteCommand(0, Command.data());
}

class RadioPlayer
{
public:
	RadioPlayer(const std::vector<std::string>& InStations, bool InAutoStart, bool InRandomizeStartTime) 
		: 
		AutoStart(InAutoStart),
		RandomizeStartTime(InRandomizeStartTime),
		Stations(InStations)
	{
		
	}

	~RadioPlayer()
	{
	}

	void Init()
	{
		INFO("{} v{} - Initializing Starfield Radio Sound System -", Plugin::NAME, Plugin::Version);
	
		if (Stations.size() <= 0) {
			INFO("{} v{} - No Stations Found, Starfield Radio Shutting Down -", Plugin::NAME, Plugin::Version);
			return;
		}

		INFO("{} v{} - Starting Starfield Radio -", Plugin::NAME, Plugin::Version);

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[0]);

		if (StationInfo.second.contains("://"))
		{
			std::wstring OpenLocalFile = to_wstring(std::format("open {} type mpegvideo alias sfradio", StationInfo.second));
			mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
		}
		else
		{
			std::wstring OpenLocalFile = to_wstring(std::format("open \".\\Data\\SFSE\\Plugins\\StarfieldGalacticRadio\\tracks\\{}\" type mpegvideo alias sfradio", StationInfo.second));
			mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
		}	
		
		if (!StationInfo.first.empty())
			Notification(std::format("On Air - {}", StationInfo.first));

		INFO("{} v{} - Selected Station {}, AutoStart: {}, Mode: {} -", Plugin::NAME, Plugin::Version, Stations[0], AutoStart, Mode);

		if (AutoStart && Mode == 0) 
		{
			IsStarted = true;
			mciSendString(L"play sfradio repeat", NULL, 0, NULL);
			mciSendString(L"setaudio sfradio volume to 0", NULL, 0, NULL);
		}

		Notification("Starfield Radio Initialized");
	}

	void SelectStation(int InStationIndex)
	{
		// Index out of bounds.
		if (Stations.size() <= InStationIndex)
			return;

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[InStationIndex]);

		const std::string& StationName = StationInfo.first;
		const std::string& StationURL = StationInfo.second;

		mciSendString(L"close sfradio", NULL, 0, NULL);

		if (StationURL.contains("://"))
		{
			std::wstring OpenLocalFile = to_wstring(std::format("open {} type mpegvideo alias sfradio", StationURL));
			mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
		}
		else
		{
			std::wstring OpenLocalFile = to_wstring(std::format("open \".\\Data\\SFSE\\Plugins\\StarfieldGalacticRadio\\tracks\\{}\" type mpegvideo alias sfradio", StationURL));
			mciSendString(OpenLocalFile.c_str(), NULL, 0, NULL);
		}
				
		if (!StationInfo.first.empty())
			Notification(std::format("On Air - {}", StationInfo.first));

		mciSendString(L"play sfradio repeat", NULL, 0, NULL);

		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);
	}

	void NextStation()
	{
		StationIndex = (StationIndex + 1) % Stations.size();

		SelectStation(StationIndex);
	}

	void PrevStation()
	{
		StationIndex = (StationIndex - 1);
		if (StationIndex < 0)
			StationIndex = Stations.size() - 1;
		SelectStation(StationIndex);
	}

	void SetVolume(float InVolume)
	{
		Volume = InVolume;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)InVolume));
		mciSendString(v.c_str(), NULL, 0, NULL);
	}

	void DecreaseVolume()
	{
		Volume = Volume - 5.0f;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);

		//Notification(std::format("Volume {}", Volume));
	}

	void IncreaseVolume()
	{
		Volume = Volume + 5.0f;
		std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
		mciSendString(v.c_str(), NULL, 0, NULL);

		//Notification(std::format("Volume {}", Volume));
	}

	uint32_t GetTrackLength()
	{
		std::wstring StatusBuffer;
		StatusBuffer.reserve(128);

		mciSendString(L"status sfradio length", StatusBuffer.data(), 128, NULL);

		// Convert Length to int
		uint32_t TrackLength = std::stoi(StatusBuffer);

		return TrackLength;
	}

	void Seek(int32_t InSeconds)
	{
		// Get current position from MCI with mciSendString
		std::wstring StatusBuffer;
		StatusBuffer.reserve(128);
		
		mciSendString(L"status sfradio length", StatusBuffer.data(), 128, NULL);

		// Convert Length to int
		int32_t TrackLength = std::stoi(StatusBuffer);
		StatusBuffer.clear();
		StatusBuffer.reserve(128);
		mciSendString(L"status sfradio position", StatusBuffer.data(), 128, NULL);

		int32_t Position = 0;

		if (StatusBuffer.contains(L":"))
		{
			std::tm t;
			std::wistringstream ss(StatusBuffer);
			ss >> std::get_time(&t, L"%H:%M:%S");
			Position = t.tm_hour * 3600 + t.tm_min * 60 + t.tm_sec;
		}
		else
		{
			Position = std::stoi(StatusBuffer);
		}

		int32_t NewPosition = Position + (InSeconds * 1000);

		if (NewPosition >= TrackLength)
			NewPosition = TrackLength - 1;

		mciSendString(to_wstring(std::format("play sfradio from {} repeat", (int)NewPosition)).c_str(), NULL, 0, NULL);
	}

	void TogglePlayer()
	{
		ENABLE_DEBUG

		IsPlaying = !IsPlaying;
		if (!IsStarted && IsPlaying) {
			IsStarted = true;
			mciSendString(L"play sfradio repeat", NULL, 0, NULL);

			if (RandomizeStartTime)
			{
				uint32_t TrackLength = GetTrackLength();
				srand(time(NULL));
				uint32_t RandomTime = rand() % TrackLength;

				Seek(RandomTime);
			}
		}

		std::pair<std::string, std::string> StationInfo = GetStationInfo(Stations[StationIndex]);

		if (IsPlaying) {
			if (!StationInfo.first.empty())
				Notification(std::format("On Air - {}", StationInfo.first));
			else
				Notification("Radio On");
		}
		else
			Notification("Radio Off");

		if (Mode == 0) 
		{
			std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)IsPlaying ? Volume : 0.0f));
			mciSendString(v.c_str(), NULL, 0, NULL);
		}
		else
		{
			if (!IsPlaying) 
			{
				mciSendString(L"stop sfradio", NULL, 0, NULL);
			}
			else
			{
				mciSendString(L"play sfradio repeat", NULL, 0, NULL);
				if (RandomizeStartTime) 
				{
					uint32_t TrackLength = GetTrackLength();
					srand(time(NULL));
					uint32_t RandomTime = rand() % TrackLength;

					Seek(RandomTime);
				}
				std::wstring v = to_wstring(std::format("setaudio sfradio volume to {}", (int)Volume));
				mciSendString(v.c_str(), NULL, 0, NULL);
			}
		}
	}

	// 0 = Radio, 1 = Podcast
	void ToggleMode()
	{
		Mode = ~Mode;

		// Handle Podcast vs Radio mode.
		// Podcast mode will actually stop the stream, while Radio mode just mutes it so that time passes when not listened to.
	}

	std::pair<std::string, std::string> GetStationInfo(const std::string& StationConfig)
	{
		std::string Station = StationConfig;
		size_t Separator = Station.find("|");

		std::string StationName = Separator != std::string::npos ? Station.substr(0, Station.find("|")) : "";
		std::string StationURL = Separator != std::string::npos ? Station.substr(Station.find("|") + 1) : Station;

		return std::make_pair(StationName, StationURL);
	}

private:
	int   Mode = 0;
	int   StationIndex = 0;
	float Volume = 100.0f;
	float Seconds = 0.0f;
	bool  RandomizeStartTime = false;
	bool  AutoStart = true;
	bool  IsStarted = false;
	bool  IsPlaying = false;

	std::vector<std::string> Stations;
};

const int    TimePerFrame = 50;
static DWORD MainLoop(void* unused)
{
	(void)unused;

	ENABLE_DEBUG 

	DEBUG("Input Loop Starting");
	
	TomlConfig            MainConfig = COMPILE_PROXY("StarfieldGalacticRadio.toml"sv);
	Boolean               AutoStartData{ "AutoStartRadio" };
	Boolean               RandomizeStartTimeData{ "RandomizeStartTime" };
	String                PlaylistData{ "Playlist" };
	Integer               ToggleRadioKeyData{ "ToggleRadioKey" };
	Integer               SwitchModeKeyData{ "SwitchModeKey" };
	Integer               VolumeUpKeyData{ "VolumeUpKey" };
	Integer               VolumeDownKeyData{ "VolumeDownKey" };
	Integer               NextStationKeyData{ "NextStationKey" };
	Integer               PreviousStationKeyData{ "PreviousStationKey" };
	Integer               SeekForwardKeyData{ "SeekForwardKey" };
	Integer               SeekBackwardKeyData{ "SeekBackwardKey" };
	static std::once_flag Bound;
	std::call_once(Bound, [&]() {
		
		MainConfig.Bind(AutoStartData, true);  // bool, no bool array support
		MainConfig.Bind(RandomizeStartTimeData, false);                     // bool, no bool array support
		MainConfig.Bind(PlaylistData, "kino.mp3", "soltrain.mp3", "nocturna.mp3");  // string array
		MainConfig.Bind(ToggleRadioKeyData, VK_NUMPAD0);
		MainConfig.Bind(SwitchModeKeyData, VK_SUBTRACT);
		MainConfig.Bind(VolumeUpKeyData, VK_ADD);
		MainConfig.Bind(VolumeDownKeyData, 0xB0); // NUMPAD ENTER
		MainConfig.Bind(NextStationKeyData, VK_NUMPAD8);
		MainConfig.Bind(PreviousStationKeyData, VK_NUMPAD7);
		MainConfig.Bind(SeekForwardKeyData, VK_MULTIPLY);
		MainConfig.Bind(SeekBackwardKeyData, VK_DIVIDE);
	});

	MainConfig.Load();

	DEBUG("Loaded config, waiting for player form...");
	while (!RE::TESForm::LookupByID(0x14))
		Sleep(1000);

	DEBUG("Pre-Initialize RadioPlayer.");

	RadioPlayer Radio(PlaylistData.get_collection(), *AutoStartData, *RandomizeStartTimeData);
	Radio.Init();

	DEBUG("Post-Initialize RadioPlayer.")

	bool ToggleRadioHoldFlag = false;
	bool SwitchModeHoldFlag = false;
	bool VolumeUpHoldFlag = false;
	bool VolumeDownHoldFlag = false;
	bool NextStationHoldFlag = false;
	bool PrevStationHoldFlag = false;
	bool SeekForwardHoldFlag = false;
	bool SeekBackwardHoldFlag = false;

	for (;;) {
		short ToggleRadioKeyState = SFSE::WinAPI::GetKeyState(*ToggleRadioKeyData);
		short SwitchModeKeyState = SFSE::WinAPI::GetKeyState(*SwitchModeKeyData);
		short VolumeUpKeyState = SFSE::WinAPI::GetKeyState(*VolumeUpKeyData);
		short VolumeDownKeyState = SFSE::WinAPI::GetKeyState(*VolumeDownKeyData);
		short NextStationKeyState = SFSE::WinAPI::GetKeyState(*NextStationKeyData);
		short PrevStationKeyState = SFSE::WinAPI::GetKeyState(*PreviousStationKeyData);
		short SeekForwardKeyState = SFSE::WinAPI::GetKeyState(*SeekForwardKeyData);
		short SeekBackwardKeyState = SFSE::WinAPI::GetKeyState(*SeekBackwardKeyData);

		//INFO("ToggleRadioKeyState: {}\nSwitchModeKeyState: {}\nVolumeUpKeyState: {}\nVolumeDownKeyState: {}\nNextStationKeyState: {}\nPrevStationKeyState: {}\nSeekForwardKeyState: {}\nSeekBackwardKeyState: {}", ToggleRadioKeyState, SwitchModeKeyState, VolumeUpKeyState, VolumeDownKeyState, NextStationKeyState, PrevStationKeyState, SeekForwardKeyState, SeekBackwardKeyState);
	
		// TODO: Handle this better
		if (ToggleRadioHoldFlag && ToggleRadioKeyState >= 0)
			ToggleRadioHoldFlag = 0;
		if (SwitchModeHoldFlag && SwitchModeKeyState >= 0)
			SwitchModeHoldFlag = 0;
		if (VolumeUpHoldFlag && VolumeUpKeyState >= 0)
			VolumeUpHoldFlag = 0;
		if (VolumeDownHoldFlag && VolumeDownKeyState >= 0)
			VolumeDownHoldFlag = 0;
		if (NextStationHoldFlag && NextStationKeyState >= 0) 
			NextStationHoldFlag = 0;
		if (PrevStationHoldFlag && PrevStationKeyState >= 0) 
			PrevStationHoldFlag = 0;
		if (SeekForwardHoldFlag && SeekForwardKeyState >= 0)
			SeekForwardHoldFlag = 0;
		if (SeekBackwardHoldFlag && SeekBackwardKeyState >= 0)
			SeekBackwardHoldFlag = 0;
	
		if (ToggleRadioKeyState < 0 && !ToggleRadioHoldFlag)
		{
			ToggleRadioHoldFlag = 1;
			Radio.TogglePlayer();
		}

		if (SwitchModeKeyState < 0 && !SwitchModeHoldFlag) {
			SwitchModeHoldFlag = 1;
			Radio.ToggleMode();
		}

		if (VolumeUpKeyState < 0 && !VolumeUpHoldFlag) {
			VolumeUpHoldFlag = 1;
			Radio.IncreaseVolume();
		}

		if (VolumeDownKeyState < 0 && !VolumeDownHoldFlag) {
			VolumeDownHoldFlag = 1;
			Radio.DecreaseVolume();
		}

		if (NextStationKeyState < 0 && !NextStationHoldFlag) {
			NextStationHoldFlag = 1;
			Radio.NextStation();
		}

		if (PrevStationKeyState < 0 && !PrevStationHoldFlag) {
			PrevStationHoldFlag = 1;
			Radio.PrevStation();
		}

		if (SeekForwardKeyState < 0 && !SeekForwardHoldFlag) {
			SeekForwardHoldFlag = 1;
			Radio.Seek(1);
		}
		if (SeekBackwardKeyState < 0 && !SeekBackwardHoldFlag) {
			SeekBackwardHoldFlag = 1;
			Radio.Seek(-1);
		}
		Sleep(TimePerFrame);
	}

	return 0;
}

DLLEXPORT constinit auto SFSEPlugin_Version = []() noexcept {
	SFSE::PluginVersionData data{};

	data.PluginVersion(Plugin::Version);
	data.PluginName(Plugin::NAME);
	data.AuthorName(Plugin::AUTHOR);
	data.UsesSigScanning(true);
	//data.UsesAddressLibrary(true);
	data.HasNoStructUse(true);
	//data.IsLayoutDependent(true);
	data.CompatibleVersions({ SFSE::RUNTIME_LATEST });

	return data;
}();

namespace
{
	void MessageCallback(SFSE::MessagingInterface::Message* a_msg) noexcept
	{
		switch (a_msg->type) {
		case SFSE::MessagingInterface::kPostLoad:
			{
				INFO("Creating Input Thread")
				CreateThread(NULL, 0, MainLoop, NULL, 0, NULL);
				break;
			}
		default:
			break;
		}
	}
}

/**
// for preload plugins
void SFSEPlugin_Preload(SFSE::LoadInterface* a_sfse);
/**/

DLLEXPORT bool SFSEAPI SFSEPlugin_Load(const SFSE::LoadInterface* a_sfse)
{
#ifndef NDEBUG
	while (!IsDebuggerPresent()) {
		Sleep(100);
	}
#endif


	SFSE::Init(a_sfse, false);

	DKUtil::Logger::Init(Plugin::NAME, std::to_string(Plugin::Version));

	INFO("{} v{} loaded", Plugin::NAME, Plugin::Version);

	// do stuff
	SFSE::AllocTrampoline(1 << 10);

	SFSE::GetMessagingInterface()->RegisterListener(MessageCallback);

	return true;
}
