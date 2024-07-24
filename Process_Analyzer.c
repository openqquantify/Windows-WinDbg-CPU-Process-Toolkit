#include <windows.h>
#include <psapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <tchar.h>
#include <stdbool.h>
#include <sddl.h>
#include <aclapi.h>
#include <time.h>

#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")

#define BUFFER_SIZE 1024
#define WINDBG_PATH _T("C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\windbg.exe")
#define COMMANDS_SCRIPT_PATH _T("windbg_commands.txt")
#define OUTPUT_FOLDER _T("windbg_output")
#define ERROR_LOG_FILE _T("error_log.txt")
#define DEBUG_LOG_FILE _T("debug_log.txt")
#define SUMMARY_FILE _T("summary.txt")
#define WINDBG_TIMEOUT_MS 60000  // 60 seconds timeout for WinDbg

// Function declarations
void LogErrorAndExit(const TCHAR *message);
void LogError(const TCHAR *message, DWORD pid, const TCHAR *processFolder);
void LogDebug(const TCHAR *message, DWORD pid, const TCHAR *processFolder);
void LogSummary(const TCHAR *message);
bool GetAllProcessIDs(DWORD *processIDs, DWORD arraySize, DWORD *bytesReturned);
bool GetProcessNameByPID(DWORD pid, TCHAR *processName, DWORD processNameSize);
void CreateDirectoryIfNotExists(LPCTSTR path);
HWND FindWinDbgWindow(void);
void CaptureWinDbgText(HWND hwnd, const TCHAR *outputFileName);
bool RunWinDbg(LPCTSTR windbgPath, DWORD pid, LPCTSTR commandsScriptPath, LPCTSTR outputFileName, const TCHAR *processFolder);
void ClassifyProcesses(DWORD pid, const TCHAR *processName, const TCHAR *processFolder);
bool IsRunAsAdmin(void);
void CreateWinDbgCommandsScript(void);
void CaptureWinDbgOutput(const TCHAR *outputFileName, const TCHAR *processFolder);
void MoveCursorAndCopy(HWND hwnd);
void SaveClipboardTextToFile(const TCHAR *outputFileName);
void HandleErrorPopups();
void TerminateWinDbgProcess(DWORD pid);

// Function implementations
void LogErrorAndExit(const TCHAR *message) {
    _tprintf(_T("%s (%d).\n"), message, GetLastError());
    exit(1);
}

void LogError(const TCHAR *message, DWORD pid, const TCHAR *processFolder) {
    TCHAR errorLogFileName[BUFFER_SIZE];
    _stprintf(errorLogFileName, _T("%s\\%s"), processFolder, ERROR_LOG_FILE);
    FILE *errorLog = _tfopen(errorLogFileName, _T("a"));
    if (errorLog) {
        _ftprintf(errorLog, _T("[%s %s] Error for PID %d: %s (%d)\n"), __DATE__, __TIME__, pid, message, GetLastError());
        fclose(errorLog);
    }
    _tprintf(_T("ERROR: %s (PID: %d, %d)\n"), message, pid, GetLastError());
}

void LogDebug(const TCHAR *message, DWORD pid, const TCHAR *processFolder) {
    TCHAR debugLogFileName[BUFFER_SIZE];
    _stprintf(debugLogFileName, _T("%s\\%s"), processFolder, DEBUG_LOG_FILE);
    FILE *debugLog = _tfopen(debugLogFileName, _T("a"));
    if (debugLog) {
        _ftprintf(debugLog, _T("[%s %s] Debug for PID %d: %s\n"), __DATE__, __TIME__, pid, message);
        fclose(debugLog);
    }
    _tprintf(_T("DEBUG: %s (PID: %d)\n"), message, pid);
}

void LogSummary(const TCHAR *message) {
    FILE *summaryFile = _tfopen(SUMMARY_FILE, _T("a"));
    if (summaryFile) {
        _ftprintf(summaryFile, _T("[%s %s] %s\n"), __DATE__, __TIME__, message);
        fclose(summaryFile);
    }
    _tprintf(_T("SUMMARY: %s\n"), message);
}

