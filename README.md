# 🛠️ Windows WinDbg CPU Process Toolkit 🛠️

![WinDbg Toolkit](https://github.com/user-attachments/assets/aaeef17a-4814-4bc0-9da9-f0ea56171337)



## Overview
The **Windows WinDbg CPU Process Toolkit** is a comprehensive solution designed to automate the process of capturing, analyzing, and managing CPU processes using WinDbg on Windows. This toolkit is ideal for developers, system administrators, and computer forensics professionals who need to extract detailed information from running processes for debugging and analysis purposes.

## Features ✨
- 🚀 **Automated Process Scanning**: Continuously scan and capture information from all running processes using WinDbg.
- 📋 **Clipboard Data Extraction**: Automatically copy and save data from the WinDbg window to local files.
- ⚠️ **Error Handling**: Detect and handle error popups, prompting user intervention to continue the process.
- 🧠 **Memory and Module Capture**: Read process memory and capture loaded modules, saving the information to transcribed and readable files.
- 🖥️ **Graphical User Interface (GUI)**: User-friendly GUI with start and stop scanning controls for easy operation.
- 🔐 **Administrator Privileges Handling**: Ensure the application runs with the necessary administrative privileges for process analysis.

# ToolKit Explaination

## Captured Information Sections

| **Section Number** | **Section Name**                | **Description**                                                                              |
|--------------------|---------------------------------|----------------------------------------------------------------------------------------------|
| 1                  | Preparing the Environment       | Initializes and configures the environment for Debugger Extensions Gallery repositories.     |
| 2                  | Path Validation Summary         | Validates the symbol and executable search paths.                                            |
| 3                  | Module Loading                  | Logs the loading of modules into memory.                                                     |
| 4                  | Initial Command Processing      | Executes initial debugger commands.                                                          |
| 5                  | Processor Information           | Attempts to display CPU information (unsupported for the target machine).                    |
| 6                  | System Information              | Displays general system information including OS version, build, uptime, and processor details. |
| 7                  | Register States                 | Displays the state of CPU registers.                                                         |
| 8                  | Disassemble Code                | Disassembles and displays code around the instruction pointer for both 32-bit and 64-bit modes. |
| 9                  | Memory Information              | Summarizes memory usage, mapping various memory regions.                                     |
| 10                 | Virtual Memory Layout           | Displays the virtual memory layout (unsupported for the target machine).                     |
| 11                 | Loaded Modules                  | Lists loaded modules with their memory addresses and names.                                  |
| 12                 | Dump Memory Contents            | Dumps the contents of memory at specific addresses for both 32-bit and 64-bit modes.         |
| 13                 | List Threads                    | Lists all threads within the process with details such as start address, priority, and affinity. |
| 14                 | Stack Traces                    | Displays stack traces for each thread.                                                       |
| 15                 | Kernel Structures               | Displays active process information and other kernel structures (symbols not available).     |
| 16                 | Handle Table                    | Displays a summary of handles opened by the process, categorized by type.                    |
| 17                 | Object Information              | Displays information about kernel objects (unsupported for the target machine).              |
| 18                 | Page Table Entries              | Displays page table entries (unsupported for the target machine).                            |
| 19                 | Kernel Memory Information       | Displays a summary of kernel memory usage (unsupported for the target machine).              |
| 20                 | Kernel Debugging Structures     | Displays kernel debugging structures (unsupported for the target machine).                   |
| 21                 | Loaded Drivers                  | Lists loaded drivers with their memory addresses and names.                                  |
| 22                 | Dump Driver Object              | Attempts to dump information about driver objects (symbols not available).                   |
| 23                 | Loaded Images                   | Attempts to list loaded images (command not supported).                                      |
| 24                 | Loaded Paged Pools              | Displays usage statistics for paged pool memory.                                             |
| 25                 | Heap Summary                    | Displays a summary of heap statistics.                                                       |
| 26                 | Memory Information (Full)       | Displays detailed memory usage information (unsupported for the target machine).             |
| 27                 | Quitting                        | Indicates the end of the script execution and the exit from the debugger.                    |
### Example "21. Loaded Drivers"
![image](https://github.com/user-attachments/assets/a2c18ca6-d35d-4d10-8165-e8c150dc972a)



# Windows WinDbg CPU Process Toolkit

This toolkit analyzes all the running processes by leveraging the Windows SDK and collects data for analysis and machine learning classification.

## Installation 🛠️
Make sure to run as Administrator if it is giving you compile or run-time issues.
1. **Clone the Repository**:
    ```sh
    git clone https://github.com/YourUsername/Windows-WinDbg-CPU-Process-Toolkit.git
    cd Windows-WinDbg-CPU-Process-Toolkit
    ```

2. **Compile the Code**:
    Use `gcc` to compile the source code:
    ```sh
    gcc -o Locate_Code.exe Locate_Code.c -lgdi32 -lgdiplus -lpsapi -lshlwapi -ladvapi32 -lcomctl32
    gcc -o Process_Analyzer.exe Process_Analyzer.c -lgdi32 -lgdiplus -lpsapi -lshlwapi -ladvapi32 -lcomctl32
    ```

3. **Run the Application**:
    Execute the compiled application using the provided batch file:
    ```sh
    Run_Applications.bat
    ```


## Batch File: `Run_Applications.bat`
This batch file automates the process of running the compiled applications and keeps the windows on top using a PowerShell script.

## Powershell Script: `Process_Analyzer_FOREGROUND.ps1`
This powershell file is designed to keep machine source code at the front in order for our toolkit to analyze the information.


## Usage 💻
- **Start Scanning**: Click the "Start Scanning" button in the GUI to begin the process scanning and data extraction.
- **Stop Scanning**: Click the "Stop Scanning" button to halt the scanning process.
- **Error Handling**: Follow the on-screen prompts to handle error popups by clicking "OK" when necessary.

## Example Workflow 📝
1. **Initialization**: The application initializes and positions the command window to avoid overlaying the WinDbg source window.
2. **Scanning Loop**: The scanning loop continuously checks for the WinDbg window and captures text data.
3. **Error Handling**: When an error popup is detected, the user is prompted to click "OK", pausing the scanning process until resolved.
4. **Data Capture**: Captured data is saved to organized folders, labeled by process ID and name, for further analysis.

## Contributing 🤝
Contributions are welcome! Please fork the repository and submit pull requests to contribute to the project. Ensure your code adheres to the project's coding standards and includes appropriate documentation.

## License 📄
This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

---

Feel free to contact us if you have any questions or need further assistance. Let's make debugging and process analysis easier and more efficient!

### Disclosure
This toolkit utilizes Microsoft WinDbg and the Windows SDK.
