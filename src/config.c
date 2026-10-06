#include "config.h"

const komutracker_config KOMUTRACKER_CONFIG = {
    .server_url = "https://tracker.komu.vn",
    .auth_url = "https://oauth2.mezon.ai",
    .oauth_client_id = "1840672452439445504",
    .oauth_redirect_uri = "https://tracker.komu.vn/api/0/auth/callback",
    .auth_timeout_seconds = 300,
    .afk_timeout_seconds = 180.0,
    .afk_poll_seconds = 10.0,
    .window_poll_seconds = 10.0,
};

const komutracker_config KOMUTRACKER_TEST_CONFIG = {
    .server_url = "http://127.0.0.1:5666",
    .auth_url = "https://oauth2.mezon.ai",
    .oauth_client_id = "2105929839394426880",
    .oauth_redirect_uri = "http://localhost:5666/api/0/auth/callback",
    .auth_timeout_seconds = 300,
    .afk_timeout_seconds = 20.0,
    .afk_poll_seconds = 1.0,
    .window_poll_seconds = 10.0,
};
