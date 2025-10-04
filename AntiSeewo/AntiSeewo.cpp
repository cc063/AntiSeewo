#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <filesystem>
#include <windows.h>
#include <tlhelp32.h>
#include <thread>
#include <atomic>
#include <chrono>
#include <unordered_set>
#include <fstream>
#include "ThirdParty/nlohmann/json.hpp"
#include <shellapi.h> // For ShellExecute
#include <cstdlib> // For system()
#include <commctrl.h> // For InitCommonControls
#include <string>

namespace fs = std::filesystem;
using json = nlohmann::json;

json readConfigFile(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Unable to open configuration file: " + filePath);
    }

    json config;
    configFile >> config;
    return config;
}

bool isCurrentTimeInRange(const std::vector<std::pair<std::string, std::string>>& timeRanges) {
    std::cout << "Checking if the current time is within the specified time ranges..." << std::endl;

    time_t now = time(0);
    tm localTime;
    localtime_s(&localTime, &now);

    char currentTime[6]; // HH:MM
    strftime(currentTime, sizeof(currentTime), "%H:%M", &localTime);

    std::string currentTimeStr(currentTime);
    std::cout << "Current time: " << currentTimeStr << std::endl;

    for (const auto& range : timeRanges) {
        std::cout << "Checking time range: " << range.first << " - " << range.second << std::endl;
        if (currentTimeStr >= range.first && currentTimeStr <= range.second) {
            std::cout << "Current time is within the range." << std::endl;
            return true;
        }
    }

    std::cout << "Current time is not within any range." << std::endl;
    return false;
}

bool isFileInAnyDisk(const std::string& fileName) {
    std::cout << "Checking if the file exists in any disk root directory: " << fileName << std::endl;

#ifdef _WIN32
    char drive[] = "A:";
    for (char letter = 'A'; letter <= 'Z'; ++letter) {
        drive[0] = letter;
        if (fs::exists(drive) && fs::is_directory(drive)) {
            std::cout << "Checking disk root: " << drive << std::endl;
            for (const auto& entry : fs::directory_iterator(drive, fs::directory_options::skip_permission_denied)) {
                if (entry.path().filename() == fileName) {
                    std::cout << "File found: " << entry.path() << std::endl;
                    return true;
                }
            }
        }
    }
#else
    for (const auto& entry : fs::directory_iterator("/", fs::directory_options::skip_permission_denied)) {
        if (entry.path().filename() == fileName) {
            std::cout << "File found: " << entry.path() << std::endl;
            return true;
        }
    }
#endif

    std::cout << "File not found: " << fileName << std::endl;
    return false;
}

