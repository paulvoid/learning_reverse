#include "RobloxProcess.h"
#include <iostream>
#include <unordered_map>

using namespace std;
//<DWORD, RobloxProcess> AttachedProcesses;

unordered_map<DWORD, RobloxProcess> AttachedProcesses;
void PrintConsole();

int main() {
    PrintConsole();
    auto process = ProcUtil::GetProcessByImageName("RobloxOpen.exe");
    ProcUtil::ProcessInfo info = ProcUtil::ProcessInfo(process);
    if (info.module.base == nullptr)
    {
        cout << "Failed to find RobloxOpen.exe" << endl;
        return 0;
    }
    ProcUtil::ModuleInfo main_module = info.module;


    // "python38.dll"+003DF910






    std::cin.get();

    return 0;
}

void setConsoleColor(int color) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(hConsole, color);
}

void setConsoleFontSize(int size) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontInfo = {0};
    fontInfo.cbSize = sizeof(fontInfo);
    GetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
    fontInfo.dwFontSize.Y = size;
    SetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
}

void setConsoleBold(bool bold) {
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_FONT_INFOEX fontInfo = {0};
    fontInfo.cbSize = sizeof(fontInfo);
    GetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
    fontInfo.FontWeight = bold ? FW_BOLD : FW_NORMAL;
    SetCurrentConsoleFontEx(hConsole, FALSE, &fontInfo);
}


void PrintConsole(){
    // Thiết lập màu sắc và font cho console app
    // màu đỏ
    SetConsoleOutputCP(CP_UTF8);
    setvbuf(stdout, nullptr, _IOFBF, 1000);

    SetConsoleTitleW(L"TitleChanger");
    setConsoleColor(12);
    setConsoleFontSize(18);
    setConsoleBold(true);

    // Hiển thị tiêu đề và thông tin app
    cout << "====================" << endl;
    cout << "TitleChanger" << endl;
    cout << "Author: PaulVoid" << endl;
    cout << "Version: 1.0.0" << endl;
    cout << "Join our Discord server: https://discord.gg/techs" << endl;
    cout << "====================" << endl << endl;

}