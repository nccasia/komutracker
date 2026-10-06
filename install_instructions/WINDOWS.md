# KomuTracker installation on Windows

The Windows release is a ZIP archive containing two standalone executables:

```text
KomuTracker.exe       Desktop system-tray application
komutracker-cli.exe   Command-line client
```

The release uses a static libcurl and MSVC runtime build. The normal Windows system DLLs are still used, but no KomuTracker DLL folder is required.

## Option 1: Run the desktop application

1. Download `KomuTracker-<version>-windows-x64.zip`.
2. Extract the ZIP to a permanent directory, for example `C:\Program Files\KomuTracker` or `%LOCALAPPDATA%\Programs\KomuTracker`.
3. Double-click `KomuTracker.exe`.
4. Open the KomuTracker system-tray menu and select **Log In**.

KomuTracker runs in the system tray only and does not open a console window. If the icon is not visible, check the tray overflow menu.

The desktop executable can be copied and run by itself; the rest of the build directory is not required.

If Microsoft Defender SmartScreen warns about an unsigned build, select **More info** and then **Run anyway** only when the file came from a trusted release.

To uninstall the portable desktop application, select **Quit KomuTracker** from the tray menu and delete its installation directory. Use **Log Out** first if the server session must also be revoked.

## Option 2: Run only the standalone CLI

Open PowerShell in the extracted directory:

```powershell
.\komutracker-cli.exe --version
.\komutracker-cli.exe
```

Only `komutracker-cli.exe` needs to be copied when distributing the CLI by itself.

To make the command available globally, place the executable in a permanent directory and add that directory to the user `PATH`. After opening a new terminal, run:

```powershell
komutracker-cli.exe
```

## CLI usage

```powershell
.\komutracker-cli.exe --login       # Start a new login and replace the previous session
.\komutracker-cli.exe --status      # Show the authenticated account
.\komutracker-cli.exe --logout      # Revoke the session and remove local credentials
.\komutracker-cli.exe --no-browser  # Print the login URL instead of opening a browser
.\komutracker-cli.exe --verbose     # Run with diagnostic output
```

The CLI `--daemon` option is not supported on Windows. Use `KomuTracker.exe` when tracking should run without a console window.

Only one tracking process can run for the current Windows user. A new successful login for the same account invalidates its previous client session.

## Local files

KomuTracker stores the device identity and authentication data below:

```text
Device ID: %LOCALAPPDATA%\komutracker\.device_id
Token:     %LOCALAPPDATA%\komutracker\auth\auth.tracker
Profile:   %LOCALAPPDATA%\komutracker\auth\profile.txt
PID/lock:  %LOCALAPPDATA%\komutracker\tracker.pid
```

The application falls back to `%APPDATA%\komutracker\` if `LOCALAPPDATA` is unavailable. Deleting the executable does not revoke the server session or automatically remove these files; use **Log Out** or `--logout` first.
