#include <iostream>
#include <vector>
#include <Windows.h>
#include <winuser.h>
#include "proc.h"
#include <map>
#include <direct.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include "IO.h"
#pragma comment(lib, "urlmon.lib")

#pragma region Global Variables

HANDLE HProcess = 0;

uintptr_t ModuleBase = 0;

std::map<unsigned int, float> CutsceneBlacklist;

#pragma endregion

#pragma region Helper Functions

/// <summary>
/// Downloads and replaces the currently existing Blacklist JSON file
/// </summary>
void DownloadBlacklistJSON()
{
	// Link to the Blacklist JSON Gist. This url will get the latest revision
	TCHAR DownloadURL[] = TEXT("https://gist.githubusercontent.com/Atorizil/734a7649471f0fa0a2a9f92a167e294b/raw/");

	// Construct the path to save the file to...
	TCHAR SavePath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, SavePath);
	wsprintf(SavePath, TEXT("%s\\Blacklist.json"), SavePath);

	// Download the file and store the result
	HRESULT Result = URLDownloadToFile(NULL, DownloadURL, SavePath, 0, NULL);

	// Error Handling
	switch (Result)
	{
	case S_OK:
		std::cout << "Blacklist JSON downloaded successfully\n";
		break;
	case E_OUTOFMEMORY:
		std::cout << "Out of Memory\n";
		break;
	case INET_E_DOWNLOAD_FAILURE:
		std::cout << "Download failure\n";
		break;
	default:
		std::cout << "Other error\n";
		break;
	}
}

/// <summary>
/// Parses the 'Blacklist.json' file
/// </summary>
/// <returns>A map containing the blacklisted cutscenes</returns>
std::map<unsigned int, float> GetCutsceneBlacklistMap()
{
	// Open the 'Blacklist.json' file
	TCHAR JSONFilePath[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, JSONFilePath);
	wsprintf(JSONFilePath, TEXT("%s\\Blacklist.json"), JSONFilePath);

	std::ifstream JSONFile(JSONFilePath);

	// Create the JSON object and read the file
	nlohmann::json BlacklistJSON;
	JSONFile >> BlacklistJSON;

	// Create a empty map
	std::map<unsigned int, float> Dummy;

	// Parse
	for (auto Element = BlacklistJSON.begin(); Element != BlacklistJSON.end(); Element++)
	{
		unsigned int CineID = std::stoul(Element.key());
		float SkipDelay = Element.value()["skip_delay"];
		Dummy.insert(std::pair<unsigned int, float>(CineID, SkipDelay));
	}

	return Dummy;
}

/// <summary>
/// Gets if SOTTR.exe is running
/// NOTE: Very unoptimised, can add a check if it is still active rather than reconnecting
/// </summary>
/// <returns>Boolean if the game is running or not</returns>
bool IsRunning()
{
	// Get the Process ID of SOTTR.exe
	DWORD ProcId = GetProcId(L"SOTTR.exe");

	if (ProcId > 0)
	{
		// Get the Base Module address of SOTTR.exe
		ModuleBase = GetModuleBaseAddress(ProcId, L"SOTTR.exe");

		// Get the Handle to the process
		HProcess = OpenProcess(PROCESS_ALL_ACCESS, NULL, ProcId);

		// Return true as we have connected to the process thus the game is running
		return true;
	}

	// Return false as the game is not running
	return false;
}

/// <summary>
/// This function reads tries the skip the currently active cutscene
/// </summary>
void TrySkip()
{
	// Abort if the game is not running
	if (!IsRunning())
	{
		std::cout << "Game is not running\n";
		return;
	}

	#pragma region TimelinePlacement Pointer Validation

	// Resovle the base address of the cutscene object pointer chain
	uintptr_t TimelinePlacementPtr = ModuleBase + 0x141B8C0;

	// Get the pointer to the current TimelinePlacement instance
	long TimelinePlacement = 0;
	ReadProcessMemory(HProcess, (BYTE*)TimelinePlacementPtr, &TimelinePlacement, sizeof(TimelinePlacement), nullptr);

	// Return if we are not in a cutscene to avoid crashes
	if (TimelinePlacement == 0)
	{
		std::cout << "Not in a cutscene, aborting...\n";
		return;
	}

#pragma endregion

	#pragma region Prompt Status Validation

	// Get the status of the prompt. (0 = Nothing, 1 = Loading, 2 = Skip)
	int PromptStatus = ReadMemory<int>(HProcess, TimelinePlacementPtr, { 0x10 });

	// To avoid skipping cinematics, return if no loading prompt is shown
	if (PromptStatus != 1)
	{
		std::cout << "Loading prompt not showing, aborting...\n";
		return;
	}

#pragma endregion

	// Default delay, ~5 seconds
	float SkipDelay = 6.0f;

	#pragma region Cutscene Blacklist

	// Get the Cutscene ID of the current cutscene
	unsigned int CutsceneID = ReadMemory<unsigned int>(HProcess, (ModuleBase + 0x360E0D0), { 0x0, 0x120, 0x10, 0x1D4 });

	// If the current cutscene is in the Blacklist, change the skip delay
	if (CutsceneBlacklist.find(CutsceneID) != CutsceneBlacklist.end())
	{
		if (CutsceneBlacklist[CutsceneID] == 20000.0f)
		{
			std::cout << "This cutscene is blacklisted... Cannot be skipped to prevent problems\n";
			return;
		}
		else
		{
			std::cout << "NOTE: This cutscene is blacklisted\n";
			SkipDelay = CutsceneBlacklist[CutsceneID];
		}
	}

#pragma endregion

	#pragma region Timeline Time Validation

	// Get the current time of the Timeline
	float TimelineTime = ReadMemory<float>(HProcess, TimelinePlacementPtr, { 0x60 });
	if (TimelineTime < SkipDelay)
	{
		std::cout << "Can skip cutscene in " << std::defaultfloat << SkipDelay - TimelineTime << " seconds\n";
		return;
	}

#pragma endregion

	WriteMemory<byte>(HProcess, TimelinePlacementPtr, { 0x129 }, 5);

	std::cout << "Cutscene Skipped!\n";
}

#pragma endregion

/// <summary>
/// Main program
/// </summary>
int main()
{
	// Download / update the 'Blacklist.json' file
	DownloadBlacklistJSON();

	// Parse the 'Blacklist.json' file and return as map
	CutsceneBlacklist = GetCutsceneBlacklistMap();

	std::cout << "Hack is loaded!\n\nPress Spacebar to try and skip the current cutscene!\nOutput will be shown in this window, though it can be minimised\n\n";

	// Infinite loop!
	while (true)
	{
		// Try to skip the cutscene if space is pressed
		if (GetAsyncKeyState(VK_SPACE) & 1)
			TrySkip();

		// Reduce CPU usage
		Sleep(10);
	}

	// Impossible to get here, but return 0 regardless
	return 0;
}