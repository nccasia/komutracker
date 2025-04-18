# Komutracker Client

## Getting Started

### Prerequisites

Ensure you have the following installed:
- Python 3.8
- Poetry (Might install by `make init-poetry`)

### Pull sub-modules
Run `make pull`

### Build and Run
1. Edit `config.py` config file at: `aw-qt\aw_qt\config.py`. Sample:
    ```toml
    [aw-qt]
    autostart_modules = ["aw-watcher-afk", "aw-watcher-window"]
    oauth2_auth_url = "https://oauth2.mezon.ai"
    oauth2_client_id = "1840672452439445504"
    oauth2_redirect_uri = "https://tracker-api.komu.vn/api/0/auth/callback"
    application_domain = "tracker.komu.vn"

    [aw-qt-testing]
    autostart_modules = ["aw-watcher-afk", "aw-watcher-window"]
    ```

2. Edit `config.py` config file at: `aw-client\aw_client\config.py`. Sample:
    ```toml
    [server]
    hostname = "tracker.komu.vn"
    port = "5600"
    secret = "secret"

    [client]
    commit_interval = 300

    [server-testing]
    hostname = "127.0.0.1"
    port = "5666"

    [client-testing]
    commit_interval = 5
    ```

2. Build the project:
    ```sh
    make build
    ```

3. Start the development client:
    - For the server:
      ```sh
      aw-qt
      ```