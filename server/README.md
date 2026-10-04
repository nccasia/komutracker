# Komutracker API

Small Node.js server for the native client. It uses TypeORM with PostgreSQL and stores only `users` and `afk_events`.

## Run

Node.js 18 or newer is required:

```bash
cd server
export DATABASE_URL="postgresql://user:password@127.0.0.1:5432/komutracker"
export MEZON_CLIENT_ID="..."
export MEZON_CLIENT_SECRET="..."
export MEZON_REDIRECT_URI="https://your-domain.example/api/0/auth/callback"
export MEZON_LEGACY_REDIRECT_URI="https://tracker-api.komu.vn/api/0/auth/callback"
npm start
```

`MEZON_REDIRECT_URI` is the callback for current clients. While legacy clients still use
`tracker-api.komu.vn`, callbacks received on that exact host use `MEZON_LEGACY_REDIRECT_URI`.
Both callback URLs must be registered with the Mezon OAuth client.

Mezon uses `https://oauth2.mezon.ai/oauth2/token` and `https://oauth2.mezon.ai/userinfo` by default. Set `DATABASE_SSL=true` when PostgreSQL requires TLS. The pool defaults to 20 connections.

If `AUTH_TOKEN` is set, the server uses fixed internal auth and skips OAuth. Otherwise `POST /api/0/auth` registers the device and polls until the Mezon callback stores an access token. The client uses that token for subsequent requests.

## API

- `POST /api/0/auth` with `{ "device_id": "..." }`: ensure the user exists and return a token as a JSON string, or `null` while OAuth is pending.
- `GET /api/0/auth/me`: return the user associated with `Device-Id` and its bearer token.
- `GET /api/0/auth/callback?code=...&state=...`: complete the Mezon authorization code flow.
- `GET /api/0/reports/users?day=YYYY-MM-DD`: return every user with `email`, `mezonId`, `name`, and `active_time` seconds for that UTC+7 day. Requires `X-API-Key: <REPORT_API_KEY>`. The day must be inside the retained 90-calendar-day window.
- `GET /api/0/users/:hostname/events?start=ISO&end=ISO`: public event detail for a hostname and ISO timespan. Hostname is the part before `@` in the user's email. The whole range must be inside the retained 90-calendar-day window in GMT+7.
- `DELETE /api/0/auth`: compatibility no-op returning success.
- `POST /api/0/buckets/:id`: compatibility no-op returning success.
- `POST /api/0/buckets/:id/heartbeat`: persist AFK events; legacy window payloads return 200 and are ignored.

Heartbeat requests need both headers:

```text
Authorization: Bearer <token returned by /api/0/auth>
Device-Id: <device id>
```

The server uses a fixed `mergeWindowSeconds` of 370 seconds by default. Overlapping heartbeats with the same status are always merged; for non-overlapping heartbeats, this value is the maximum gap that can be merged. Client `pulsetime` query parameters are ignored for merge decisions. Override the server policy with `MERGE_WINDOW_SECONDS` if needed.

AFK events are stored with `status`, `start_at`, and `end_at`. Consecutive heartbeats with the same status update the row with the latest end time inside a PostgreSQL transaction; writes are serialized per user so concurrent first heartbeats cannot create duplicate rows. A new row is created when the status changes or a non-overlapping gap exceeds the merge window. Run the TypeORM migrations before starting a production release.

The all-users report requires `X-API-Key`; the per-user event query is public and returns 404 when the hostname is not found. The event query uses the `(user_id, start_at, end_at)` composite index and the daily report filters event overlap before aggregating, so unrelated users and periods are not scanned into the result.

The server keeps the current GMT+7 calendar day plus the preceding 89 days. An in-process cron job (`0 1 * * *`, `Asia/Ho_Chi_Minh`) runs daily at 01:00 GMT+7 and removes activity events that ended before that retention window. Overlapping cleanup runs are skipped.

Build the server with:

```bash
npm run build
```

## Migrations

Run these commands from `server/`. The database connection comes from `DATABASE_URL` in `.env` or the environment:

```bash
# Generate a TypeScript migration
npm run migration:generate --name=AddSomething

# Apply pending migrations
npm run migration:run

# Show migration status
npm run migration:show

# Revert the latest migration
npm run migration:revert
```
