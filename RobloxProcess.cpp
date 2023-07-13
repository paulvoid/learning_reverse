//
// Created by Administrator on 4/20/2023.
//

#include "RobloxProcess.h"

bool RobloxProcess::Attach(HANDLE process, int retry_count)
{
    handle = process;
    retries_left = retry_count;
    if (!BlockingLoadModuleInfo())
    {
        std::cout << "Failed to load module info" << std::endl;
        retries_left = -1;
        return false;
    }
    cout << "Module info loaded" << endl;

    if (main_module.size < 1024 * 1024 * 10)
    {
        std::cout << "Module size is too small" << std::endl;
        retries_left = -1;
        return false;
    }
    Tick();
    return dwGame != NULL;
}

bool RobloxProcess::BlockingLoadModuleInfo()
{
    int tries = 5;
    int wait_time = 100;
    cout << "Finding process base" << endl;
    while (true){
        ProcUtil::ProcessInfo info = ProcUtil::ProcessInfo(handle);
        if (info.module.base != nullptr)
        {
            main_module = info.module;

            return true;
        }

        if (tries--)
        {
            printf("[%p] Retrying in %dms...\n", handle, wait_time);
            Sleep(wait_time);
            wait_time *= 2;
        } else
        {
            return false;
        }
    }
}

void RobloxProcess::FindAll()
{
    // find dwGame
    cout << "Finding dwGame" << endl;
    auto base = (DWORD)main_module.base;
    dwGame = ProcUtil::Scan(handle,base, Addresses::DataModel - 0x400000 + base);
    if (!dwGame)
    {
        cout << "Failed to find dwGame" << endl;
        retries_left--;
        return;
    }
    cout << "Found dwGame " << hex << dwGame << endl;
    dwPlayers = ProcUtil::GetService(handle,dwGame, "Players");
    DWORD dwWorkspace = ProcUtil::GetService(handle,dwGame, "Workspace");
    cout << "Found dwWorkspace " << hex << dwWorkspace << endl;
    dwLocalPlayer = ProcUtil::GetChildren(handle,dwPlayers)[0];
    cout << "Found dwLocalPlayer " << hex << dwLocalPlayer << endl;
    username = ProcUtil::GetName(handle,dwLocalPlayer);
    cout << "Found username " << username << endl;

    // 55 8B EC 6A FF 68 ? ? ? ? 64 A1 00 00 00 00 50 64 89 25 00 00 00 00 83 EC 1C 8B 55 0C 8D
    auto start = (const uint8_t *)main_module.base;
    auto end = start + main_module.size;
    auto printAddress = ProcUtil::ScanProcess(handle, "\x55\x8B\xEC\x6A\xFF\x68\x00\x00\x00\x00\x64\xA1\x00\x00\x00\x00\x50\x64\x89\x25\x00\x00\x00\x00\x83\xEC\x1C\x8B\x55\x0C\x8D\x45\x10", "xxxxxx????xx????xxxx????xxxxxxxxx", start, end);
    cout << "Found printAddress " << hex << printAddress << endl;
    const auto print = reinterpret_cast< unsigned int( __cdecl* )( int, const char*, ... ) >( printAddress );
    print( 0, "Found dwGame: %p\n");






}

void RobloxProcess::Tick()
{
    if (retries_left < 0) {
        return;
    }

}

HWND g_HWND=NULL;
BOOL CALLBACK EnumWindowsProcMy(HWND hwnd,LPARAM lParam)
{
    DWORD lpdwProcessId;
    GetWindowThreadProcessId(hwnd,&lpdwProcessId);
    if(lpdwProcessId==lParam)
    {
        char title[256];
        GetWindowText(hwnd, title, 256);
        if (strstr(title, "Roblox") == NULL)
        {
            return TRUE;
        }
        g_HWND=hwnd;
        return FALSE;
    }
    // if title not roblox, return false

    return TRUE;
}


void RobloxProcess::UpdateWindowTitle()
{
    cout << "Updating window title" << endl;
    if (hWnd == NULL)
    {
        DWORD dwProcessId = GetProcessId(handle);
        EnumWindows(EnumWindowsProcMy, dwProcessId);
        hWnd = g_HWND;
    }
    if (hWnd != NULL)
    {
        // get window title
        char title[256];
        GetWindowText(hWnd, title, 256);
        string newTitle = "Roblox - " + username;
        cout << "Setting window title to: " << newTitle << endl;
        SetWindowText(hWnd, newTitle.c_str());
    }
}