void killProcesses(const std::vector<std::string>& appNames) {
    std::cout << "Attempting to terminate specified application processes..." << std::endl;

    std::unordered_set<std::string> targetProcesses(appNames.begin(), appNames.end());
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Unable to create process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            std::wstring wstr(pe.szExeFile);
            std::string processName(wstr.begin(), wstr.end());

            if (targetProcesses.find(processName) != targetProcesses.end()) {
                std::cout << "Found process: " << processName << ", attempting to terminate..." << std::endl;
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    std::cout << "Terminated process: " << processName << std::endl;
                }
                else {
                    std::cerr << "Unable to open process: " << processName << std::endl;
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

bool isRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;

    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

// Function to register the program as a scheduled task
void registerScheduledTask() {
    std::cout << "Registering the program as a scheduled task..." << std::endl;

    // Delete the existing task if it exists
    std::wstring deleteCommand = L"schtasks /delete /tn \"AntiSeewo\" /f";
    int deleteResult = _wsystem(deleteCommand.c_str());
    if (deleteResult == 0) {
        std::cout << "Existing scheduled task deleted successfully." << std::endl;
    }
    else {
        std::cout << "No existing scheduled task to delete or failed to delete." << std::endl;
    }

    // Get the path to the current executable
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);

    // Convert the executable path to a string for the command
    std::wstring createCommand = L"schtasks /create /tn \"AntiSeewo\" /tr \"";
    createCommand += exePath;
    createCommand += L"\" /sc onlogon /rl highest /f";

    // Execute the command to register the task
    int createResult = _wsystem(createCommand.c_str());
    if (createResult == 0) {
        std::cout << "Scheduled task registered successfully." << std::endl;
    }
    else {
        std::cerr << "Failed to register the scheduled task. Error code: " << createResult << std::endl;
    }
}

bool isAnyProcessRunning(const std::vector<std::string>& processNames) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Unable to create process snapshot." << std::endl;
        return false;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            std::wstring wstr(pe.szExeFile);
            std::string processName(wstr.begin(), wstr.end());

            if (std::find(processNames.begin(), processNames.end(), processName) != processNames.end()) {
                CloseHandle(hSnapshot);
                return true;
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return false;
}

// Function to terminate a process tree
void terminateProcessTree(DWORD pid) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Unable to create process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            if (pe.th32ParentProcessID == pid) {
                terminateProcessTree(pe.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
}

// Function to terminate all processes in the given list along with their process trees
void killProcessTrees(const std::vector<std::string>& processNames) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Unable to create process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            std::wstring wstr(pe.szExeFile);
            std::string processName(wstr.begin(), wstr.end());

            if (std::find(processNames.begin(), processNames.end(), processName) != processNames.end()) {
                terminateProcessTree(pe.th32ProcessID);
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

// Thread function to monitor and terminate process trees
void monitorAndTerminate(const std::vector<std::string>& monitorProcesses, const std::vector<std::string>& targetProcesses) {
    try {
        while (true) {
            std::cout << "Monitoring processes..." << std::endl;
            if (isAnyProcessRunning(monitorProcesses)) {
                std::cout << "Monitor condition met. Terminating target processes..." << std::endl;
                for (const auto& process : targetProcesses) {
                    std::cout << "Attempting to terminate: " << process << std::endl;
                }
                killProcessTrees(targetProcesses);
                std::this_thread::sleep_for(std::chrono::seconds(300));
            } else {
                std::cout << "Monitor condition not met. Waiting..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in monitorAndTerminate: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in monitorAndTerminate." << std::endl;
    }
}

NOTIFYICONDATA nid;
HMENU hTrayMenu;

// Helper function to convert std::string to std::wstring
std::wstring stringToWString(const std::string& str) {
    return std::wstring(str.begin(), str.end());
}

// Function to handle tray menu commands
void ShowConfigEditor() {
    // Open the configuration file in the default text editor
    ShellExecute(NULL, L"open", L"config.json", NULL, NULL, SW_SHOWNORMAL);
}

void ExitProgram() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    PostQuitMessage(0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case 1: // Edit Config
            ShowConfigEditor();
            break;
        case 2: // Exit
            ExitProgram();
            break;
        }
        break;

    case WM_USER + 1: // Tray icon message
        if (lParam == WM_RBUTTONDOWN) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hTrayMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
        }
        break;

    case WM_DESTROY:
        ExitProgram();
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void AddTrayIcon(HWND hwnd) {
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(nid.szTip, L"AntiSeewo");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

int RunTrayIcon() {
    const wchar_t* className = L"AntiSeewoTrayClass";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = className;

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0, className, L"AntiSeewo Tray", 0, 0, 0, 0, 0, NULL, NULL, GetModuleHandle(NULL), NULL);

    AddTrayIcon(hwnd);

    hTrayMenu = CreatePopupMenu();
    AppendMenu(hTrayMenu, MF_STRING, 1, L"Edit Config");
    AppendMenu(hTrayMenu, MF_STRING, 2, L"Exit");

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}

int main() {
    // Hide the console window
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }

    if (!isRunningAsAdmin()) {
        std::cout << "Program is not running as administrator. Attempting to elevate..." << std::endl;

        wchar_t exePath[MAX_PATH];
        GetModuleFileNameW(NULL, exePath, MAX_PATH);

        SHELLEXECUTEINFO sei = { sizeof(SHELLEXECUTEINFO) };
        sei.lpVerb = L"runas";
        sei.lpFile = exePath;
        sei.hwnd = NULL;
        sei.nShow = SW_NORMAL;

        if (!ShellExecuteEx(&sei)) {
            std::cerr << "Failed to elevate to administrator privileges." << std::endl;
            return 1;
        }

        return 0; // Exit the current instance
    }

    std::cout << "Program is running as administrator." << std::endl;

    // Register the program as a scheduled task
    registerScheduledTask();
    std::cout << "Program started..." << std::endl;

    std::vector<std::pair<std::string, std::string>> defaultTimeRanges = {
        {"12:10", "13:40"},
        {"17:20", "18:30"}
    };
    std::vector<std::string> defaultAppsToKill = { "SeewoCore.exe",
    "SeewoAbility.exe",
    "SeewoServiceAssistant.exe",
    "media_capture.exe",
    "screenCapture.exe",
    "rtcRemoteDesktop.exe" };
    std::string defaultFileNameToCheck = "antiseewo.txt";

    std::vector<std::string> defaultMonitorProcesses = { "screen_capture.exe", "rtcRemoteDesktop.exe", "notepad.exe" };
    std::vector<std::string> defaultTargetProcesses = { "explorer.exe" };

    std::vector<std::pair<std::string, std::string>> timeRanges = defaultTimeRanges;
    std::vector<std::string> appsToKill = defaultAppsToKill;
    std::string fileNameToCheck = defaultFileNameToCheck;
    std::vector<std::string> monitorProcesses = defaultMonitorProcesses;
    std::vector<std::string> targetProcesses = defaultTargetProcesses;

    std::string configFilePath = "config.json";
    if (fs::exists(configFilePath)) {
        try {
            std::cout << "Reading configuration file: " << configFilePath << std::endl;
            json config = readConfigFile(configFilePath);

            if (config.contains("timeRanges")) {
                timeRanges.clear();
                for (const auto& range : config["timeRanges"]) {
                    timeRanges.emplace_back(range["start"], range["end"]);
                }
            }

            if (config.contains("appsToKill")) {
                appsToKill = config["appsToKill"].get<std::vector<std::string>>();
            }

            if (config.contains("fileNameToCheck")) {
                fileNameToCheck = config["fileNameToCheck"].get<std::string>();
            }

            if (config.contains("monitorProcesses")) {
                monitorProcesses = config["monitorProcesses"].get<std::vector<std::string>>();
            }

            if (config.contains("targetProcesses")) {
                targetProcesses = config["targetProcesses"].get<std::vector<std::string>>();
            }
        }
        catch (const std::exception& e) {
            std::cerr << "Error reading configuration file: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Configuration file not found: " << configFilePath << ", creating default configuration." << std::endl;

        json defaultConfig = {
            {"timeRanges", json::array({
                { {"start", "12:10"}, {"end", "13:40"} },
                { {"start", "17:20"}, {"end", "18:30"} }
            })},
            {"appsToKill", defaultAppsToKill},
            {"fileNameToCheck", defaultFileNameToCheck},
            {"monitorProcesses", defaultMonitorProcesses},
            {"targetProcesses", defaultTargetProcesses}
        };

        std::ofstream configFile(configFilePath);
        if (configFile.is_open()) {
            configFile << defaultConfig.dump(4); // Pretty print with 4 spaces
            configFile.close();
            std::cout << "Default configuration file created: " << configFilePath << std::endl;
        } else {
            std::cerr << "Failed to create default configuration file: " << configFilePath << std::endl;
        }
    }

    std::atomic<bool> isInTimeRange(false);

    std::thread timeChecker([&]() {
        while (true) {
            std::cout << "Thread 1: Checking time range and file status..." << std::endl;
            bool inTimeRange = isCurrentTimeInRange(timeRanges);
            bool fileExists = isFileInAnyDisk(fileNameToCheck);
            isInTimeRange.store(inTimeRange || fileExists);
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        });

    std::thread processKiller([&]() {
        while (true) {
            if (isInTimeRange.load()) {
                std::cout << "Thread 2: Condition met, attempting to terminate processes..." << std::endl;
                killProcesses(appsToKill);
            }
            else {
                std::cout << "Thread 2: Condition not met, waiting..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
        });

    std::thread monitorThread(monitorAndTerminate, monitorProcesses, targetProcesses);
    monitorThread.detach();

    RunTrayIcon();

    timeChecker.join();
    processKiller.join();

    return 0;
}