# Profile Cleanup App

This Windows desktop application lists removable user profiles and allows administrators to delete them safely. Profiles that are currently loaded or marked as special are omitted from the list. The list view supports sorting by user name, creation date, and profile size.

## Building

The project is a simple Win32 application that depends on the Windows SDK. Build using Visual Studio or `x86_64-w64-mingw32` on Windows:

```
cl /EHsc /DUNICODE /I"%ProgramFiles%\Windows Kits\10\Include" src\*.cpp /link userenv.lib wbemuuid.lib shlwapi.lib comctl32.lib
```

## Usage

Run the compiled executable with administrative privileges. Check the profiles you want to remove and click **Remove Profiles**. A confirmation dialog will appear listing profiles selected for deletion.

