#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#include <stdbool.h>
#include <gdiplus.h>
#include <winuser.h>
#include <ctype.h>
#include <time.h>
#include <commctrl.h>

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "Comctl32.lib")

#define BUFFER_SIZE 1024
#define INTERVAL_MS 5000  // 5 seconds interval
#define BASE_OUTPUT_FOLDER _T("windbg_outputs")
#define COPY_TIMEOUT_SECONDS 30  // 30 seconds timeout for copy-paste operations

ULONG_PTR gdiplusToken;
bool scanningActive = false;

// Function declarations
void CaptureWinDbgText(HWND hwnd, const TCHAR *outputFolder, bool *quitDetected);
bool SaveClipboardTextToFile(const TCHAR *outputFileName, bool *quitDetected);
HWND FindWinDbgWindow(void);
void InitGDIPlus();
void CleanupGDIPlus();
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
void CaptureTextFromAllWindows(DWORD pid, const TCHAR *outputFolder, bool *quitDetected);
void CaptureTextFromMemory(DWORD pid, const TCHAR *outputFileName);
void CaptureModules(DWORD pid, const TCHAR *outputFileName);
bool GetProcessNameByPID(DWORD pid, TCHAR *processName, DWORD processNameSize);
void TerminateWinDbgProcess(DWORD pid);
void MoveCursorAndCopy(HWND hwnd, bool *timedOut);
void PositionCmdWindow();
void PrintWarningMessage();
void HandleErrorPopups();
void PromptUserToClickOK();
void StartScanning(HWND hwnd);
void StopScanning(HWND hwnd);

// GUI Function Declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void AddControls(HWND hwnd);

