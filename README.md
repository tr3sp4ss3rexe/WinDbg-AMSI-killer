![img](https://github.com/tr3sp4ss3rexe/WinDbg-AMSI-killer/blob/main/patchAmsi.png)
# WinDbg AMSI killer extension

A simple DLL extension for WinDbg to patch AmsiScanBuffer from amsi.dll

## Compile
Compile in Microsoft Visual Studio as a Windows DLL

## Usage
### Loading the extension into WinDbg
> .load "C:\\\PathToDll\\\amsikiller.dll"

### Locating amsi!AmsiScanBuffer 
> !checkamsi

### Patching the function
> !patchamsi

---

WinDbg: https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

AmsiScanBuffer: https://learn.microsoft.com/en-us/windows/win32/api/amsi/nf-amsi-amsiscanbuffer
