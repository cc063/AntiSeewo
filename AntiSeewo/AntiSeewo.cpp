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

namespace fs = std::filesystem;
using json = nlohmann::json;

// Read JSON configuration file
json readConfigFile(const std::string& filePath) {
    std::ifstream configFile(filePath);
    if (!configFile.is_open()) {
        throw std::runtime_error("Unable to open configuration file: " + filePath);
    }

    json config;
    configFile >> config;
    return config;
}

// Check if the current time is within the given time ranges
bool isCurrentTimeInRange(const std::vector<std::pair<std::string, std::string>>& timeRanges) {
    std::cout << "Checking if the current time is within the given time ranges..." << std::endl;

    // Get the current time
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

// Check if the specified file exists in any disk root directory
bool isFileInAnyDisk(const std::string& fileName) {
    std::cout << "Quickly checking if the file exists in any disk root directory: " << fileName << std::endl;

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
    // For non-Windows systems, only check the root directory
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

// Forcefully terminate specified application processes
void killProcesses(const std::vector<std::string>& appNames) {
    std::cout << "Attempting to terminate specified application processes..." << std::endl;

    // Store target process names in a hash set for faster matching
    std::unordered_set<std::string> targetProcesses(appNames.begin(), appNames.end());

    // Create a snapshot of all processes
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        std::cerr << "Unable to create process snapshot." << std::endl;
        return;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hSnapshot, &pe)) {
        do {
            // Convert process name to std::string for comparison
            std::wstring wstr(pe.szExeFile);
            std::string processName(wstr.begin(), wstr.end());

            if (targetProcesses.find(processName) != targetProcesses.end()) {
                std::cout << "Found process: " << processName << ", attempting to terminate..." << std::endl;
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 0);
                    CloseHandle(hProcess);
                    std::cout << "Terminated process: " << processName << std::endl;
                } else {
                    std::cerr << "Unable to open process: " << processName << std::endl;
                }
            }
        } while (Process32Next(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
}

int main() {
    // Hide the console window
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        ShowWindow(hwnd, SW_HIDE);
    }

    std::cout << "Program started..." << std::endl;

    // Default parameters
    std::vector<std::pair<std::string, std::string>> defaultTimeRanges = {
        {"12:10", "13:40"},
        {"17:20", "18:30"}
    };
    std::vector<std::string> defaultAppsToKill = {"SeewoCore.exe",
    "SeewoAbility.exe",
    "SeewoServiceAssistant.exe",
    "media_capture.exe",
    "screenCapture.exe",
    "rtcRemoteDesktop.exe" };
    std::string defaultFileNameToCheck = "antiseewo.txt";

    // Configuration parameters
    std::vector<std::pair<std::string, std::string>> timeRanges = defaultTimeRanges;
    std::vector<std::string> appsToKill = defaultAppsToKill;
    std::string fileNameToCheck = defaultFileNameToCheck;

    // Check and read configuration file in the program directory
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
        } catch (const std::exception& e) {
            std::cerr << "Error reading configuration file: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "Configuration file not found: " << configFilePath << ", using default configuration." << std::endl;
    }

    // Shared variable indicating whether the current time is within the given time range or the file exists
    std::atomic<bool> isInTimeRange(false);

    // Thread 1: Check time range or file existence every 30 seconds
    std::thread timeChecker([&]() {
        while (true) {
            std::cout << "Thread 1: Checking time range and file status..." << std::endl;
            bool inTimeRange = isCurrentTimeInRange(timeRanges);
            bool fileExists = isFileInAnyDisk(fileNameToCheck);
            isInTimeRange.store(inTimeRange || fileExists);
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    });

    // Thread 2: Check variable status every 5 seconds, terminate processes if true
    std::thread processKiller([&]() {
        while (true) {
            if (isInTimeRange.load()) {
                std::cout << "Thread 2: Condition met, attempting to terminate processes..." << std::endl;
                killProcesses(appsToKill);
            } else {
                std::cout << "Thread 2: Condition not met, waiting..." << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::seconds(3));
        }
    });

    // Wait for threads to finish (they won't actually finish)
    timeChecker.join();
    processKiller.join();

    return 0;
}