void InitGDIPlus() {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

void CleanupGDIPlus() {
    GdiplusShutdown(gdiplusToken);
}

void CaptureWinDbgText(HWND hwnd, const TCHAR *outputFolder, bool *quitDetected) {
    bool timedOut = false;
    MoveCursorAndCopy(hwnd, &timedOut);
    if (timedOut) {
        _tprintf(_T("Copy-paste operation timed out. Moving to next application...\n"));
        return;
    }
    TCHAR outputFileName[BUFFER_SIZE];
    _stprintf(outputFileName, _T("%s\\windbg_output_clipboard.txt"), outputFolder);
    if (SaveClipboardTextToFile(outputFileName, quitDetected)) {
        _tprintf(_T("Detected '=== Quitting ===' in clipboard text. Moving to next application...\n"));
    }
}

// Function to simulate mouse movement and key presses to copy text from WinDbg window
void MoveCursorAndCopy(HWND hwnd, bool *timedOut) {
    RECT rect;
    GetWindowRect(hwnd, &rect);

    // Define movement ranges within certain parts
    int movementRangeX = (rect.right - rect.left) / 3;
    int movementRangeY = (rect.bottom - rect.top) / 3;

    // Move cursor to the middle left of the WinDbg window
    SetCursorPos(rect.left + movementRangeX, rect.top + movementRangeY);
    Sleep(500);  // Wait for the cursor to move

    // Simulate mouse click to focus on the window
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    Sleep(500);  // Wait for the window to focus

    // Simulate Ctrl+A to select all text
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('A', 0, 0, 0);
    Sleep(500);  // Wait for selection
    keybd_event('A', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

    // Simulate Ctrl+C to copy the text
    keybd_event(VK_CONTROL, 0, 0, 0);
    keybd_event('C', 0, 0, 0);
    Sleep(500);  // Wait for copy
    keybd_event('C', 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_CONTROL, 0, KEYEVENTF_KEYUP, 0);

    // Start timer
    clock_t start_time = clock();
    while (!OpenClipboard(NULL)) {
        if (((clock() - start_time) / CLOCKS_PER_SEC) > COPY_TIMEOUT_SECONDS) {
            *timedOut = true;
            return;
        }
        Sleep(100);  // Check every 100 ms
    }
    CloseClipboard();
}

// Function to save the clipboard text to a file
bool SaveClipboardTextToFile(const TCHAR *outputFileName, bool *quitDetected) {
    bool foundQuit = false;
    if (OpenClipboard(NULL)) {
        HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
        if (hClipboardData) {
            WCHAR *pchData = (WCHAR*)GlobalLock(hClipboardData);
            if (pchData) {
                FILE *outputFile = _tfopen(outputFileName, _T("w"));
                if (outputFile) {
                    _ftprintf(outputFile, _T("%ls\n"), pchData);
                    if (wcsstr(pchData, L"=== Quitting ===") != NULL) {  // Use wcsstr for wide characters
                        foundQuit = true;
                        *quitDetected = true;
                    }
                    fclose(outputFile);
                }
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
    return foundQuit;
}

// Function to find the WinDbg window
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    TCHAR windowTitle[BUFFER_SIZE];
    GetWindowText(hwnd, windowTitle, BUFFER_SIZE);
    if (_tcsstr(windowTitle, _T("WinDbg")) != NULL || _tcsstr(windowTitle, _T("Windows GUI Symbolic Debugger")) != NULL) {
        HWND *targetWnd = (HWND *)lParam;
        *targetWnd = hwnd;
        return FALSE;  // Stop enumeration
    }
    return TRUE;  // Continue enumeration
}

HWND FindWinDbgWindow() {
    HWND hwnd = NULL;
    EnumWindows(EnumWindowsProc, (LPARAM)&hwnd);
    return hwnd;
}

// Function to terminate the WinDbg process
void TerminateWinDbgProcess(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess != NULL) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    } else {
        _tprintf(_T("Failed to terminate WinDbg process (PID: %d)\n"), pid);
    }
}

// Function to enumerate all windows for a specific PID and capture text
void CaptureTextFromAllWindows(DWORD pid, const TCHAR *outputFolder, bool *quitDetected) {
    HWND hwnd = FindWinDbgWindow();
    if (hwnd) {
        bool complete = false;
        while (!complete && scanningActive) {
            CaptureWinDbgText(hwnd, outputFolder, quitDetected);
            complete = *quitDetected;
            if (!complete) {
                HandleErrorPopups();  // Handle error pop-ups
            }
        }
        if (complete) {
            TerminateWinDbgProcess(pid);
        }
    }
}

// Function to read memory of a process and transcribe text
void CaptureTextFromMemory(DWORD pid, const TCHAR *outputFileName) {
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == NULL) {
        _tprintf(_T("Failed to open process %d\n"), pid);
        return;
    }

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    MEMORY_BASIC_INFORMATION memInfo;

    LPVOID addr = sysInfo.lpMinimumApplicationAddress;
    FILE *outputFile = _tfopen(outputFileName, _T("w"));

    while (addr < sysInfo.lpMaximumApplicationAddress) {
        if (VirtualQueryEx(hProcess, addr, &memInfo, sizeof(memInfo)) == sizeof(memInfo)) {
            if (memInfo.State == MEM_COMMIT && (memInfo.Type == MEM_MAPPED || memInfo.Type == MEM_PRIVATE)) {
                char *buffer = (char*)malloc(memInfo.RegionSize);
                SIZE_T bytesRead;
                if (ReadProcessMemory(hProcess, addr, buffer, memInfo.RegionSize, &bytesRead)) {
                    fwrite(buffer, 1, bytesRead, outputFile);
                }
                free(buffer);
            }
            addr = (LPVOID)((SIZE_T)addr + memInfo.RegionSize);
        } else {
            addr = (LPVOID)((SIZE_T)addr + 0x1000);  // Move to next page
        }
    }

    fclose(outputFile);
    CloseHandle(hProcess);

    // Transcribe memory content to readable text
    FILE *memFile = _tfopen(outputFileName, _T("r"));
    FILE *transcribedFile = _tfopen(_T("transcribed_memory_output.txt"), _T("w"));
    if (memFile && transcribedFile) {
        char buffer[BUFFER_SIZE];
        while (fgets(buffer, BUFFER_SIZE, memFile)) {
            for (int i = 0; buffer[i] != '\0'; i++) {
                if (isprint(buffer[i])) {
                    fputc(buffer[i], transcribedFile);
                } else {
                    fputc('.', transcribedFile);
                }
            }
        }
        fclose(memFile);
        fclose(transcribedFile);
    }
}

// Function to capture loaded modules of a process
void CaptureModules(DWORD pid, const TCHAR *outputFileName) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        _tprintf(_T("Failed to open process %d\n"), pid);
        return;
    }

    HMODULE hMods[1024];
    DWORD cbNeeded;
    FILE *outputFile = _tfopen(outputFileName, _T("w"));

    if (EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        for (unsigned int i = 0; i < (cbNeeded / sizeof(HMODULE)); i++) {
            TCHAR szModName[MAX_PATH];

            if (GetModuleFileNameEx(hProcess, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR))) {
                _ftprintf(outputFile, _T("%s\n"), szModName);
            }
        }
    }

    fclose(outputFile);
    CloseHandle(hProcess);
}

