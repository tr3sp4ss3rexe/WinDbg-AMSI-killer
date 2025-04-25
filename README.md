# WinDbg AMSI killer extension

A simple extension for WinDbg in the form of a DLL to patch AmsiScanBuffer from amsi.dll

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
