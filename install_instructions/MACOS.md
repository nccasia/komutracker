# KomuTracker installation on macOS

The macOS release provides a menu-bar application in a DMG. A standalone CLI binary can also be distributed separately.

## Option 1: Install the desktop application from DMG

1. Download `KomuTracker-<version>-macOS.dmg`.
2. Open the DMG.
3. Drag `KomuTracker.app` to the **Applications** folder.
4. Open **Applications** and launch **KomuTracker**.
5. Open the KomuTracker menu-bar icon and select **Log In**.

KomuTracker is a menu-bar-only application; it does not open a normal application window.

If macOS reports that the application is from an unidentified developer, Control-click `KomuTracker.app`, select **Open**, and confirm **Open**. Only bypass this warning when the application came from a trusted release.

For an unsigned trusted local build, quarantine can be removed from this application only:

```bash
xattr -dr com.apple.quarantine /Applications/KomuTracker.app
```

Do not disable Gatekeeper system-wide.

To uninstall the desktop application, quit it from the menu-bar menu and move `/Applications/KomuTracker.app` to Trash.

## Option 2: Install only the standalone CLI

The standalone CLI file is named `komutracker`. It must be built for the Mac's architecture, or as a compatible universal binary.

Install it system-wide:

```bash
chmod +x komutracker
sudo install -m 0755 komutracker /usr/local/bin/komutracker
```

Alternatively, run it from its current directory:

```bash
chmod +x komutracker
./komutracker
```

If macOS quarantines a trusted downloaded CLI binary:

```bash
xattr -d com.apple.quarantine ./komutracker
```

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

```text
Device ID: ~/Library/Application Support/komutracker/.device_id
Token:     ~/Library/Caches/komutracker/auth/auth.tracker
Profile:   ~/Library/Caches/komutracker/auth/profile.txt
PID:       ~/Library/Caches/komutracker/tracker.pid
```

Removing the application does not automatically remove these user credentials. Use `komutracker --logout` or the desktop **Log Out** action before uninstalling if the session must also be revoked on the server.

