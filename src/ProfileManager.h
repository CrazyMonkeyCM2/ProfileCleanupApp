#pragma once

#include <windows.h>
#include <string>
#include <vector>

struct ProfileInfo {
    std::wstring sid;       // Security identifier
    std::wstring name;      // Account display name
    std::wstring path;      // Path to profile directory
    FILETIME creationTime;  // Creation time of profile directory
    ULONGLONG size;         // Size in bytes of profile directory
};

class ProfileManager {
public:
    // Enumerate removable profiles (ignores loaded or special profiles)
    std::vector<ProfileInfo> EnumerateProfiles();

    // Delete selected profiles using DeleteProfile API
    bool DeleteProfiles(const std::vector<ProfileInfo>& profiles);

private:
    bool CalculateDirectorySize(const std::wstring& path, ULONGLONG& size);
};