// Function to get process name by PID
bool GetProcessNameByPID(DWORD pid, TCHAR *processName, DWORD processNameSize) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess) {
        HMODULE hMod;
        DWORD cbNeeded;
        if (EnumProcessModules(hProcess, &hMod, sizeof(hMod), &cbNeeded)) {
            GetModuleBaseName(hProcess, hMod, processName, processNameSize / sizeof(TCHAR));
        } else {
            _tcsncpy(processName, _T("Unknown"), processNameSize / sizeof(TCHAR));
        }
        CloseHandle(hProcess);
    } else {
        _tcsncpy(processName, _T("Unknown"), processNameSize / sizeof(TCHAR));
        return false;
    }
    return true;
}

void PositionCmdWindow() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd != NULL) {
        // Get the screen dimensions
        RECT desktop;
        const HWND hDesktop = GetDesktopWindow();
        GetWindowRect(hDesktop, &desktop);
        int width = desktop.right;
        int height = desktop.bottom;

        // Move the console window to the right half of the screen
        SetWindowPos(hwnd, 0, width / 2, 0, width / 2, height, SWP_NOZORDER);
    }
}

void PrintWarningMessage() {
    printf("DO NOT OVERLAY ANYTHING OVER THE WINDBG SOURCE WINDOW!\n");
}

// Function to handle error popups
void HandleErrorPopups() {
    HWND hwnd = FindWindow(NULL, _T("Error"));
    if (hwnd) {
        // Simulate pressing "OK" on the error popup
        RECT rect;
        GetWindowRect(hwnd, &rect);
        SetCursorPos(rect.left + (rect.right - rect.left) / 2, rect.top + (rect.bottom - rect.top) / 2);
        Sleep(500);  // Wait for the cursor to move
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Sleep(500);  // Wait for the click to register
        PromptUserToClickOK();
    }
}

// Function to prompt user to click "OK" on error popups
void PromptUserToClickOK() {
    _tprintf(_T("An error popup has been detected. Please click 'OK' to continue.\n"));
}

// Function to start scanning
void StartScanning(HWND hwnd) {
    scanningActive = true;
    EnableWindow(GetDlgItem(hwnd, 1), FALSE);  // Disable the "Start Scanning" button
    EnableWindow(GetDlgItem(hwnd, 2), TRUE);   // Enable the "Stop Scanning" button
}

// Function to stop scanning
void StopScanning(HWND hwnd) {
    scanningActive = false;
    EnableWindow(GetDlgItem(hwnd, 1), TRUE);  // Enable the "Start Scanning" button
    EnableWindow(GetDlgItem(hwnd, 2), FALSE); // Disable the "Stop Scanning" button
}

