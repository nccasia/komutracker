import express, { Express, NextFunction, Request, Response } from 'express';
import crypto from 'node:crypto';
import path from 'node:path';
import { DataSource } from 'typeorm';
import { createAuth, SessionUser, UserProfile } from './auth';
import { AppConfig } from './config';
import { createActivity } from './activity';
import { createDataSource } from './database';
import { createReports, UserNotFoundError } from './reports';

interface AuthService {
    poll(deviceId: string): Promise<string | null>;
    callback(query: URLSearchParams): Promise<UserProfile | null>;
    session(request: Request): Promise<SessionUser | null>;
    logout(): void;
}

interface ActivityService {
    heartbeat(userId: string, value: { timestamp: string; duration?: number; data: { status?: string } }): Promise<void>;
}

interface ReportsService {
    usersForDay(day: string): Promise<unknown[]>;
    eventsForHostname(hostname: string, start: string, end: string): Promise<unknown[]>;
}

interface AppDependencies {
    dataSource: DataSource;
    config: AppConfig;
    auth?: AuthService;
    activity?: ActivityService;
    reports?: ReportsService;
}

const MAX_BODY = '64kb';
const PUBLIC_DIR = path.join(__dirname, '..', '..', 'public');

function asyncRoute(handler: (request: Request, response: Response, next: NextFunction) => Promise<void>) {
    return (request: Request, response: Response, next: NextFunction): void => {
        void handler(request, response, next).catch(next);
    };
}

function requireSession(auth: AuthService, request: Request, response: Response): Promise<SessionUser | null> {
    return auth.session(request).then((session) => {
        if (!session) response.status(401).json({ error: 'Unauthorized' });
        return session;
    });
}

function hasReportApiKey(request: Request, configuredKey: string): boolean {
    const suppliedKey = request.header('x-api-key');
    if (!configuredKey || !suppliedKey) return false;
    const expected = Buffer.from(configuredKey);
    const actual = Buffer.from(suppliedKey);
    return expected.length === actual.length && crypto.timingSafeEqual(expected, actual);
}

export function createApp({ dataSource, config, auth: providedAuth, activity: providedActivity, reports: providedReports }: AppDependencies): Express {
    const auth = providedAuth || createAuth(dataSource, config);
    const activity = providedActivity || createActivity(dataSource, config);
    const reports = providedReports || createReports(dataSource);
    const app = express();

    app.use(express.json({ limit: MAX_BODY }));
    app.use(express.static(PUBLIC_DIR));

    app.get('/', (_request, response) => response.sendFile(path.join(PUBLIC_DIR, 'index.html')));

    app.get('/health', (_request, response) => response.json({ ok: true }));
    app.get('/api/0/auth/success', (_request, response) => response.json({ message: 'Logged in successfully, welcome to komutracker!' }));

    app.get('/api/0/auth/callback', asyncRoute(async (request, response) => {
        const profile = await auth.callback(new URL(request.originalUrl, 'http://localhost').searchParams);
        response.redirect(`/?username=${profile?.email.split('@')[0] || 'username'}`);
    }));

    app.post('/api/0/auth', asyncRoute(async (request, response) => {
        const token = await auth.poll(String(request.body?.device_id || ''));
        response.json(token);
    }));

    app.delete('/api/0/auth', asyncRoute(async (_request, response) => {
        auth.logout();
        response.status(204).end();
    }));

    app.get('/api/0/auth/me', asyncRoute(async (request, response) => {
        const session = await requireSession(auth, request, response);
        if (!session) return;
        response.json({ name: session.name, email: session.email });
    }));

    app.get('/api/0/reports/users', asyncRoute(async (request, response) => {
        if (!hasReportApiKey(request, config.reportApiKey)) {
            response.status(401).json({ error: 'Invalid report API key' });
            return;
        }
        response.json(await reports.usersForDay(String(request.query.day || '')));
    }));

    app.get('/api/0/users/:hostname/events', asyncRoute(async (request, response) => {
        response.json(await reports.eventsForHostname(
            String(request.params.hostname),
            String(request.query.start || ''),
            String(request.query.end || ''),
        ));
    }));

    app.post('/api/0/buckets/:bucketId', asyncRoute(async (request, response) => {
        const session = await requireSession(auth, request, response);
        if (!session) return;
        response.status(201).json({ ok: true });
    }));

    app.post('/api/0/buckets/:bucketId/heartbeat', asyncRoute(async (request, response) => {
        const session = await requireSession(auth, request, response);
        if (!session) return;
        const input = request.body;
        if (input?.data?.status) {
            await activity.heartbeat(session.id, input);
        }
        response.json({ ok: true });
    }));

    app.use((error: unknown, _request: Request, response: Response, _next: NextFunction) => {
        const message = error instanceof Error ? error.message : 'Internal server error';
        const status = error instanceof UserNotFoundError ? error.statusCode : /Invalid|requires|Unknown|large/.test(message) ? 400 : 500;
        response.status(status).json({ error: message });
    });

    return app;
}

export async function start(config: AppConfig): Promise<ReturnType<Express['listen']>> {
    const dataSource = createDataSource(config);
    await dataSource.initialize();
    const app = createApp({ dataSource, config });
    return app.listen(config.port, config.host, () => {
        console.log(`komutracker API listening on http://${config.host}:${config.port}`);
    });
}
