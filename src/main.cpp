#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "ProfileManager.h"

#pragma comment(lib, "comctl32.lib")

// Global variables
HINSTANCE g_hInst;
HWND g_hList;
ProfileManager g_manager;
std::vector<ProfileInfo> g_profiles;

enum Columns { COL_NAME, COL_CREATED, COL_SIZE, COL_COUNT };

void AddColumns(HWND hList) {
    LVCOLUMNW col = {0};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    col.cx = 200; col.pszText = const_cast<LPWSTR>(L"User");
    ListView_InsertColumn(hList, COL_NAME, &col);

    col.cx = 150; col.pszText = const_cast<LPWSTR>(L"Created");
    ListView_InsertColumn(hList, COL_CREATED, &col);

    col.cx = 100; col.pszText = const_cast<LPWSTR>(L"Size (MB)");
    ListView_InsertColumn(hList, COL_SIZE, &col);
}

std::wstring FormatFileTime(const FILETIME& ft) {
    SYSTEMTIME stUTC, stLocal;
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

    wchar_t buffer[64];
    swprintf(buffer, 64, L"%02d/%02d/%04d %02d:%02d", stLocal.wMonth, stLocal.wDay, stLocal.wYear, stLocal.wHour, stLocal.wMinute);
    return buffer;
}

void PopulateList(HWND hList) {
    ListView_DeleteAllItems(hList);
    int index = 0;
    for (const auto& info : g_profiles) {
        LVITEMW item = {0};
        item.mask = LVIF_TEXT;
        item.iItem = index;
        item.pszText = const_cast<LPWSTR>(info.name.c_str());
        ListView_InsertItem(hList, &item);

        ListView_SetItemText(hList, index, COL_CREATED, const_cast<LPWSTR>(FormatFileTime(info.creationTime).c_str()));

        wchar_t sizeBuf[32];
        swprintf(sizeBuf, 32, L"%llu", info.size / (1024*1024));
        ListView_SetItemText(hList, index, COL_SIZE, sizeBuf);

        ++index;
    }
}

void RefreshProfiles(HWND hList) {
    g_profiles = g_manager.EnumerateProfiles();
    PopulateList(hList);
}

int CALLBACK CompareFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort) {
    int column = static_cast<int>(lParamSort);
    const ProfileInfo& a = g_profiles[lParam1];
    const ProfileInfo& b = g_profiles[lParam2];
    switch(column) {
        case COL_NAME:
            return _wcsicmp(a.name.c_str(), b.name.c_str());
        case COL_CREATED: {
            ULONGLONG at = (ULONGLONG)a.creationTime.dwHighDateTime << 32 | a.creationTime.dwLowDateTime;
            ULONGLONG bt = (ULONGLONG)b.creationTime.dwHighDateTime << 32 | b.creationTime.dwLowDateTime;
            return at < bt ? -1 : (at > bt ? 1 : 0);
        }
        case COL_SIZE:
            return (a.size < b.size) ? -1 : ((a.size > b.size) ? 1 : 0);
    }
    return 0;
}

void OnColumnClick(NMHDR* nmhdr) {
    NMLISTVIEW* nmlv = reinterpret_cast<NMLISTVIEW*>(nmhdr);
    ListView_SortItemsEx(g_hList, CompareFunc, nmlv->iSubItem);
}

void OnRemoveProfiles(HWND hWnd) {
    std::vector<ProfileInfo> toDelete;
    int count = ListView_GetItemCount(g_hList);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(g_hList, i)) {
            toDelete.push_back(g_profiles[i]);
        }
    }
    if (toDelete.empty()) return;

    std::wstring message = L"The following profiles will be deleted:\n";
    for (const auto& p : toDelete) {
        message += L" - " + p.name + L"\n";
    }
    message += L"Continue?";

    if (MessageBoxW(hWnd, message.c_str(), L"Confirm Deletion", MB_OKCANCEL | MB_ICONWARNING) == IDOK) {
        g_manager.DeleteProfiles(toDelete);
        RefreshProfiles(g_hList);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        InitCommonControls();
        g_hList = CreateWindowW(WC_LISTVIEW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
                                10, 10, 500, 300, hWnd, nullptr, g_hInst, nullptr);
        ListView_SetExtendedListViewStyle(g_hList, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        AddColumns(g_hList);
        RefreshProfiles(g_hList);
        CreateWindowW(L"BUTTON", L"Remove Profiles", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
                      10, 320, 150, 30, hWnd, (HMENU)1, g_hInst, nullptr);
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {
            OnRemoveProfiles(hWnd);
        }
        break;
    case WM_NOTIFY:
        if (((LPNMHDR)lParam)->hwndFrom == g_hList && ((LPNMHDR)lParam)->code == LVN_COLUMNCLICK) {
            OnColumnClick((NMHDR*)lParam);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
    return 0;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    g_hInst = hInstance;
    const wchar_t CLASS_NAME[] = L"ProfileCleanupMain";
    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowExW(0, CLASS_NAME, L"Profile Cleanup", WS_OVERLAPPEDWINDOW,
                                CW_USEDEFAULT, CW_USEDEFAULT, 540, 400, nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return 0;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