// Function to create GUI controls
void AddControls(HWND hwnd) {
    CreateWindowW(L"Button", L"Start Scanning", WS_VISIBLE | WS_CHILD, 50, 50, 150, 50, hwnd, (HMENU)1, NULL, NULL);
    CreateWindowW(L"Button", L"Stop Scanning", WS_VISIBLE | WS_CHILD, 50, 150, 150, 50, hwnd, (HMENU)2, NULL, NULL);
}

// Window procedure function
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            AddControls(hwnd);
            StopScanning(hwnd);  // Ensure scanning is initially stopped
            break;
        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case 1:  // Start Scanning button
                    StartScanning(hwnd);
                    break;
                case 2:  // Stop Scanning button
                    StopScanning(hwnd);
                    break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Main function to create GUI and handle messages
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    wc.lpszClassName = L"MainWindowClass";
    wc.hInstance = hInstance;
    wc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));  // Black background
    wc.lpfnWndProc = WndProc;
    wc.hCursor = LoadCursor(0, IDC_ARROW);

    RegisterClassW(&wc);
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"WinDbg Text Extractor", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 300, 250, 0, 0, hInstance, 0);

    // Set window text colors and font
    HFONT hFont = CreateFont(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, VARIABLE_PITCH, TEXT("Consolas"));
    SendMessage(hwnd, WM_SETFONT, (WPARAM)hFont, TRUE);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}

// The main loop for the scanning process
void ScanningLoop() {
    PositionCmdWindow();
    PrintWarningMessage();

    InitGDIPlus();

    TCHAR baseOutputPath[MAX_PATH];
    GetModuleFileName(NULL, baseOutputPath, MAX_PATH);
    PathRemoveFileSpec(baseOutputPath);
    PathAppend(baseOutputPath, BASE_OUTPUT_FOLDER);
    CreateDirectory(baseOutputPath, NULL);

    while (true) {
        if (scanningActive) {
            HWND hwnd = FindWinDbgWindow();
            if (hwnd) {
                DWORD pid;
                GetWindowThreadProcessId(hwnd, &pid);

                TCHAR processName[BUFFER_SIZE];
                if (!GetProcessNameByPID(pid, processName, sizeof(processName))) {
                    _tprintf(_T("Failed to get process name for PID %d\n"), pid);
                    continue;
                }

                // Create output folder for each process
                TCHAR outputFolder[BUFFER_SIZE];
                _stprintf(outputFolder, _T("%s\\%d_%s"), baseOutputPath, pid, processName);
                CreateDirectory(outputFolder, NULL);

                _tprintf(_T("Found WinDbg window for process %s (PID: %d). Capturing text from main window, popups, and memory...\n"), processName, pid);

                bool quitDetected = false;

                CaptureTextFromAllWindows(pid, outputFolder, &quitDetected);

                TCHAR memoryOutputFileName[BUFFER_SIZE];
                _stprintf(memoryOutputFileName, _T("%s\\windbg_output_memory.txt"), outputFolder);
                CaptureTextFromMemory(pid, memoryOutputFileName);

                TCHAR modulesOutputFileName[BUFFER_SIZE];
                _stprintf(modulesOutputFileName, _T("%s\\windbg_output_modules.txt"), outputFolder);
                CaptureModules(pid, modulesOutputFileName);

                if (quitDetected) {
                    TerminateWinDbgProcess(pid);
                }
            } else {
                _tprintf(_T("WinDbg window not found. Retrying...\n"));
            }
        }
        Sleep(INTERVAL_MS);
    }

    CleanupGDIPlus();
}

int main(void) {
    // Create a separate thread for the scanning loop
    CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)ScanningLoop, NULL, 0, NULL);

    // Run the GUI
    HINSTANCE hInstance = GetModuleHandle(NULL);
    wWinMain(hInstance, NULL, NULL, SW_SHOWNORMAL);

    return 0;
}
