# komutracker

Native activity tracker written in C. KomuTracker is available as a desktop tray/menu-bar application and as a command-line client. It detects AFK state and sends ActivityWatch-compatible heartbeats without enumerating background processes.

## Install dependencies

The build requires the libcurl development library, not only the `curl` command-line program.

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libglib2.0-dev libsqlite3-dev libx11-dev libxss-dev libgtk-3-dev libayatana-appindicator3-dev
```

Verify libcurl installation:

```bash
pkg-config --modversion libcurl
```

If CMake still reports `Could NOT find CURL`, remove the old build directory and configure again:

```bash
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### Fedora/RHEL

```bash
sudo dnf install gcc make cmake pkgconf-pkg-config libcurl-devel glib2-devel sqlite-devel libX11-devel libXScrnSaver-devel gtk3-devel libappindicator-gtk3-devel
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake pkgconf curl glib2 sqlite libx11 libxss gtk3 libappindicator-gtk3
```

### macOS

Install the Xcode command-line tools, CMake, pkg-config, and libcurl with Homebrew:

```bash
xcode-select --install
brew install cmake pkg-config curl
```

Homebrew installs curl as keg-only on many systems. If CMake cannot find it, configure with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
  -DCURL_ROOT="$(brew --prefix curl)"
```

### Windows

Install Visual Studio Build Tools with the **Desktop development with C++** workload, then install CMake and libcurl through vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install curl:x64-windows-static sqlite3:x64-windows-static
```

Configure using the vcpkg toolchain from a Developer PowerShell:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows-static
cmake --build build --config Release
```

On Linux, AFK tracking supports X11 and native Wayland sessions. Wayland uses
the session D-Bus idle API provided by Mutter (GNOME/Ubuntu/Phosh), with the
freedesktop ScreenSaver API as a fallback.

## Build

```bash
cd /mnt/nccasia/komutracker
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

CMake generates a Makefile in `build`, so after configuring you can also build with:

```bash
make -C build
```

The build produces both the CLI and desktop application:

- macOS: `build/KomuTracker.app` and `build/komutracker`.
- Windows: `build/KomuTracker.exe` and `build/komutracker-cli.exe`.
- Ubuntu: `build/komutracker-desktop` and `build/komutracker`.

The desktop application only shows a tray/menu-bar icon. Use its menu to log in,
log out, open the dashboard, inspect connection status and locally tracked
non-AFK time for the current day, or quit.

## Package a release

Build packages on their native operating system after a Release build:

```bash
cpack --config build/CPackConfig.cmake -B dist
```

Generated artifacts are:

- Windows: a ZIP containing the standalone GUI and CLI executables.
- macOS: a DMG containing `KomuTracker.app`.
- Ubuntu: a DEB that installs the executable, AppIndicator launcher, desktop entry, and icons.

Public macOS and Windows releases should be code-signed. The generated local DMG and
ZIP are otherwise complete but operating-system security prompts may identify them as
coming from an unknown developer.

On Ubuntu, install and launch the package with:

```bash
sudo apt install ./dist/komutracker-*-ubuntu-amd64.deb
komutracker-desktop
```

The desktop entry uses `Terminal=false`, so launching KomuTracker from the Ubuntu
application menu does not display a terminal. Git tags matching `v*` run the GitHub
Actions workflow and attach all three platform packages to the release.

## Test

```bash
ctest --test-dir build --output-on-failure
```

Or:

```bash
make -C build test
```

## Login and run

The API server defaults to `https://tracker.komu.vn`. The OAuth callback is `https://tracker.komu.vn/api/0/auth/callback`, matching the URL registered for the Mezon OAuth client.

For normal desktop use, launch `KomuTracker.app`, `KomuTracker.exe`, or KomuTracker
from Ubuntu's application menu. Select **Log In** from the tray menu; the browser opens
for OAuth and the tray changes to the authenticated user's name when login completes.
Selecting **Log Out** stops tracking and removes the saved credentials without closing
the tray application.

On first run, the CLI creates a device ID, opens the Mezon login page in the default browser, and waits for authentication. After login completes in the browser, control returns to the CLI and the token is saved for future runs.

```bash
./build/komutracker
```

If the browser cannot open or the machine is headless, print the login URL without launching a browser:

```bash
./build/komutracker --no-browser
```

Force a new login and exit after showing the authenticated user:

```bash
./build/komutracker --login
```

Check the current login:

```bash
./build/komutracker --status
```

Log out locally and from the server:

```bash
./build/komutracker --logout
```

The login waits for up to five minutes by default. Change the timeout with `--auth-timeout`, or cancel with `Ctrl+C`:

```bash
./build/komutracker --auth-timeout 600
```

Manual credentials remain supported for automation:

```bash
./build/komutracker \
  --server https://tracker.komu.vn \
  --token "YOUR_TOKEN" \
  --device-id "YOUR_DEVICE_ID"
```

Configure AFK timeout and polling:

```bash
./build/komutracker \
  --timeout 180 \
  --poll-time 10 \
  --verbose
```

Testing mode uses a 20-second timeout, polls every second, and defaults to `http://localhost:5666`:

```bash
./build/komutracker --testing --verbose
```

Stop the tracker with `Ctrl+C`.

### Run as a daemon

Pass `-d` or `--daemon` to detach the process and keep tracking in the background:

```bash
./build/komutracker --daemon
```

