#pragma once

#include <vector>
#include <windows.h>
#include <TlHelp32.h>

DWORD GetProcId(const wchar_t* procName);

uintptr_t GetModuleBaseAddress(DWORD procId, const wchar_t* modName);

uintptr_t FindDMAAddy(HANDLE hProc, uintptr_t ptr, std::vector<unsigned int> offsets);

template <typename T>
T ReadMemory(HANDLE HProcess, uintptr_t BaseAddress, std::vector<unsigned int> PointerChain)
{
	uintptr_t Addr = FindDMAAddy(HProcess, BaseAddress, PointerChain);

	T Value = NULL;
	ReadProcessMemory(HProcess, (BYTE*)Addr, &Value, sizeof(Value), nullptr);

	return Value;
}

template<typename T>
void WriteMemory(HANDLE HProcess, uintptr_t BaseAddress, std::vector<unsigned int> PointerChain, T Value)
{
	uintptr_t Addr = FindDMAAddy(HProcess, BaseAddress, PointerChain);

	T WriteValue = Value;
	WriteProcessMemory(HProcess, (BYTE*)Addr, &WriteValue, sizeof(WriteValue), nullptr);
}