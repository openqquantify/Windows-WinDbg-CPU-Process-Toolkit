# PowerShell script to keep specific windows on top

Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32 {
    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);

    [DllImport("user32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
    
    public static readonly IntPtr HWND_TOPMOST = new IntPtr(-1);
    public static readonly IntPtr HWND_NOTOPMOST = new IntPtr(-2);
    public const uint SWP_NOMOVE = 0x0002;
    public const uint SWP_NOSIZE = 0x0001;
    public const uint TOPMOST_FLAGS = SWP_NOMOVE | SWP_NOSIZE;
}
"@

function Set-WindowTopMost {
    param (
        [string]$windowName
    )

    $hwnd = [Win32]::FindWindow($null, $windowName)
    if ($hwnd -ne [IntPtr]::Zero) {
        [Win32]::SetWindowPos($hwnd, [Win32]::HWND_TOPMOST, 0, 0, 0, 0, [Win32]::TOPMOST_FLAGS)
    } else {
        Write-Output "Window not found: $windowName"
    }
}

function Set-WindowForeground {
    param (
        [string]$windowName
    )

    $hwnd = [Win32]::FindWindow($null, $windowName)
    if ($hwnd -ne [IntPtr]::Zero) {
        [Win32]::SetForegroundWindow($hwnd)
    } else {
        Write-Output "Window not found: $windowName"
    }
}

function Keep-WindowsOnTop {
    while ($true) {
        Set-WindowTopMost -windowName "Process_Analyzer"

        Set-WindowForeground -windowName "Process_Analyzer"
        
        Start-Sleep -Milliseconds 500
    }
}

# Ensure the windows are on top before starting the loop
Set-WindowTopMost -windowName "Process_Analyzer"

Set-WindowForeground -windowName "Process_Analyzer"

# Start the loop to keep windows on top
Keep-WindowsOnTop
