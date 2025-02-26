/*
*************************************************************
*                                                           *
*   Flowhooks Software - Open Source License                *
*                                                           *
*  This software is licensed under the GNU Affero General   *
*  Public License v3. You may use, modify, and distribute   *
*  this code under the terms of the AGPLv3.                 *
*                                                           *
*  This program is distributed in the hope that it will be  *
*  useful, but WITHOUT ANY WARRANTY; without even the       *
*  implied warranty of MERCHANTABILITY or FITNESS FOR A     *
*  PARTICULAR PURPOSE. See the GNU AGPLv3 for more details. *
*                                                           *
*  Author: Felipe Cezar Paz (git@felipecezar.com)           *
*  File:                                                    *
*  Description: Counter-Strike: Source Speed Hack           *
*                                                           *
*************************************************************
*/

#include <Windows.h>

constexpr DWORD kCLMoveOffset = 0xBC1E0;

using CLMoveFunction = void(__cdecl*)(float, bool);
CLMoveFunction originalCLMove;

constexpr int kCommandsToRun = 10;

void __cdecl HookedCLMove(float accumulatedExtraSamples, bool isFinalTick)
{
    if (GetAsyncKeyState('C'))
        return;

    originalCLMove(accumulatedExtraSamples, isFinalTick);

    if (GetAsyncKeyState('V'))
    {
        for (int i = 0; i < kCommandsToRun; ++i)
        {
            originalCLMove(accumulatedExtraSamples, i == kCommandsToRun - 1);
        }
    }
}

bool HookFunction(BYTE* src, BYTE* dst, uintptr_t len)
{
    if (len < 5) return false;

    DWORD oldProtect;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &oldProtect);

    uintptr_t relativeAddress = dst - src - 5;

    *src = 0xE9; // JMP instruction
    *(uintptr_t*)(src + 1) = relativeAddress;

    VirtualProtect(src, len, oldProtect, &oldProtect);
    return true;
}

BYTE* CreateTrampolineHook(BYTE* src, BYTE* dst, uintptr_t len)
{
    if (len < 5) return nullptr;

    BYTE* gateway = (BYTE*)VirtualAlloc(nullptr, len, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!gateway) return nullptr; // Check for allocation failure

    memcpy(gateway, src, len);

    uintptr_t gatewayRelativeAddress = src - gateway - 5;

    *(gateway + len) = 0xE9; // JMP instruction
    *(uintptr_t*)(gateway + len + 1) = gatewayRelativeAddress;

    HookFunction(src, dst, len);
    return gateway;
}

void UnhookFunction(BYTE* src, BYTE* gateway, uintptr_t len)
{
    DWORD oldProtect;
    VirtualProtect(src, len, PAGE_EXECUTE_READWRITE, &oldProtect);

    memcpy(src, gateway, len);

    VirtualProtect(src, len, oldProtect, &oldProtect);

    VirtualFree(gateway, 0, MEM_RELEASE); // Free the allocated memory correctly
}

DWORD WINAPI HackThread(HMODULE hModule)
{
    HMODULE hEngine = GetModuleHandleW(L"engine.dll");

    BYTE* clMoveAddress = (BYTE*)((DWORD_PTR)hEngine + kCLMoveOffset);
    originalCLMove = (CLMoveFunction)CreateTrampolineHook(clMoveAddress, (BYTE*)HookedCLMove, 6);

    while (!GetAsyncKeyState(VK_DELETE))
    {
        Sleep(2000);
    }

    UnhookFunction(clMoveAddress, (BYTE*)originalCLMove, 6);

    FreeLibraryAndExitThread(hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ulReasonForCall, LPVOID lpReserved)
{
    if (ulReasonForCall == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);

        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)HackThread, hModule, 0, nullptr);
        if (hThread)
        {
            CloseHandle(hThread);
        }
    }

    return TRUE;
}
