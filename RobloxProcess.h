//
// Created by Administrator on 4/20/2023.
//

#ifndef REL_ROBLOXPROCESS_H
#define REL_ROBLOXPROCESS_H


#include "procutil.h"
#include "Addresses.h"
#include <iostream>

using namespace std;
class RobloxProcess {

public:
    HANDLE handle = NULL;
    ProcUtil::ModuleInfo main_module{};
    DWORD dwGame = NULL;
    DWORD dwPlayers = NULL;
    DWORD dwLocalPlayer = NULL;
    std::string username = "";

    HWND hWnd = NULL;

    int retries_left = 0;


    bool Attach(HANDLE process, int retry_count);

    bool BlockingLoadModuleInfo();

    // find dw all
    void FindAll();

    void Tick();

    void UpdateWindowTitle();
};

#endif //REL_ROBLOXPROCESS_H