The process double-forks, reparents to init, redirects its standard streams, and writes its PID to `~/.cache/komutracker/tracker.pid` (or `$XDG_CACHE_HOME/komutracker/tracker.pid` when `XDG_CACHE_HOME` is set). The directory is created automatically if it does not exist. To stop it, send it a signal (or use `systemd`/your service manager):

```bash
kill "$(cat ~/.cache/komutracker/tracker.pid)"
```

Only one instance runs at a time. Launching the command again while another instance is already tracking exits immediately with `Another komutracker instance is already running`. The lock is released automatically if the process crashes.

## Options

```text
--login               Force browser authentication and exit
--logout              Revoke and remove saved credentials
--status              Print the authenticated user and exit
--no-browser          Print the login URL without opening it
--auth-timeout SEC    Browser login timeout; default: 300
--auth-url URL        OAuth provider base URL
--client-id ID        OAuth client ID
--redirect-uri URL    Remote OAuth callback URL
--testing             Use testing defaults
-v, --verbose         Additionally print transport errors, the logged-in user, and bucket names
--version, -V         Print the komutracker version and exit
--timeout SECONDS     Idle time before AFK status
--poll-time SECONDS   AFK polling interval; default: 10
--window-poll-time SEC Foreground-process polling interval; default: 10
--exclude-title        Send `excluded` instead of the focused window title
-d, --daemon           Detach and run in the background (single instance)
--server URL          ActivityWatch-compatible server URL
--token TOKEN         Bearer authentication token
--device-id ID        Device-Id request header
```

Saved credentials:

- Linux device ID: `~/.local/share/komutracker/.device_id`
- Linux event journal: `~/.local/share/komutracker/events.db`
- Linux token: `~/.cache/komutracker/auth/auth.tracker`
- macOS device ID: `~/Library/Application Support/komutracker/.device_id`
- macOS event journal: `~/Library/Application Support/komutracker/events.db`
- macOS token: `~/Library/Caches/komutracker/auth/auth.tracker`
- Windows: under `%LOCALAPPDATA%\komutracker`

Token, device, and event-journal files use owner-only permissions on POSIX systems. Tokens are never printed by the CLI.

AFK heartbeats are merged into the current state segment in memory. That
segment is checkpointed to the SQLite event journal every five minutes, on each
AFK/non-AFK transition, and during a clean shutdown. This keeps normal SQLite
writes low; an abrupt crash can lose at most the uncheckpointed part of the
current five-minute interval. Pending events are replayed oldest-first when the
server is reachable again. Network heartbeats continue directly from the
in-memory segment, so checkpointing does not add upload latency.

Consecutive heartbeats with the same status and an interval gap of at most 370
seconds are merged into one local state segment, matching the default server
merge policy. Sent and pending segments remain available locally for
diagnostics and are removed five days after their last update. The tracked-time
summary reads SQLite after startup and checkpoints, then combines that cached
total with the current in-memory segment instead of querying the database on
every tray refresh.

If the server rejects an authenticated client with HTTP 401 or 403 (for
example, because the same account was activated on another device), tracking
stops immediately. The client checkpoints its final in-memory segment and
marks every unsent event for that server and device as discarded. Discarded
events remain in the five-day local journal for future diagnostics, but are
never uploaded after re-authentication and are excluded from the tray's tracked
time total. Network timeouts, DNS failures, and server errors do not discard
events.

Application defaults are defined once in `src/config.c` and shared by the desktop
and CLI clients. KomuTracker does not read application settings or credentials from
environment variables. The CLI flags above remain available for testing and automation.

## Logs

All progress output is printed only with `-v`/`--verbose`; running without it is silent. Each line is prefixed with the version and a local `[HH:MM:SS]` timestamp, e.g. `komutracker 1.0.0 [14:05:23] afk heartbeat: OK`. It logs:

- **startup** — `komutracker 1.0.0 started for <server>`.
- **authentication polling** — each poll of the token endpoint logs `authentication poll attempt N pending`, `… succeeded`, or `… failed` while waiting for the browser login to finish.
- **data sends** — each window heartbeat logs `foreground-process heartbeat: OK` (or `FAILED`) and each AFK heartbeat logs `afk heartbeat: OK` (or `FAILED`).

With `-v`/`--verbose`, the CLI additionally logs the authenticated user (`logged in as <name> <<email>>`) and the two bucket names (`afk bucket: …`, `foreground-process bucket: …`) at startup, and prints the exact HTTP error reason for a failed request (timeout, bad status, etc.):

```bash
./build/komutracker --verbose
```

Check the version:

```bash
./build/komutracker --version
# komutracker 1.0.0
```

## Data sent

The combined process publishes two buckets, named after the authenticated username (the part of the email before `@`, e.g. `nguyentran`):

- `aw-watcher-afk_<username>` / `afkstatus`: `afk` or `not-afk`.
- `aw-watcher-window_<username>` / `currentwindow`: the focused process in `app` and its window title in `title`.

Only the foreground process is sent. Background process lists are never collected. Use `--exclude-title` to avoid sending window titles:

```bash
./build/komutracker --exclude-title --window-poll-time 10
```

## Platform backends

- Windows: `GetForegroundWindow`, `QueryFullProcessImageName`, and `GetLastInputInfo`.
- macOS: CoreGraphics window list and idle event APIs. Window titles may require Screen Recording permission.
- Linux AFK: XScreenSaver on X11; Mutter IdleMonitor or the freedesktop ScreenSaver D-Bus API on Wayland.
