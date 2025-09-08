#include "ProfileManager.h"

#include <Shlwapi.h>
#include <Userenv.h>
#include <Wbemidl.h>
#include <comdef.h>
#include <atlbase.h>
#include <sddl.h>
#include <memory>

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "shlwapi.lib")

namespace {
    // Helper to convert FILETIME to ULONGLONG
    ULONGLONG FileTimeToULL(const FILETIME& ft) {
        ULARGE_INTEGER uli;
        uli.LowPart = ft.dwLowDateTime;
        uli.HighPart = ft.dwHighDateTime;
        return uli.QuadPart;
    }
}

std::vector<ProfileInfo> ProfileManager::EnumerateProfiles() {
    std::vector<ProfileInfo> profiles;

    // Initialize COM
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        return profiles;
    }

    // Set general COM security levels
    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
                              RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
    if (FAILED(hr) && hr != RPC_E_TOO_LATE) {
        CoUninitialize();
        return profiles;
    }

    CComPtr<IWbemLocator> locator;
    hr = locator.CoCreateInstance(CLSID_WbemLocator);
    if (FAILED(hr)) {
        CoUninitialize();
        return profiles;
    }

    CComPtr<IWbemServices> services;
    hr = locator->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services);
    if (FAILED(hr)) {
        CoUninitialize();
        return profiles;
    }

    // Set proxy security
    hr = CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           nullptr, EOAC_NONE);
    if (FAILED(hr)) {
        CoUninitialize();
        return profiles;
    }

    // Query profiles
    CComPtr<IEnumWbemClassObject> enumerator;
    hr = services->ExecQuery(_bstr_t(L"WQL"),
                             _bstr_t(L"SELECT SID, LocalPath, Loaded, Special FROM Win32_UserProfile"),
                             WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                             nullptr, &enumerator);
    if (FAILED(hr)) {
        CoUninitialize();
        return profiles;
    }

    while (enumerator) {
        CComPtr<IWbemClassObject> obj;
        ULONG returned = 0;
        hr = enumerator->Next(WBEM_INFINITE, 1, &obj, &returned);
        if (returned == 0) break;

        VARIANT sidVar, pathVar, loadedVar, specialVar;
        obj->Get(L"SID", 0, &sidVar, nullptr, nullptr);
        obj->Get(L"LocalPath", 0, &pathVar, nullptr, nullptr);
        obj->Get(L"Loaded", 0, &loadedVar, nullptr, nullptr);
        obj->Get(L"Special", 0, &specialVar, nullptr, nullptr);

        bool loaded = loadedVar.boolVal == VARIANT_TRUE;
        bool special = specialVar.boolVal == VARIANT_TRUE;
        if (loaded || special) {
            VariantClear(&sidVar);
            VariantClear(&pathVar);
            VariantClear(&loadedVar);
            VariantClear(&specialVar);
            continue; // Skip profiles in use or special profiles
        }

        ProfileInfo info;
        info.sid = sidVar.bstrVal;
        info.path = pathVar.bstrVal;

        // Lookup account name from SID
        wchar_t name[256];
        wchar_t domain[256];
        DWORD nameLen = 256, domainLen = 256;
        SID_NAME_USE sidType;
        PSID psid = nullptr;
        if (ConvertStringSidToSidW(info.sid.c_str(), &psid)) {
            if (LookupAccountSidW(nullptr, psid, name, &nameLen, domain, &domainLen, &sidType)) {
                info.name = std::wstring(domain) + L"\\" + name;
            } else {
                info.name = info.sid; // Fallback
            }
            LocalFree(psid);
        } else {
            info.name = info.sid; // Fallback
        }

        // Get creation time
        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExW(info.path.c_str(), GetFileExInfoStandard, &data)) {
            info.creationTime = data.ftCreationTime;
        } else {
            info.creationTime.dwLowDateTime = 0;
            info.creationTime.dwHighDateTime = 0;
        }

        // Calculate directory size
        CalculateDirectorySize(info.path, info.size);

        profiles.push_back(info);

        VariantClear(&sidVar);
        VariantClear(&pathVar);
        VariantClear(&loadedVar);
        VariantClear(&specialVar);
    }

    CoUninitialize();
    return profiles;
}

bool ProfileManager::DeleteProfiles(const std::vector<ProfileInfo>& profiles) {
    bool allSuccess = true;
    for (const auto& profile : profiles) {
        if (!DeleteProfileW(profile.sid.c_str(), profile.name.c_str(), profile.path.c_str())) {
            allSuccess = false;
        }
    }
    return allSuccess;
}

bool ProfileManager::CalculateDirectorySize(const std::wstring& path, ULONGLONG& size) {
    WIN32_FIND_DATAW ffd;
    std::wstring searchPath = path + L"\\*";
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        return false;
    }
    size = 0;
    do {
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) {
            continue;
        }
        std::wstring itemPath = path + L"\\" + ffd.cFileName;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            ULONGLONG subSize = 0;
            CalculateDirectorySize(itemPath, subSize);
            size += subSize;
        } else {
            ULONGLONG fileSize = (static_cast<ULONGLONG>(ffd.nFileSizeHigh) << 32) | ffd.nFileSizeLow;
            size += fileSize;
        }
    } while (FindNextFileW(hFind, &ffd) != 0);
    FindClose(hFind);
    return true;
}