// Function to retrieve all running process IDs
bool GetAllProcessIDs(DWORD *processIDs, DWORD arraySize, DWORD *bytesReturned) {
    if (!EnumProcesses(processIDs, arraySize, bytesReturned)) {
        LogError(_T("EnumProcesses failed"), 0, _T("."));
        return false;
    }
    LogDebug(_T("Retrieved all process IDs."), 0, _T("."));
    return true;
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
    LogDebug(_T("Retrieved process name."), pid, _T("."));
    return true;
}

// Function to create a directory if it doesn't exist
void CreateDirectoryIfNotExists(LPCTSTR path) {
    if (!CreateDirectory(path, NULL)) {
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
            LogErrorAndExit(_T("Failed to create directory"));
        }
    }
    LogDebug(_T("Directory created or already exists."), 0, path);
}

// Function to find the WinDbg window
HWND FindWinDbgWindow() {
    HWND hwnd = FindWindow(NULL, _T("WinDbg"));
    if (!hwnd) {
        LogError(_T("Failed to find WinDbg window"), 0, _T("."));
    }
    return hwnd;
}

// Function to capture text from WinDbg window
void CaptureWinDbgText(HWND hwnd, const TCHAR *outputFileName) {
    TCHAR buffer[BUFFER_SIZE];
    FILE *outputFile = _tfopen(outputFileName, _T("w"));
    if (!outputFile) {
        LogError(_T("Failed to open output file"), 0, _T("."));
        return;
    }

    // Retrieve the text from the WinDbg window
    SendMessage(hwnd, WM_GETTEXT, (WPARAM)BUFFER_SIZE, (LPARAM)buffer);

    // Write the retrieved text to the output file
    _fputts(buffer, outputFile);

    fclose(outputFile);
    LogDebug(_T("Captured WinDbg text and saved to file"), 0, _T("."));
}

// Function to simulate mouse movement and key presses to copy text from WinDbg window
void MoveCursorAndCopy(HWND hwnd) {
    RECT rect;
    GetWindowRect(hwnd, &rect);

    // Move cursor to the middle left of the WinDbg window
    SetCursorPos(rect.left + (rect.right - rect.left) / 3, rect.top + (rect.bottom - rect.top) / 3);
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
}

