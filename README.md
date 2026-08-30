# komutracker

Native command-line activity tracker written in C. One process detects AFK state and the process owning the currently focused window, then sends ActivityWatch-compatible heartbeats to separate server buckets. It does not enumerate all running processes and has no GUI.

## Install dependencies

The build requires the libcurl development library, not only the `curl` command-line program.

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libcurl4-openssl-dev libx11-dev libxss-dev
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
sudo dnf install gcc make cmake pkgconf-pkg-config libcurl-devel libX11-devel libXScrnSaver-devel
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake pkgconf curl libx11 libxss
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
  -DCURL_ROOT="$(brew --prefix curl)"
```

### Windows

Install Visual Studio Build Tools with the **Desktop development with C++** workload, then install CMake and libcurl through vcpkg:

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install curl:x64-windows
```

Configure using the vcpkg toolchain from a Developer PowerShell:

```powershell
cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Release
```

The Linux implementation currently requires an X11 display. A Wayland session without XWayland is not supported.

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

## Test

```bash
ctest --test-dir build --output-on-failure
```

Or:

```bash
make -C build test
```

## Login and run

The API server defaults to `https://tracker-api.komu.vn`. The OAuth callback is `https://tracker-api.komu.vn/api/0/auth/callback`, matching the URL registered for the Mezon OAuth client.

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
  --server https://tracker-api.komu.vn \
  --token "YOUR_TOKEN" \
  --device-id "YOUR_DEVICE_ID"
```

Or configure them through environment variables:

```bash
export AW_SERVER_URL="https://tracker-api.komu.vn"
export AW_AUTH_TOKEN="YOUR_TOKEN"
export AW_DEVICE_ID="YOUR_DEVICE_ID"

./build/komutracker
```

Configure AFK timeout and polling:

```bash
./build/komutracker \
  --timeout 180 \
  --poll-time 5 \
  --verbose
```

Testing mode uses a 20-second timeout, polls every second, and defaults to `http://localhost:5666`:

```bash
./build/komutracker --testing --verbose
```

Stop the tracker with `Ctrl+C`.

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
-v, --verbose         Additionally print HTTP transport errors (URL + reason)
--version, -V         Print the komutracker version and exit
--timeout SECONDS     Idle time before AFK status
--poll-time SECONDS   AFK polling interval
--window-poll-time SEC Foreground-process polling interval; default: 10
--exclude-title        Send `excluded` instead of the focused window title
--server URL          ActivityWatch-compatible server URL
--token TOKEN         Bearer authentication token
--device-id ID        Device-Id request header
```

Saved credentials:

- Linux device ID: `~/.local/share/komutracker/.device_id`
- Linux token: `~/.cache/komutracker/auth/auth.tracker`
- macOS device ID: `~/Library/Application Support/komutracker/.device_id`
- macOS token: `~/Library/Caches/komutracker/auth/auth.tracker`
- Windows: under `%LOCALAPPDATA%\komutracker`

Token and device files use owner-only permissions on POSIX systems. Tokens are never printed by the CLI.

Environment variables:

```text
AW_SERVER_URL
AW_AUTH_TOKEN
AW_DEVICE_ID
AW_AUTH_URL
AW_CLIENT_ID
AW_REDIRECT_URI
AW_AUTH_TIMEOUT
```

## Logs

The CLI prints progress to the screen, prefixed with the version and a local `[HH:MM:SS]` timestamp, e.g. `komutracker 1.0.0 [14:05:23] afk heartbeat: OK`. It logs:

- **startup** — `komutracker 1.0.0 started for <server>`.
- **authentication polling** — each poll of the token endpoint logs `authentication poll attempt N pending`, `… succeeded`, or `… failed` while waiting for the browser login to finish.
- **data sends** — each window heartbeat logs `foreground-process heartbeat: OK` (or `FAILED`) and each AFK heartbeat logs `afk heartbeat: OK` (or `FAILED`).

The exact HTTP error reason for a failed request (timeout, bad status, etc.) is only printed with `-v`/`--verbose`:

```bash
./build/komutracker --verbose
```

Check the version:

```bash
./build/komutracker --version
# komutracker 1.0.0
```

## Data sent

The combined process publishes two buckets:

- `aw-watcher-afk_<hostname>` / `afkstatus`: `afk` or `not-afk`.
- `aw-watcher-window_<hostname>` / `currentwindow`: the focused process in `app` and its window title in `title`.

Only the foreground process is sent. Background process lists are never collected. Use `--exclude-title` to avoid sending window titles:

```bash
./build/komutracker --exclude-title --window-poll-time 10
```

## Platform backends

- Windows: `GetForegroundWindow`, `QueryFullProcessImageName`, and `GetLastInputInfo`.
- macOS: CoreGraphics window list and idle event APIs. Window titles may require Screen Recording permission.
- Linux: X11 `_NET_ACTIVE_WINDOW`, process metadata, and XScreenSaver. A Wayland session without XWayland is not supported.
