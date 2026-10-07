# KomuTracker installation on Linux

The published Linux release targets 64-bit Ubuntu. It contains both the desktop tray application and the command-line client.

## Option 1: Install the DEB package

Download `komutracker-<version>-ubuntu-amd64.deb`, open a terminal in the download directory, and run:

```bash
sudo apt install ./komutracker-<version>-ubuntu-amd64.deb
```

Using `apt install` instead of `dpkg -i` allows Ubuntu to install the required runtime libraries automatically.

Launch **KomuTracker** from the application menu, or run:

```bash
komutracker-desktop
```

The desktop application runs in the system tray only; it does not open a normal window or terminal. Open the tray menu and select **Log In**. If the icon is not visible, check the desktop's hidden/background-app indicators.

The DEB also installs the CLI:

```bash
komutracker --version
```

To uninstall the package:

```bash
sudo apt remove komutracker
```

## Option 2: Install only the standalone CLI

The standalone CLI file is named `komutracker`. It is dynamically linked and requires these runtime libraries:

On Ubuntu/Debian:
```bash
sudo apt update
sudo apt install libcurl4 libx11-6 libxss1
```

On Arch Linux:
```bash
sudo pacman -S --needed curl libx11 libxss
```


Install the binary system-wide:

```bash
chmod +x komutracker
sudo install -m 0755 komutracker /usr/local/bin/komutracker
```

Alternatively, run it directly without installing:

```bash
chmod +x komutracker
./komutracker
```

Check whether any runtime library is missing:

```bash
ldd ./komutracker
```

No line should contain `not found`.

## CLI usage

Start tracking and authenticate when prompted:

```bash
komutracker
```

Useful commands:

```bash
komutracker --login       # Start a new login and replace the previous session
komutracker --status      # Show the authenticated account
komutracker --logout      # Revoke the session and remove local credentials
komutracker --no-browser  # Print the login URL instead of opening a browser
komutracker --verbose     # Run with diagnostic output
komutracker --daemon      # Track in the background
```

Only one tracking process can run for the current user. A new successful login for the same account invalidates its previous client session.

## Local files

KomuTracker stores its files under the current user's home directory:

```text
Device ID: ~/.local/share/komutracker/.device_id
Token:     ~/.cache/komutracker/auth/auth.tracker
Profile:   ~/.cache/komutracker/auth/profile.txt
PID:       ~/.cache/komutracker/tracker.pid
```

The corresponding XDG directories are used when `XDG_DATA_HOME` or `XDG_CACHE_HOME` is configured.

