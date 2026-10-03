import assert from 'node:assert/strict';
import { AddressInfo } from 'node:net';
import { afterEach, describe, it } from 'node:test';
import { DataSource } from 'typeorm';
import { createApp } from '../app';
import { AppConfig } from '../config';

const config: AppConfig = {
    port: 0,
    host: '127.0.0.1',
    databaseUrl: '',
    databaseSsl: false,
    databasePoolSize: 1,
    authToken: 'legacy-token',
    reportApiKey: 'report-key',
    mergeWindowSeconds: 370,
    oauth: {
        clientId: '',
        clientSecret: '',
        tokenUrl: '',
        userInfoUrl: '',
        redirectUri: '',
    },
};

const servers: Array<ReturnType<ReturnType<typeof createApp>['listen']>> = [];

afterEach(async () => {
    await Promise.all(servers.splice(0).map((server) => new Promise<void>((resolve, reject) => {
        server.close((error) => error ? reject(error) : resolve());
    })));
});

async function serve(auth: Parameters<typeof createApp>[0]['auth']) {
    const app = createApp({
        dataSource: {} as DataSource,
        config,
        auth,
        activity: { async heartbeat() {} },
        reports: { async usersForDay() { return []; }, async eventsForHostname() { return []; } },
    });
    const server = app.listen(0, '127.0.0.1');
    servers.push(server);
    await new Promise<void>((resolve) => server.once('listening', resolve));
    const address = server.address() as AddressInfo;
    return `http://127.0.0.1:${address.port}`;
}

describe('GET /api/0/auth/me legacy compatibility', () => {
    it('ensures the device user before checking its authenticated session', async () => {
        const calls: string[] = [];
        const baseUrl = await serve({
            async ensureUser(deviceId) { calls.push(`ensure:${deviceId}`); },
            async session() {
                calls.push('session');
                return {
                    id: 'user-id',
                    name: 'Legacy User',
                    email: 'legacy@example.com',
                    deviceId: 'legacy-device',
                    authToken: null,
                };
            },
            async poll() { return null; },
            async callback() { return null; },
            logout() {},
        });

        const response = await fetch(`${baseUrl}/api/0/auth/me`, {
            headers: {
                Authorization: 'Bearer legacy-token',
                'Device-Id': 'legacy-device',
            },
        });

        assert.equal(response.status, 200);
        assert.deepEqual(await response.json(), {
            name: 'Legacy User',
            email: 'legacy@example.com',
        });
        assert.deepEqual(calls, ['ensure:legacy-device', 'session']);
    });

    it('keeps returning unauthorized when Device-Id is missing', async () => {
        let ensureCalls = 0;
        const baseUrl = await serve({
            async ensureUser() { ensureCalls += 1; },
            async session() { return null; },
            async poll() { return null; },
            async callback() { return null; },
            logout() {},
        });

        const response = await fetch(`${baseUrl}/api/0/auth/me`, {
            headers: { Authorization: 'Bearer legacy-token' },
        });

        assert.equal(response.status, 401);
        assert.equal(ensureCalls, 0);
    });
});
