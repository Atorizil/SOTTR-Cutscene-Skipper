#include "stdafx.h"
#include <iostream>
#include <vector>
#include <Windows.h>
#include <winuser.h>
#include "proc.h"
#include <map>

HANDLE HProcess = 0;

uintptr_t ModuleBase = 0;

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

std::map<int, float> CutsceneBlacklist =
{
    // First cutscene in Paititi after being captured by Unuratu
    // This cutscene can be skipped. The only problem is that there is a 5 second pause before the game continues as normal
    //{ 463826843 , 999999999.0f }
    // Cutscene after rescuing Unuratu when opening the door before Unuratu fights the guards
    // A delay is needed for the game to load in correctly (Unuratu won't TP to Lara)
    { 315494741, 30.0f },
    // Cutscene during the slow walk with Unuratu before the sacrifice slow walk.
    // Unskippable due to it softlocking the game at the sacrifice slow walk due to Lara being ahead of Unuratu
    { 2745466653, 999999999.0f  },
    // Boat cutscene before Oil Fields, after Unuratu's death
    // Delay is needed because skipping to early will cause Lara to die
    { 702620817, 15.0f },
    // Cutscene of the helicopter crash in Oil Fields
    // Skipping softlocks and reloading checkpoint replays the cutscene
    { 1586019898, 999999999.0f }
};


void TrySkip()
{
    if (IsRunning())
    {
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
            
        // Get the current time of the Timeline
        std::vector<unsigned int> TimelineTimeOffsets = { 0x60 };
        uintptr_t TimelineTimeAddr = FindDMAAddy(HProcess, TimelinePlacementPtr, TimelineTimeOffsets);

        float TimelineTime = 0.0f;
        ReadProcessMemory(HProcess, (BYTE*)TimelineTimeAddr, &TimelineTime, sizeof(TimelineTime), nullptr);

        // Get the Cutscene ID of the current cutscene
        uintptr_t CutsceneIDPtr = ModuleBase + 0x360E0D0;

        std::vector<unsigned int> CutsceneIDOffsets = { 0x0, 0x120, 0x10, 0x1D4 };
        uintptr_t CutsceIDAddr = FindDMAAddy(HProcess, CutsceneIDPtr, CutsceneIDOffsets);
        
        unsigned int CutsceneID = 0;
        ReadProcessMemory(HProcess, (BYTE*)CutsceIDAddr, &CutsceneID, sizeof(CutsceneID), nullptr);

        // Get the status of the prompt
        // (0 = Nothing, 1 = Loading, 2 = Skip)
        std::vector<unsigned int> PromptStatusOffsets = { 0x10 };
        uintptr_t PromptStatusAddr = FindDMAAddy(HProcess, TimelinePlacementPtr, PromptStatusOffsets);

        int PromptStatus = 0;
        ReadProcessMemory(HProcess, (BYTE*)PromptStatusAddr, &PromptStatus, sizeof(PromptStatus), nullptr);
        
        // Default delay, ~5 seconds
        float SkipDelay = 6.0f;

        // If the current cutscene is in the Blacklist, change the skip delay
        if (CutsceneBlacklist.find(CutsceneID) != CutsceneBlacklist.end())
        {
            if (CutsceneBlacklist[CutsceneID] == 999999999.0f)
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

        // Prompt status is important here because it prevenets the potential skipping of
        // cinematics that aren't actually cutscene such as getting attacked by an ell.
        // Skipping these cinematics have very strange side effects so leave the unskippable
        if (TimelineTime > SkipDelay)
        {
            if (PromptStatus != 1)
            {
                std::cout << "The loading prompt is not showing, aborting skip\n";
                return;
            }

            // Get the address of the TimelineState
            std::vector<unsigned int> TimelineStateOffsets = { 0x129 };
            uintptr_t TimelineStateAddr = FindDMAAddy(HProcess, TimelinePlacementPtr, TimelineStateOffsets);

            // Write the TimelineState (Enum: None, Loading, Loaded, Playing, Paused, Skipping)
            int NewState = 5;
            WriteProcessMemory(HProcess, (BYTE*)TimelineStateAddr, &NewState, sizeof(NewState), nullptr);

            std::cout << "Cutscene Skipped!\n";
        }
        else
        {
            std::cout << "Can skip cutscene in " << std::defaultfloat << SkipDelay - TimelineTime << " seconds\n";
        }
    }
    else
    {
        std::cout << "Game is not running\n";
    }
    
}

int main()
{
    while (true)
    {
        if (GetAsyncKeyState(VK_SPACE) & 1)
        {
            TrySkip();
        }
        Sleep(10);
    }

    return 0;
}