// Function to save the clipboard text to a file
void SaveClipboardTextToFile(const TCHAR *outputFileName) {
    if (OpenClipboard(NULL)) {
        HANDLE hClipboardData = GetClipboardData(CF_TEXT);
        if (hClipboardData) {
            char *pchData = (char*)GlobalLock(hClipboardData);
            if (pchData) {
                FILE *outputFile = _tfopen(outputFileName, _T("w"));
                if (outputFile) {
                    _ftprintf(outputFile, _T("%hs"), pchData);
                    fclose(outputFile);
                }
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
}

// Function to handle error popups and close WinDbg if needed
void HandleErrorPopups(DWORD pid) {
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

        // Terminate WinDbg process
        TerminateWinDbgProcess(pid);
    }
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

// Function to run WinDbg with a script and capture its output
bool RunWinDbg(LPCTSTR windbgPath, DWORD pid, LPCTSTR commandsScriptPath, LPCTSTR outputFileName, const TCHAR *processFolder) {
    TCHAR commandLine[BUFFER_SIZE];
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    HANDLE hOutputFile;
    SECURITY_ATTRIBUTES sa;

    LogDebug(_T("Preparing to run WinDbg."), pid, processFolder);

    // Set the security attributes struct
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.lpSecurityDescriptor = NULL;
    sa.bInheritHandle = TRUE;

    // Prepare command line
    _stprintf(commandLine, _T("\"%s\" -p %d -c \"$$><%s\""), windbgPath, pid, commandsScriptPath);

    // Open the output file
    hOutputFile = CreateFile(outputFileName, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOutputFile == INVALID_HANDLE_VALUE) {
        LogError(_T("Failed to create output file"), pid, processFolder);
        return false;
    }

    // Initialize STARTUPINFO structure
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hOutputFile;
    si.hStdError = hOutputFile;

    // Initialize PROCESS_INFORMATION structure
    ZeroMemory(&pi, sizeof(pi));

    // Create WinDbg process
    if (!CreateProcess(NULL, commandLine, NULL, NULL, TRUE, CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL, &si, &pi)) {
        LogError(_T("CreateProcess failed"), pid, processFolder);
        CloseHandle(hOutputFile);
        return false;
    }

    LogDebug(_T("WinDbg process created successfully."), pid, processFolder);

    // Wait for the WinDbg process to finish or timeout
    DWORD waitResult = WaitForSingleObject(pi.hProcess, WINDBG_TIMEOUT_MS);
    if (waitResult == WAIT_TIMEOUT) {
        LogDebug(_T("WinDbg process timed out, terminating."), pid, processFolder);
        TerminateProcess(pi.hProcess, 0);
    }

    // Check the exit code of the WinDbg process
    DWORD exitCode;
    if (GetExitCodeProcess(pi.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
        LogDebug(_T("WinDbg process still active, terminating."), pid, processFolder);
        TerminateProcess(pi.hProcess, 0);
    }

    // Ensure the WinDbg window is in the foreground
    HWND hwnd = FindWinDbgWindow();
    if (hwnd) {
        SetForegroundWindow(hwnd);
        Sleep(1000);  // Wait for the window to be in the foreground

        // Move the cursor, select all text, and copy it
        MoveCursorAndCopy(hwnd);
        Sleep(1000);  // Wait for the copy operation

        // Save the copied text from the clipboard to a file
        SaveClipboardTextToFile(outputFileName);
    } else {
        LogError(_T("Failed to find WinDbg window"), pid, processFolder);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutputFile);
    return true;
}

// Function to classify processes based on criteria
void ClassifyProcesses(DWORD pid, const TCHAR *processName, const TCHAR *processFolder) {
    // Example classification based on process name
    const TCHAR *classification;
    if (_tcsstr(processName, _T("chrome")) != NULL) {
        classification = _T("Browser");
    } else if (_tcsstr(processName, _T("svchost")) != NULL) {
        classification = _T("System Service");
    } else if (_tcsstr(processName, _T("notepad")) != NULL) {
        classification = _T("Editor");
    } else if (_tcsstr(processName, _T("explorer")) != NULL) {
        classification = _T("File Explorer");
    } else if (_tcsstr(processName, _T("mspaint")) != NULL) {
        classification = _T("Graphics Editor");
    } else if (_tcsstr(processName, _T("cmd")) != NULL) {
        classification = _T("Command Line Tool");
    } else if (_tcsstr(processName, _T("msiexec")) != NULL) {
        classification = _T("Installer");
    } else if (_tcsstr(processName, _T("taskmgr")) != NULL) {
        classification = _T("Task Manager");
    } else if (_tcsstr(processName, _T("outlook")) != NULL) {
        classification = _T("Email Client");
    } else {
        classification = _T("General Application");
    }

    TCHAR classificationLog[BUFFER_SIZE];
    _stprintf(classificationLog, _T("Process classified: %s (PID: %d) Classification: %s"), processName, pid, classification);
    LogSummary(classificationLog);

    TCHAR classificationFileName[BUFFER_SIZE];
    _stprintf(classificationFileName, _T("%s\\classification.txt"), processFolder);
    FILE *classificationFile = _tfopen(classificationFileName, _T("a"));
    if (classificationFile) {
        _ftprintf(classificationFile, _T("[%s %s] Process: %s (PID: %d) Classification: %s\n"), __DATE__, __TIME__, processName, pid, classification);
        fclose(classificationFile);
    } else {
        LogError(_T("Failed to open classification log file"), pid, processFolder);
    }

    _tprintf(_T("Process: %s (PID: %d) Classification: %s\n"), processName, pid, classification);
    LogDebug(_T("Process classified."), pid, processFolder);
}

// Function to check if the program is running with administrative privileges
bool IsRunAsAdmin() {
    BOOL fIsRunAsAdmin = FALSE;
    PSID pAdministratorsGroup = NULL;

    // Allocate and initialize a SID of the administrators group.
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    if (!AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0, &pAdministratorsGroup)) {
        return false;
    }

    // Determine whether the SID of administrators group is enabled in the primary access token.
    if (!CheckTokenMembership(NULL, pAdministratorsGroup, &fIsRunAsAdmin)) {
        fIsRunAsAdmin = FALSE;
    }

    // Free the allocated SID.
    if (pAdministratorsGroup) {
        FreeSid(pAdministratorsGroup);
    }

    LogDebug(_T("Checked for administrative privileges."), 0, _T("."));
    return fIsRunAsAdmin;
}

// Function to create the WinDbg commands script
void CreateWinDbgCommandsScript() {
    FILE *commandsFile = _tfopen(COMMANDS_SCRIPT_PATH, _T("w"));
    if (commandsFile) {
        _ftprintf(commandsFile, _T(".echo === Processor Information ===\n!cpuinfo\n"));
        _ftprintf(commandsFile, _T(".echo === System Information ===\nvertarget\n"));
        _ftprintf(commandsFile, _T(".echo === Register States ===\nr\n"));
        _ftprintf(commandsFile, _T(".echo === Disassemble Code (EIP) for 32-bit ===\nu eip\n"));
        _ftprintf(commandsFile, _T(".echo === Disassemble Code (RIP) for 64-bit ===\nu rip\n"));
        _ftprintf(commandsFile, _T(".echo === Memory Information ===\n!address -summary\n"));
        _ftprintf(commandsFile, _T(".echo === Virtual Memory Layout ===\n!vm\n"));
        _ftprintf(commandsFile, _T(".echo === Loaded Modules ===\nlm\n"));
        _ftprintf(commandsFile, _T(".echo === Dump Memory Contents (EIP) for 32-bit ===\ndd eip\n"));
        _ftprintf(commandsFile, _T(".echo === Dump Memory Contents (RIP) for 64-bit ===\ndd rip\n"));
        _ftprintf(commandsFile, _T(".echo === List Threads ===\n~*\n"));
        _ftprintf(commandsFile, _T(".echo === Thread Information ===\n!thread\n"));
        _ftprintf(commandsFile, _T(".echo === Stack Traces ===\nkp\n"));
        _ftprintf(commandsFile, _T(".echo === Kernel Structures ===\n!process 0 0\n!session\n"));
        _ftprintf(commandsFile, _T(".echo === Handle Table ===\n!handle 0 0\n"));
        _ftprintf(commandsFile, _T(".echo === Object Information ===\n!object\n"));
        _ftprintf(commandsFile, _T(".echo === Page Table Entries ===\n!pte\n"));
        _ftprintf(commandsFile, _T(".echo === Kernel Memory Information ===\n!memusage\n"));
        _ftprintf(commandsFile, _T(".echo === Kernel Debugging Structures ===\n!kd\n"));
        _ftprintf(commandsFile, _T(".echo === Kernel Modules ===\n!lm\n"));
        _ftprintf(commandsFile, _T(".echo === Loaded Drivers ===\nlm t n\n"));
        _ftprintf(commandsFile, _T(".echo === Dump Driver Object ===\n!object \\Driver\\\n"));
        _ftprintf(commandsFile, _T(".echo === Loaded Images ===\n!imgscan\n"));
        _ftprintf(commandsFile, _T(".echo === Loaded Paged Pools ===\n!poolused /t\n"));
        _ftprintf(commandsFile, _T(".echo === Heap Summary ===\n!heap -s\n"));
        _ftprintf(commandsFile, _T(".echo === Memory Information (Full) ===\n!memusage 7\n"));
        _ftprintf(commandsFile, _T(".echo === Quitting ===\n.quit\n"));
        fclose(commandsFile);
        LogDebug(_T("WinDbg commands script created."), 0, _T("."));
    } else {
        LogError(_T("Failed to create WinDbg commands script"), 0, _T("."));
    }
}

// Function to capture WinDbg output
void CaptureWinDbgOutput(const TCHAR *outputFileName, const TCHAR *processFolder) {
    TCHAR line[BUFFER_SIZE];
    FILE *outputFile = _tfopen(outputFileName, _T("r"));
    if (outputFile) {
        while (_fgetts(line, BUFFER_SIZE, outputFile)) {
            _tprintf(_T("%s"), line);  // Print to console
            LogDebug(line, 0, processFolder);  // Log to debug file
            LogSummary(line);  // Log to summary file
        }
        fclose(outputFile);
    } else {
        LogError(_T("Failed to open output file"), 0, processFolder);
    }
}

int main(void) {
    // Check for admin privileges
    if (!IsRunAsAdmin()) {
        LogErrorAndExit(_T("This program requires administrative privileges."));
    }

    // Create the output folder
    CreateDirectoryIfNotExists(OUTPUT_FOLDER);

    // Create the WinDbg commands script
    CreateWinDbgCommandsScript();

    // Step 1: Get all running process IDs
    DWORD processIDs[1024], bytesReturned;
    if (!GetAllProcessIDs(processIDs, sizeof(processIDs), &bytesReturned)) {
        return 1;  // Exit if we cannot get process IDs
    }
    DWORD numProcesses = bytesReturned / sizeof(DWORD);

    // Step 2: Run WinDbg for each process
    for (DWORD i = 0; i < numProcesses; i++) {
        DWORD pid = processIDs[i];
        if (pid == 0) continue;  // Skip system idle process

        TCHAR processName[BUFFER_SIZE];
        if (!GetProcessNameByPID(pid, processName, sizeof(processName))) {
            LogError(_T("Failed to get process name"), pid, _T("."));
            continue;
        }

        _tprintf(_T("Analyzing process %s (PID: %d)\n"), processName, pid);

        // Create a directory for this process
        TCHAR processFolder[BUFFER_SIZE];
        _stprintf(processFolder, _T("%s\\%d_%s"), OUTPUT_FOLDER, pid, processName);
        CreateDirectoryIfNotExists(processFolder);

        // Create output file path
        TCHAR outputFileName[BUFFER_SIZE];
        _stprintf(outputFileName, _T("%s\\windbg_output.txt"), processFolder);

        // Retry mechanism for running WinDbg
        int retryCount = 0;
        const int maxRetries = 3;
        bool success = false;
        while (retryCount < maxRetries) {
            if (RunWinDbg(WINDBG_PATH, pid, COMMANDS_SCRIPT_PATH, outputFileName, processFolder)) {
                success = true;
                _tprintf(_T("Successfully analyzed process %s (PID: %d)\n"), processName, pid);
                CaptureWinDbgOutput(outputFileName, processFolder);  // Capture and print output
                break;
            } else {
                retryCount++;
                LogError(_T("Failed to attach to process, retrying..."), pid, processFolder);
                _tprintf(_T("Retry %d for process %s (PID: %d)\n"), retryCount, processName, pid);
                HandleErrorPopups(pid);  // Handle error popups
            }
        }

        if (!success) {
            LogError(_T("Failed to attach to process after multiple attempts"), pid, processFolder);
            _tprintf(_T("Skipping process %s (PID: %d) after multiple attempts\n"), processName, pid);
        }

        // Classify and log the process
        ClassifyProcesses(pid, processName, processFolder);

        // Ensure WinDbg process has completely finished before moving to the next process
        _tprintf(_T("Analysis for process %s (PID: %d) completed. Moving to the next process.\n"), processName, pid);
    }

    _tprintf(_T("Analysis completed for all processes.\n"));

    return 0;
}
